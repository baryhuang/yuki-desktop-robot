# Yuki Demo Video

Target length: 2 minutes 45 seconds. Hard limit: 3 minutes.

## Recording rule

Show the working robot in the first three seconds. Use close shots where the screen, head movement, LEDs, and your interaction are visible at the same time. Record clean live audio, add English captions, and do not use copyrighted music. Use short cuts, but do not fake hardware behavior or replace the robot display with a mockup.

## Shot list and narration

### 0:00-0:12 — The result first

**Video:** Start on Yuki already running. Move your face from one side of the camera to the other so her eyes and physical head follow. Touch her head and show the shy reaction.

**Narration:** “This is Yuki, a native C++ AI desktop robot built on an ESP32-S3. She sees me, follows my face, reacts to touch, and performs through voice, animation, physical movement, and light.”

### 0:12-0:35 — Natural interaction

**Video:** Say “Hi Yuki,” or use the wake gesture. Ask one short question. Keep both your gesture and Yuki in frame.

**Suggested prompt:** “Hi Yuki. Can you see me, and can you look toward me?”

**Narration:** “Yuki wakes by voice, a camera gesture, or head touch. Face detection runs locally. The language-model backend is replaceable; her camera perception, animation, sensors, safety limits, and hardware tools live in the firmware.”

### 0:35-1:00 — Prove that the agent has a body

**Video:** Ask Yuki to check whether she can see you, move her head, and change her LEDs. Capture the physical response and, if possible, briefly overlay the corresponding MCP tool names without hiding the robot.

**Suggested prompt:** “Can you see me? Turn your head a little to your right and make your LEDs pink.”

**On-screen evidence:** `self.camera.get_presence`, `self.robot.get_head_angles`, `self.robot.set_head_angles`, `self.robot.set_led_color`.

**Narration:** “The agent discovers typed MCP tools for the real camera, calibrated servos, touch and motion history, reminders, curiosity settings, and twelve individually controlled LEDs. Deterministic firmware keeps authority over hardware limits.”

### 1:00-1:25 — Anime limited animation

**Video:** Use a tight screen shot. Show a normal blink, speech mouth movement, blush, and at least one emotional reaction. Slow down only a short replay of one blink so the eyelid timing is visible.

**Narration:** “I replaced StackChan’s original expression library with a layered Yuki renderer in C++ and LVGL. It uses Japanese limited-animation techniques: held key poses, fast me-pachi blinks, emotion-specific three-shape kuchi-paku mouths, gaze offsets, blush, symbolic effects, anticipation, and delayed follow-through. The goal is readable acting, not mechanical interpolation.”

### 1:25-2:20 — Testing Codex at the embedded boundary

**Video:** Cut between the physical robot, the C++ source, a successful build and flash, and selected serial-log lines. Show these exact failure signals briefly: TLS allocation failure, `Unable to start vision task`, the cache-freeze assertion, and the final `Physical face tracking enabled` plus `Tracking head target` lines.

**Narration:** “This project tested where Codex helps in embedded engineering and where it still needs a human hardware loop. Codex was strong at tracing an unfamiliar C++ firmware across FreeRTOS, LVGL, ESP-DL, TLS, MQTT, MCP, servo drivers, and the flash layout. It could implement a change, compile it, flash the device, inspect the serial log, and follow the failure into another subsystem much faster than I could do that code archaeology manually.”

**Narration:** “Its boundary was equally important. Codex could read that tracking commands were sent, but it could not see whether the head actually moved naturally; I had to report the physical result. It sometimes treated a short successful log window as proof of long-term behavior. It also proposed moving a task stack to PSRAM, which looked correct from heap numbers but violated an ESP32 cache-freeze constraint and caused a reboot loop. The human observation changed the evidence, and Codex then traced the assertion and redesigned startup around a cache-safe internal stack.”

**Narration:** “The same loop found that a VAD value kept extending a short servo pause indefinitely. Codex was effective once the failure was observable, but embedded work still required cautious hypotheses, explicit instrumentation, one-change-at-a-time flashing, and verification on the physical device.”

### 2:20-2:42 — What the collaboration produced

**Video:** Show a clean wide shot of Yuki following you, then speaking with synchronized face, head, and LEDs. Briefly show the Git history or Codex session only as supporting evidence.

**Narration:** “I made the product decisions and supplied the physical and visual judgment that the model could not observe. Codex accelerated code archaeology, implementation, instrumentation, and repeated diagnosis. The result shows both its value and its current boundary: Codex is highly effective inside an evidence-rich embedded workflow, but the human must keep the hardware itself inside that loop.”

### 2:42-2:50 — Close

**Video:** Yuki looks toward you and gives a small final reaction.

**Narration:** “Yuki turns an AI assistant into a character that is visibly present.”

## Required capture checklist

- One uninterrupted shot proving face tracking on the physical head
- One wake interaction
- One head-touch shy reaction
- One spoken exchange with animated mouth and audible reply
- One MCP-driven camera, servo, or LED action
- One close shot of blink and mouth timing
- Codex, build, flash, and serial-log evidence
- English captions and public YouTube upload
- Final runtime below 3:00

## Serial-log evidence to capture

```text
StateMachine: State: connecting -> listening
YukiVision: Face following and wave wake are active
YukiVision: Stable face detected
YukiVision: Physical face tracking enabled
YukiVision: Tracking head target yaw=... pitch=...
MCP: tools/list: returning 17 tool(s)
```

Use failure lines only as a fast technical montage. End the montage on the working tracking lines so judges see the failure was resolved.
