const MCP_PROTOCOL_VERSION = '2024-11-05';
const DEFAULT_TIMEOUT_MS = 10_000;

export class DeviceMcpClient {
  constructor(sendJson, {timeoutMs = DEFAULT_TIMEOUT_MS, visionUrl, visionToken} = {}) {
    this.sendJson = sendJson;
    this.timeoutMs = timeoutMs;
    this.visionUrl = visionUrl;
    this.visionToken = visionToken;
    this.nextId = 1;
    this.pending = new Map();
    this.closed = false;
  }

  async discoverTools() {
    await this.request('initialize', {
      protocolVersion: MCP_PROTOCOL_VERSION,
      capabilities: this.visionUrl ? {
        vision: {url: this.visionUrl, token: this.visionToken ?? ''},
      } : {},
      clientInfo: {name: 'yuki-gateway', version: '0.2.0'},
    });
    this.notify('notifications/initialized', {});

    const tools = [];
    let cursor;
    do {
      const params = cursor ? {cursor} : {};
      const result = await this.request('tools/list', params);
      tools.push(...(result.tools ?? []));
      cursor = result.nextCursor;
    } while (cursor);

    return tools.map(toGeminiFunctionDeclaration);
  }

  callTool(name, args = {}) {
    return this.request('tools/call', {name, arguments: args});
  }

  handleMessage(payload) {
    if (!payload || !Number.isInteger(payload.id)) {
      return false;
    }
    const request = this.pending.get(payload.id);
    if (!request) {
      return false;
    }

    clearTimeout(request.timer);
    this.pending.delete(payload.id);
    if (payload.error) {
      request.reject(new Error(payload.error.message ?? 'Device MCP request failed'));
    } else {
      request.resolve(payload.result);
    }
    return true;
  }

  close() {
    this.closed = true;
    for (const request of this.pending.values()) {
      clearTimeout(request.timer);
      request.reject(new Error('Device MCP connection closed'));
    }
    this.pending.clear();
  }

  request(method, params) {
    if (this.closed) {
      return Promise.reject(new Error('Device MCP connection is closed'));
    }

    const id = this.nextId++;
    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => {
        this.pending.delete(id);
        reject(new Error(`Device MCP ${method} timed out`));
      }, this.timeoutMs);
      this.pending.set(id, {resolve, reject, timer});
      this.sendJson({
        type: 'mcp',
        payload: {jsonrpc: '2.0', id, method, params},
      });
    });
  }

  notify(method, params) {
    this.sendJson({
      type: 'mcp',
      payload: {jsonrpc: '2.0', method, params},
    });
  }
}

function toGeminiFunctionDeclaration(tool) {
  return {
    name: tool.name,
    description: tool.description,
    parametersJsonSchema: tool.inputSchema ?? {type: 'object', properties: {}},
  };
}
