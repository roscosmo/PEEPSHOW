import { useEffect, useState } from "react";
import { createRoot } from "react-dom/client";
import { ReactFlowProvider, useReactFlow } from "@xyflow/react";
import "@xyflow/react/dist/style.css";
import "../src/styles.css";
import { SceneFlowView, StateGraphView, type SceneSelection } from "../src/SceneInspection";
import type { ProjectEditorData, SceneDocument } from "../src/types";

const noop = () => {};
const scene: SceneDocument = {
  scene_id: "menu", display_name: "Menu", scene_type: "STATE_SCENE", entry_state: "destination",
  states: ["first", "second", "destination"].map((state_id) => ({ state_id, display_name: state_id, waiting_visual_ref: "empty" })),
  input_actions: [{ action_id: "a", logical_source: "BUTTON_A" }],
  routes: ["first", "second"].map((source) => ({
    route_id: `from-${source}`, action_ref: "a", from_states: [source], target_state: "destination", guards: [], actions: [],
  })),
};
const sceneFlow: SceneDocument[] = [scene, ...["first", "second"].map((scene_id) => ({
  ...scene, scene_id, display_name: scene_id,
  scene_exits: [
    { scene_exit_id: `${scene_id}-menu`, display_name: "Menu", target_scene: "menu" },
    { scene_exit_id: `${scene_id}-goto`, display_name: "Go To", target_scene: "menu" },
  ],
}))];
const initialEditor: ProjectEditorData = {
  state_graph: { scenes: { menu: {
    entry: { target_handle: "entry-top-left", target_side: "top" },
    nodes: { first: { x: 0, y: 0 }, second: { x: 450, y: 0 }, destination: { x: 360, y: 460 }, "scene-entry": { x: -150, y: 360 } },
    routes: Object.fromEntries(["first", "second"].map((source) => [`from-${source}`, { sources: { [source]: {
      routing_version: 3, target_handle: "entry-top-left", target_side: "top", rails: [],
    } } }])),
  } } },
  scene_flow: {
    nodes: { first: { x: 0, y: 0 }, second: { x: 0, y: 480 }, menu: { x: 630, y: 80 } },
    package_entry: { x: 400, y: -160 },
    references: { "go-menu": { target_scene: "menu", x: 640, y: 480 } },
    exit_references: { first: { "scene_exit:first-goto": "go-menu" }, second: { "scene_exit:second-goto": "go-menu" } },
  },
};

function FixtureControls() {
  const { setNodes } = useReactFlow();
  useEffect(() => {
    const moveNode = (event: Event) => {
      const { id } = (event as CustomEvent<{ id: string }>).detail;
      setNodes((nodes) => nodes.map((node) => node.id !== id ? node : {
        ...node, position: { x: node.position.x + 30, y: node.position.y + 25 },
      }));
    };
    window.addEventListener("fixture-move-node", moveNode);
    return () => window.removeEventListener("fixture-move-node", moveNode);
  }, [setNodes]);
  return null;
}

function Fixture() {
  const [editor, setEditor] = useState(initialEditor);
  const [selected, setSelected] = useState<SceneSelection>({ kind: "scene" });
  const [sceneExit, setSceneExit] = useState<string | null>(null);
  const [packageSelected, setPackageSelected] = useState(false);
  const [entryState, setEntryState] = useState("destination");
  useEffect(() => { document.body.dataset.editor = JSON.stringify(editor); }, [editor]);
  const record = (value: unknown) => { document.body.dataset.result = JSON.stringify(value); };
  if (new URLSearchParams(location.search).get("graph") === "scene") {
    return <SceneFlowView scenes={sceneFlow} entrySceneId="menu" thumbnails={{}} editor={editor} layoutStatus="fixture"
      selectedSceneId={null} selectedSceneExitId={sceneExit} selectedRouteId={null} selectedReferenceId={null} packageEntrySelected={packageSelected}
      canEdit canAddScene={false} onAddScene={noop} onSelectScene={noop} onSelectSceneRoute={noop}
      onSelectSceneExit={(_, id) => { setSceneExit(id); setPackageSelected(false); record({ selected: id }); }}
      onSelectPackageEntry={() => { setPackageSelected(true); setSceneExit(null); record({ selected: "package-entry" }); }}
      onSelectSceneReference={noop} onAddSceneExit={noop} onAddSceneReference={noop} onDeleteSceneExit={noop}
      onDeleteLegacyRoute={noop} onDeleteSceneReference={noop} onMovePackageEntry={noop} onMoveSceneReference={noop}
      onMoveSceneNode={(id, x, y) => setEditor((value) => ({ ...value, scene_flow: { ...value.scene_flow, nodes: { ...value.scene_flow?.nodes, [id]: { x, y } } } }))}
      onSetEntryScene={noop} onSetSceneExitTarget={noop} onConnectSceneExit={noop}
      onSetRouteLayout={(source, kind, id, rails) => {
        record({ rails });
        setEditor((value) => ({ ...value, scene_flow: { ...value.scene_flow,
          routes: { ...value.scene_flow?.routes, [source]: { ...value.scene_flow?.routes?.[source], [`${kind}:${id}`]: { routing_version: 1, rails } } },
        } }));
      }} />;
  }
  return <StateGraphView scene={{ ...scene, entry_state: entryState }} activeStateId={null} editor={editor} layoutStatus="fixture" selected={selected}
    physicalEventKinds={["press"]} peepOSTriggers={[]} canCreateState={false} canEdit
    onSelect={(value) => { setSelected(value); record(value); }} onCreateState={noop} onDeleteState={noop}
    onMoveStateNode={(_, id, x, y) => setEditor((value) => ({ ...value, state_graph: { scenes: { menu: {
      ...value.state_graph!.scenes!.menu, nodes: { ...value.state_graph!.scenes!.menu.nodes, [id]: { x, y } },
    } } } }))}
    onSetEntryConnection={(_, state, handle, side) => {
      record({ entry: state, handle, side });
      setEntryState(state);
      setEditor((value) => ({ ...value, state_graph: { scenes: { menu: {
        ...value.state_graph!.scenes!.menu, entry: { target_handle: handle, target_side: side },
      } } } }));
    }}
    onSetRouteLayout={(_, route, source, rails, handle, side, token_positions) => {
      record({ route, source, rails, handle, side });
      setEditor((value) => ({ ...value, state_graph: { scenes: { menu: {
        ...value.state_graph!.scenes!.menu,
        routes: { ...value.state_graph!.scenes!.menu.routes, [route]: { sources: { [source]: {
          routing_version: 3, rails, target_handle: handle ?? undefined, target_side: side ?? undefined, token_positions,
        } } } },
      } } } }));
    }}
    onCreateTriggerRoute={noop} onRebindTriggerRoute={noop} onConnectRouteToSceneExit={noop} onDeleteSystemExit={noop} />;
}
createRoot(document.getElementById("root")!).render(<ReactFlowProvider><FixtureControls /><Fixture /></ReactFlowProvider>);
