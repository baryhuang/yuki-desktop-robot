import OpusScript from 'opusscript';

export const INPUT_SAMPLE_RATE = 16000;
export const OUTPUT_SAMPLE_RATE = 24000;
export const CHANNELS = 1;
export const FRAME_DURATION_MS = 60;

export function createInputDecoder() {
  return new OpusScript(INPUT_SAMPLE_RATE, CHANNELS, OpusScript.Application.AUDIO);
}

export function createOutputEncoder() {
  return new OpusScript(OUTPUT_SAMPLE_RATE, CHANNELS, OpusScript.Application.AUDIO);
}

export function opusPacketToPcm(decoder, packet) {
  return Buffer.from(decoder.decode(packet, INPUT_SAMPLE_RATE * FRAME_DURATION_MS / 1000));
}

export function parseIncomingOpusFrame(data, version) {
  const frame = Buffer.from(data);

  if (version === 2) {
    if (frame.length < 16 || frame.readUInt16BE(2) !== 0) {
      throw new Error('Invalid protocol v2 audio frame');
    }
    const payloadSize = frame.readUInt32BE(12);
    if (payloadSize !== frame.length - 16) {
      throw new Error('Invalid protocol v2 payload length');
    }
    return frame.subarray(16);
  }

  if (version === 3) {
    if (frame.length < 4 || frame[0] !== 0) {
      throw new Error('Invalid protocol v3 audio frame');
    }
    const payloadSize = frame.readUInt16BE(2);
    if (payloadSize !== frame.length - 4) {
      throw new Error('Invalid protocol v3 payload length');
    }
    return frame.subarray(4);
  }

  return frame;
}

export function serializeOutgoingOpusFrame(packet, version) {
  if (version === 2) {
    const frame = Buffer.alloc(16 + packet.length);
    frame.writeUInt16BE(2, 0);
    frame.writeUInt16BE(0, 2);
    frame.writeUInt32BE(0, 4);
    frame.writeUInt32BE(0, 8);
    frame.writeUInt32BE(packet.length, 12);
    packet.copy(frame, 16);
    return frame;
  }

  if (version === 3) {
    const frame = Buffer.alloc(4 + packet.length);
    frame[0] = 0;
    frame[1] = 0;
    frame.writeUInt16BE(packet.length, 2);
    packet.copy(frame, 4);
    return frame;
  }

  return packet;
}
