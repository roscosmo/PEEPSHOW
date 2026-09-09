import { useEffect, useState } from "react";
import { createRoot } from "react-dom/client";
import "../src/styles.css";
import { EmulatorPanel } from "../src/EmulatorPanel";
import type { PreviewSnapshot } from "../src/types";

function Fixture() {
  const [playing, setPlaying] = useState(false);
  const [ticks, setTicks] = useState(0);
  const [inputs, setInputs] = useState<string[]>([]);
  const [resets, setResets] = useState(0);
  useEffect(() => {
    if (!playing) return;
    const timer = setInterval(() => setTicks((value) => value + 1), 100);
    return () => clearInterval(timer);
  }, [playing]);
  useEffect(() => { document.body.dataset.engine = JSON.stringify({ ticks, playing, inputs, resets }); }, [ticks, playing, inputs, resets]);
  const bytes = new Uint8Array(21 * 144);
  for (let y = 0; y < 144; y++) bytes[y * 21 + ticks % 21] = 255;
  const preview: PreviewSnapshot = {
    project_revision: 1, preview_revision: ticks, scene: { scene_id: "menu", state_id: `selection-${ticks % 3}`, state_index: ticks % 3, display_name: `Menu selection ${ticks % 3 + 1}` },
    timeline: { elapsed_ms: ticks * 100, presentation_id: 1, step_index: ticks % 3, step_elapsed_ms: 0, phase_quantum_ms: 100, step_count: 3 },
    variables: {}, input: null,
    framebuffer: { width: 168, height: 144, row_stride_bytes: 21, encoding: "mono1_msb", size_bytes: bytes.length,
      black_pixel_count: 144 * 8, sha256: "fixture", data_base64: btoa(String.fromCharCode(...bytes)) },
  };
  const width = Number(new URLSearchParams(location.search).get("width") ?? 320);
  return <div className="project-pane" style={{ width, maxWidth: "100vw" }}>
    <EmulatorPanel preview={preview} sceneName="Main menu" playing={playing}
      onReset={() => { setTicks(0); setResets((value) => value + 1); }}
      onTogglePlaying={() => setPlaying((value) => !value)} onAdvance={() => setTicks((value) => value + 1)}
      onInput={(source) => setInputs((values) => [...values, source])} />
    <div id="hierarchy" style={{ height: 100, borderTop: "1px solid #ddd" }} />
  </div>;
}
createRoot(document.getElementById("root")!).render(<Fixture />);
