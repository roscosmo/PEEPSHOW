import { useId, useState } from "react";
import { ArrowDown, ArrowLeft, ArrowRight, ArrowUp, ChevronDown, ChevronRight, Pause, Play, RotateCcw, StepForward } from "lucide-react";
import { FramebufferCanvas } from "./FramebufferCanvas";
import type { PreviewSnapshot } from "./types";
import "./EmulatorPanel.css";

const directions = [
  { source: "JOY_UP", label: "Up", Icon: ArrowUp, position: "up" },
  { source: "JOY_LEFT", label: "Left", Icon: ArrowLeft, position: "left" },
  { source: "JOY_RIGHT", label: "Right", Icon: ArrowRight, position: "right" },
  { source: "JOY_DOWN", label: "Down", Icon: ArrowDown, position: "down" },
];

export function EmulatorPanel({ preview, sceneName, playing, onReset, onTogglePlaying, onAdvance, onInput }: {
  preview: PreviewSnapshot | null;
  sceneName: string;
  playing: boolean;
  onReset: () => void;
  onTogglePlaying: () => void;
  onAdvance: () => void;
  onInput: (source: string) => void;
}) {
  const [collapsed, setCollapsed] = useState(false);
  const id = useId();
  const available = preview !== null;
  return (
    <section className={`emulator-panel ${collapsed ? "collapsed" : "expanded"}`} aria-label="Device emulator">
      <div className="emulator-toolbar">
        <button className="emulator-collapse" type="button" aria-expanded={!collapsed}
          aria-controls={`${id}-transport ${id}-inputs`}
          title={collapsed ? "Expand emulator controls" : "Collapse emulator controls"}
          aria-label={collapsed ? "Expand emulator controls" : "Collapse emulator controls"}
          onClick={() => setCollapsed((value) => !value)}>
          {collapsed ? <ChevronRight size={18} /> : <ChevronDown size={18} />}
        </button>
        <div className="emulator-transport" id={`${id}-transport`} hidden={collapsed} aria-label="Preview playback">
          <button type="button" onClick={onReset} disabled={!available} title="Reset preview" aria-label="Reset preview"><RotateCcw size={17} /></button>
          <button type="button" onClick={onTogglePlaying} disabled={!available}
            title={playing ? "Pause preview" : "Play preview"} aria-label={playing ? "Pause preview" : "Play preview"}>
            {playing ? <Pause size={18} /> : <Play size={18} />}
          </button>
          <button type="button" onClick={onAdvance} disabled={!available} title="Advance 250 ms" aria-label="Advance 250 ms"><StepForward size={17} /></button>
        </div>
      </div>
      <div className="emulator-display"><FramebufferCanvas framebuffer={preview?.framebuffer ?? null} /></div>
      <div className="emulator-inputs" id={`${id}-inputs`} hidden={collapsed}>
        <div className="emulator-joystick" aria-label="Joystick cardinal inputs">
          {directions.map(({ source, label, Icon, position }) => (
            <button key={source} type="button" className={`emulator-direction direction-${position}`}
              title={`Joystick ${label.toLowerCase()}`} aria-label={`Joystick ${label.toLowerCase()}`}
              disabled={!available} onClick={() => onInput(source)}><Icon size={16} /></button>
          ))}
        </div>
        <div className="emulator-center">
          <button className="emulator-start" type="button" title="Start" aria-label="Start" disabled={!available} onClick={() => onInput("BUTTON_START")}>START</button>
          <div className="emulator-identity">
            <span title={`Scene: ${sceneName}`} aria-label={`Scene: ${sceneName}`}>{sceneName}</span>
            <strong title={`State: ${preview?.scene.display_name ?? "No active state"}`} aria-label={`State: ${preview?.scene.display_name ?? "No active state"}`}>{preview?.scene.display_name ?? "No active state"}</strong>
          </div>
          {preview !== null && <div className="emulator-timeline" aria-label="Preview timing">
            <span>{preview.timeline.ownership === "scene_objects" ? "Scene time"
              : `Step ${(preview.timeline.step_index ?? 0) + 1}/${preview.timeline.step_count ?? 1}`}</span>
            <span>{preview.timeline.elapsed_ms} ms</span>
          </div>}
        </div>
        <div className="emulator-buttons" aria-label="Device buttons">
          {["L", "R", "B", "A"].map((label) => (
            <button type="button" key={label} className={`emulator-button button-${label.toLowerCase()}`}
              title={`Button ${label}`} aria-label={`Button ${label}`} disabled={!available}
              onClick={() => onInput(`BUTTON_${label}`)}>{label}</button>
          ))}
        </div>
      </div>
    </section>
  );
}
