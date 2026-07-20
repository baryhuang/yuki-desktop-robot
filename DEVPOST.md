# Yuki: The Curious Desktop Robot

> An expressive desktop robot that follows your face, wakes to gestures, explores the web around your interests, and starts conversations through animation, voice, movement, and light.

## Inspiration

Most AI assistants wait behind a wake word. Even when placed inside a robot, they often behave like a speaker with a screen: they answer questions, but they do not feel present.

I wanted to explore a different idea. What if a desktop robot could notice when I was nearby, understand how I interacted with it, remain curious while idle, and return with something worth sharing?

That idea became **Yuki**, an expressive desktop robot with a life between conversations.

## What it does

Yuki uses her camera to find and follow a face. Instead of requiring a spoken wake word, she can recognize a deliberate gesture as an invitation to interact.

Her screen is not a collection of static reaction images. Yuki has an original, fully animated character face with continuous blinking, gaze, mouth movement, emotional transitions, and idle behavior. Her expressions work together with her physical body: speech can be accompanied by head movement, LED patterns, voice, and reactions to touch or motion.

When Yuki is idle, she can explore articles based on her owner's interests. Rather than reading a feed aloud, she evaluates what she finds and saves topics that may lead to an interesting conversation. When the moment is appropriate, she can turn toward the user and start the conversation herself. If the user is busy, she returns to a quiet state.

The goal is not to make another command-driven assistant. It is to make an AI character feel present in the physical space it shares with a person.

## How I built it

Yuki is built on an M5Stack StackChan K151, a desktop robot powered by an ESP32-S3. The embedded experience is implemented in native C and C++ using ESP-IDF, FreeRTOS, and LVGL.

I extended the open-source factory firmware rather than replacing the robot with a simplified demo. This preserves its existing audio, display, servo, touch, IMU, networking, and application capabilities while allowing Yuki's new behavior to become part of the complete product experience.

The robot exposes physical abilities to its AI agent through Model Context Protocol tools. I added tools that:

- Report recent head-touch gestures and shaking
- Preserve event timing and interaction counts across asynchronous tasks
- Control all 12 body LEDs individually for expressive patterns
- Return structured errors when generated tool input is invalid

The architecture separates the embedded character and hardware-control system from the language-model backend. This allows Yuki's animation, perception, and physical behavior to remain native to the robot without locking the project to one runtime AI provider.

OpenAI Codex was my engineering collaborator throughout the project. I used it to investigate an unfamiliar firmware codebase, distinguish between different StackChan software lineages, trace hardware events across FreeRTOS tasks, inspect OTA behavior, protect device-specific calibration data, design the MCP interface, implement C++ changes, review failure cases, and verify the firmware build.

I made the product decisions: Yuki's character, interaction model, autonomous behavior, privacy boundaries, and the requirement that her face, voice, movement, and light behave as one coordinated expression system.

## Challenges

### Working safely with real hardware

The robot stores irreplaceable servo calibration and device identity in a small NVS partition. A normal-looking factory erase would destroy that information. I had to understand the complete flash layout, OTA slot selection, rollback behavior, and erase boundaries before modifying the device.

### Connecting asynchronous senses to an AI agent

Touch and IMU events are pushed from independent FreeRTOS tasks, while MCP tools are requested by the agent on demand. I built a thread-safe interaction log using atomics so the agent can ask what happened, how recently it happened, and how often it has happened.

### Making expression feel coherent

An animated face alone is not enough. Eyes, mouth, head movement, LEDs, voice, and physical reactions must agree in both emotion and timing. Small synchronization problems can make an otherwise sophisticated character feel mechanical.

### Balancing autonomy with interruption

A companion that initiates conversations can quickly become distracting. Yuki needs to consider presence, recent interaction, user preferences, and whether the user responds before deciding to continue or quietly return to idle exploration.

### Building within embedded constraints

Face tracking, animation, audio, networking, and servo control share limited CPU time, memory, flash, and power on an ESP32-S3. Every feature must be designed as part of one constrained real-time system.

## What I learned

I learned that embodiment is not created by putting a chatbot inside a plastic shell. Physical presence comes from timing, attention, memory, and coordinated expression.

I also learned that MCP is useful beyond software automation. It provides a clean boundary between an AI agent and physical capabilities, allowing a model to understand touch, movement, and light as meaningful tools rather than device-specific commands.

Most importantly, autonomy is not simply doing more without being asked. A good companion must also know when to wait, when to approach, and when to leave someone alone.

## Built with

- OpenAI Codex
- C
- C++
- ESP-IDF
- FreeRTOS
- LVGL
- Model Context Protocol (MCP)
- ESP32-S3
- M5Stack StackChan
- CMake
- GitHub

