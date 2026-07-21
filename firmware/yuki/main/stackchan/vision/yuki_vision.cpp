/*
 * SPDX-FileCopyrightText: 2026 Bury Huang
 *
 * SPDX-License-Identifier: MIT
 */
#include "yuki_vision.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>

#include <esp_heap_caps.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <human_face_detect.hpp>
#include <linux/videodev2.h>

#include <hal/board/hal_bridge.h>
#include <hal/board/stackchan_camera.h>
#include <hal/hal.h>

namespace stackchan {
namespace {

constexpr char kTag[]                  = "YukiVision";
constexpr int kMaxFrameBytes           = 320 * 240 * 3;
constexpr int kMotionColumns           = 40;
constexpr int kMotionRows              = 30;
constexpr uint32_t kCaptureIntervalMs  = 280;
constexpr uint32_t kStartupDelayMs     = 3000;
constexpr uint32_t kFaceTimeoutMs      = 1100;
constexpr uint32_t kGestureWindowMs    = 1800;
constexpr uint32_t kGestureCooldownMs  = 5000;

std::atomic<int> face_x{-1};
std::atomic<int> face_y{-1};
std::atomic<int> frame_width{0};
std::atomic<int> frame_height{0};
std::atomic<uint32_t> face_seen_at{0};
std::atomic<bool> vision_started{false};
std::atomic<bool> vision_enabled{false};
std::atomic<uint32_t> manual_override_until{0};

uint8_t sample_luma(const uint8_t* frame, int width, int x, int y, int format)
{
    if (format == V4L2_PIX_FMT_YUYV) {
        const size_t pair_offset = (static_cast<size_t>(y) * width + (x & ~1)) * 2;
        return frame[pair_offset + ((x & 1) ? 2 : 0)];
    }
    if (format == V4L2_PIX_FMT_GREY) {
        return frame[static_cast<size_t>(y) * width + x];
    }
    if (format == V4L2_PIX_FMT_RGB565) {
        const auto* pixels = reinterpret_cast<const uint16_t*>(frame);
        const uint16_t pixel = pixels[static_cast<size_t>(y) * width + x];
        const int red = (pixel >> 11) & 0x1f;
        const int green = (pixel >> 5) & 0x3f;
        const int blue = pixel & 0x1f;
        return static_cast<uint8_t>((red * 77 + green * 75 + blue * 29) >> 5);
    }
    if (format == V4L2_PIX_FMT_RGB24) {
        const size_t offset = (static_cast<size_t>(y) * width + x) * 3;
        return static_cast<uint8_t>((frame[offset] * 77 + frame[offset + 1] * 150 + frame[offset + 2] * 29) >> 8);
    }
    return 0;
}

dl::image::pix_type_t to_dl_pixel_type(int format)
{
    switch (format) {
        case V4L2_PIX_FMT_YUYV:
            return dl::image::DL_IMAGE_PIX_TYPE_YUYV;
        case V4L2_PIX_FMT_GREY:
            return dl::image::DL_IMAGE_PIX_TYPE_GRAY;
        case V4L2_PIX_FMT_RGB565:
            return dl::image::DL_IMAGE_PIX_TYPE_RGB565LE;
        case V4L2_PIX_FMT_RGB24:
        default:
            return dl::image::DL_IMAGE_PIX_TYPE_RGB888;
    }
}

class WaveDetector {
public:
    bool update(const uint8_t* frame, int width, int height, int format, uint32_t now)
    {
        uint8_t current[kMotionColumns * kMotionRows];
        int moving_pixels = 0;
        int motion_x_sum = 0;

        for (int row = 0; row < kMotionRows; ++row) {
            const int source_y = row * height / kMotionRows;
            for (int column = 0; column < kMotionColumns; ++column) {
                const int source_x = column * width / kMotionColumns;
                const int index = row * kMotionColumns + column;
                current[index] = sample_luma(frame, width, source_x, source_y, format);
                if (has_previous_ && std::abs(static_cast<int>(current[index]) - previous_[index]) > 34) {
                    ++moving_pixels;
                    motion_x_sum += column;
                }
            }
        }

        memcpy(previous_, current, sizeof(previous_));
        if (!has_previous_) {
            has_previous_ = true;
            return false;
        }

        if (window_started_at_ != 0 && now - window_started_at_ > kGestureWindowMs) {
            reset_window();
        }
        if (moving_pixels < 60 || moving_pixels > 720) {
            return false;
        }

        const int center_x = motion_x_sum / moving_pixels;
        if (window_started_at_ == 0) {
            window_started_at_ = now;
            previous_center_x_ = center_x;
            min_center_x_ = center_x;
            max_center_x_ = center_x;
            return false;
        }

        min_center_x_ = std::min(min_center_x_, center_x);
        max_center_x_ = std::max(max_center_x_, center_x);
        const int delta = center_x - previous_center_x_;
        if (std::abs(delta) >= 4) {
            const int direction = delta > 0 ? 1 : -1;
            if (last_direction_ != 0 && direction != last_direction_) {
                ++direction_changes_;
            }
            last_direction_ = direction;
            previous_center_x_ = center_x;
        }

        const bool is_wave = direction_changes_ >= 2 && max_center_x_ - min_center_x_ >= 12;
        if (is_wave) {
            reset_window();
            if (now - last_triggered_at_ >= kGestureCooldownMs) {
                last_triggered_at_ = now;
                return true;
            }
        }
        return false;
    }

private:
    void reset_window()
    {
        window_started_at_ = 0;
        previous_center_x_ = 0;
        min_center_x_ = kMotionColumns;
        max_center_x_ = 0;
        last_direction_ = 0;
        direction_changes_ = 0;
    }

    uint8_t previous_[kMotionColumns * kMotionRows] = {};
    bool has_previous_ = false;
    uint32_t window_started_at_ = 0;
    uint32_t last_triggered_at_ = 0;
    int previous_center_x_ = 0;
    int min_center_x_ = kMotionColumns;
    int max_center_x_ = 0;
    int last_direction_ = 0;
    int direction_changes_ = 0;
};

void yuki_vision_task(void*)
{
    while (!vision_enabled.load()) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    // The first listening transition overlaps Xiaozhi's MCP tool discovery.
    // Let those short TLS responses finish before the face model claims SRAM.
    vTaskDelay(pdMS_TO_TICKS(kStartupDelayMs));

    auto* frame = static_cast<uint8_t*>(heap_caps_malloc(kMaxFrameBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (frame == nullptr) {
        ESP_LOGE(kTag, "Unable to allocate vision frame buffer");
        vision_started.store(false);
        vTaskDelete(nullptr);
        return;
    }

    auto detector = std::make_unique<HumanFaceDetect>();

    WaveDetector wave_detector;
    float filtered_face_x = 0.0f;
    float filtered_face_y = 0.0f;
    int stable_face_samples = 0;
    uint32_t last_stack_log_at = 0;
    ESP_LOGI(kTag, "Face following and wave wake are active");

    while (true) {
        auto* camera = hal_bridge::board_get_camera();
        size_t length = 0;
        int width = 0;
        int height = 0;
        int format = 0;
        if (camera != nullptr && camera->CaptureForVision(frame, kMaxFrameBytes, length, width, height, format)) {
            const uint32_t now = GetHAL().millis();
            dl::image::img_t image = {
                .data = frame,
                .width = static_cast<uint16_t>(width),
                .height = static_cast<uint16_t>(height),
                .pix_type = to_dl_pixel_type(format),
            };
            auto& faces = detector->run(image);
            if (last_stack_log_at == 0 || now - last_stack_log_at >= 30000) {
                ESP_LOGI(kTag, "Vision task stack free: %u bytes",
                         static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
                last_stack_log_at = now;
            }
            if (!faces.empty()) {
                const auto best = std::max_element(faces.begin(), faces.end(), [](const auto& left, const auto& right) {
                    return left.box_area() < right.box_area();
                });
                if (best != faces.end() && best->box.size() >= 4) {
                    const float detected_x = (best->box[0] + best->box[2]) * 0.5f;
                    const float detected_y = (best->box[1] + best->box[3]) * 0.5f;
                    const bool discontinuity = stable_face_samples > 0 &&
                                               (std::abs(detected_x - filtered_face_x) > width * 0.18f ||
                                                std::abs(detected_y - filtered_face_y) > height * 0.18f);
                    if (stable_face_samples == 0 || discontinuity) {
                        filtered_face_x = detected_x;
                        filtered_face_y = detected_y;
                        stable_face_samples = 1;
                    } else {
                        constexpr float kFaceFilterAlpha = 0.24f;
                        filtered_face_x += (detected_x - filtered_face_x) * kFaceFilterAlpha;
                        filtered_face_y += (detected_y - filtered_face_y) * kFaceFilterAlpha;
                        stable_face_samples = std::min(stable_face_samples + 1, 10);
                    }

                    if (stable_face_samples >= 3) {
                        face_x.store(static_cast<int>(filtered_face_x));
                        face_y.store(static_cast<int>(filtered_face_y));
                        frame_width.store(width);
                        frame_height.store(height);
                        face_seen_at.store(now);
                    }
                }
            } else if (face_seen_at.load() != 0 && now - face_seen_at.load() > kFaceTimeoutMs) {
                stable_face_samples = 0;
            }

            if (wave_detector.update(frame, width, height, format, now) && hal_bridge::is_xiaozhi_ready() &&
                hal_bridge::is_xiaozhi_idle()) {
                ESP_LOGI(kTag, "Wave gesture detected; waking conversation");
                hal_bridge::toggle_xiaozhi_chat_state();
            }
        }
        vTaskDelay(pdMS_TO_TICKS(kCaptureIntervalMs));
    }
}

}  // namespace

void YukiFaceTrackingModifier::_update(Modifiable& stackchan)
{
    if (!stackchan.hasAvatar()) {
        return;
    }

    const uint32_t now = GetHAL().millis();
    if (static_cast<int32_t>(manual_override_until.load() - now) > 0) {
        if (tracking_) {
            stackchan.motion().setModifyLock(false);
            stackchan.avatar().leftEye().setPosition({0, 0});
            stackchan.avatar().rightEye().setPosition({0, 0});
            tracking_ = false;
        }
        if (head_tracking_) {
            stackchan.motion().setAutoAngleSyncEnabled(true);
            head_tracking_ = false;
        }
        return;
    }
    const uint32_t seen_at = face_seen_at.load();
    const int width = frame_width.load();
    const int height = frame_height.load();
    const bool has_face = seen_at != 0 && now - seen_at <= kFaceTimeoutMs && width > 0 && height > 0;

    if (!has_face) {
        if (tracking_) {
            ESP_LOGI(kTag, "Face lost; centering gaze and stopping head tracking");
            stackchan.motion().setModifyLock(false);
            stackchan.avatar().leftEye().setPosition({0, 0});
            stackchan.avatar().rightEye().setPosition({0, 0});
            tracking_ = false;
        }
        if (head_tracking_) {
            stackchan.motion().setAutoAngleSyncEnabled(true);
            head_tracking_ = false;
        }
        return;
    }

    if (!tracking_) {
        ESP_LOGI(kTag, "Stable face detected at (%d,%d) in %dx%d frame", face_x.load(), face_y.load(), width, height);
    }
    tracking_ = true;
    const float error_x = (face_x.load() - width * 0.5f) / (width * 0.5f);
    const float error_y = (face_y.load() - height * 0.5f) / (height * 0.5f);
    const int gaze_x = std::clamp(static_cast<int>(error_x * 65.0f), -65, 65);
    const int gaze_y = std::clamp(static_cast<int>(error_y * 50.0f), -50, 50);
    stackchan.avatar().leftEye().setPosition({gaze_x, gaze_y});
    stackchan.avatar().rightEye().setPosition({gaze_x, gaze_y});

    // Before wake-up Yuki may look with her eyes, but the physical head stays
    // still. Once the conversation is active, face tracking owns head motion.
    const bool conversation_active = hal_bridge::is_xiaozhi_ready() && !hal_bridge::is_xiaozhi_idle();
    if (!conversation_active) {
        voice_pause_until_ = 0;
        voice_pause_latched_ = false;
        stackchan.motion().setModifyLock(false);
        if (head_tracking_) {
            stackchan.motion().setAutoAngleSyncEnabled(true);
            head_tracking_ = false;
        }
        return;
    }

    const bool voice_detected = hal_bridge::is_xiaozhi_voice_detected();
    if (voice_detected && !voice_pause_latched_) {
        ESP_LOGI(kTag, "User speech detected; briefly pausing physical head");
        voice_pause_until_ = now + 1200;
        voice_pause_latched_ = true;
    } else if (!voice_detected) {
        voice_pause_latched_ = false;
    }
    if (static_cast<int32_t>(voice_pause_until_ - now) > 0) {
        // The pause is edge-triggered and bounded. A stale VAD signal must not
        // keep the physical head frozen for the rest of the conversation.
        stackchan.motion().setModifyLock(false);
        return;
    }
    stackchan.motion().setModifyLock(true);

    if (!head_tracking_) {
        // These servos do not return reliable position feedback. Keep tracking
        // continuity in the motion engine instead of blocking on UART reads.
        stackchan.motion().setAutoAngleSyncEnabled(false);
        head_tracking_ = true;
        ESP_LOGI(kTag, "Physical face tracking enabled");
    }

    if (now < next_motion_tick_ || (std::abs(error_x) < 0.10f && std::abs(error_y) < 0.12f)) {
        return;
    }
    const int requested_yaw = std::clamp(static_cast<int>(error_x * 360.0f), -350, 350);
    const int requested_pitch = std::clamp(90 - static_cast<int>(error_y * 100.0f), 30, 220);
    const int target_yaw = std::clamp(requested_yaw, last_target_yaw_ - 80, last_target_yaw_ + 80);
    const int target_pitch = std::clamp(requested_pitch, last_target_pitch_ - 35, last_target_pitch_ + 35);
    if (std::abs(target_yaw - last_target_yaw_) < 35 && std::abs(target_pitch - last_target_pitch_) < 18) {
        return;
    }

    next_motion_tick_ = now + 650;
    last_target_yaw_ = target_yaw;
    last_target_pitch_ = target_pitch;
    ESP_LOGI(kTag, "Tracking head target yaw=%d pitch=%d (face error %.2f,%.2f)", target_yaw, target_pitch,
             static_cast<double>(error_x), static_cast<double>(error_y));
    stackchan.motion().moveWithSpeed(target_yaw, target_pitch, 120);
}

void StartYukiVision()
{
    bool expected = false;
    if (!vision_started.compare_exchange_strong(expected, true)) {
        return;
    }
    if (xTaskCreatePinnedToCore(yuki_vision_task, "yuki_vision", 8192, nullptr, 2, nullptr, 0) != pdPASS) {
        ESP_LOGE(kTag, "Unable to start vision task");
        vision_started.store(false);
    }
}

void EnableYukiVision()
{
    vision_enabled.store(true);
}

bool YukiFaceSeenRecently(uint32_t within_ms)
{
    const uint32_t seen_at = face_seen_at.load();
    return seen_at != 0 && GetHAL().millis() - seen_at <= within_ms;
}

void YieldYukiFaceTracking(uint32_t duration_ms)
{
    manual_override_until.store(GetHAL().millis() + duration_ms);
}

}  // namespace stackchan
