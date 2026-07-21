import test from 'node:test';
import assert from 'node:assert/strict';
import {
  parseIncomingOpusFrame,
  serializeOutgoingOpusFrame,
} from '../src/audio.js';

for (const version of [0, 2, 3]) {
  test(`Opus packet round-trips through protocol v${version}`, () => {
    const packet = Buffer.from([1, 2, 3, 4]);
    assert.deepEqual(parseIncomingOpusFrame(serializeOutgoingOpusFrame(packet, version), version), packet);
  });
}
