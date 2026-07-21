import { spawn } from 'node:child_process';
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

export function pcmToWav(pcm, sampleRate) {
  const header = Buffer.alloc(44);
  header.write('RIFF', 0);
  header.writeUInt32LE(36 + pcm.length, 4);
  header.write('WAVE', 8);
  header.write('fmt ', 12);
  header.writeUInt32LE(16, 16);
  header.writeUInt16LE(1, 20);
  header.writeUInt16LE(CHANNELS, 22);
  header.writeUInt32LE(sampleRate, 24);
  header.writeUInt32LE(sampleRate * CHANNELS * 2, 28);
  header.writeUInt16LE(CHANNELS * 2, 32);
  header.writeUInt16LE(16, 34);
  header.write('data', 36);
  header.writeUInt32LE(pcm.length, 40);
  return Buffer.concat([header, pcm]);
}

export async function wavToOutputPcm(wav) {
  return runFfmpeg(wav, [
    '-i', 'pipe:0',
    '-f', 's16le',
    '-acodec', 'pcm_s16le',
    '-ac', '1',
    '-ar', String(OUTPUT_SAMPLE_RATE),
    'pipe:1',
  ]);
}

export function pcmToOpusPackets(encoder, pcm) {
  const frameBytes = OUTPUT_SAMPLE_RATE * FRAME_DURATION_MS / 1000 * 2;
  const packets = [];

  for (let offset = 0; offset < pcm.length; offset += frameBytes) {
    const frame = Buffer.alloc(frameBytes);
    pcm.copy(frame, 0, offset, Math.min(offset + frameBytes, pcm.length));
    packets.push(Buffer.from(encoder.encode(frame, OUTPUT_SAMPLE_RATE * FRAME_DURATION_MS / 1000)));
  }

  return packets;
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

function runFfmpeg(input, args) {
  return new Promise((resolve, reject) => {
    const ffmpeg = spawn('ffmpeg', ['-hide_banner', '-loglevel', 'error', ...args]);
    const stdout = [];
    const stderr = [];

    ffmpeg.stdout.on('data', (chunk) => stdout.push(chunk));
    ffmpeg.stderr.on('data', (chunk) => stderr.push(chunk));
    ffmpeg.on('error', (error) => reject(new Error(`Unable to start ffmpeg: ${error.message}`)));
    ffmpeg.on('close', (code) => {
      if (code === 0) {
        resolve(Buffer.concat(stdout));
      } else {
        reject(new Error(`ffmpeg failed (${code}): ${Buffer.concat(stderr).toString().trim()}`));
      }
    });

    ffmpeg.stdin.end(input);
  });
}
