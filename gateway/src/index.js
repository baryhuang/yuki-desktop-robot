import {randomUUID} from 'node:crypto';
import {createServer} from 'node:http';
import { WebSocketServer, WebSocket } from 'ws';
import {isAuthorized} from './authorization.js';
import { GeminiLiveBridge } from './gemini-live.js';
import { DeviceMcpClient } from './mcp-client.js';
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
import {createHttpHandler, VertexVision} from './vision.js';

const config = readConfig(process.env);
const vision = config.voiceBackend === 'gemini-live' ? new VertexVision(config) : null;
const httpServer = createServer(createHttpHandler({gatewayToken: config.gatewayToken, vision}));
const server = new WebSocketServer({server: httpServer});
httpServer.listen(config.port, '0.0.0.0');

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
    session.liveBridge?.close();
    session.mcp.close();
  });
  socket.on('error', (error) => {
    console.warn(`[${session.id}] websocket error: ${error.message}`);
  });
});

function createSession(socket, headers) {
  const session = {
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
    liveBridge: null,
    liveReady: null,
    liveTurnPromise: null,
    liveInputQueue: [],
  };
  session.mcp = new DeviceMcpClient((message) => sendJson(session, message), {
    visionUrl: config.voiceBackend === 'gemini-live' ? config.visionUrl : undefined,
    visionToken: config.gatewayToken,
  });
  return session;
}

async function handleMessage(session, data, isBinary) {
  if (isBinary) {
    if (!session.listening || session.turnInFlight) {
      return;
    }
    const packet = parseIncomingOpusFrame(data, session.version);
    if (config.voiceBackend === 'gemini-live') {
      const pcm = opusPacketToPcm(session.decoder, packet);
      if (session.liveBridge?.listening) {
        session.liveBridge.sendInputPcm(pcm);
      } else if (session.liveInputQueue.length < 200) {
        session.liveInputQueue.push(pcm);
      }
    } else {
      session.packets.push(packet);
    }
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
      session.liveBridge?.abort();
      break;
    case 'mcp':
      if (!session.mcp.handleMessage(message.payload)) {
        console.warn(`[${session.id}] ignored unmatched device MCP response`);
      }
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
  if (config.voiceBackend === 'gemini-live') {
    prepareGeminiLive(session).catch((error) => handleSessionError(session, error));
  }
}

async function handleListening(session, message) {
  if (message.state === 'detect' && isProactivePrompt(message.text)) {
    session.generation++;
    if (config.voiceBackend === 'gemini-live') {
      const bridge = await prepareGeminiLive(session);
      await bridge.sendText(message.text.trim());
    } else {
      await runTextTurn(session, message.text.trim());
    }
    return;
  }

  if (message.state === 'start') {
    session.generation++;
    session.decoder = createInputDecoder();
    session.packets = [];
    session.liveInputQueue = [];
    session.listening = true;
    if (config.voiceBackend === 'gemini-live') {
      const generation = session.generation;
      session.liveTurnPromise = beginGeminiTurn(session, generation);
      await session.liveTurnPromise;
    }
    return;
  }

  if (message.state === 'stop' && session.listening && !session.turnInFlight) {
    session.listening = false;
    if (config.voiceBackend === 'gemini-live') {
      await session.liveTurnPromise;
      session.liveBridge?.stopListening();
      session.liveInputQueue = [];
      return;
    }
    if (session.packets.length === 0) {
      return;
    }
    await runTurn(session);
  }
}

function isProactivePrompt(text) {
  return typeof text === 'string' && text.startsWith('This is an autonomous Yuki curiosity moment.');
}

async function beginGeminiTurn(session, generation) {
  const bridge = await prepareGeminiLive(session);
  if (!isCurrent(session, generation)) {
    return;
  }
  await bridge.startListening();
  if (!isCurrent(session, generation)) {
    bridge.abort();
    return;
  }
  for (const pcm of session.liveInputQueue) {
    bridge.sendInputPcm(pcm);
  }
  session.liveInputQueue = [];
}

function prepareGeminiLive(session) {
  if (session.liveBridge) {
    return Promise.resolve(session.liveBridge);
  }
  if (session.liveReady) {
    return session.liveReady;
  }

  session.liveReady = (async () => {
    const toolDeclarations = await session.mcp.discoverTools();
    if (session.closed) {
      throw new Error('Device disconnected during Gemini Live setup');
    }
    const bridge = new GeminiLiveBridge({
      config,
      toolDeclarations,
      callTool: (name, args) => session.mcp.callTool(name, args),
      sendJson: (message) => sendJson(session, message),
      sendAudio: (packet) => sendAudio(session, packet),
      onError: (error) => handleSessionError(session, error),
    });
    await bridge.connect();
    session.liveBridge = bridge;
    console.info(`[${session.id}] Gemini Live ready with ${toolDeclarations.length} device tools`);
    return bridge;
  })().catch((error) => {
    session.liveReady = null;
    throw error;
  });
  return session.liveReady;
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
    await completeDigitalOceanTurn(session, generation, transcript);
  } finally {
    session.turnInFlight = false;
  }
}

async function runTextTurn(session, text) {
  if (session.turnInFlight) {
    return;
  }
  session.turnInFlight = true;
  const generation = session.generation;
  try {
    await completeDigitalOceanTurn(session, generation, text);
  } finally {
    session.turnInFlight = false;
  }
}

async function completeDigitalOceanTurn(session, generation, prompt) {
  appendHistory(session, 'user', prompt);
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

function sendAudio(session, packet) {
  if (!session.closed && session.socket.readyState === WebSocket.OPEN) {
    session.socket.send(serializeOutgoingOpusFrame(packet, session.version), {binary: true});
  }
}

function isCurrent(session, generation) {
  return !session.closed && session.generation === generation;
}

function readConfig(env) {
  const voiceBackend = env.YUKI_VOICE_BACKEND ?? 'digitalocean';
  if (!['digitalocean', 'gemini-live'].includes(voiceBackend)) {
    throw new Error(`Unsupported YUKI_VOICE_BACKEND: ${voiceBackend}`);
  }
  if (voiceBackend === 'digitalocean' && !env.DIGITALOCEAN_TOKEN) {
    throw new Error('DIGITALOCEAN_TOKEN must be set');
  }
  return {
    voiceBackend,
    digitalOceanToken: env.DIGITALOCEAN_TOKEN,
    chatModel: env.YUKI_DO_CHAT_MODEL ?? 'openai-gpt-oss-20b',
    ttsModel: env.YUKI_DO_TTS_MODEL ?? 'qwen3-tts-voicedesign',
    ttsVoice: env.YUKI_TTS_VOICE ?? 'alloy',
    sttBaseUrl: env.YUKI_STT_BASE_URL?.replace(/\/$/, ''),
    sttApiKey: env.YUKI_STT_API_KEY,
    sttModel: env.YUKI_STT_MODEL,
    gatewayToken: env.YUKI_GATEWAY_TOKEN,
    googleCloudProject: env.GOOGLE_CLOUD_PROJECT,
    googleCloudLocation: env.GOOGLE_CLOUD_LOCATION ?? 'us-west1',
    geminiModel: env.YUKI_GEMINI_MODEL ?? 'gemini-live-2.5-flash-native-audio',
    geminiVoice: env.YUKI_GEMINI_VOICE ?? 'Leda',
    geminiVisionModel: env.YUKI_GEMINI_VISION_MODEL ?? 'gemini-2.5-flash',
    googleSearchEnabled: env.YUKI_ENABLE_GOOGLE_SEARCH !== 'false',
    visionUrl: env.YUKI_VISION_URL ?? (env.YUKI_GATEWAY_HOST ? `https://${env.YUKI_GATEWAY_HOST}/vision` : undefined),
    port: Number.parseInt(env.PORT ?? '8787', 10),
  };
}

function sleep(milliseconds) {
  return new Promise((resolve) => setTimeout(resolve, milliseconds));
}
