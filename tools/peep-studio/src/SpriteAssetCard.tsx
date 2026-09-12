import { useEffect, useRef, useState } from "react";
import { FramePreviewCanvas } from "./FramebufferCanvas";
import type { CompiledAssetFrame } from "./types";

export function SpriteAssetCard({ frames, name, selected, playback, onSelect, durations, disabled = false }: {
  frames: CompiledAssetFrame[];
  name: string;
  selected: boolean;
  playback: "hover" | "always" | "off";
  onSelect: () => void;
  durations?: number[];
  disabled?: boolean;
}) {
  const ref = useRef<HTMLButtonElement>(null);
  const [hovered, setHovered] = useState(false);
  const [focused, setFocused] = useState(false);
  const [visible, setVisible] = useState(false);
  const [foreground, setForeground] = useState(!document.hidden);
  const [step, setStep] = useState(0);
  const running = visible && foreground && frames.length > 1
    && (playback === "always" || (playback === "hover" && (hovered || focused)));
  useEffect(() => {
    const observer = new IntersectionObserver(entries => setVisible(entries[0]?.isIntersecting === true));
    if (ref.current) observer.observe(ref.current);
    const onVisibility = () => setForeground(!document.hidden);
    document.addEventListener("visibilitychange", onVisibility);
    return () => {
      observer.disconnect();
      document.removeEventListener("visibilitychange", onVisibility);
    };
  }, []);
  useEffect(() => {
    setStep(0);
  }, [running, frames.length]);
  useEffect(() => {
    if (!running) return;
    const timer = window.setTimeout(() => setStep(current => (current + 1) % frames.length), durations?.[step % frames.length] ?? 250);
    return () => window.clearTimeout(timer);
  }, [running, frames.length, step, durations]);
  const frame = frames[running ? step % frames.length : 0];
  if (!frame) return null;
  return (
    <button ref={ref} type="button" disabled={disabled} className={selected ? "selected" : ""}
      data-preview-frame={frame.frame_id} title={name} onClick={onSelect}
      onMouseEnter={() => setHovered(true)} onMouseLeave={() => setHovered(false)}
      onFocus={() => setFocused(true)} onBlur={() => setFocused(false)}>
      <span className="asset-frame-preview"><FramePreviewCanvas frame={frame} /></span>
      <strong>{name}</strong>
      <small>{frames.length} frame{frames.length === 1 ? "" : "s"} / {frames[0].width}x{frames[0].height}</small>
    </button>
  );
}
