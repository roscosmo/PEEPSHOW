import { useEffect, useId, useState } from "react";
import { ArrowDown, ArrowLeft, ArrowRight, ArrowUp, ChevronDown, ChevronRight } from "lucide-react";
import { FramebufferCanvas } from "./FramebufferCanvas";
import type { PreviewSnapshot } from "./types";
import "./EmulatorPanel.css";

const directions = [
  { source: "JOY_UP", label: "Up", Icon: ArrowUp, position: "up" },
  { source: "JOY_LEFT", label: "Left", Icon: ArrowLeft, position: "left" },
  { source: "JOY_RIGHT", label: "Right", Icon: ArrowRight, position: "right" },
  { source: "JOY_DOWN", label: "Down", Icon: ArrowDown, position: "down" },
];

export function EmulatorPanel({ preview, sceneName, playing, onReset, onTogglePlaying, onAdvance, onInput, onCollapsedChange, mode = "docked" }: {
  preview: PreviewSnapshot | null;
  sceneName: string;
  playing: boolean;
  onReset: () => void;
  onTogglePlaying: () => void;
  onAdvance: () => void;
  onInput: (source: string) => void;
  onCollapsedChange?: (collapsed: boolean) => void;
  mode?: "docked" | "popout";
}) {
  const [collapsed, setCollapsed] = useState(false);
  const id = useId();
  const available = preview !== null;
  useEffect(() => {
    onCollapsedChange?.(collapsed);
  }, [collapsed, onCollapsedChange]);
  const transport = (
    <div className="emulator-transport" aria-label="Preview playback">
      <button className="emulator-system-control control-reset" type="button" onClick={onReset} disabled={!available} title="Reset preview" aria-label="Reset preview">RESET</button>
      <button className="emulator-system-control control-play" type="button" onClick={onTogglePlaying} disabled={!available}
        title={playing ? "Pause preview" : "Play preview"} aria-label={playing ? "Pause preview" : "Play preview"}>
        {playing ? "PAUSE" : "PLAY"}
      </button>
      <button className="emulator-system-control control-step" type="button" onClick={onAdvance} disabled={!available} title="Advance 250 ms" aria-label="Advance 250 ms">STEP</button>
    </div>
  );
  return (
    <section className={`emulator-panel ${collapsed ? "collapsed" : "expanded"} ${mode === "popout" ? "popout" : "docked"}`} aria-label={`Device emulator: ${sceneName}`}>
      <div className="emulator-display"><FramebufferCanvas framebuffer={preview?.framebuffer ?? null} /></div>
      <div className="emulator-controls" id={`${id}-controls`}>
        <button className="emulator-collapse" type="button" aria-expanded={!collapsed}
          aria-controls={`${id}-controls`}
          title={collapsed ? "Expand emulator controls" : "Collapse emulator controls"}
          aria-label={collapsed ? "Expand emulator controls" : "Collapse emulator controls"}
          onClick={() => setCollapsed((value) => !value)}>
          {collapsed ? <ChevronRight size={18} /> : <ChevronDown size={18} />}
        </button>
        {collapsed ? (
          <div className="emulator-compact-transport">
            {transport}
          </div>
        ) : (
          <>
            <div className="emulator-joystick" aria-label="Joystick cardinal inputs">
              {directions.map(({ source, label, Icon, position }) => (
                <button key={source} type="button" className={`emulator-direction direction-${position}`}
                  title={`Joystick ${label.toLowerCase()}`} aria-label={`Joystick ${label.toLowerCase()}`}
                  disabled={!available} onClick={() => onInput(source)}><Icon size={16} /></button>
              ))}
            </div>
            <div className="emulator-center">
              {transport}
              <button className="emulator-start" type="button" title="Start" aria-label="Start" disabled={!available} onClick={() => onInput("BUTTON_START")}>START</button>
            </div>
            <div className="emulator-buttons" aria-label="Device buttons">
              {["L", "R", "B", "A"].map((label) => (
                <button type="button" key={label} className={`emulator-button button-${label.toLowerCase()}`}
                  title={`Button ${label}`} aria-label={`Button ${label}`} disabled={!available}
                  onClick={() => onInput(`BUTTON_${label}`)}>{label}</button>
              ))}
            </div>
          </>
        )}
      </div>
    </section>
  );
}
