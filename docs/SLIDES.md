# Yuki Slide Deck

Use seven slides. Each slide should be readable in under five seconds and use real photos, screen captures, code, or serial logs from the project. Do not use decorative stock art.

## Slide 1 — Yuki

**Title:** Yuki: The Curious Desktop Robot

**Subtitle:** A native C++ embodied AI character that sees, reacts, moves, and starts conversations.

**Visual:** Full-frame photo of the working robot with the final Yuki face visible.

## Slide 2 — Not a chatbot in a shell

**Headline:** Presence continues between prompts.

**Facts:** Local face detection and gaze; voice, gesture, and touch wake; animated expression; calibrated head movement; 12 RGB LEDs; interest-guided curiosity.

**Visual:** One real interaction sequence: face tracking, head touch, conversation, and LED response.

## Slide 3 — Native embedded architecture

**Headline:** The body runs on the device.

**Facts:** ESP32-S3; 16 MB flash; 8 MB PSRAM; C/C++; ESP-IDF; FreeRTOS; LVGL; ESP-DL; MCP.

**Architecture:** Camera, touch, IMU, microphone → deterministic firmware state and safety → avatar, servos, LEDs, audio → typed MCP tools → replaceable runtime model.

**Visual:** A direct architecture diagram with hardware on the left, firmware in the center, and the replaceable AI runtime on the right.

## Slide 4 — Where Codex was strong

**Headline:** Fast cross-layer reasoning when evidence was available.

**Facts:** Traced an unfamiliar multi-component firmware; followed behavior across C++, FreeRTOS, LVGL, ESP-DL, drivers, networking, and MCP; protected unit-specific NVS calibration; implemented instrumentation; compiled with ESP-IDF; flashed the robot; converted serial failures into focused code changes.

**Visual:** Source path, build, flash, and serial-log evidence connected as one iteration loop.

**Speaker line:** “Codex was strongest at code archaeology and fast iteration across subsystem boundaries. GPT-5.6 through Codex was my engineering environment, not Yuki’s required LLM backend.”

## Slide 5 — Where Codex reached its boundary

**Headline:** A log is not the physical world.

**What worked:** Once a failure produced code or telemetry, Codex could usually trace it quickly.

**What required the human:** Seeing whether a face still looked beautiful; hearing servo noise in the microphone; noticing that a head command did not move the hardware; rejecting unsafe motion; reporting that an apparent short-term recovery later froze.

**What went wrong:** Codex initially treated short logs as proof that tracking had recovered. It proposed a PSRAM task stack based on available-memory reasoning, but ESP-DL freezes flash cache while mapping the model, so the external stack triggered `s_task_stack_is_sane_when_cache_frozen()` and a reboot loop.

**Failure-to-fix sequence:** Face model before TLS → MQTT allocation failure. Task created after TLS → fragmented internal-RAM failure. Task stack moved to PSRAM → cache-freeze assertion and reboot loop. Final design → internal stack reserved early, model activated after secure connection.

**What can improve:** Better uncertainty calibration from partial logs; awareness of chip-specific cache and memory-capability rules before proposing a flash; automatic rollback after boot loops; persistent telemetry that does not reset the device when attached; and explicit separation between “command issued” and “physical behavior verified.”

**Visual:** Put the real reboot assertion beside the corrected architecture. Label the failed PSRAM-stack idea as a Codex hypothesis rejected by physical testing.

**Speaker line:** “The important result is not that Codex was always right. It is that the project exposed exactly what evidence Codex needs to be effective on embedded hardware.”

## Slide 6 — Natural animation under embedded constraints

**Headline:** Fewer frames, stronger acting.

**Facts:** Layer-aligned eyelids and features; three-state me-pachi blink timing; emotion-specific kuchi-paku mouth sets; held poses; tame, tsume, and follow-through; synchronized head and LEDs.

**Visual:** The actual open, half-closed, closed, and speaking poses aligned over the same character base.

## Slide 7 — The result

**Headline:** A complete character, not a proof of concept.

**Facts:** Working physical robot; installable native firmware; replaceable model backend; hardware-safe control; documented Codex collaboration; public source and reproducible build.

**Visual:** Final clean photo plus repository URL and demo-video URL.

## Judge-facing message

Technological implementation: The project exercised Codex across cross-layer investigation, C++ implementation, instrumentation, flashing, live failure diagnosis, and correction after a model-generated approach failed on real hardware.

Design: Yuki presents one coherent character across animation, voice, gaze, movement, touch, and light.

Potential impact: It demonstrates an interaction model for companions that remain attentive and expressive without requiring continuous cloud control.

Quality of idea: It combines Japanese limited-animation grammar, an always-aware physical character, and typed agent tools in constrained microcontroller firmware.
