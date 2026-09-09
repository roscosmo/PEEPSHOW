import { useState } from "react";
import { createRoot } from "react-dom/client";
import { EditableActionList } from "../src/SceneInspection";
import type { StateRoute } from "../src/types";
import "../src/styles.css";

const params = new URLSearchParams(location.search);
const width = Number(params.get("width") ?? 360);
function Fixture() {
  const [route, setRoute] = useState<StateRoute>({
    route_id: "move", action_ref: "right", from_states: ["start"], target_state: "start", guards: [],
    actions: [
      { kind: "request_render" },
      { kind: "set_element_position", element_ref: "wizard", x: 48, y: 32 },
      { kind: "set_variable", variable_ref: "gold", operation: "add", value: 1 },
      { kind: "set_element_visibility", element_ref: "wizard", visible: true },
      { kind: "play_sfx", cue_ref: "select" },
    ],
  });
  document.body.dataset.actions = JSON.stringify(route.actions);
  return <aside style={{ width, maxWidth: "100%", padding: 12, boxSizing: "border-box" }}>
    <EditableActionList sceneId="game" route={route}
      targetElements={[{ element_id: "wizard", kind: "sprite", x: 48, y: 32, width: 78, height: 96, z_order: 1 }]}
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
  </aside>;
}
createRoot(document.getElementById("root")!).render(<Fixture />);
