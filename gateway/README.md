# Yuki Gateway

Yuki Gateway replaces Xiaozhi's cloud protocol. The StackChan speaks a small
WebSocket protocol over the local network; this service keeps the cloud keys on
the server, sends the conversation to DigitalOcean Serverless Inference, and
returns DigitalOcean TTS as paced Opus frames that the firmware already knows
how to play.

## Why there is a second speech credential

DigitalOcean Serverless Inference supplies the chat and TTS endpoints used here.
It does not expose a documented speech-to-text endpoint. The gateway therefore
uses an OpenAI-compatible transcription endpoint for microphone input. This is
an explicit integration boundary, not a hidden Xiaozhi dependency.

## Run locally

```sh
cd gateway
cp .env.example .env
# Set DIGITALOCEAN_TOKEN and the three YUKI_STT_* values in .env.
set -a; source .env; set +a
npm install
npm start
```

The gateway listens on `ws://<your-mac-lan-ip>:8787`. For a public deployment,
run it behind TLS and configure the firmware with `wss://...`.

## Deploy with Docker

```sh
cd gateway
docker build -t yuki-gateway .
docker run --env-file .env -p 8787:8787 yuki-gateway
```

Never put `DIGITALOCEAN_TOKEN` or `YUKI_STT_API_KEY` in firmware, source files,
screenshots, or Git. The firmware only receives the gateway URL and optional
gateway access token.
