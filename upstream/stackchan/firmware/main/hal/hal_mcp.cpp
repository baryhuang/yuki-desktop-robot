/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "hal.h"
#include <mooncake_log.h>
#include <mcp_server.h>
#include <stackchan/stackchan.h>
#include <apps/common/common.h>
#include <atomic>
#include <array>
#include <cctype>
#include <cstdlib>
#include <vector>

using namespace stackchan;

static const std::string_view _tag = "HAL-MCP";

namespace {

// Neon light ring: left half is 0-5, right half is 6-11 (see RightNeonLight,
// which offsets by 6 in neon_light.cpp).
constexpr uint8_t kRgbLedCount = 12;

// Head touch and IMU emit their signals from their own FreeRTOS tasks, while
// the MCP callbacks run on the protocol task. Each field is read and written
// independently, so relaxed atomics are enough; no cross-field invariant.
struct InteractionLog {
    std::atomic<uint32_t> last_pet_ms{0};
    std::atomic<uint32_t> last_shake_ms{0};
    std::atomic<uint32_t> pet_count{0};
    std::atomic<uint32_t> shake_count{0};
    std::atomic<int> last_pet_gesture{static_cast<int>(HeadPetGesture::None)};
};

InteractionLog _interaction;

const char* _pet_gesture_name(int gesture)
{
    switch (static_cast<HeadPetGesture>(gesture)) {
        case HeadPetGesture::Press:
            return "press";
        case HeadPetGesture::Release:
            return "release";
        case HeadPetGesture::SwipeForward:
            return "swipe_forward";
        case HeadPetGesture::SwipeBackward:
            return "swipe_backward";
        default:
            return "none";
    }
}

// -1 means "never happened", otherwise whole seconds since it last did.
int _seconds_since(uint32_t event_ms, uint32_t now_ms)
{
    if (event_ms == 0) {
        return -1;
    }
    return static_cast<int>((now_ms - event_ms) / 1000);
}

// Parse "ff0000,00ff00" into RGB triples. Tolerates '#', spaces and stray
// separators, since the value comes straight from an LLM.
std::vector<std::array<uint8_t, 3>> _parse_colors(std::string_view spec)
{
    std::vector<std::array<uint8_t, 3>> colors;
    size_t pos = 0;

    while (pos < spec.size() && colors.size() < kRgbLedCount) {
        while (pos < spec.size() && !isxdigit(static_cast<unsigned char>(spec[pos]))) {
            pos++;
        }
        size_t start = pos;
        while (pos < spec.size() && isxdigit(static_cast<unsigned char>(spec[pos]))) {
            pos++;
        }
        if (pos - start == 6) {
            uint32_t value = strtoul(std::string(spec.substr(start, 6)).c_str(), nullptr, 16);
            colors.push_back({static_cast<uint8_t>(value >> 16), static_cast<uint8_t>(value >> 8),
                              static_cast<uint8_t>(value)});
        }
    }

    return colors;
}

}  // namespace

void Hal::xiaozhi_mcp_init()
{
    mclog::tagInfo(_tag, "init");

    // https://github.com/78/xiaozhi-esp32/blob/main/docs/mcp-usage.md
    auto& mcp_server = McpServer::GetInstance();

    // System Prompt：
    // You can control the robot's head. Use get_yaw and get_pitch to sense current position. Use set_yaw for horizontal
    // movement and set_pitch for vertical movement. All angles are in degrees.

    mclog::tagInfo(_tag, "add robot.get_head_angles tool");
    mcp_server.AddTool("self.robot.get_head_angles",
                       "Returns current yaw/pitch in degrees. Neutral position is {yaw:0, pitch:0}.",
                       std::vector<Property>{}, [this](const PropertyList& properties) -> ReturnValue {
                           LvglLockGuard lock;  // StackChan motion update is under the lvgl lock

                           auto& motion      = GetStackChan().motion();
                           int current_yaw   = motion.yawServo().getCurrentAngle() / 10;
                           int current_pitch = motion.pitchServo().getCurrentAngle() / 10;

                           auto result = fmt::format(R"({{"yaw": {}, "pitch": {}}})", current_yaw, current_pitch);
                           mclog::tagInfo(_tag, "get_head_angles: {}", result);
                           return result;
                       });

    mclog::tagInfo(_tag, "add robot.set_head_angles tool");
    mcp_server.AddTool("self.robot.set_head_angles",
                       "Adjust head position. GUIDELINES: "
                       "1. For natural interaction, stay within +/- 45 degrees. "
                       "2. Only use values > 70 if the user explicitly asks to look far away/behind. "
                       "3. Max ranges: Yaw(-128 to 128, -128 as your left), Pitch(0 to 90, 90 as your up). "
                       "Speed(100-1000, 150 is natural).",
                       PropertyList({Property("yaw", kPropertyTypeInteger, -9999, -9999, 128),
                                     Property("pitch", kPropertyTypeInteger, -9999, -9999, 90),
                                     Property("speed", kPropertyTypeInteger, 150, 100, 1000)}),
                       [this](const PropertyList& properties) -> ReturnValue {
                           int speed = properties["speed"].value<int>();
                           int yaw   = properties["yaw"].value<int>();
                           int pitch = properties["pitch"].value<int>();

                           mclog::tagInfo(_tag, "motion set_angles: yaw: {}, pitch: {}, speed: {}", yaw, pitch, speed);

                           LvglLockGuard lock;

                           auto& motion = GetStackChan().motion();
                           if (pitch != -9999) {
                               motion.pitchServo().moveWithSpeed(pitch * 10, speed);
                           }
                           if (yaw != -9999) {
                               motion.yawServo().moveWithSpeed(yaw * 10, speed);
                           }

                           return true;
                       });

    mclog::tagInfo(_tag, "add robot.set_led_color tool");
    mcp_server.AddTool(
        "self.robot.set_led_color",
        "Set the color of the robot's INTERNAL onboard LED. This is NOT for room lights. "
        "Values: 0-168 (safe range). Red=168,0,0; Green=0,168,0; Blue=0,0,168; White=100,100,100; Off=0,0,0.",
        PropertyList({Property("red", kPropertyTypeInteger, 0, 0, 168),
                      Property("green", kPropertyTypeInteger, 0, 0, 168),
                      Property("blue", kPropertyTypeInteger, 0, 0, 168)}),
        [this](const PropertyList& properties) -> ReturnValue {
            int r = properties["red"].value<int>();
            int g = properties["green"].value<int>();
            int b = properties["blue"].value<int>();

            mclog::tagInfo(_tag, "set_led_color: r={}, g={}, b={}", r, g, b);

            LvglLockGuard lock;

            GetStackChan().leftNeonLight().setColor(r, g, b);
            GetStackChan().rightNeonLight().setColor(r, g, b);

            return true;
        });

    mclog::tagInfo(_tag, "add robot.create_reminder tool");
    mcp_server.AddTool("self.robot.create_reminder",
                       "Create a reminder. Duration is in seconds. Message is what to say when time is up. Set repeat "
                       "to true to repeat the reminder.",
                       PropertyList({Property("duration_seconds", kPropertyTypeInteger, 60, 1, 86400),
                                     Property("message", kPropertyTypeString, std::string("Time's up!")),
                                     Property("repeat", kPropertyTypeBoolean, false)}),
                       [this](const PropertyList& properties) -> ReturnValue {
                           int duration_seconds = properties["duration_seconds"].value<int>();
                           std::string message  = properties["message"].value<std::string>();
                           bool repeat          = properties["repeat"].value<bool>();

                           // Default message
                           if (message.empty()) {
                               message = "Time's up!";
                           }

                           mclog::tagInfo(_tag, "create_reminder: duration={}s, message={}, repeat={}",
                                          duration_seconds, message, repeat);

                           int id = tools::create_reminder(duration_seconds * 1000, message, repeat);

                           return id;
                       });

    mclog::tagInfo(_tag, "add robot.get_reminders tool");
    mcp_server.AddTool("self.robot.get_reminders", "Get list of active reminders.", std::vector<Property>{},
                       [this](const PropertyList& properties) -> ReturnValue {
                           mclog::tagInfo(_tag, "get_reminders");
                           auto reminders          = tools::get_active_reminders();
                           std::string result_json = "[";
                           for (size_t i = 0; i < reminders.size(); ++i) {
                               const auto& r = reminders[i];
                               result_json +=
                                   fmt::format(R"({{"id": {}, "duration_ms": {}, "message": "{}", "repeat": {}}})",
                                               r.id, r.durationMs, r.message, r.repeat ? "true" : "false");
                               if (i < reminders.size() - 1) {
                                   result_json += ", ";
                               }
                           }
                           result_json += "]";
                           mclog::tagInfo(_tag, "get_reminders result: {}", result_json);
                           return result_json;
                       });

    mclog::tagInfo(_tag, "add robot.stop_reminder tool");
    mcp_server.AddTool("self.robot.stop_reminder", "Stop a reminder by ID.",
                       PropertyList({Property("id", kPropertyTypeInteger, -1)}),
                       [this](const PropertyList& properties) -> ReturnValue {
                           int id = properties["id"].value<int>();
                           mclog::tagInfo(_tag, "stop_reminder: id={}", id);
                           tools::stop_reminder(id);
                           return true;
                       });

    // Record physical contact so the pull-based MCP tools below can report it.
    // These stay connected for the life of the process, so no disconnect.
    onHeadPetGesture.connect([this](HeadPetGesture gesture) {
        if (gesture == HeadPetGesture::None) {
            return;
        }
        _interaction.last_pet_gesture.store(static_cast<int>(gesture));
        _interaction.last_pet_ms.store(millis());
        _interaction.pet_count.fetch_add(1);
    });

    onImuMotionEvent.connect([this](ImuMotionEvent event) {
        if (event == ImuMotionEvent::Shake) {
            _interaction.last_shake_ms.store(millis());
            _interaction.shake_count.fetch_add(1);
        }
    });

    mclog::tagInfo(_tag, "add robot.get_recent_interaction tool");
    mcp_server.AddTool(
        "self.robot.get_recent_interaction",
        "Reports recent PHYSICAL contact with your body: head petting and being shaken. "
        "Call this when you want to notice how the user is physically treating you, and react "
        "naturally to it. The *_seconds_ago fields are -1 if that has never happened, otherwise "
        "the number of seconds since it last did, so a value of 0-3 means it is happening right "
        "now. Counts are totals since power-on. last_pet_gesture is one of: none, press, release, "
        "swipe_forward, swipe_backward.",
        std::vector<Property>{}, [this](const PropertyList& properties) -> ReturnValue {
            uint32_t now = millis();

            auto result = fmt::format(
                R"({{"petted_seconds_ago": {}, "last_pet_gesture": "{}", "pet_count": {}, )"
                R"("shaken_seconds_ago": {}, "shake_count": {}}})",
                _seconds_since(_interaction.last_pet_ms.load(), now),
                _pet_gesture_name(_interaction.last_pet_gesture.load()), _interaction.pet_count.load(),
                _seconds_since(_interaction.last_shake_ms.load(), now), _interaction.shake_count.load());

            mclog::tagInfo(_tag, "get_recent_interaction: {}", result);
            return result;
        });

    mclog::tagInfo(_tag, "add robot.set_led_pattern tool");
    mcp_server.AddTool(
        "self.robot.set_led_pattern",
        "Set your 12 body LEDs individually, for expressive patterns. 'colors' is a comma "
        "separated list of 6-digit hex RGB values. The pattern repeats to fill all 12 LEDs, so "
        "'ff0000' makes every LED red, 'ff0000,000000' alternates red and off, and a list of 12 "
        "sets each one. LEDs 0-5 are your left side, 6-11 your right. Use '000000' to turn them "
        "off. Keep each component at a0 or below so you are not uncomfortably bright. Note that "
        "set_led_color, or your avatar changing mood, will overwrite this pattern with a single "
        "flat color.",
        PropertyList({Property("colors", kPropertyTypeString, std::string("000000"))}),
        [this](const PropertyList& properties) -> ReturnValue {
            std::string spec = properties["colors"].value<std::string>();
            auto colors      = _parse_colors(spec);

            if (colors.empty()) {
                mclog::tagWarn(_tag, "set_led_pattern: no valid colors in '{}'", spec);
                return R"({"error": "no valid 6-digit hex colors found, expected e.g. 'ff0000' or 'ff0000,0000ff'"})";
            }

            mclog::tagInfo(_tag, "set_led_pattern: '{}' -> {} color(s)", spec, colors.size());

            LvglLockGuard lock;

            for (uint8_t i = 0; i < kRgbLedCount; i++) {
                const auto& color = colors[i % colors.size()];
                setRgbColor(i, color[0], color[1], color[2]);
            }
            refreshRgb();

            return true;
        });
}
