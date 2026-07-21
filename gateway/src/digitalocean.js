import { yukiSystemInstruction } from './persona.js';

const INFERENCE_BASE_URL = 'https://inference.do-ai.run/v1';
const REQUEST_TIMEOUT_MS = 30_000;

export async function createReply(config, history) {
  const response = await fetchWithTimeout(`${INFERENCE_BASE_URL}/chat/completions`, {
    method: 'POST',
    headers: authorizationHeaders(config.digitalOceanToken),
    body: JSON.stringify({
      model: config.chatModel,
      messages: [systemPrompt(), ...history],
      temperature: 0.7,
      max_completion_tokens: 512,
    }),
  });
  const payload = await readJson(response, 'DigitalOcean chat');
  const text = payload.choices?.[0]?.message?.content?.trim();
  if (!text) {
    throw new Error('DigitalOcean chat returned no assistant text');
  }
  return text;
}

export async function synthesizeSpeech(config, text) {
  const response = await fetchWithTimeout(`${INFERENCE_BASE_URL}/audio/speech`, {
    method: 'POST',
    headers: authorizationHeaders(config.digitalOceanToken),
    body: JSON.stringify({
      model: config.ttsModel,
      input: text,
      voice: config.ttsVoice,
      response_format: 'wav',
      instructions: 'Speak naturally, warmly, and concisely as a young anime heroine.',
    }),
  });
  const body = Buffer.from(await response.arrayBuffer());
  if (!response.ok) {
    throw new Error(`DigitalOcean TTS failed (${response.status}): ${body.toString('utf8', 0, 300)}`);
  }
  const contentType = response.headers.get('content-type') ?? '';
  if (!contentType.includes('json')) {
    return body;
  }

  const payload = JSON.parse(body.toString('utf8'));
  const encoded = payload.data?.[0]?.b64_json ?? payload.b64_json;
  if (!encoded) {
    throw new Error('DigitalOcean TTS response contained no audio data');
  }
  return Buffer.from(encoded, 'base64');
}

export async function transcribeSpeech(config, wav) {
  if (!config.sttBaseUrl || !config.sttModel) {
    throw new Error('Yuki speech-to-text is not configured');
  }

  const form = new FormData();
  form.append('model', config.sttModel);
  form.append('file', new Blob([wav], {type: 'audio/wav'}), 'yuki-input.wav');
  const headers = {};
  if (config.sttApiKey) {
    headers.authorization = `Bearer ${config.sttApiKey}`;
  }
  const response = await fetchWithTimeout(`${config.sttBaseUrl}/audio/transcriptions`, {
    method: 'POST',
    headers,
    body: form,
  });
  const payload = await readJson(response, 'speech-to-text');
  const text = payload.text?.trim();
  if (!text) {
    throw new Error('Speech-to-text returned no transcription');
  }
  return text;
}

function authorizationHeaders(token) {
  return {
    authorization: `Bearer ${token}`,
    'content-type': 'application/json',
  };
}

function fetchWithTimeout(url, options) {
  return fetch(url, {...options, signal: AbortSignal.timeout(REQUEST_TIMEOUT_MS)});
}

function systemPrompt() {
  return {
    role: 'system',
    content: yukiSystemInstruction('digitalocean'),
  };
}

async function readJson(response, label) {
  const text = await response.text();
  let payload;
  try {
    payload = JSON.parse(text);
  } catch {
    throw new Error(`${label} returned invalid JSON (${response.status})`);
  }
  if (!response.ok) {
    throw new Error(`${label} failed (${response.status}): ${text.slice(0, 300)}`);
  }
  return payload;
}
