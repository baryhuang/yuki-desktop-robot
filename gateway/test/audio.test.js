import test from 'node:test';
import assert from 'node:assert/strict';
import {
  parseIncomingOpusFrame,
  pcmToWav,
  serializeOutgoingOpusFrame,
} from '../src/audio.js';

test('PCM is wrapped as a mono 16-bit WAV', () => {
  const pcm = Buffer.from([1, 0, 2, 0]);
  const wav = pcmToWav(pcm, 16000);
  assert.equal(wav.subarray(0, 4).toString(), 'RIFF');
  assert.equal(wav.subarray(8, 12).toString(), 'WAVE');
  assert.equal(wav.readUInt32LE(24), 16000);
  assert.equal(wav.readUInt32LE(40), pcm.length);
  assert.deepEqual(wav.subarray(44), pcm);
});

for (const version of [0, 2, 3]) {
  test(`Opus packet round-trips through protocol v${version}`, () => {
    const packet = Buffer.from([1, 2, 3, 4]);
    assert.deepEqual(parseIncomingOpusFrame(serializeOutgoingOpusFrame(packet, version), version), packet);
  });
}
