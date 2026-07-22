# Yuki Gateway

Yuki Gateway terminates the StackChan's authenticated Xiaozhi-compatible WSS
connection on a CPU-only DigitalOcean Droplet. It decodes each
60 ms Opus input packet immediately, streams the resulting 16 kHz PCM to the
Gemini Live API, and encodes Gemini's streaming 24 kHz PCM response back to Opus.
No complete utterance or WAV response is buffered before the next stage starts.
The gateway uses the current Gemini Developer API SDK, `@google/genai@2.13.0`,
with `gemini-3.1-flash-live-preview` for realtime voice and
`gemini-3.6-flash` for explicit camera analysis.

The gateway also discovers the robot's MCP tools at connection time. Gemini
function calls are sent to the physical device as MCP `tools/call` requests,
and the device results are returned to the same Gemini Live session. During MCP
initialization, the gateway also gives the camera its authenticated `/vision`
endpoint. Camera JPEGs are analyzed by the Gemini API only when Gemini calls the
camera tool; continuous local face tracking remains on the device.

The firmware's idle curiosity prompt arrives as a `listen/detect` text turn.
The gateway sends it into the existing Live session with Google Search grounding
enabled, so proactive conversation does not need synthetic microphone audio.
Set `YUKI_ENABLE_GOOGLE_SEARCH=false` to disable grounding during rollout.

## DigitalOcean deployment

```sh
cd gateway
cp .env.example .env
# Set GEMINI_API_KEY, YUKI_GATEWAY_TOKEN, and YUKI_GATEWAY_HOST in .env.
docker compose up --build -d
```

The Gemini API key remains in the server's `.env` and must never be committed.
The gateway binds only to `127.0.0.1:8787`, and the Droplet's Caddy instance
terminates TLS using [`deploy/Caddyfile`](deploy/Caddyfile). The firmware connects
to `wss://<YUKI_GATEWAY_HOST>` using a separate gateway bearer token.

The only inference path is:

`ESP32 -> WSS on DigitalOcean -> Yuki Gateway -> Gemini Live API -> Yuki Gateway -> ESP32`

Never put a cloud credential in firmware, source files, screenshots, or Git.
The firmware only receives the gateway URL and its independent access token.
