import {GoogleGenAI} from '@google/genai';
import {isAuthorized} from './authorization.js';

const MAX_IMAGE_REQUEST_BYTES = 2 * 1024 * 1024;

export class GeminiVision {
  constructor(config, {createClient = defaultCreateClient} = {}) {
    this.config = config;
    this.client = createClient(config);
  }

  async analyze(image, question) {
    const response = await this.client.models.generateContent({
      model: this.config.geminiVisionModel,
      contents: [
        {inlineData: {mimeType: 'image/jpeg', data: image.toString('base64')}},
        {text: question || 'Briefly describe what Yuki can see.'},
      ],
      config: {
        systemInstruction: 'Answer the camera question directly and briefly. Describe only visible evidence. Do not identify a person or infer sensitive traits.',
        thinkingConfig: {thinkingLevel: 'minimal'},
        maxOutputTokens: 256,
      },
    });
    const result = response.text?.trim();
    if (!result) {
      throw new Error('Gemini vision returned no description');
    }
    return result;
  }
}

export function createHttpHandler({gatewayToken, vision}) {
  return async (request, response) => {
    const url = new URL(request.url, `http://${request.headers.host ?? 'localhost'}`);
    if (request.method === 'GET' && url.pathname === '/health') {
      writeJson(response, 200, {ok: true, vision: Boolean(vision)});
      return;
    }
    if (request.method !== 'POST' || url.pathname !== '/vision') {
      writeJson(response, 404, {success: false, message: 'Not found'});
      return;
    }
    if (!isAuthorized(request.headers.authorization, gatewayToken)) {
      writeJson(response, 401, {success: false, message: 'Unauthorized'});
      return;
    }
    if (!vision) {
      writeJson(response, 503, {success: false, message: 'Camera understanding is not configured'});
      return;
    }

    try {
      const body = await readBody(request, MAX_IMAGE_REQUEST_BYTES);
      const formRequest = new Request('http://localhost/vision', {
        method: 'POST',
        headers: {'content-type': request.headers['content-type'] ?? ''},
        body,
      });
      const form = await formRequest.formData();
      const file = form.get('file');
      const question = String(form.get('question') ?? '').trim().slice(0, 1000);
      if (!(file instanceof Blob) || file.type !== 'image/jpeg' || file.size === 0) {
        writeJson(response, 400, {success: false, message: 'A non-empty JPEG file is required'});
        return;
      }
      const result = await vision.analyze(Buffer.from(await file.arrayBuffer()), question);
      writeJson(response, 200, {success: true, result});
    } catch (error) {
      const status = error.code === 'REQUEST_TOO_LARGE' ? 413 : 500;
      console.error(`Vision request failed: ${error.message}`);
      writeJson(response, status, {success: false, message: status === 413 ? error.message : 'Camera understanding failed'});
    }
  };
}

function defaultCreateClient(config) {
  return new GoogleGenAI({
    apiKey: config.geminiApiKey,
  });
}

function readBody(request, limit) {
  return new Promise((resolve, reject) => {
    const chunks = [];
    let size = 0;
    let exceeded = false;
    request.on('data', (chunk) => {
      if (exceeded) {
        return;
      }
      size += chunk.length;
      if (size > limit) {
        exceeded = true;
        chunks.length = 0;
        const error = new Error(`Camera upload exceeds ${limit} bytes`);
        error.code = 'REQUEST_TOO_LARGE';
        reject(error);
        return;
      }
      chunks.push(chunk);
    });
    request.on('end', () => {
      if (!exceeded) {
        resolve(Buffer.concat(chunks));
      }
    });
    request.on('error', reject);
  });
}

function writeJson(response, status, value) {
  const body = JSON.stringify(value);
  response.writeHead(status, {
    'content-type': 'application/json; charset=utf-8',
    'content-length': Buffer.byteLength(body),
    'cache-control': 'no-store',
  });
  response.end(body);
}
