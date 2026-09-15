import { useEffect, useState } from "react";
import { EmulatorPanel } from "./EmulatorPanel";
import type { PreviewSnapshot } from "./types";

export type EmulatorPopoutState = {
  preview: PreviewSnapshot | null;
  sceneName: string;
  playing: boolean;
};

const emptyPopoutState: EmulatorPopoutState = {
  preview: null,
  sceneName: "No active scene",
  playing: false,
};

function readPopoutState(value: unknown): EmulatorPopoutState {
  if (value === null || typeof value !== "object") {
    return emptyPopoutState;
  }
  const record = value as Partial<EmulatorPopoutState>;
  return {
    preview: record.preview ?? null,
    sceneName: typeof record.sceneName === "string" ? record.sceneName : emptyPopoutState.sceneName,
    playing: record.playing === true,
  };
}

export function isEmulatorPopoutRoute(): boolean {
  return new URLSearchParams(window.location.search).get("view") === "emulator-popout";
}

export function EmulatorPopoutApp() {
  const bridge = window.peepStudio;
  const [state, setState] = useState<EmulatorPopoutState>(emptyPopoutState);

  useEffect(() => {
    return bridge?.onEmulatorPopoutState?.((value) => setState(readPopoutState(value)));
  }, [bridge]);

  const sendCommand = (command: unknown) => {
    void bridge?.sendEmulatorPopoutCommand?.(command);
  };

  return (
    <main className="emulator-popout-shell">
      <EmulatorPanel
        mode="popout"
        preview={state.preview}
        sceneName={state.sceneName}
        playing={state.playing}
        onReset={() => sendCommand({ kind: "reset" })}
        onTogglePlaying={() => sendCommand({ kind: "togglePlaying" })}
        onAdvance={() => sendCommand({ kind: "advance", elapsedMs: 250 })}
        onInput={(source) => sendCommand({ kind: "input", source })}
        onCollapsedChange={(collapsed) => void bridge?.setEmulatorPopoutCollapsed?.(collapsed)}
      />
    </main>
  );
}
