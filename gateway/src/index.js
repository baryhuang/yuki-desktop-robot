import { randomUUID, timingSafeEqual } from 'node:crypto';
import { WebSocketServer, WebSocket } from 'ws';
import {
  CHANNELS,
  FRAME_DURATION_MS,
  INPUT_SAMPLE_RATE,
  OUTPUT_SAMPLE_RATE,
  createInputDecoder,
  createOutputEncoder,
  opusPacketToPcm,
  parseIncomingOpusFrame,
  pcmToOpusPackets,
  pcmToWav,
  serializeOutgoingOpusFrame,
  wavToOutputPcm,
} from './audio.js';
import { createReply, synthesizeSpeech, transcribeSpeech } from './digitalocean.js';

const config = readConfig(process.env);
const server = new WebSocketServer({port: config.port, host: '0.0.0.0'});

server.on('listening', () => {
  console.log(`Yuki gateway listening on port ${config.port}`);
});

server.on('connection', (socket, request) => {
  if (!isAuthorized(request.headers.authorization, config.gatewayToken)) {
    socket.close(1008, 'Unauthorized');
    return;
  }

  const session = createSession(socket, request.headers);
  socket.on('message', (data, isBinary) => {
    handleMessage(session, data, isBinary).catch((error) => handleSessionError(session, error));
  });
  socket.on('close', () => {
    session.closed = true;
    session.generation++;
  });
  socket.on('error', (error) => {
    console.warn(`[${session.id}] websocket error: ${error.message}`);
  });
});

function createSession(socket, headers) {
  return {
    id: randomUUID(),
    socket,
    deviceId: headers['device-id'] ?? 'unknown-device',
    version: 0,
    decoder: createInputDecoder(),
    encoder: createOutputEncoder(),
    packets: [],
    history: [],
    listening: false,
    turnInFlight: false,
    generation: 0,
    closed: false,
  };
}

async function handleMessage(session, data, isBinary) {
  if (isBinary) {
    if (!session.listening || session.turnInFlight) {
      return;
    }
    session.packets.push(parseIncomingOpusFrame(data, session.version));
    return;
  }

  const message = JSON.parse(Buffer.from(data).toString('utf8'));
  switch (message.type) {
    case 'hello':
      handleHello(session, message);
      break;
    case 'listen':
      await handleListening(session, message);
      break;
    case 'abort':
      session.generation++;
      break;
    case 'mcp':
      console.info(`[${session.id}] device MCP response received`);
      break;
    default:
      console.warn(`[${session.id}] ignored message type: ${message.type}`);
  }
}

function handleHello(session, message) {
  session.version = Number.isInteger(message.version) ? message.version : 0;
  if (![0, 2, 3].includes(session.version)) {
    throw new Error(`Unsupported protocol version ${session.version}`);
  }
  sendJson(session, {
    type: 'hello',
    transport: 'websocket',
    session_id: session.id,
    audio_params: {
      format: 'opus',
      sample_rate: OUTPUT_SAMPLE_RATE,
      channels: CHANNELS,
      frame_duration: FRAME_DURATION_MS,
    },
  });
  console.info(`[${session.id}] connected ${session.deviceId}, protocol v${session.version}`);
}

async function handleListening(session, message) {
  if (message.state === 'start') {
    session.generation++;
    session.decoder = createInputDecoder();
    session.packets = [];
    session.listening = true;
    return;
  }

  if (message.state === 'stop' && session.listening && !session.turnInFlight) {
    session.listening = false;
    if (session.packets.length === 0) {
      return;
    }
    await runTurn(session);
  }
}

async function runTurn(session) {
  session.turnInFlight = true;
  const generation = session.generation;
  try {
    const pcm = Buffer.concat(session.packets.map((packet) => opusPacketToPcm(session.decoder, packet)));
    session.packets = [];
    const transcript = await transcribeSpeech(config, pcmToWav(pcm, INPUT_SAMPLE_RATE));
    if (!isCurrent(session, generation)) {
      return;
    }

    sendJson(session, {type: 'stt', text: transcript});
    appendHistory(session, 'user', transcript);
    const reply = await createReply(config, session.history);
    if (!isCurrent(session, generation)) {
      return;
    }

    appendHistory(session, 'assistant', reply);
    const wav = await synthesizeSpeech(config, reply);
    if (!isCurrent(session, generation)) {
      return;
    }

    await streamSpeech(session, generation, reply, wav);
  } finally {
    session.turnInFlight = false;
  }
}

async function streamSpeech(session, generation, text, wav) {
  const pcm = await wavToOutputPcm(wav);
  session.encoder = createOutputEncoder();
  const packets = pcmToOpusPackets(session.encoder, pcm);
  if (!isCurrent(session, generation)) {
    return;
  }

  sendJson(session, {type: 'llm', emotion: emotionFor(text)});
  sendJson(session, {type: 'tts', state: 'start'});
  sendJson(session, {type: 'tts', state: 'sentence_start', text});

  for (const packet of packets) {
    if (!isCurrent(session, generation)) {
      return;
    }
    session.socket.send(serializeOutgoingOpusFrame(packet, session.version), {binary: true});
    await sleep(FRAME_DURATION_MS - 5);
  }

  if (isCurrent(session, generation)) {
    sendJson(session, {type: 'tts', state: 'stop'});
  }
}

function appendHistory(session, role, content) {
  session.history.push({role, content});
  if (session.history.length > 12) {
    session.history.splice(0, session.history.length - 12);
  }
}

function emotionFor(text) {
  const lower = text.toLowerCase();
  if (/(sorry|sad|unfortunately|miss you)/.test(lower)) return 'sad';
  if (/(wow|amazing|great|wonderful|love|happy|yay)/.test(lower)) return 'happy';
  if (/(what\?|really\?|surprised)/.test(lower)) return 'surprised';
  return 'neutral';
}

function handleSessionError(session, error) {
  console.error(`[${session.id}] ${error.message}`);
  if (!session.closed) {
    sendJson(session, {
      type: 'alert',
      status: 'Yuki service error',
      message: 'Yuki could not complete that request.',
      emotion: 'sad',
    });
  }
  session.turnInFlight = false;
}

function sendJson(session, message) {
  if (!session.closed && session.socket.readyState === WebSocket.OPEN) {
    session.socket.send(JSON.stringify(message));
  }
}

function isCurrent(session, generation) {
  return !session.closed && session.generation === generation;
}

function isAuthorized(header, expectedToken) {
  if (!expectedToken) {
    return true;
  }
  const provided = header?.replace(/^Bearer\s+/i, '') ?? '';
  const left = Buffer.from(provided);
  const right = Buffer.from(expectedToken);
  return left.length === right.length && timingSafeEqual(left, right);
}

function readConfig(env) {
  if (!env.DIGITALOCEAN_TOKEN) {
    throw new Error('DIGITALOCEAN_TOKEN must be set');
  }
  return {
    digitalOceanToken: env.DIGITALOCEAN_TOKEN,
    chatModel: env.YUKI_DO_CHAT_MODEL ?? 'openai-gpt-oss-20b',
    ttsModel: env.YUKI_DO_TTS_MODEL ?? 'qwen3-tts-voicedesign',
    ttsVoice: env.YUKI_TTS_VOICE ?? 'alloy',
    sttBaseUrl: env.YUKI_STT_BASE_URL?.replace(/\/$/, ''),
    sttApiKey: env.YUKI_STT_API_KEY,
    sttModel: env.YUKI_STT_MODEL,
    gatewayToken: env.YUKI_GATEWAY_TOKEN,
    port: Number.parseInt(env.PORT ?? '8787', 10),
  };
}

function sleep(milliseconds) {
  return new Promise((resolve) => setTimeout(resolve, milliseconds));
}
