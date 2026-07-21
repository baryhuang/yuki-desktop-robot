import {GoogleGenAI, Modality} from '@google/genai';
import {
  FRAME_DURATION_MS,
  OUTPUT_SAMPLE_RATE,
  createOutputEncoder,
} from './audio.js';
import {yukiSystemInstruction} from './persona.js';

const OUTPUT_FRAME_BYTES = OUTPUT_SAMPLE_RATE * FRAME_DURATION_MS / 1000 * 2;

export class GeminiLiveBridge {
  constructor({
    config,
    toolDeclarations = [],
    callTool,
    sendJson,
    sendAudio,
    onError,
    createClient = defaultCreateClient,
    createEncoder = createOutputEncoder,
    frameDelayMs = FRAME_DURATION_MS - 5,
  }) {
    this.config = config;
    this.toolDeclarations = toolDeclarations;
    this.callTool = callTool;
    this.sendJson = sendJson;
    this.sendAudio = sendAudio;
    this.onError = onError;
    this.createClient = createClient;
    this.createEncoder = createEncoder;
    this.frameDelayMs = frameDelayMs;
    this.client = null;
    this.liveSession = null;
    this.connectPromise = null;
    this.closed = false;
    this.listening = false;
    this.acceptOutput = false;
    this.inputTranscript = '';
    this.outputTranscript = '';
    this.lastSentOutputTranscript = '';
    this.outputEncoder = this.createEncoder();
    this.outputPcm = Buffer.alloc(0);
    this.outputQueue = [];
    this.outputPumping = false;
    this.outputFinishRequested = false;
    this.outputStarted = false;
    this.outputGeneration = 0;
  }

  async connect() {
    if (this.liveSession) {
      return this.liveSession;
    }
    if (this.connectPromise) {
      return this.connectPromise;
    }

    this.connectPromise = this.openSession();
    try {
      return await this.connectPromise;
    } finally {
      this.connectPromise = null;
    }
  }

  async startListening() {
    const live = await this.connect();
    this.stopOutput(true);
    this.inputTranscript = '';
    this.outputTranscript = '';
    this.lastSentOutputTranscript = '';
    this.listening = true;
    this.acceptOutput = false;
    live.sendRealtimeInput({activityStart: {}});
  }

  async sendText(text) {
    const live = await this.connect();
    this.stopOutput(true);
    this.inputTranscript = '';
    this.outputTranscript = '';
    this.lastSentOutputTranscript = '';
    this.acceptOutput = true;
    live.sendClientContent({
      turns: [{role: 'user', parts: [{text}]}],
      turnComplete: true,
    });
  }

  sendInputPcm(pcm) {
    if (!this.listening || !this.liveSession || pcm.length === 0) {
      return;
    }
    this.liveSession.sendRealtimeInput({
      audio: {
        data: pcm.toString('base64'),
        mimeType: 'audio/pcm;rate=16000',
      },
    });
  }

  stopListening() {
    if (!this.listening || !this.liveSession) {
      return;
    }
    this.listening = false;
    this.acceptOutput = true;
    this.liveSession.sendRealtimeInput({activityEnd: {}});
  }

  abort() {
    if (this.listening && this.liveSession) {
      this.liveSession.sendRealtimeInput({activityEnd: {}});
    }
    this.listening = false;
    this.acceptOutput = false;
    this.stopOutput(true);
  }

  close() {
    this.closed = true;
    this.listening = false;
    this.acceptOutput = false;
    this.stopOutput(true);
    this.liveSession?.close();
    this.liveSession = null;
  }

  async handleMessage(message) {
    if (message.toolCall?.functionCalls?.length) {
      await this.handleToolCalls(message.toolCall.functionCalls);
    }

    const content = message.serverContent;
    if (!content) {
      return;
    }

    this.handleInputTranscription(content.inputTranscription);
    this.handleInputTranscription(content.interimInputTranscription, false);
    this.handleOutputTranscription(content.outputTranscription);

    for (const part of content.modelTurn?.parts ?? []) {
      const audio = part.inlineData;
      if (audio?.data && audio.mimeType?.startsWith('audio/pcm')) {
        this.enqueueOutputPcm(Buffer.from(audio.data, 'base64'));
      }
    }

    if (content.interrupted) {
      this.stopOutput(true);
    } else if (content.turnComplete) {
      this.finishOutput();
    }
  }

  async openSession() {
    if (this.closed) {
      throw new Error('Gemini Live bridge is closed');
    }
    validateConfig(this.config);
    this.client ??= this.createClient(this.config);
    const tools = [];
    if (this.config.googleSearchEnabled) {
      tools.push({googleSearch: {}});
    }
    if (this.toolDeclarations.length > 0) {
      tools.push({functionDeclarations: this.toolDeclarations});
    }

    const live = await this.client.live.connect({
      model: this.config.geminiModel,
      config: {
        responseModalities: [Modality.AUDIO],
        systemInstruction: yukiSystemInstruction(),
        speechConfig: {
          voiceConfig: {prebuiltVoiceConfig: {voiceName: this.config.geminiVoice}},
        },
        thinkingConfig: {thinkingBudget: 0},
        inputAudioTranscription: {},
        outputAudioTranscription: {},
        realtimeInputConfig: {
          automaticActivityDetection: {disabled: true},
        },
        contextWindowCompression: {
          triggerTokens: '100000',
          slidingWindow: {targetTokens: '50000'},
        },
        tools: tools.length > 0 ? tools : undefined,
      },
      callbacks: {
        onmessage: (message) => {
          this.handleMessage(message).catch((error) => this.reportError(error));
        },
        onerror: (event) => this.reportError(event.error ?? new Error(event.message ?? 'Gemini Live error')),
        onclose: (event) => {
          this.liveSession = null;
          if (!this.closed) {
            this.reportError(new Error(`Gemini Live closed (${event.code ?? 'unknown'})`));
          }
        },
      },
    });
    this.liveSession = live;
    return live;
  }

  handleInputTranscription(transcription, sendWhenFinished = true) {
    if (!transcription?.text) {
      return;
    }
    this.inputTranscript = mergeTranscript(this.inputTranscript, transcription.text);
    if (sendWhenFinished && transcription.finished) {
      const text = this.inputTranscript.trim();
      if (text) {
        this.sendJson({type: 'stt', text});
      }
    }
  }

  handleOutputTranscription(transcription) {
    if (!this.acceptOutput || !transcription?.text) {
      return;
    }
    this.outputTranscript = mergeTranscript(this.outputTranscript, transcription.text);
    const text = this.outputTranscript.trim();
    if (text && text !== this.lastSentOutputTranscript) {
      this.lastSentOutputTranscript = text;
      this.sendJson({type: 'tts', state: 'sentence_start', text});
      this.sendJson({type: 'llm', emotion: emotionFor(text)});
    }
  }

  enqueueOutputPcm(pcm) {
    if (!this.acceptOutput || pcm.length === 0) {
      return;
    }
    this.ensureOutputStarted();
    this.outputPcm = Buffer.concat([this.outputPcm, pcm]);
    while (this.outputPcm.length >= OUTPUT_FRAME_BYTES) {
      const frame = this.outputPcm.subarray(0, OUTPUT_FRAME_BYTES);
      this.outputPcm = this.outputPcm.subarray(OUTPUT_FRAME_BYTES);
      this.outputQueue.push(this.encodeOutputFrame(frame));
    }
    this.pumpOutput().catch((error) => this.reportError(error));
  }

  finishOutput() {
    if (!this.outputStarted) {
      return;
    }
    if (this.outputPcm.length > 0) {
      const frame = Buffer.alloc(OUTPUT_FRAME_BYTES);
      this.outputPcm.copy(frame);
      this.outputPcm = Buffer.alloc(0);
      this.outputQueue.push(this.encodeOutputFrame(frame));
    }
    this.outputFinishRequested = true;
    this.pumpOutput().catch((error) => this.reportError(error));
  }

  stopOutput(interrupted) {
    this.outputGeneration++;
    this.outputQueue = [];
    this.outputPcm = Buffer.alloc(0);
    this.outputFinishRequested = false;
    this.outputPumping = false;
    if (this.outputStarted) {
      this.sendJson({type: 'tts', state: 'stop', interrupted});
    }
    this.outputStarted = false;
    this.outputEncoder = this.createEncoder();
  }

  ensureOutputStarted() {
    if (this.outputStarted) {
      return;
    }
    this.outputStarted = true;
    this.outputFinishRequested = false;
    this.sendJson({type: 'llm', emotion: 'neutral'});
    this.sendJson({type: 'tts', state: 'start'});
  }

  encodeOutputFrame(frame) {
    return Buffer.from(this.outputEncoder.encode(frame, OUTPUT_SAMPLE_RATE * FRAME_DURATION_MS / 1000));
  }

  async pumpOutput() {
    if (this.outputPumping) {
      return;
    }
    this.outputPumping = true;
    const generation = this.outputGeneration;

    while (generation === this.outputGeneration && this.outputQueue.length > 0) {
      this.sendAudio(this.outputQueue.shift());
      if (this.frameDelayMs > 0) {
        await sleep(this.frameDelayMs);
      }
    }

    if (generation !== this.outputGeneration) {
      return;
    }
    this.outputPumping = false;
    if (this.outputFinishRequested && this.outputQueue.length === 0) {
      this.sendJson({type: 'tts', state: 'stop'});
      this.outputStarted = false;
      this.outputFinishRequested = false;
      this.outputEncoder = this.createEncoder();
    }
  }

  async handleToolCalls(calls) {
    if (!this.callTool || !this.liveSession) {
      throw new Error('Gemini requested a tool but the device MCP bridge is unavailable');
    }
    const functionResponses = await Promise.all(calls.map(async (call) => {
      try {
        const output = await this.callTool(call.name, call.args ?? {});
        return {id: call.id, name: call.name, response: {output}};
      } catch (error) {
        return {id: call.id, name: call.name, response: {error: error.message}};
      }
    }));
    this.liveSession.sendToolResponse({functionResponses});
  }

  reportError(error) {
    this.onError?.(error instanceof Error ? error : new Error(String(error)));
  }
}

function defaultCreateClient(config) {
  return new GoogleGenAI({
    vertexai: true,
    project: config.googleCloudProject,
    location: config.googleCloudLocation,
  });
}

function validateConfig(config) {
  if (!config.googleCloudProject) {
    throw new Error('GOOGLE_CLOUD_PROJECT must be set for Gemini Live');
  }
  if (!config.googleCloudLocation) {
    throw new Error('GOOGLE_CLOUD_LOCATION must be set for Gemini Live');
  }
  if (!config.geminiModel) {
    throw new Error('YUKI_GEMINI_MODEL must be set for Gemini Live');
  }
}

function mergeTranscript(current, update) {
  if (!current) return update;
  if (update.startsWith(current)) return update;
  if (current.endsWith(update)) return current;
  return current + update;
}

function emotionFor(text) {
  const lower = text.toLowerCase();
  if (/(sorry|sad|unfortunately|miss you)/.test(lower)) return 'sad';
  if (/(wow|amazing|great|wonderful|love|happy|yay)/.test(lower)) return 'happy';
  if (/(what\?|really\?|surprised)/.test(lower)) return 'surprised';
  return 'neutral';
}

function sleep(milliseconds) {
  return new Promise((resolve) => setTimeout(resolve, milliseconds));
}
