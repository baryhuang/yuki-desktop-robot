import test from 'node:test';
import assert from 'node:assert/strict';
import {DeviceMcpClient} from '../src/mcp-client.js';

test('device MCP tools are discovered and converted for Gemini', async () => {
  const messages = [];
  const client = new DeviceMcpClient((message) => messages.push(message), {
    timeoutMs: 1000,
    visionUrl: 'https://yuki.example.com/vision',
    visionToken: 'secret',
  });
  const discovery = client.discoverTools();

  await waitFor(() => messages.length === 1);
  assert.deepEqual(messages[0].payload.params.capabilities, {
    vision: {url: 'https://yuki.example.com/vision', token: 'secret'},
  });
  client.handleMessage({jsonrpc: '2.0', id: 1, result: {capabilities: {tools: {}}}});
  await waitFor(() => messages.length === 3);
  assert.equal(messages[1].payload.method, 'notifications/initialized');
  assert.equal(messages[2].payload.method, 'tools/list');

  client.handleMessage({
    jsonrpc: '2.0',
    id: 2,
    result: {
      tools: [{
        name: 'self.robot.set_led_pattern',
        description: 'Set body LEDs.',
        inputSchema: {
          type: 'object',
          properties: {pattern: {type: 'string'}},
          required: ['pattern'],
        },
      }],
    },
  });

  assert.deepEqual(await discovery, [{
    name: 'self.robot.set_led_pattern',
    description: 'Set body LEDs.',
    parametersJsonSchema: {
      type: 'object',
      properties: {pattern: {type: 'string'}},
      required: ['pattern'],
    },
  }]);
  client.close();
});

test('device MCP tool results resolve matching requests', async () => {
  const messages = [];
  const client = new DeviceMcpClient((message) => messages.push(message), {timeoutMs: 1000});
  const result = client.callTool('self.camera.get_presence', {});
  await waitFor(() => messages.length === 1);

  assert.equal(messages[0].payload.method, 'tools/call');
  client.handleMessage({jsonrpc: '2.0', id: 1, result: {present: true}});
  assert.deepEqual(await result, {present: true});
  client.close();
});

async function waitFor(predicate) {
  for (let attempt = 0; attempt < 20; attempt++) {
    if (predicate()) return;
    await new Promise((resolve) => setTimeout(resolve, 0));
  }
  throw new Error('Condition was not met');
}
