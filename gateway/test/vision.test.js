import test from 'node:test';
import assert from 'node:assert/strict';
import {createServer} from 'node:http';
import {createHttpHandler, GeminiVision} from '../src/vision.js';

test('camera JPEG is analyzed and returned in the firmware response format', async (context) => {
  const calls = [];
  const vision = new GeminiVision(testConfig(), {
    createClient: () => ({models: {generateContent: async (request) => {
      calls.push(request);
      return {text: 'A person is sitting in front of Yuki.'};
    }}}),
  });
  const server = createServer(createHttpHandler({gatewayToken: 'secret', vision}));
  await new Promise((resolve) => server.listen(0, '127.0.0.1', resolve));
  context.after(() => server.close());

  const form = new FormData();
  form.set('question', 'What can you see?');
  form.set('file', new Blob([Buffer.from([0xff, 0xd8, 0xff, 0xd9])], {type: 'image/jpeg'}), 'camera.jpg');
  const response = await fetch(`http://127.0.0.1:${server.address().port}/vision`, {
    method: 'POST',
    headers: {authorization: 'Bearer secret'},
    body: form,
  });

  assert.equal(response.status, 200);
  assert.deepEqual(await response.json(), {
    success: true,
    result: 'A person is sitting in front of Yuki.',
  });
  assert.equal(calls[0].model, 'gemini-3.6-flash');
  assert.deepEqual(calls[0].config.thinkingConfig, {thinkingLevel: 'minimal'});
  assert.equal('temperature' in calls[0].config, false);
  assert.equal(calls[0].contents[1].text, 'What can you see?');
  assert.equal(calls[0].contents[0].inlineData.mimeType, 'image/jpeg');
});

test('camera endpoint requires the gateway bearer token', async (context) => {
  const server = createServer(createHttpHandler({gatewayToken: 'secret', vision: {}}));
  await new Promise((resolve) => server.listen(0, '127.0.0.1', resolve));
  context.after(() => server.close());

  const response = await fetch(`http://127.0.0.1:${server.address().port}/vision`, {method: 'POST'});
  assert.equal(response.status, 401);
});

function testConfig() {
  return {
    geminiApiKey: 'test-key',
    geminiVisionModel: 'gemini-3.6-flash',
  };
}
