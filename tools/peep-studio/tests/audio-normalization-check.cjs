const assert = require("node:assert/strict");
const { inspectPcmWave, normalizePcmWave, preparePcmWave } = require("../dist-electron/audioNormalization.js");

function pcmWave(bitsPerSample, samples, channels = 1, format = 1) {
  const bytesPerSample = bitsPerSample / 8;
  const dataLength = samples.length * bytesPerSample;
  const wav = Buffer.alloc(44 + dataLength);
  wav.write("RIFF", 0, "ascii");
  wav.writeUInt32LE(wav.length - 8, 4);
  wav.write("WAVEfmt ", 8, "ascii");
  wav.writeUInt32LE(16, 16);
  wav.writeUInt16LE(format, 20);
  wav.writeUInt16LE(channels, 22);
  wav.writeUInt32LE(16000, 24);
  wav.writeUInt32LE(16000 * channels * bytesPerSample, 28);
  wav.writeUInt16LE(channels * bytesPerSample, 32);
  wav.writeUInt16LE(bitsPerSample, 34);
  wav.write("data", 36, "ascii");
  wav.writeUInt32LE(dataLength, 40);
  samples.forEach((sample, index) => {
    const offset = 44 + index * bytesPerSample;
    if (bitsPerSample === 8) wav.writeUInt8(sample + 128, offset);
    else if (bitsPerSample === 16) wav.writeInt16LE(sample, offset);
    else if (bitsPerSample === 24) wav.writeIntLE(sample, offset, 3);
    else wav.writeInt32LE(sample, offset);
  });
  return wav;
}

for (const bits of [8, 16, 24, 32]) {
  const quarterScale = 2 ** (bits - 3);
  const source = pcmWave(bits, [quarterScale, -quarterScale, quarterScale / 2, 0]);
  const original = Buffer.from(source);
  const result = normalizePcmWave(source, -6);
  assert.deepEqual(source, original, `${bits}-bit source must remain unchanged`);
  assert.equal(result.bitsPerSample, bits);
  assert.equal(result.normalized, true);
  assert(Math.abs(result.inputPeakDbfs + 12.041) < 0.05, `${bits}-bit input peak`);
  assert(Math.abs(result.outputPeakDbfs + 6) < (bits === 8 ? 0.2 : 0.01), `${bits}-bit output peak`);
  assert(Math.abs(result.gainDb - 6.041) < 0.05, `${bits}-bit gain`);
}

const stereo = normalizePcmWave(pcmWave(16, [1000, -2000, 3000, -4000], 2), -9);
assert.equal(stereo.channels, 2);
assert(Math.abs(stereo.outputPeakDbfs + 9) < 0.01);

const silence = normalizePcmWave(pcmWave(16, [0, 0, 0, 0]), -6);
assert.equal(silence.normalized, false);
assert.equal(silence.inputPeakDbfs, null);
assert.equal(silence.outputPeakDbfs, null);
assert.equal(silence.gainDb, 0);

assert.throws(() => normalizePcmWave(pcmWave(16, [1000], 1, 3), -6), /uncompressed PCM/);
assert.throws(() => normalizePcmWave(pcmWave(16, [1000]), -3), /between -24 and -6/);

const paddedSamples = Array(1000).fill(0);
paddedSamples[400] = 4000;
paddedSamples[599] = -4000;
const padded = pcmWave(16, paddedSamples);
const inspected = inspectPcmWave(padded, 8);
assert.equal(inspected.waveformPeaks.length, 8);
assert(inspected.suggestedTrimStartMs > 0);
assert(inspected.suggestedTrimEndMs < inspected.durationMs);
const trimmed = preparePcmWave(padded, {
  normalize: true,
  targetPeakDbfs: -6,
  trimStartMs: (400 * 1000) / 16000,
  trimEndMs: (600 * 1000) / 16000,
});
assert.equal(Math.round(trimmed.originalDurationMs * 16000 / 1000), 1000);
assert.equal(Math.round(trimmed.outputDurationMs * 16000 / 1000), 200);
assert(Math.abs(trimmed.outputPeakDbfs + 6) < 0.01);
assert.equal(trimmed.wav.readUInt32LE(40), 400);
assert.equal(trimmed.wav.readUInt32LE(4), trimmed.wav.length - 8);

const baseWithData = pcmWave(16, [1000, -1000, 500, -500]);
const oddMetadata = Buffer.alloc(11);
oddMetadata.write("bext", 0, "ascii");
oddMetadata.writeUInt32LE(3, 4);
oddMetadata.write("abc", 8, "ascii");
const unpaddedWave = Buffer.concat([baseWithData.subarray(0, 36), oddMetadata, baseWithData.subarray(36)]);
unpaddedWave.writeUInt32LE(unpaddedWave.length - 8, 4);
const unpaddedInspection = inspectPcmWave(unpaddedWave, 4);
assert.equal(unpaddedInspection.durationMs, 0.25);
assert.doesNotThrow(() => preparePcmWave(unpaddedWave, {
  normalize: true, targetPeakDbfs: -6, trimStartMs: 0, trimEndMs: unpaddedInspection.durationMs,
}));

console.log("PCM WAV normalization and frame-aligned trimming passed for supported depths, stereo, silence, and invalid inputs");
