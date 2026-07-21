# Yuki Hackathon Slide Deck

The final five-page OpenAI Build Week deck is committed in [`docs/slides/`](slides/):

1. **Yuki: The Curious Desktop Robot** - the working physical build and native embedded stack.
2. **Presence continues between prompts** - voice, touch, face tracking, physical performance, and idle curiosity.
3. **The body runs on the device** - sensing, deterministic C++ control, expression, MCP tools, and Japanese limited-animation technique.
4. **Codex moved fast when hardware produced evidence** - cross-layer strengths, the physical verification boundary, and the cache-safe vision startup failure sequence.
5. **A complete character, not a proof of concept** - the demonstrated result and the embedded-system lesson.

Each slide is a 2560 x 1440 PNG built from the final demo recording, firmware facts, serial evidence, and the Yuki character reference. The editable source is [`yuki-hackathon-deck.pptx`](slides/yuki-hackathon-deck.pptx).

## Files

- [`slide-1.png`](slides/slide-1.png)
- [`slide-2.png`](slides/slide-2.png)
- [`slide-3.png`](slides/slide-3.png)
- [`slide-4.png`](slides/slide-4.png)
- [`slide-5.png`](slides/slide-5.png)
- [`yuki-hackathon-deck.pptx`](slides/yuki-hackathon-deck.pptx)

## Judge-facing message

**Technological implementation:** Codex was used for code archaeology, C++ implementation, instrumentation, compilation, flashing, live serial diagnosis, and correction after a model-proposed memory change failed on the physical ESP32-S3.

**Design:** Yuki presents one character across anime expression, voice, gaze, calibrated head movement, touch, and light.

**Potential impact:** The project demonstrates an attentive companion whose perception and physical behavior remain native even when the runtime language-model service changes.

**Quality of the idea:** Yuki combines Japanese limited-animation grammar, continuous local presence, and typed agent tools inside constrained microcontroller firmware.
