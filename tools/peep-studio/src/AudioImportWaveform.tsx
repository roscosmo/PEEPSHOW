import { Maximize2, Minus, Plus } from "lucide-react";
import { useEffect, useRef, useState, type PointerEvent as ReactPointerEvent, type WheelEvent as ReactWheelEvent } from "react";

const MAX_ZOOM = 16;

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
  const dragRef = useRef<"start" | "end" | "pan" | null>(null);
  const panRef = useRef({ clientX: 0, viewStartMs: 0 });
  const [zoom, setZoom] = useState(1);
  const [viewStartMs, setViewStartMs] = useState(0);
  const viewDurationMs = durationMs / zoom;
  const viewEndMs = viewStartMs + viewDurationMs;
  const maximumViewStart = Math.max(0, durationMs - viewDurationMs);

  useEffect(() => {
    setZoom(1);
    setViewStartMs(0);
  }, [durationMs, peaks]);

  useEffect(() => {
    setViewStartMs((current) => Math.max(0, Math.min(maximumViewStart, current)));
  }, [maximumViewStart]);

  useEffect(() => {
    const canvas = canvasRef.current;
    const context = canvas?.getContext("2d");
    if (!canvas || !context) return;
    const width = canvas.width;
    const height = canvas.height;
    const toX = (timeMs: number) => ((timeMs - viewStartMs) / viewDurationMs) * width;
    context.clearRect(0, 0, width, height);
    context.fillStyle = "#ff66ff";
    context.fillRect(0, 0, width, height);
    context.fillStyle = "#152019";
    const middle = height / 2;
    const barWidth = (width / Math.max(1, peaks.length)) * zoom;
    peaks.forEach((peak, index) => {
      const timeMs = ((index + 0.5) / peaks.length) * durationMs;
      if (timeMs < viewStartMs || timeMs > viewEndMs) return;
      const amplitude = Math.max(1, peak * (height - 22));
      context.fillRect(toX(timeMs) - barWidth / 2, middle - amplitude / 2, Math.max(1, barWidth - 1), amplitude);
    });
    const startX = toX(startMs);
    const endX = toX(endMs);
    context.fillStyle = "rgb(17 24 21 / 62%)";
    context.fillRect(0, 0, Math.max(0, Math.min(width, startX)), height);
    context.fillRect(Math.max(0, Math.min(width, endX)), 0, Math.max(0, width - endX), height);
    context.strokeStyle = "#f08a24";
    context.lineWidth = 4;
    context.beginPath();
    if (startX >= 0 && startX <= width) {
      context.moveTo(startX, 0);
      context.lineTo(startX, height);
    }
    if (endX >= 0 && endX <= width) {
      context.moveTo(endX, 0);
      context.lineTo(endX, height);
    }
    context.stroke();
    if (progress !== null) {
      context.strokeStyle = "#16727a";
      context.lineWidth = 3;
      const progressTime = startMs + Math.max(0, Math.min(1, progress)) * (endMs - startMs);
      const progressX = toX(progressTime);
      if (progressX >= 0 && progressX <= width) {
        context.beginPath();
        context.moveTo(progressX, 0);
        context.lineTo(progressX, height);
        context.stroke();
      }
    }
  }, [durationMs, endMs, peaks, progress, startMs, viewDurationMs, viewEndMs, viewStartMs, zoom]);

  const setZoomAt = (nextZoomValue: number, anchorRatio = 0.5) => {
    const nextZoom = Math.max(1, Math.min(MAX_ZOOM, nextZoomValue));
    const anchorTime = viewStartMs + anchorRatio * viewDurationMs;
    const nextDuration = durationMs / nextZoom;
    setZoom(nextZoom);
    setViewStartMs(Math.max(0, Math.min(durationMs - nextDuration, anchorTime - anchorRatio * nextDuration)));
  };
  const positionMs = (event: ReactPointerEvent<HTMLCanvasElement>) => {
    const bounds = event.currentTarget.getBoundingClientRect();
    return Math.max(viewStartMs, Math.min(viewEndMs, viewStartMs + ((event.clientX - bounds.left) / bounds.width) * viewDurationMs));
  };
  const updateBoundary = (event: ReactPointerEvent<HTMLCanvasElement>) => {
    const position = positionMs(event);
    const target = dragRef.current === "start" || dragRef.current === "end"
      ? dragRef.current
      : Math.abs(position - startMs) <= Math.abs(position - endMs) ? "start" : "end";
    dragRef.current = target;
    if (target === "start") onChange(Math.min(position, endMs - 1), endMs);
    else onChange(startMs, Math.max(position, startMs + 1));
  };
  const pan = (event: ReactPointerEvent<HTMLCanvasElement>) => {
    const bounds = event.currentTarget.getBoundingClientRect();
    const deltaMs = ((event.clientX - panRef.current.clientX) / bounds.width) * viewDurationMs;
    setViewStartMs(Math.max(0, Math.min(maximumViewStart, panRef.current.viewStartMs - deltaMs)));
  };
  const handleWheel = (event: ReactWheelEvent<HTMLCanvasElement>) => {
    event.preventDefault();
    const bounds = event.currentTarget.getBoundingClientRect();
    const anchor = Math.max(0, Math.min(1, (event.clientX - bounds.left) / bounds.width));
    setZoomAt(event.deltaY < 0 ? zoom * 1.25 : zoom / 1.25, anchor);
  };
  const finishPointer = (event: ReactPointerEvent<HTMLCanvasElement>) => {
    if (event.currentTarget.hasPointerCapture(event.pointerId)) event.currentTarget.releasePointerCapture(event.pointerId);
    dragRef.current = null;
  };
  const startPercent = ((startMs - viewStartMs) / viewDurationMs) * 100;
  const endPercent = ((endMs - viewStartMs) / viewDurationMs) * 100;

  return <div className={`audio-waveform-editor ${compact ? "compact" : ""}`}>
    <div className={`audio-import-waveform ${compact ? "compact" : ""}`}>
      <canvas
        ref={canvasRef}
        width={1200}
        height={220}
        aria-label="Audio waveform trim editor"
        onWheel={handleWheel}
        onAuxClick={(event) => event.preventDefault()}
        onPointerDown={(event) => {
          event.currentTarget.setPointerCapture(event.pointerId);
          if (event.button === 1) {
            event.preventDefault();
            dragRef.current = "pan";
            panRef.current = { clientX: event.clientX, viewStartMs };
            return;
          }
          if (event.button === 0) updateBoundary(event);
        }}
        onPointerMove={(event) => {
          if (dragRef.current === "pan") pan(event);
          else if (dragRef.current !== null) updateBoundary(event);
        }}
        onPointerUp={finishPointer}
        onPointerCancel={finishPointer}
      />
      {startPercent >= 0 && startPercent <= 100 && <span className="audio-trim-handle start" style={{ left: `${startPercent}%` }} aria-hidden="true" />}
      {endPercent >= 0 && endPercent <= 100 && <span className="audio-trim-handle end" style={{ left: `${endPercent}%` }} aria-hidden="true" />}
    </div>
    <div className="audio-waveform-zoom-controls">
      <button className="icon-button" type="button" title="Zoom out" aria-label="Zoom out"
        disabled={zoom <= 1} onClick={() => setZoomAt(zoom / 2)}><Minus size={14} aria-hidden="true" /></button>
      <input type="range" min={1} max={MAX_ZOOM} step={0.25} value={zoom}
        aria-label="Waveform zoom" onChange={(event) => setZoomAt(Number(event.target.value))} />
      <button className="icon-button" type="button" title="Zoom in" aria-label="Zoom in"
        disabled={zoom >= MAX_ZOOM} onClick={() => setZoomAt(zoom * 2)}><Plus size={14} aria-hidden="true" /></button>
      <button className="icon-button" type="button" title="Fit full waveform" aria-label="Fit full waveform"
        disabled={zoom === 1} onClick={() => { setZoom(1); setViewStartMs(0); }}><Maximize2 size={14} aria-hidden="true" /></button>
      <output>{zoom.toFixed(zoom % 1 === 0 ? 0 : 1)}x</output>
      <span>{Math.round(viewStartMs)}-{Math.round(viewEndMs)} ms</span>
    </div>
  </div>;
}
