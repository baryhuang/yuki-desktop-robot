# Yuki: The Curious Desktop Robot

![Yuki character reference](docs/assets/yuki-character-reference.jpg)

> An expressive desktop robot that follows your face, wakes to gestures, explores the web around your interests, and starts conversations through animation, voice, movement, and light.

Yuki is an embodied AI companion built for OpenAI Build Week 2026 on the M5Stack StackChan K151. The project extends the open-source factory firmware in native C and C++ instead of replacing it with a simplified demonstration.

## Reviewer start here

This repository retains the upstream StackChan firmware so Yuki can be built and tested on the real device. All Yuki hackathon work starts at the [`hackathon-upstream-baseline`](https://github.com/baryhuang/yuki-desktop-robot/tree/hackathon-upstream-baseline) tag. Review the complete implementation in one comparison: [Yuki hackathon diff](https://github.com/baryhuang/yuki-desktop-robot/compare/hackathon-upstream-baseline...main).

For the shortest review path, start with:

- [`firmware/main/stackchan/avatar/skins/yuki/`](firmware/main/stackchan/avatar/skins/yuki/) - original C++/LVGL character renderer and limited-animation scheduler
- [`firmware/main/stackchan/vision/yuki_vision.cpp`](firmware/main/stackchan/vision/yuki_vision.cpp) - ESP-DL face tracking, gaze, wave wake, and servo safety coordination
- [`firmware/main/stackchan/curiosity/yuki_curiosity.cpp`](firmware/main/stackchan/curiosity/yuki_curiosity.cpp) - idle-time interest-guided web curiosity behavior
- [`firmware/main/hal/hal_mcp.cpp`](firmware/main/hal/hal_mcp.cpp) - robot capability MCP tools exposed to the configured agent backend
- [`firmware/patches/xiaozhi-esp32.patch`](firmware/patches/xiaozhi-esp32.patch) - required changes to the fetched Xiaozhi runtime, kept as a patch rather than committing an opaque nested repository

## Project status

Implemented during the hackathon:

- `self.robot.get_recent_interaction`, an MCP tool that reports recent head-touch gestures and shaking with event age and counts
- `self.robot.set_led_pattern`, an MCP tool that controls all 12 body LEDs individually or as repeating patterns
- An original native C++/LVGL Yuki character skin with animated eyes, mouth, gaze, blinking, speech, and emotion states
- On-device ESP-DL face detection with coordinated eye gaze and safety-limited pan/tilt tracking
- Camera motion analysis that recognizes a deliberate left-right wave and wakes the conversation from standby
- A mutex-protected camera path that allows continuous perception to coexist with photos and visual questions
- Speech-state coordination that synchronizes Yuki's mouth, safety-aware head gestures, and two-color LED pulses while preserving face-tracking priority
- Configurable interest-guided curiosity that autonomously asks the active backend to explore the web and start a short conversation, gated by idle state and recent face presence
- MCP tools to configure curiosity, inspect its settings, and trigger an immediate demo without tying the firmware to one LLM provider
- A verified ESP-IDF 5.5.4 build for the ESP32-S3 hardware
- A verified 16 MB flash layout with dual OTA slots, a dedicated face-model partition, assets, coredump storage, and untouched calibration NVS

In active development:

- Hardware tuning of face-tracking direction, gain, and wave-detection thresholds in varied lighting

The runtime language-model backend is intentionally replaceable. Yuki's perception, animation, physical behavior, and MCP interface remain native to the robot.

## Hardware

- M5Stack StackChan K151
- M5Stack CoreS3 with ESP32-S3, 16 MB flash, and 8 MB PSRAM
- 2-inch touch display and 0.3 MP camera
- Two microphones and a speaker
- Two feedback servos for pan and tilt
- 12 individually addressable RGB LEDs
- Head-touch sensor and 9-axis IMU

## Build

The firmware requires ESP-IDF 5.5.4.

```sh
cd firmware
python3 ./fetch_repos.py
. "$HOME/esp/esp-idf/export.sh"
idf.py set-target esp32s3
idf.py build
```

## Flash

Use a full `idf.py flash`, not `app-flash`. Yuki uses a custom dual-OTA layout with separate face-model and asset partitions, so an app-only flash omits required runtime data.

```sh
cd firmware
. "$HOME/esp/esp-idf/export.sh"
idf.py -p /dev/cu.usbmodem1101 flash
```

> **Hardware warning:** Do not run `erase-flash` or `nvs_flash_erase()` on a configured StackChan. NVS contains device-specific servo calibration and identity values. The tilt servo must remain within its safe physical range.

For a quick curiosity demo, open `AI.AGENT`, remain in view of the camera, and ask Yuki to set your interests or to share something now. The request is deferred until the current conversation returns to standby, then Yuki sends the exploration prompt through the configured Xiaozhi-compatible backend.

## How Codex contributed

OpenAI Codex was used throughout the project to investigate the unfamiliar firmware, distinguish between multiple StackChan software lineages, trace events across FreeRTOS tasks, understand the flash and OTA layout, protect hardware-specific calibration, design the MCP interface, implement and review C++ changes, diagnose build issues, and verify the resulting firmware.

Codex accelerated source analysis and implementation. Product decisions, including Yuki's interaction model, character direction, autonomous behavior, privacy boundaries, and coordinated expression system, were made by the project author.

See [DEVPOST.md](DEVPOST.md) for the full project story.

## Hackathon work and upstream work

This repository is based on [M5Stack/StackChan](https://github.com/m5stack/StackChan). The `hackathon-upstream-baseline` tag identifies the upstream source immediately before the Yuki implementation; the comparison above is the authoritative distinction between upstream and hackathon work.

The original StackChan firmware is Copyright (c) 2026 M5Stack Technology CO LTD and distributed under the MIT License. See [LICENSE](LICENSE). StackChan is a product of M5Stack; Yuki is an independent hackathon project built on its open-source firmware.
