export type AudioNormalizationResult = {
  wav: Buffer;
  normalized: boolean;
  inputPeakDbfs: number | null;
  outputPeakDbfs: number | null;
  gainDb: number;
  channels: number;
  sampleRateHz: number;
  bitsPerSample: number;
  originalDurationMs: number;
  outputDurationMs: number;
};

export type AudioWaveInspection = {
  channels: number;
  sampleRateHz: number;
  bitsPerSample: number;
  durationMs: number;
  peakDbfs: number | null;
  waveformPeaks: number[];
  suggestedTrimStartMs: number;
  suggestedTrimEndMs: number;
};

export type AudioPreparationOptions = {
  normalize: boolean;
  targetPeakDbfs: number;
  trimStartMs?: number;
  trimEndMs?: number;
};

type PcmWaveFormat = {
  channels: number;
  sampleRateHz: number;
  blockAlign: number;
  bitsPerSample: number;
  dataOffset: number;
  dataLength: number;
};

function looksLikeChunk(wav: Buffer, offset: number): boolean {
  if (offset + 8 > wav.length) return false;
  for (let index = 0; index < 4; index += 1) {
    const value = wav[offset + index];
    if (value < 0x20 || value > 0x7e) return false;
  }
  const length = wav.readUInt32LE(offset + 4);
  return length <= wav.length - offset - 8;
}

function nextChunkOffset(wav: Buffer, payloadEnd: number, chunkLength: number): number {
  if ((chunkLength & 1) === 0 || payloadEnd >= wav.length) return payloadEnd;
  const paddedOffset = payloadEnd + 1;
  return looksLikeChunk(wav, payloadEnd) && !looksLikeChunk(wav, paddedOffset)
    ? payloadEnd
    : paddedOffset;
}

function parsePcmWave(wav: Buffer): PcmWaveFormat {
  if (wav.length < 44 || wav.toString("ascii", 0, 4) !== "RIFF" || wav.toString("ascii", 8, 12) !== "WAVE") {
    throw new Error("Audio source must be a RIFF WAV file");
  }
  let format: Omit<PcmWaveFormat, "dataOffset" | "dataLength"> | null = null;
  let dataOffset = -1;
  let dataLength = 0;
  for (let offset = 12; offset + 8 <= wav.length;) {
    const chunkId = wav.toString("ascii", offset, offset + 4);
    const chunkLength = wav.readUInt32LE(offset + 4);
    const payloadOffset = offset + 8;
    const payloadEnd = payloadOffset + chunkLength;
    if (payloadEnd > wav.length) throw new Error("WAV chunk extends beyond the file");
    if (chunkId === "fmt ") {
      if (chunkLength < 16) throw new Error("WAV format chunk is incomplete");
      const audioFormat = wav.readUInt16LE(payloadOffset);
      const channels = wav.readUInt16LE(payloadOffset + 2);
      const sampleRateHz = wav.readUInt32LE(payloadOffset + 4);
      const blockAlign = wav.readUInt16LE(payloadOffset + 12);
      const bitsPerSample = wav.readUInt16LE(payloadOffset + 14);
      if (audioFormat !== 1) throw new Error("Only uncompressed PCM WAV audio can be normalized");
      if (channels < 1 || channels > 2) throw new Error("PCM WAV audio must be mono or stereo");
      if (![8, 16, 24, 32].includes(bitsPerSample)) throw new Error("PCM WAV audio must use 8, 16, 24, or 32-bit samples");
      if (blockAlign !== channels * (bitsPerSample / 8)) throw new Error("PCM WAV block alignment is invalid");
      format = { channels, sampleRateHz, blockAlign, bitsPerSample };
    } else if (chunkId === "data" && dataOffset < 0) {
      dataOffset = payloadOffset;
      dataLength = chunkLength;
    }
    offset = nextChunkOffset(wav, payloadEnd, chunkLength);
  }
  if (format === null || dataOffset < 0) throw new Error("WAV file must contain format and audio data chunks");
  if (dataLength === 0 || dataLength % format.blockAlign !== 0) throw new Error("PCM WAV audio data is empty or incomplete");
  return { ...format, dataOffset, dataLength };
}

function readSample(wav: Buffer, offset: number, bitsPerSample: number): number {
  if (bitsPerSample === 8) return wav.readUInt8(offset) - 128;
  if (bitsPerSample === 16) return wav.readInt16LE(offset);
  if (bitsPerSample === 24) return wav.readIntLE(offset, 3);
  return wav.readInt32LE(offset);
}

function writeSample(wav: Buffer, offset: number, bitsPerSample: number, sample: number): void {
  if (bitsPerSample === 8) wav.writeUInt8(sample + 128, offset);
  else if (bitsPerSample === 16) wav.writeInt16LE(sample, offset);
  else if (bitsPerSample === 24) wav.writeIntLE(sample, offset, 3);
  else wav.writeInt32LE(sample, offset);
}

function peakDbfs(peak: number): number | null {
  return peak > 0 ? 20 * Math.log10(peak) : null;
}

function framePeak(wav: Buffer, format: PcmWaveFormat, frame: number): number {
  const bytesPerSample = format.bitsPerSample / 8;
  const frameOffset = format.dataOffset + frame * format.blockAlign;
  let peak = 0;
  for (let channel = 0; channel < format.channels; channel += 1) {
    peak = Math.max(peak, Math.abs(readSample(wav, frameOffset + channel * bytesPerSample, format.bitsPerSample)));
  }
  return peak;
}

export function inspectPcmWave(source: Buffer, binCount = 192): AudioWaveInspection {
  const format = parsePcmWave(source);
  const frameCount = format.dataLength / format.blockAlign;
  const fullScale = 2 ** (format.bitsPerSample - 1);
  const peaks = Array(Math.max(1, binCount)).fill(0) as number[];
  let peakSample = 0;
  let firstAudibleFrame = frameCount;
  let lastAudibleFrame = -1;
  const silenceThreshold = fullScale * (10 ** (-48 / 20));
  for (let frame = 0; frame < frameCount; frame += 1) {
    const peak = framePeak(source, format, frame);
    peakSample = Math.max(peakSample, peak);
    const bin = Math.min(peaks.length - 1, Math.floor((frame / frameCount) * peaks.length));
    peaks[bin] = Math.max(peaks[bin], peak / fullScale);
    if (peak >= silenceThreshold) {
      firstAudibleFrame = Math.min(firstAudibleFrame, frame);
      lastAudibleFrame = frame;
    }
  }
  const displayPeak = Math.max(...peaks);
  const waveformPeaks = displayPeak > 0 ? peaks.map((peak) => peak / displayPeak) : peaks;
  const paddingFrames = Math.round(format.sampleRateHz * 0.02);
  const suggestedStartFrame = lastAudibleFrame < 0 ? 0 : Math.max(0, firstAudibleFrame - paddingFrames);
  const suggestedEndFrame = lastAudibleFrame < 0 ? frameCount : Math.min(frameCount, lastAudibleFrame + paddingFrames + 1);
  return {
    channels: format.channels,
    sampleRateHz: format.sampleRateHz,
    bitsPerSample: format.bitsPerSample,
    durationMs: (frameCount * 1000) / format.sampleRateHz,
    peakDbfs: peakDbfs(peakSample / fullScale),
    waveformPeaks,
    suggestedTrimStartMs: (suggestedStartFrame * 1000) / format.sampleRateHz,
    suggestedTrimEndMs: (suggestedEndFrame * 1000) / format.sampleRateHz,
  };
}

function trimPcmWave(source: Buffer, startMs: number, endMs: number): Buffer {
  const format = parsePcmWave(source);
  const frameCount = format.dataLength / format.blockAlign;
  const startFrame = Math.max(0, Math.min(frameCount - 1, Math.floor((startMs * format.sampleRateHz) / 1000)));
  const endFrame = Math.max(startFrame + 1, Math.min(frameCount, Math.ceil((endMs * format.sampleRateHz) / 1000)));
  if (startFrame === 0 && endFrame === frameCount) return Buffer.from(source);
  const startByte = format.dataOffset + startFrame * format.blockAlign;
  const endByte = format.dataOffset + endFrame * format.blockAlign;
  const trimmedData = source.subarray(startByte, endByte);
  const oldTrailingOffset = nextChunkOffset(source, format.dataOffset + format.dataLength, format.dataLength);
  const padding = trimmedData.length & 1 ? Buffer.alloc(1) : Buffer.alloc(0);
  const output = Buffer.concat([
    source.subarray(0, format.dataOffset),
    trimmedData,
    padding,
    source.subarray(oldTrailingOffset),
  ]);
  output.writeUInt32LE(trimmedData.length, format.dataOffset - 4);
  output.writeUInt32LE(output.length - 8, 4);
  return output;
}

export function preparePcmWave(source: Buffer, options: AudioPreparationOptions): AudioNormalizationResult {
  const original = inspectPcmWave(source, 1);
  const startMs = options.trimStartMs ?? 0;
  const endMs = options.trimEndMs ?? original.durationMs;
  if (!Number.isFinite(startMs) || !Number.isFinite(endMs) || startMs < 0 || endMs <= startMs || endMs > original.durationMs + 0.1) {
    throw new Error("Audio trim range is invalid");
  }
  const trimmed = trimPcmWave(source, startMs, endMs);
  const prepared = options.normalize
    ? normalizePcmWave(trimmed, options.targetPeakDbfs)
    : (() => {
      const inspection = inspectPcmWave(trimmed, 1);
      return {
        wav: trimmed,
        normalized: false,
        inputPeakDbfs: inspection.peakDbfs,
        outputPeakDbfs: inspection.peakDbfs,
        gainDb: 0,
        channels: inspection.channels,
        sampleRateHz: inspection.sampleRateHz,
        bitsPerSample: inspection.bitsPerSample,
        originalDurationMs: inspection.durationMs,
        outputDurationMs: inspection.durationMs,
      };
    })();
  return { ...prepared, originalDurationMs: original.durationMs, outputDurationMs: inspectPcmWave(prepared.wav, 1).durationMs };
}

export function normalizePcmWave(source: Buffer, targetPeakDbfs = -6): AudioNormalizationResult {
  if (!Number.isFinite(targetPeakDbfs) || targetPeakDbfs > -6 || targetPeakDbfs < -24) {
    throw new Error("Audio normalization target must be between -24 and -6 dBFS");
  }
  const format = parsePcmWave(source);
  const output = Buffer.from(source);
  const bytesPerSample = format.bitsPerSample / 8;
  const fullScale = 2 ** (format.bitsPerSample - 1);
  let peakSample = 0;
  for (let offset = format.dataOffset; offset < format.dataOffset + format.dataLength; offset += bytesPerSample) {
    peakSample = Math.max(peakSample, Math.abs(readSample(source, offset, format.bitsPerSample)));
  }
  const inputPeak = peakSample / fullScale;
  if (peakSample === 0) {
    return {
      wav: output,
      normalized: false,
      inputPeakDbfs: null,
      outputPeakDbfs: null,
      gainDb: 0,
      channels: format.channels,
      sampleRateHz: format.sampleRateHz,
      bitsPerSample: format.bitsPerSample,
      originalDurationMs: (format.dataLength / format.blockAlign * 1000) / format.sampleRateHz,
      outputDurationMs: (format.dataLength / format.blockAlign * 1000) / format.sampleRateHz,
    };
  }
  const targetPeak = 10 ** (targetPeakDbfs / 20);
  const gain = targetPeak / inputPeak;
  const minimum = -fullScale;
  const maximum = fullScale - 1;
  let outputPeakSample = 0;
  for (let offset = format.dataOffset; offset < format.dataOffset + format.dataLength; offset += bytesPerSample) {
    const scaled = Math.max(minimum, Math.min(maximum, Math.round(readSample(source, offset, format.bitsPerSample) * gain)));
    writeSample(output, offset, format.bitsPerSample, scaled);
    outputPeakSample = Math.max(outputPeakSample, Math.abs(scaled));
  }
  return {
    wav: output,
    normalized: Math.abs(gain - 1) > 0.000001,
    inputPeakDbfs: peakDbfs(inputPeak),
    outputPeakDbfs: peakDbfs(outputPeakSample / fullScale),
    gainDb: 20 * Math.log10(gain),
    channels: format.channels,
    sampleRateHz: format.sampleRateHz,
    bitsPerSample: format.bitsPerSample,
    originalDurationMs: (format.dataLength / format.blockAlign * 1000) / format.sampleRateHz,
    outputDurationMs: (format.dataLength / format.blockAlign * 1000) / format.sampleRateHz,
  };
}
