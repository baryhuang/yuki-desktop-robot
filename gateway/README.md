# Yuki Gateway

Yuki Gateway mirrors Xiaozhi's server-side architecture. The StackChan sends
16 kHz Opus frames and listening boundaries over WebSocket. The gateway sends
each finished utterance to its private Whisper service, returns the transcript
to the device as `{"type":"stt"}`, asks DigitalOcean Serverless Inference for
the reply, and returns DigitalOcean TTS as paced Opus frames.

## Why there is a second speech credential

DigitalOcean Serverless Inference supplies the chat and TTS endpoints used here.
It does not expose a documented speech-to-text endpoint. `compose.yaml` instead
starts a private faster-whisper HTTP service behind the gateway. This is an
explicit local component, not a hidden Xiaozhi dependency or a second cloud key.

## Run locally

```sh
cd gateway
cp .env.example .env
# Set DIGITALOCEAN_TOKEN in .env.
docker compose up --build
```

The gateway listens on `ws://<your-mac-lan-ip>:8787`; Whisper stays private on
the Compose network. For a public deployment, terminate TLS in front of the
gateway and configure the firmware with `wss://...`.

## Deploy with Docker

```sh
cd gateway
docker compose up --build -d
```

The Whisper model downloads when the service starts for the first time. Use a
Droplet with at least 4 GB memory for the `base` model. Never put
`DIGITALOCEAN_TOKEN` in firmware, source files, screenshots, or Git. The
firmware only receives the gateway URL and optional gateway access token.
