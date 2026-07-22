import test from 'node:test';
import assert from 'node:assert/strict';
import {GeminiLiveBridge} from '../src/gemini-live.js';

test('Gemini Live streams device PCM and returns paced audio events', async () => {
  const realtimeInputs = [];
  const toolResponses = [];
  const jsonMessages = [];
  const audioPackets = [];
  let callbacks;
  const fakeSession = {
    sendRealtimeInput: (input) => realtimeInputs.push(input),
    sendToolResponse: (response) => toolResponses.push(response),
    close: () => {},
  };
  const createClient = () => ({
    live: {
      connect: async (params) => {
        callbacks = params.callbacks;
        assert.equal(params.model, 'gemini-3.1-flash-live-preview');
        assert.equal(params.config.realtimeInputConfig.automaticActivityDetection.disabled, true);
        assert.deepEqual(params.config.tools[0], {googleSearch: {}});
        assert.equal(params.config.tools[1].functionDeclarations[0].name, 'self.robot.wave');
        return fakeSession;
      },
    },
  });
  const bridge = new GeminiLiveBridge({
    config: testConfig(),
    toolDeclarations: [{name: 'self.robot.wave', parametersJsonSchema: {type: 'object'}}],
    callTool: async () => ({ok: true}),
    sendJson: (message) => jsonMessages.push(message),
    sendAudio: (packet) => audioPackets.push(packet),
    onError: (error) => assert.fail(error.message),
    createClient,
    frameDelayMs: 0,
  });

  await bridge.connect();
  await bridge.startListening();
  bridge.sendInputPcm(Buffer.from([1, 0, 2, 0]));
  bridge.stopListening();

  assert.deepEqual(realtimeInputs[0], {activityStart: {}});
  assert.equal(realtimeInputs[1].audio.mimeType, 'audio/pcm;rate=16000');
  assert.deepEqual(Buffer.from(realtimeInputs[1].audio.data, 'base64'), Buffer.from([1, 0, 2, 0]));
  assert.deepEqual(realtimeInputs[2], {activityEnd: {}});

  await bridge.handleMessage({
    serverContent: {inputTranscription: {text: 'Hello Yuki', finished: true}},
  });
  await bridge.handleMessage({
    serverContent: {
      outputTranscription: {text: 'Hello!', finished: true},
      modelTurn: {parts: [{inlineData: {
        mimeType: 'audio/pcm;rate=24000',
        data: Buffer.alloc(2880).toString('base64'),
      }}]},
    },
  });
  await bridge.handleMessage({serverContent: {turnComplete: true}});
  await waitFor(() => jsonMessages.some((message) => message.type === 'tts' && message.state === 'stop'));

  assert.ok(jsonMessages.some((message) => message.type === 'stt' && message.text === 'Hello Yuki'));
  assert.ok(jsonMessages.some((message) => message.type === 'tts' && message.state === 'start'));
  assert.ok(jsonMessages.some((message) => message.type === 'tts' && message.text === 'Hello!'));
  assert.equal(audioPackets.length, 1);
  assert.ok(audioPackets[0].length > 0);
  assert.deepEqual(toolResponses, []);
  assert.equal(typeof callbacks.onmessage, 'function');
  bridge.close();
});

test('proactive curiosity is sent as an ordered text turn', async () => {
  const realtimeInput = [];
  const fakeSession = {
    sendRealtimeInput: (content) => realtimeInput.push(content),
    close: () => {},
  };
  const bridge = new GeminiLiveBridge({
    config: testConfig(),
    sendJson: () => {},
    sendAudio: () => {},
    onError: (error) => assert.fail(error.message),
    createClient: () => ({live: {connect: async () => fakeSession}}),
    frameDelayMs: 0,
  });

  await bridge.sendText('Find one fresh robotics story.');
  assert.deepEqual(realtimeInput, [{text: 'Find one fresh robotics story.'}]);
  bridge.close();
});

test('Gemini function calls are executed through device MCP', async () => {
  const toolResponses = [];
  const calls = [];
  const fakeSession = {
    sendRealtimeInput: () => {},
    sendToolResponse: (response) => toolResponses.push(response),
    close: () => {},
  };
  const bridge = new GeminiLiveBridge({
    config: testConfig(),
    callTool: async (name, args) => {
      calls.push({name, args});
      return {angle: 12};
    },
    sendJson: () => {},
    sendAudio: () => {},
    onError: (error) => assert.fail(error.message),
    createClient: () => ({live: {connect: async () => fakeSession}}),
    frameDelayMs: 0,
  });
  await bridge.connect();
  await bridge.handleMessage({
    toolCall: {functionCalls: [{id: 'call-1', name: 'self.robot.set_head_angle', args: {x: 12}}]},
  });

  assert.deepEqual(calls, [{name: 'self.robot.set_head_angle', args: {x: 12}}]);
  assert.deepEqual(toolResponses, [{functionResponses: [{
    id: 'call-1',
    name: 'self.robot.set_head_angle',
    response: {output: {angle: 12}},
  }]}]);
  bridge.close();
});

test('Gemini Live resumes the session when the server sends GoAway', async () => {
  const connections = [];
  const callbacks = [];
  const sessions = [];
  const createClient = () => ({
    live: {
      connect: async (params) => {
        connections.push(params);
        callbacks.push(params.callbacks);
        const session = {close: () => { session.closed = true; }};
        sessions.push(session);
        return session;
      },
    },
  });
  const bridge = new GeminiLiveBridge({
    config: testConfig(),
    sendJson: () => {},
    sendAudio: () => {},
    onError: (error) => assert.fail(error.message),
    createClient,
    frameDelayMs: 0,
  });

  await bridge.connect();
  callbacks[0].onmessage({
    sessionResumptionUpdate: {resumable: true, newHandle: 'resume-yuki-1'},
  });
  callbacks[0].onmessage({goAway: {timeLeft: '30s'}});
  await waitFor(() => connections.length === 2);

  assert.equal(sessions[0].closed, true);
  assert.deepEqual(connections[0].config.sessionResumption, {});
  assert.deepEqual(connections[1].config.sessionResumption, {handle: 'resume-yuki-1'});
  bridge.close();
});

function testConfig() {
  return {
    geminiApiKey: 'test-key',
    geminiModel: 'gemini-3.1-flash-live-preview',
    geminiVoice: 'Leda',
    googleSearchEnabled: true,
  };
}

async function waitFor(predicate) {
  for (let attempt = 0; attempt < 20; attempt++) {
    if (predicate()) return;
    await new Promise((resolve) => setTimeout(resolve, 0));
  }
  throw new Error('Condition was not met');
}
