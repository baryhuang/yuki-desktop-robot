# Yuki Gateway

Yuki Gateway mirrors Xiaozhi's server-side architecture. The StackChan sends
16 kHz Opus frames and listening boundaries over WebSocket. The gateway sends
each finished utterance to its private Whisper service, returns the transcript
to the device as `{"type":"stt"}`, asks DigitalOcean Serverless Inference for
the reply, and returns DigitalOcean TTS as paced Opus frames.

## Why speech recognition is a separate service

DigitalOcean Serverless Inference supplies the chat and TTS endpoints used here.
It does not expose a documented speech-to-text endpoint. `compose.yaml` instead
starts a private faster-whisper HTTP service behind the gateway. This is an
internal DigitalOcean Droplet component, not a hidden Xiaozhi dependency or a
second cloud key.

## DigitalOcean deployment

```sh
cd gateway
cp .env.example .env
# Set DIGITALOCEAN_TOKEN, YUKI_GATEWAY_TOKEN, and YUKI_GATEWAY_HOST in .env.
docker compose up --build -d
```

Run this Compose stack on a DigitalOcean Droplet. The gateway binds only to
`127.0.0.1:8787`, Whisper stays private on the Compose network, and the Droplet's
Caddy instance terminates TLS using [`deploy/Caddyfile`](deploy/Caddyfile). The
firmware connects to `wss://<YUKI_GATEWAY_HOST>`.

The production path is therefore:

`ESP32 -> WSS on DigitalOcean -> Yuki Gateway -> private Whisper -> DigitalOcean Serverless Inference -> Yuki Gateway -> ESP32`

The Whisper model downloads when the service starts for the first time. Use a
Droplet with at least 4 GB memory for the `small` model. Never put
`DIGITALOCEAN_TOKEN` in firmware, source files, screenshots, or Git. The
firmware only receives the gateway URL and optional gateway access token.
