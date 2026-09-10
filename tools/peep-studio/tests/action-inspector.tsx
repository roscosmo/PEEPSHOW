import { useState } from "react";
import { createRoot } from "react-dom/client";
import { EditableActionList } from "../src/SceneInspection";
import { ObjectActionContext } from "../src/ObjectActionContext";
import type { PreviewSnapshot, StateRoute } from "../src/types";
import "../src/styles.css";

const params = new URLSearchParams(location.search);
const width = Number(params.get("width") ?? 360);
const native = params.has("native");
const activeState = { state_id: "active", display_name: "Marker right", object_overrides: [{ object_ref: "wizard", x: 120 }] };
const targetState = { state_id: "start", display_name: "Destination", object_overrides: [{ object_ref: "wizard", y: 24 }] };
const preview: PreviewSnapshot = {
  project_revision: 1, preview_revision: 1,
  scene: { scene_id: params.has("otherScene") ? "other" : "game", state_id: "active", state_index: 1, display_name: "Game" },
  timeline: { elapsed_ms: 1000, ownership: "scene_objects" }, variables: {}, input: null,
  framebuffer: { width: 168, height: 144, row_stride_bytes: 21, encoding: "", size_bytes: 0, black_pixel_count: 0, sha256: "", data_base64: "" },
  objects: [{ object_id: "wizard", underlying: { x: 85, y: 40 }, effective: { x: 120, y: 40 } }],
};
function Fixture() {
  const [route, setRoute] = useState<StateRoute>({
    route_id: "move", action_ref: "right", from_states: ["start"], target_state: "start", guards: [],
    actions: [
      { kind: "request_render" },
      native ? { kind: "object.move_by", object_ref: "wizard", dx: 5, dy: params.has("horizontal") ? 0 : 1 } : { kind: "set_element_position", element_ref: "wizard", x: 48, y: 32 },
      { kind: "set_variable", variable_ref: "gold", operation: "add", value: 1 },
      { kind: "set_element_visibility", element_ref: "wizard", visible: true },
      { kind: "play_sfx", cue_ref: "select" },
    ],
  });
  document.body.dataset.actions = JSON.stringify(route.actions);
  return <aside style={{ width, maxWidth: "100%", padding: 12, boxSizing: "border-box" }}>
    <ObjectActionContext.Provider value={{
      scene: { scene_id: "game", display_name: "Game", scene_type: "state_scene", states: [activeState, targetState] },
      preview: params.has("noPreview") ? null : preview, label: () => "Wizard",
    }}>
    <EditableActionList sceneId="game" route={route} sceneObjects={native} targetState={native ? targetState : undefined}
      targetElements={[{ element_id: "wizard", kind: "sprite", x: 48, y: 32, width: 78, height: 96, z_order: 1 },
        ...(params.has("duplicates") ? [{ element_id: "wizard_2", kind: "sprite", x: 0, y: 0, width: 8, height: 8, z_order: 2 }] : [])]}
      variables={[{ variable_id: "gold", value_type: "int32", initial: 0, minimum: 0, maximum: 100 }]}
      waitingVisuals={[]} assets={[]} audioCues={[{ cue_id: "select", display_name: "Selection sound", asset_ref: "sound", priority: 1, volume: 128 }]}
      localActionsAllowed canAddActions canEdit={params.get("readonly") !== "true"}
      onSetRouteAction={async (_, __, index, action) => setRoute((r) => ({ ...r, actions: r.actions.map((a, i) => i === index ? action as typeof a : a) }))}
      onAddRouteAction={async (_, __, index, action) => setRoute((r) => {
        const actions = [...r.actions]; actions.splice(index, 0, action as typeof actions[number]); return { ...r, actions };
      })}
      onDeleteRouteAction={async (_, __, index) => setRoute((r) => ({ ...r, actions: r.actions.filter((_, i) => i !== index) }))}
      onMoveRouteAction={async (_, __, index, target) => setRoute((r) => {
        const actions = [...r.actions]; const [action] = actions.splice(index, 1); actions.splice(target, 0, action!); return { ...r, actions };
      })}
    />
    </ObjectActionContext.Provider>
  </aside>;
}
createRoot(document.getElementById("root")!).render(<Fixture />);
