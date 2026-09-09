import assert from "node:assert/strict";
import { baseObjectRows, canEditLegacyScene, canPreviewSceneObjects, supportsNativeCreation, supportsStateManagement, supportsLocalGraphCommand, supportsObjectCommand, usesSceneObjects } from "../src/sceneCapabilities.js";
import type { SceneCapabilities, SceneDocument, ServiceHello } from "../src/types.js";

const legacy: SceneDocument = { scene_id: "old", scene_type: "STATE_SCENE", display_name: "Old" };
const modern: SceneDocument = { ...legacy, scene_id: "new", schema_version: 2 };
const capability: SceneCapabilities = { schema_version: 2, execution_model: "scene_objects",
  host_editing: true, host_preview: true, egg_export: false, supported_commands: ["scene.rename"], legacy_command_catalog: false };
assert(canEditLegacyScene(legacy));
assert(!canEditLegacyScene(modern));
assert(!canEditLegacyScene(legacy, capability));
assert(usesSceneObjects(modern));
assert(!canPreviewSceneObjects(null, capability));
const service = { scene_object_authoring: { status: "host_available", execution_model: "scene_objects" } } as ServiceHello;
assert(canPreviewSceneObjects(service, capability));
assert(!canPreviewSceneObjects(service));
assert(!canPreviewSceneObjects(service, { ...capability, host_preview: false }));
const object = { object_id: "box", kind: "outline_rect", width: 8, height: 9, z_order: 1,
  defaults: { x: 12, y: 13, visible: false } };
const projection = { objects: [object], states: {}, state_scoped_element_ids: [] };
assert.deepEqual(baseObjectRows(modern, projection), [{ element_id: "box", kind: "outline_rect",
  width: 8, height: 9, z_order: 1, x: 12, y: 13, visible: false }]);
assert.deepEqual(object.defaults, { x: 12, y: 13, visible: false });
assert.deepEqual(baseObjectRows(modern), []);
console.log("Scene capability and projection tests passed");

const editingHost = { ...service, operations: ["project.apply_commands"], scene_object_authoring: {
  ...service.scene_object_authoring!, commands: ["object.set_defaults"],
} };
const editingScene = { ...capability, supported_commands: ["object.set_defaults"] };
assert(supportsObjectCommand(editingHost, editingScene, "object.set_defaults"));
assert(!supportsObjectCommand(editingHost, capability, "object.set_defaults"));
assert(!supportsObjectCommand(editingHost, { ...editingScene, host_editing: false }, "object.set_defaults"));
assert(!supportsObjectCommand({ ...editingHost, scene_object_authoring: { ...editingHost.scene_object_authoring, commands: [] } }, editingScene, "object.set_defaults"));
assert(!supportsObjectCommand(editingHost, undefined, "object.set_defaults"));

const creationHost = { ...editingHost, scene_creation: {
  project_operation: "project.create", scene_command: "scene.add", entry_scene_command: "project.set_entry_scene",
  version_parameter: "scene_schema_version", supported_versions: [1, 2], default_version: 1,
}, scene_object_authoring: { ...editingHost.scene_object_authoring,
  commands: ["state.create"], state_management_commands: ["state.create"],
} };
assert(supportsNativeCreation(creationHost));
assert(!supportsNativeCreation(editingHost));
assert(!supportsNativeCreation({ ...creationHost, scene_creation: { ...creationHost.scene_creation, supported_versions: [1] } }));
const stateScene = { ...capability, supported_commands: ["state.create"] };
assert(supportsStateManagement(creationHost, stateScene, "state.create"));
assert(!supportsStateManagement(creationHost, capability, "state.create"));
assert(!supportsStateManagement({ ...creationHost, scene_object_authoring: {
  ...creationHost.scene_object_authoring, state_management_commands: [],
} }, stateScene, "state.create"));
assert(!supportsStateManagement(creationHost, stateScene, "route.add"));

const graphHost = { ...creationHost, scene_object_authoring: { ...creationHost.scene_object_authoring,
  commands: ["route.create_trigger"], local_graph_commands: ["route.create_trigger"],
  graph_construction_commands: true, scene_connection_commands: false,
} };
const graphScene = { ...stateScene, supported_commands: ["route.create_trigger"], local_graph_commands: ["route.create_trigger"] };
assert(supportsLocalGraphCommand(graphHost, graphScene, "route.create_trigger"));
assert(!supportsLocalGraphCommand(graphHost, { ...graphScene, supported_commands: [] }, "route.create_trigger"));
assert(!supportsLocalGraphCommand(graphHost, { ...graphScene, local_graph_commands: [] }, "route.create_trigger"));
assert(!supportsLocalGraphCommand({ ...graphHost, scene_object_authoring: { ...graphHost.scene_object_authoring,
  local_graph_commands: [],
} }, graphScene, "route.create_trigger"));
assert(!supportsLocalGraphCommand(graphHost, graphScene, "scene_exit.add"));
