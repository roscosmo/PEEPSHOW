import assert from "node:assert/strict";
import { baseObjectRows, canEditLegacyScene, canPreviewSceneObjects, usesSceneObjects } from "../src/sceneCapabilities.js";
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
