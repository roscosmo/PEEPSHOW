import { useEffect, useRef, type PointerEvent as ReactPointerEvent } from "react";

export function AudioImportWaveform({ peaks, durationMs, startMs, endMs, progress = null, compact = false, onChange }: {
  peaks: number[];
  durationMs: number;
  startMs: number;
  endMs: number;
  progress?: number | null;
  compact?: boolean;
  onChange: (startMs: number, endMs: number) => void;
}) {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const dragRef = useRef<"start" | "end" | null>(null);
  useEffect(() => {
    const canvas = canvasRef.current;
    const context = canvas?.getContext("2d");
    if (!canvas || !context) return;
    const width = canvas.width;
    const height = canvas.height;
    context.clearRect(0, 0, width, height);
    context.fillStyle = "#ff66ff";
    context.fillRect(0, 0, width, height);
    context.fillStyle = "#152019";
    const middle = height / 2;
    const barWidth = width / Math.max(1, peaks.length);
    peaks.forEach((peak, index) => {
      const amplitude = Math.max(1, peak * (height - 22));
      context.fillRect(index * barWidth, middle - amplitude / 2, Math.max(1, barWidth - 1), amplitude);
    });
    const startX = (startMs / durationMs) * width;
    const endX = (endMs / durationMs) * width;
    context.fillStyle = "rgb(17 24 21 / 62%)";
    context.fillRect(0, 0, startX, height);
    context.fillRect(endX, 0, width - endX, height);
    context.strokeStyle = "#f08a24";
    context.lineWidth = 4;
    context.beginPath();
    context.moveTo(startX, 0);
    context.lineTo(startX, height);
    context.moveTo(endX, 0);
    context.lineTo(endX, height);
    context.stroke();
    if (progress !== null) {
      context.strokeStyle = "#16727a";
      context.lineWidth = 3;
      const progressX = Math.max(0, Math.min(1, progress)) * width;
      context.beginPath();
      context.moveTo(progressX, 0);
      context.lineTo(progressX, height);
      context.stroke();
    }
  }, [durationMs, endMs, peaks, progress, startMs]);

  const positionMs = (event: ReactPointerEvent<HTMLCanvasElement>) => {
    const bounds = event.currentTarget.getBoundingClientRect();
    return Math.max(0, Math.min(durationMs, ((event.clientX - bounds.left) / bounds.width) * durationMs));
  };
  const update = (event: ReactPointerEvent<HTMLCanvasElement>) => {
    const position = positionMs(event);
    const target = dragRef.current ?? (Math.abs(position - startMs) <= Math.abs(position - endMs) ? "start" : "end");
    dragRef.current = target;
    if (target === "start") onChange(Math.min(position, endMs - 1), endMs);
    else onChange(startMs, Math.max(position, startMs + 1));
  };
  return <div className={`audio-import-waveform ${compact ? "compact" : ""}`}>
    <canvas
      ref={canvasRef}
      width={1200}
      height={220}
      aria-label="Audio waveform trim editor"
      onPointerDown={(event) => {
        event.currentTarget.setPointerCapture(event.pointerId);
        update(event);
      }}
      onPointerMove={(event) => {
        if (dragRef.current !== null) update(event);
      }}
      onPointerUp={(event) => {
        if (event.currentTarget.hasPointerCapture(event.pointerId)) event.currentTarget.releasePointerCapture(event.pointerId);
        dragRef.current = null;
      }}
      onPointerCancel={() => { dragRef.current = null; }}
    />
    <span className="audio-trim-handle start" style={{ left: `${(startMs / durationMs) * 100}%` }} aria-hidden="true" />
    <span className="audio-trim-handle end" style={{ left: `${(endMs / durationMs) * 100}%` }} aria-hidden="true" />
  </div>;
}
