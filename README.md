# Yuki: The Curious Desktop Robot

> An expressive desktop robot that follows your face, wakes to gestures, explores the web around your interests, and starts conversations through animation, voice, movement, and light.

Yuki is an embodied AI companion built for OpenAI Build Week 2026 on the M5Stack StackChan K151. The project extends the open-source factory firmware in native C and C++ instead of replacing it with a simplified demonstration.

## Project status

Implemented during the hackathon:

- `self.robot.get_recent_interaction`, an MCP tool that reports recent head-touch gestures and shaking with event age and counts
- `self.robot.set_led_pattern`, an MCP tool that controls all 12 body LEDs individually or as repeating patterns
- A verified ESP-IDF 5.5.4 build for the ESP32-S3 hardware

In active development:

- Camera-based face following
- Visual gesture wake-up
- An original, fully animated Yuki facial-expression system replacing the stock expression library
- Coordinated facial animation, head motion, voice, and LED expression
- Interest-guided web exploration and proactive conversation while idle

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

Use a full `idf.py flash`, not `app-flash`. The factory partition layout has dual OTA slots and no factory app slot, so an app-only flash may write an image that the device does not boot.

```sh
cd firmware
. "$HOME/esp/esp-idf/export.sh"
idf.py -p /dev/cu.usbmodem1101 flash
```

> **Hardware warning:** Do not run `erase-flash` or `nvs_flash_erase()` on a configured StackChan. NVS contains device-specific servo calibration and identity values. The tilt servo must remain within its safe physical range.

## How Codex contributed

OpenAI Codex was used throughout the project to investigate the unfamiliar firmware, distinguish between multiple StackChan software lineages, trace events across FreeRTOS tasks, understand the flash and OTA layout, protect hardware-specific calibration, design the MCP interface, implement and review C++ changes, diagnose build issues, and verify the resulting firmware.

Codex accelerated source analysis and implementation. Product decisions, including Yuki's interaction model, character direction, autonomous behavior, privacy boundaries, and coordinated expression system, were made by the project author.

See [DEVPOST.md](DEVPOST.md) for the full project story.

## Hackathon work and upstream work

This repository is based on [M5Stack/StackChan](https://github.com/m5stack/StackChan). The upstream source predates the hackathon. New work is maintained in commits dated during the OpenAI Build Week submission period so judges can distinguish the Yuki implementation from the original firmware.

The original StackChan firmware is Copyright (c) 2026 M5Stack Technology CO LTD and distributed under the MIT License. See [LICENSE](LICENSE). StackChan is a product of M5Stack; Yuki is an independent hackathon project built on its open-source firmware.
