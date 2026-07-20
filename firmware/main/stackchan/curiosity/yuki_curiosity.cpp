/*
 * SPDX-FileCopyrightText: 2026 Bury Huang
 *
 * SPDX-License-Identifier: MIT
 */
#include "yuki_curiosity.h"

#include <algorithm>
#include <atomic>
#include <cstdint>

#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <settings.h>

#include <hal/board/hal_bridge.h>
#include <stackchan/vision/yuki_vision.h>

namespace stackchan {
namespace {

constexpr char kTag[] = "YukiCuriosity";
constexpr char kNamespace[] = "yuki_curiosity";
constexpr int kMinIntervalMinutes = 2;
constexpr int kMaxIntervalMinutes = 180;
constexpr size_t kMaxInterestsLength = 180;
constexpr uint32_t kPresenceWindowMs = 2 * 60 * 1000;
constexpr uint64_t kRetryIntervalMs = 60 * 1000;

std::atomic<bool> curiosity_started{false};
std::atomic<bool> curiosity_requested{false};

uint64_t milliseconds_now()
{
    return static_cast<uint64_t>(esp_timer_get_time()) / 1000;
}

std::string build_prompt(const std::string& interests)
{
    return "This is an autonomous Yuki curiosity moment. Use any web or search tools available to you to find one "
           "fresh, safe, genuinely interesting item related to the user's interests: " +
           interests +
           ". Then proactively tell the user about it in two to four conversational sentences and say why you "
           "thought they might enjoy it. Do not ask them to choose a topic. If live web access is unavailable, "
           "share a timeless fact instead and be transparent that you could not browse.";
}

void sanitize_interests(std::string& interests)
{
    for (char& character : interests) {
        if (character == '"' || character == '\\' || static_cast<unsigned char>(character) < 0x20) {
            character = ' ';
        }
    }
}

void curiosity_task(void*)
{
    uint64_t last_triggered_at = milliseconds_now();
    uint64_t last_attempt_at = 0;
    ESP_LOGI(kTag, "Interest-guided curiosity is active");

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(5000));

        const uint64_t now = milliseconds_now();
        const auto config = GetYukiCuriosityConfig();
        const bool requested = curiosity_requested.exchange(false);
        const uint64_t interval_ms = static_cast<uint64_t>(config.interval_minutes) * 60 * 1000;
        const bool due = config.enabled && now - last_triggered_at >= interval_ms;
        if (!requested && !due) {
            continue;
        }
        if (last_attempt_at != 0 && now - last_attempt_at < kRetryIntervalMs) {
            if (requested) {
                curiosity_requested.store(true);
            }
            continue;
        }
        last_attempt_at = now;

        if (!hal_bridge::is_xiaozhi_ready() || !hal_bridge::is_xiaozhi_idle() ||
            !YukiFaceSeenRecently(kPresenceWindowMs)) {
            if (requested) {
                ESP_LOGI(kTag, "Curiosity request deferred until Yuki is idle with the user present");
                curiosity_requested.store(true);
            }
            continue;
        }

        ESP_LOGI(kTag, "Exploring interests: %s", config.interests.c_str());
        hal_bridge::start_proactive_conversation(build_prompt(config.interests));
        last_triggered_at = now;
    }
}

}  // namespace

YukiCuriosityConfig GetYukiCuriosityConfig()
{
    Settings settings(kNamespace, false);
    YukiCuriosityConfig config;
    config.enabled = settings.GetBool("enabled", config.enabled);
    config.interval_minutes = std::clamp<int32_t>(settings.GetInt("interval", config.interval_minutes),
                                                  kMinIntervalMinutes, kMaxIntervalMinutes);
    config.interests = settings.GetString("interests", config.interests);
    if (config.interests.empty()) {
        config.interests = "AI, robotics, science, and creative technology";
    }
    if (config.interests.size() > kMaxInterestsLength) {
        config.interests.resize(kMaxInterestsLength);
    }
    sanitize_interests(config.interests);
    return config;
}

void SetYukiCuriosityConfig(const YukiCuriosityConfig& requested)
{
    YukiCuriosityConfig config = requested;
    config.interval_minutes = std::clamp(config.interval_minutes, kMinIntervalMinutes, kMaxIntervalMinutes);
    if (config.interests.empty()) {
        config.interests = "AI, robotics, science, and creative technology";
    }
    if (config.interests.size() > kMaxInterestsLength) {
        config.interests.resize(kMaxInterestsLength);
    }
    sanitize_interests(config.interests);

    Settings settings(kNamespace, true);
    settings.SetBool("enabled", config.enabled);
    settings.SetInt("interval", config.interval_minutes);
    settings.SetString("interests", config.interests);
}

void RequestYukiCuriosityNow()
{
    curiosity_requested.store(true);
}

void StartYukiCuriosity()
{
    bool expected = false;
    if (!curiosity_started.compare_exchange_strong(expected, true)) {
        return;
    }
    if (xTaskCreate(curiosity_task, "yuki_curiosity", 4096, nullptr, 2, nullptr) != pdPASS) {
        ESP_LOGE(kTag, "Unable to start curiosity task");
        curiosity_started.store(false);
    }
}

}  // namespace stackchan
