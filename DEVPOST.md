# Yuki: The Curious Desktop Robot

Yuki is an always-aware desktop robot built on an M5Stack StackChan K151. She continuously looks for a face, follows it with her animated gaze, wakes when you say **“Hi Yuki,”** wave, or touch her head, and responds through voice, an animated character face, commanded head movement, and 12 RGB LEDs. When idle, she can explore the web around her owner’s saved interests, select something worth sharing, and start a conversation on her own.

This is not a web interface attached to a robot and not a video playing on its screen. Yuki runs as native C and C++ firmware on an ESP32-S3 with 16 MB of flash and 8 MB of PSRAM. Camera perception, wake behavior, animation, touch and IMU sensing, servo control, LED control, audio state coordination, and the agent’s physical tools all run on the device.

## What it does

- Keeps the camera active in standby, detects a face on-device, moves Yuki’s gaze toward it, and produces safety-limited targets for her pan-and-tilt head.
- Wakes through three natural inputs: the local **“Hi Yuki”** wake phrase, a deliberate left-right hand gesture detected by the camera, or a head touch that also triggers a shy reaction.
- Replaces StackChan’s original expression library with a layered Yuki character renderer built in C++ and LVGL. It uses anime limited-animation timing: held key poses, fast three-state blinks, emotion-specific three-state mouth shapes, gaze offsets, blush and symbolic effect layers, breathing, and timed transitions instead of continuously morphing every feature.
- Coordinates speech with small mouth shapes, head gestures, and two-color LED pulses so the screen and physical body perform as one character.
- Gives the agent hardware capabilities through MCP tools, including head control, camera use, recent touch and motion history, reminders, curiosity settings, and individual control of all 12 LEDs.
- Stores the owner’s interests and runs an idle curiosity scheduler. It only proposes a story after recently seeing the user and while no conversation is active; the runtime model and web-search provider remain replaceable.

## Inspiration

Most voice assistants disappear until a command is spoken. Putting the same interaction inside a robot still produces a smart speaker with a face. I wanted a desktop character that remains visibly attentive between requests: she notices that I am present, reacts when I touch or wave to her, and sometimes returns with something I would actually want to hear.

Japanese TV animation supplied the visual constraint. A small embedded display does not need 60 FPS character animation. A strong held drawing, one quick blink, a two-pixel breath, three well-timed mouth poses, and a delayed head movement can communicate more personality with less memory and computation. Yuki applies that limited-animation grammar to a physical AI companion.

## How I built it

I extended M5Stack’s complete open-source factory firmware instead of replacing it with a minimal demo. That kept the production audio pipeline, networking, display, touch, IMU, camera, feedback servos, LEDs, settings, and application framework intact while changing the robot’s identity and behavior at the firmware level.

The perception loop uses the camera continuously in standby and runs face detection locally. A FreeRTOS task publishes recent face position and gesture state through atomics; an avatar modifier turns that state into eye offsets and safety-limited servo targets. Manual MCP head commands temporarily take priority so autonomous tracking cannot immediately overwrite an intentional movement.

The face is rendered as aligned LVGL layers over a character base: brows, eyelids, pupils and highlights, mouth poses, blush, shadows, hair foreground, and anime effect symbols. The animation engine schedules poses by duration rather than treating every property as a smooth tween. Speech uses emotion-specific closed, medium, and open mouth states; blinks use short in-between frames and long open-eye holds.

The agent receives physical abilities through Model Context Protocol tools. Touch and IMU events originate in independent FreeRTOS tasks, so I added a lock-free interaction record containing event age and count rather than exposing transient booleans. LED input is parsed into validated patterns for 12 independently addressable pixels. Camera, servo, reminder, interaction, and curiosity tools expose concrete device state to the conversation agent.

Curiosity is a separate on-device scheduler, not a hard-coded backend feature. It stores interests and timing preferences, checks conversation state and recent visual presence, then asks the configured Yuki Gateway to research and choose one item. A CPU-only DigitalOcean Droplet hosts the authenticated WSS gateway. For conversation, the gateway decodes each 60 ms Opus packet and streams 16 kHz PCM to Vertex AI Gemini Live, then streams native 24 kHz audio back as Opus. This keeps Yuki’s body, senses, timing, and tool interface on the robot while avoiding a serial STT, chat, and TTS pipeline.

The same Live session accepts proactive curiosity as an ordered text turn and can ground it with Google Search. Camera tool calls use a separate authenticated `/vision` route because the firmware captures and uploads a JPEG only when the model asks to see; continuous face tracking never sends camera frames to the cloud.

## How I used Codex and GPT-5.6

Codex with GPT-5.6 was the engineering environment used to build Yuki during the hackathon. It traced an unfamiliar embedded codebase across C++, FreeRTOS tasks, LVGL objects, device drivers, a nested runtime dependency, and the flash layout; implemented and reviewed the firmware changes; compiled the ESP-IDF project; flashed the physical robot; and diagnosed failures from the live USB serial log.

The collaboration was iterative and hardware-driven. I specified the product behavior and animation direction, tested each build on the robot, and reported what I could see and hear. Codex then followed the relevant execution path, changed the code, rebuilt, flashed, and checked the device log. That loop caught issues that a simulator would miss: an eyelid layer with the wrong geometry, blush spanning the whole face, a mouth pose that looked aggressive at speech speed, autonomous tracking overriding manual head commands, a camera request exhausting internal SRAM, and an OTA path repeatedly checking for updates that this project does not use.

I made the product decisions: Yuki’s identity, the three wake interactions, the use of Japanese limited-animation principles, the balance between attention and interruption, the decision to keep the runtime AI backend replaceable, and the requirement that the face and physical body behave as one performance. Codex accelerated code archaeology, implementation, verification, and debugging; it did not choose the product for me.

This project also exposed the boundary of that collaboration. Codex was strongest when source code, build output, protocol messages, and serial logs made a failure observable. It could not independently judge whether a servo actually moved naturally, whether electrical noise was audible, or whether a facial layer preserved the character’s appeal; those required direct human observation. It also sometimes inferred long-term success from a short log window. One Codex-proposed fix moved a vision task stack into PSRAM based on available-memory reasoning, but ESP-DL froze the flash cache while mapping its model partition and the external stack caused an assertion and reboot loop. My physical observation supplied the missing evidence; Codex then traced the backtrace, rejected its earlier assumption, and redesigned task creation and model activation separately.

The experience suggests concrete improvements for embedded use: express uncertainty when telemetry proves only that a command was issued; distinguish software state from physical behavior; check chip-specific memory capabilities and cache rules before flashing; preserve a known-good image for automatic rollback; and collect persistent telemetry without resetting the target when a monitor attaches. Codex was highly effective inside a disciplined hardware feedback loop, not as a substitute for that loop.

## Challenges

### Running the complete experience on an ESP32-S3

Audio streaming, wake-word detection, continuous camera capture, face detection, LVGL animation, networking, and two servos compete for the same CPU and memory. One camera implementation had enough total PSRAM but failed because creating another encoder thread required scarce internal SRAM. I replaced that path with synchronous JPEG encoding into PSRAM and treated task stacks, heap capabilities, and request latency as separate constraints rather than one “free memory” number.

Startup order became a hardware constraint. Loading the face detector before the secure runtime connection completed left too little contiguous internal memory for TLS and caused MQTT writes to fail. Starting the vision task only after the connection succeeded avoided the TLS collision, but by then allocating its 10 KB internal task stack could fail because the heap was fragmented. Moving the task stack into PSRAM looked like the obvious fix, but ESP-DL maps its model partition by temporarily freezing the flash cache; an executing task cannot safely keep its stack in external RAM during that operation, so the device hit `s_task_stack_is_sane_when_cache_frozen()` and entered a reboot loop.

The working design separates task creation from model activation. Yuki reserves the vision task's stack in internal RAM early in boot, keeps that task blocked while TLS, MQTT, and MCP initialize, and only loads the frame buffer and face model after the runtime reaches the listening state. This preserves a cache-safe internal stack without making the face model compete with the network handshake.

### Modifying real hardware without destroying calibration

The robot’s NVS partition contains unit-specific yaw and pitch zero positions plus its device identity. Erasing the flash would permanently lose those values. I mapped the complete 16 MB layout, preserved NVS, enlarged the application partition, added dedicated face-model and asset partitions, verified OTA boot selection and rollback behavior, and flashed only known regions. The tilt axis is also mechanically limited, so every autonomous and model-requested movement is clamped before reaching the servo.

### Sharing one camera between continuous perception and agent tools

Face tracking must never appear asleep, but the agent also needs the camera for visual questions. Both paths touch the same capture device and large frame buffers. I added a mutex-protected capture path and explicit ownership so always-on perception can pause for a photo and resume without corrupting a frame or allocating a second full camera pipeline.

### Making a static character face feel naturally animated

The hardest visual problem was not drawing more frames. It was making a detailed anime face feel alive without destroying the original illustration or turning it into mechanical UI animation. Smoothly rotating eyebrows, scaling one generic mouth from audio volume, or evenly interpolating an eyelid all produced uncanny results.

I rebuilt the face around Japanese limited-animation techniques. The high-quality base drawing remains still while aligned replacement layers handle brows, eyelids, pupils, highlights, mouth shapes, blush, shadows, hair foreground, and symbolic effects. Blinks use **me-pachi** timing: a long open-eye hold, one very short half-lid frame, one or two closed frames, then a fast return. Speech uses **kuchi-paku** sets of closed, medium, and open drawings made separately for each emotion instead of stretching one mouth. Reactions use held key poses, **tame** before the action, tightly timed **tsume** at the change, and delayed follow-through in the hair, head, or LEDs.

Layer geometry had to remain anchored across every pose. A closed eyelid can be only a few pixels tall, but its corners must land on exactly the same points as the open eye. Blush must sit on the cheeks rather than span the face. The maximum talking mouth had to stay small enough to preserve Yuki’s character design. These were tuned on the physical 320 × 240 display, because a pose that looked acceptable in source coordinates could still feel wrong at the robot’s real size and speech speed.

The result uses dozens of carefully timed states rather than hundreds of continuous frames. Static periods require no redraw, breathing updates only a few times per second, blinks are event-driven, and higher frame rates are reserved for short emotional reactions. This made natural character acting possible within the ESP32-S3’s memory and rendering limits.

### Making separate systems perform as one character

A valid facial animation can still fail if the physical channels disagree with it. A head command appears broken if face tracking overwrites it on the next cycle; a cheerful voice feels wrong with an angry LED pattern; simultaneous eye, head, and hair motion looks robotic. I added behavior priority, manual-control windows, staggered movement, and delayed physical follow-through so gaze, expression, voice, servos, and LEDs share one performance rather than running as unrelated effects.

Microphone and servo coordination required another state-machine fix. Physical head movement can inject enough electrical and mechanical noise to trigger voice activity detection, so tracking pauses briefly when the user starts speaking. The first implementation extended an 800 ms deadline on every update while `voice_detected` remained true. A stale VAD state therefore turned a short pause into an indefinite freeze after a conversation. The final implementation triggers one bounded 1.2 second pause only on the speech-state rising edge; a stuck VAD value cannot keep the head disabled, and tracking resumes without waiting for the entire conversation to reset.

### Giving an AI truthful knowledge of its body

The conversation model initially claimed it had no camera even though the hardware and MCP implementation existed. The fix was not to pretend in a system prompt. I verified the MCP initialization handshake, tool enumeration, descriptions, and calls over the live protocol, then described each capability with concrete ranges and usage rules. The agent can now discover what the robot can actually sense and do.

### Replacing a correct but unusably slow voice pipeline

The first replacement backend was functionally complete: private faster-whisper on a four-vCPU DigitalOcean Droplet, DigitalOcean Serverless Inference chat, and DigitalOcean TTS. An end-to-end protocol test correctly transcribed Opus audio, generated a reply, and returned playable Opus frames. It was still a product failure. A 2.3 second utterance took about 9 seconds to transcribe; chat added about 1.4 seconds and TTS added another 3–4 seconds. Yuki waited roughly 14 seconds before speaking.

Optimizing the CPU model would not fix the architecture. The gateway waited for the utterance, STT waited for the complete audio, chat waited for STT, and TTS waited for chat. I replaced that path with a Gemini Live bridge that forwards every 60 ms input frame immediately and returns native audio as it arrives. The same bridge dynamically enumerates the robot’s MCP tools and maps Gemini function calls back to physical `tools/call` requests. The old DigitalOcean/Whisper path remains an explicit rollback profile, not the production interaction design.

## What I learned

Embodiment is a scheduling problem as much as a model problem. Presence comes from keeping perception active, choosing which behavior owns the body at a given moment, and making voice, gaze, expression, light, and motion agree within a few hundred milliseconds.

Limited animation is especially effective on embedded hardware because it spends resources on readable poses and timing instead of frame count. The result is both cheaper to render and more expressive than mechanically interpolating every facial feature.

MCP also works well as a boundary between an AI and a physical device. The model does not need access to servo packets, GPIOs, or FreeRTOS events. It needs truthful, typed capabilities such as “move the head safely,” “what interaction just happened,” and “show this LED pattern,” while deterministic firmware retains authority over hardware limits and timing.

Finally, proactive behavior requires restraint. Yuki’s curiosity is gated by recent presence and idle state because starting fewer relevant conversations is better than producing more interruptions.

## Built with

- OpenAI Codex
- GPT-5.6
- C and C++
- ESP-IDF 5.5.4
- FreeRTOS
- LVGL
- ESP-DL
- Model Context Protocol (MCP)
- Vertex AI Gemini Live
- DigitalOcean Serverless Inference
- ESP32-S3
- M5Stack StackChan K151
- CMake
- Git and GitHub
