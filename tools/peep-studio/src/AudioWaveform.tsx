import { useEffect, useRef, useState } from "react";
import { Volume2 } from "lucide-react";

const BINS = 64;
const CACHE_KEY = "peep-studio.waveforms.v1";
const inFlight = new Map<string, Promise<number[]>>();

async function decodePeaks(key: string, data: string): Promise<number[]> {
  let cache: Record<string, number[]> = {};
  try {
    const stored = JSON.parse(localStorage.getItem(CACHE_KEY) ?? "{}");
    if (stored && typeof stored === "object" && !Array.isArray(stored)) cache = stored;
    const peaks = cache[key];
    if (Array.isArray(peaks) && peaks.length === BINS && peaks.every(n => Number.isFinite(n) && n >= 0 && n <= 1)) return peaks;
  } catch { /* A damaged cache must not prevent a fresh preview. */ }
  const bytes = Uint8Array.from(atob(data), char => char.charCodeAt(0));
  const context = new OfflineAudioContext(1, 1, 44100);
  const audio = await context.decodeAudioData(bytes.buffer);
  const peaks = Array<number>(BINS).fill(0);
  for (let channel = 0; channel < audio.numberOfChannels; channel++) {
    const samples = audio.getChannelData(channel);
    for (let i = 0; i < samples.length; i++) {
      const bin = Math.min(BINS - 1, Math.floor(i * BINS / samples.length));
      peaks[bin] = Math.max(peaks[bin], Math.min(1, Math.abs(samples[i])));
    }
  }
  // Scale only the drawing, never the source audio or its playback level.
  const maximum = Math.max(...peaks);
  const normalized = maximum > 0 ? peaks.map(value => value / maximum) : peaks;
  try {
    // Other asset decodes may have completed while this one was pending.
    try {
      const current = JSON.parse(localStorage.getItem(CACHE_KEY) ?? "{}");
      if (current && typeof current === "object" && !Array.isArray(current)) cache = current;
    } catch { /* Replace malformed storage with the decoded preview. */ }
    const entries = Object.entries(cache).filter(([id]) => id !== key).slice(-127);
    localStorage.setItem(CACHE_KEY, JSON.stringify(Object.fromEntries([...entries, [key, normalized]])));
  } catch { /* Preview remains available when persistent storage is full. */ }
  return normalized;
}

export function AudioWaveform({ projectPath, sourcePath, revision }: {
  projectPath: string | null; sourcePath?: string; revision?: number;
}) {
  const [peaks, setPeaks] = useState<number[] | null>(null);
  const ref = useRef<HTMLCanvasElement>(null);
  useEffect(() => {
    let cancelled = false;
    setPeaks(null);
    const read = window.peepStudio?.audioThumbnailSource;
    if (!read || !projectPath || !sourcePath) return;
    void read(projectPath, sourcePath).then(source => {
      let pending = inFlight.get(source.key);
      if (!pending) {
        pending = decodePeaks(source.key, source.data);
        inFlight.set(source.key, pending);
        void pending.finally(() => inFlight.delete(source.key)).catch(() => {});
      }
      return pending;
    }).then(result => { if (!cancelled) setPeaks(result); }).catch(() => {
      if (!cancelled) setPeaks(null);
    });
    return () => { cancelled = true; };
  }, [projectPath, sourcePath, revision]);
  useEffect(() => {
    const context = ref.current?.getContext("2d");
    if (!context || !peaks) return;
    context.clearRect(0, 0, 128, 40);
    context.fillStyle = "#377d82";
    peaks.forEach((peak, index) => {
      const height = Math.max(1, Math.round(peak * 36));
      context.fillRect(index * 2, (40 - height) / 2, 1, height);
    });
  }, [peaks]);
  return <span className="audio-waveform" aria-hidden="true">
    {peaks ? <canvas ref={ref} width={128} height={40} /> : <Volume2 size={22} />}
  </span>;
}
