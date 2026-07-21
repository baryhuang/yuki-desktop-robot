# Yuki Gateway

Yuki Gateway terminates the StackChan's authenticated Xiaozhi-compatible WSS
connection on a CPU-only DigitalOcean Droplet. In production it decodes each
60 ms Opus input packet immediately, streams the resulting 16 kHz PCM to Vertex
AI Gemini Live, and encodes Gemini's streaming 24 kHz PCM response back to Opus.
No complete utterance or WAV response is buffered before the next stage starts.

The gateway also discovers the robot's MCP tools at connection time. Gemini
function calls are sent to the physical device as MCP `tools/call` requests,
and the device results are returned to the same Gemini Live session. During MCP
initialization, the gateway also gives the camera its authenticated `/vision`
endpoint. Camera JPEGs are analyzed by Vertex AI only when Gemini calls the
camera tool; continuous local face tracking remains on the device.

The firmware's idle curiosity prompt arrives as a `listen/detect` text turn.
The gateway sends it into the existing Live session with Google Search grounding
enabled, so proactive conversation does not need synthetic microphone audio.
Set `YUKI_ENABLE_GOOGLE_SEARCH=false` to disable grounding during rollout.

## DigitalOcean deployment

```sh
cd gateway
cp .env.example .env
# Set GOOGLE_CLOUD_PROJECT, YUKI_GATEWAY_TOKEN, and YUKI_GATEWAY_HOST in .env.
export GOOGLE_APPLICATION_CREDENTIALS_HOST=/secure/path/yuki-gcp-service-account.json
docker compose -f compose.yaml -f compose.gemini.yaml up --build -d
```

The service-account file is mounted read-only and must never be committed. The
gateway binds only to `127.0.0.1:8787`, and the Droplet's Caddy instance
terminates TLS using [`deploy/Caddyfile`](deploy/Caddyfile). The firmware
connects to `wss://<YUKI_GATEWAY_HOST>`.

The production path is therefore:

`ESP32 -> WSS on DigitalOcean -> Yuki Gateway -> Vertex AI Gemini Live -> Yuki Gateway -> ESP32`

## Temporary fallback

The previous DigitalOcean chat/TTS plus private CPU Whisper pipeline remains
available for rollback, but it is not suitable for natural realtime dialogue:

```sh
# Set YUKI_VOICE_BACKEND=digitalocean and the DIGITALOCEAN_TOKEN/STT settings.
docker compose --profile digitalocean-fallback up --build -d
```

Never put a cloud credential in firmware, source files, screenshots, or Git.
The firmware only receives the gateway URL and its independent access token.
