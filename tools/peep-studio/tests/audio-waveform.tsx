import { createRoot } from "react-dom/client";
import { AudioWaveform } from "../src/AudioWaveform";
import "../src/styles.css";

const originalDecode = OfflineAudioContext.prototype.decodeAudioData;
let decodes = 0;
OfflineAudioContext.prototype.decodeAudioData = function (...args: Parameters<typeof originalDecode>) {
  decodes++;
  document.body.dataset.decodes = String(decodes);
  return originalDecode.apply(this, args);
};
const projectPath = new URLSearchParams(location.search).get("project")!;
createRoot(document.getElementById("root")!).render(
  <div className="audio-cue-gallery" style={{maxWidth:520,padding:16}}>
    {["pulse", "ramp", "silence", "missing"].map(name => <div key={name}>
      <button className="audio-cue-select" data-audio={name}>
        <AudioWaveform projectPath={projectPath} sourcePath={`assets/${name}.wav`} />
        <span><strong>{name}</strong><small>Source waveform</small></span>
      </button>
    </div>)}
  </div>,
);
