import {randomUUID} from 'node:crypto';
import {createServer} from 'node:http';
import { WebSocketServer, WebSocket } from 'ws';
import {isAuthorized} from './authorization.js';
import { GeminiLiveBridge } from './gemini-live.js';
import { DeviceMcpClient } from './mcp-client.js';
import {
  CHANNELS,
  FRAME_DURATION_MS,
  OUTPUT_SAMPLE_RATE,
  createInputDecoder,
  opusPacketToPcm,
  parseIncomingOpusFrame,
  serializeOutgoingOpusFrame,
} from './audio.js';
import {createHttpHandler, GeminiVision} from './vision.js';

const config = readConfig(process.env);
const vision = new GeminiVision(config);
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
    listening: false,
    generation: 0,
    closed: false,
    liveBridge: null,
    liveReady: null,
    liveTurnPromise: null,
    liveInputQueue: [],
  };
  session.mcp = new DeviceMcpClient((message) => sendJson(session, message), {
    visionUrl: config.visionUrl,
    visionToken: config.gatewayToken,
  });
  return session;
}

async function handleMessage(session, data, isBinary) {
  if (isBinary) {
    if (!session.listening) {
      return;
    }
    const packet = parseIncomingOpusFrame(data, session.version);
    const pcm = opusPacketToPcm(session.decoder, packet);
    if (session.liveBridge?.listening) {
      session.liveBridge.sendInputPcm(pcm);
    } else if (session.liveInputQueue.length < 200) {
      session.liveInputQueue.push(pcm);
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
  prepareGeminiLive(session).catch((error) => handleSessionError(session, error));
}

async function handleListening(session, message) {
  if (message.state === 'detect' && isProactivePrompt(message.text)) {
    session.generation++;
    const bridge = await prepareGeminiLive(session);
    await bridge.sendText(message.text.trim());
    return;
  }

  if (message.state === 'start') {
    session.generation++;
    session.decoder = createInputDecoder();
    session.liveInputQueue = [];
    session.listening = true;
    const generation = session.generation;
    session.liveTurnPromise = beginGeminiTurn(session, generation);
    await session.liveTurnPromise;
    return;
  }

  if (message.state === 'stop' && session.listening) {
    session.listening = false;
    await session.liveTurnPromise;
    session.liveBridge?.stopListening();
    session.liveInputQueue = [];
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
  return {
    gatewayToken: env.YUKI_GATEWAY_TOKEN,
    geminiApiKey: env.GEMINI_API_KEY,
    geminiModel: env.YUKI_GEMINI_MODEL ?? 'gemini-3.1-flash-live-preview',
    geminiVoice: env.YUKI_GEMINI_VOICE ?? 'Zephyr',
    geminiVisionModel: env.YUKI_GEMINI_VISION_MODEL ?? 'gemini-3.6-flash',
    googleSearchEnabled: env.YUKI_ENABLE_GOOGLE_SEARCH !== 'false',
    visionUrl: env.YUKI_VISION_URL ?? (env.YUKI_GATEWAY_HOST ? `https://${env.YUKI_GATEWAY_HOST}/vision` : undefined),
    port: Number.parseInt(env.PORT ?? '8787', 10),
  };
}
