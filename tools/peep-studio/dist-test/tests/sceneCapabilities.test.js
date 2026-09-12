"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
const strict_1 = __importDefault(require("node:assert/strict"));
const sceneCapabilities_js_1 = require("../src/sceneCapabilities.js");
const legacy = { scene_id: "old", scene_type: "STATE_SCENE", display_name: "Old" };
const modern = { ...legacy, scene_id: "new", schema_version: 2 };
const capability = { schema_version: 2, execution_model: "scene_objects",
    host_editing: true, host_preview: true, egg_export: false, supported_commands: ["scene.rename"], legacy_command_catalog: false };
(0, strict_1.default)((0, sceneCapabilities_js_1.canEditLegacyScene)(legacy));
(0, strict_1.default)(!(0, sceneCapabilities_js_1.canEditLegacyScene)(modern));
(0, strict_1.default)(!(0, sceneCapabilities_js_1.canEditLegacyScene)(legacy, capability));
(0, strict_1.default)((0, sceneCapabilities_js_1.usesSceneObjects)(modern));
(0, strict_1.default)(!(0, sceneCapabilities_js_1.canPreviewSceneObjects)(null, capability));
const service = { scene_object_authoring: { status: "host_available", execution_model: "scene_objects" } };
(0, strict_1.default)((0, sceneCapabilities_js_1.canPreviewSceneObjects)(service, capability));
(0, strict_1.default)(!(0, sceneCapabilities_js_1.canPreviewSceneObjects)(service));
(0, strict_1.default)(!(0, sceneCapabilities_js_1.canPreviewSceneObjects)(service, { ...capability, host_preview: false }));
const object = { object_id: "box", kind: "outline_rect", width: 8, height: 9, z_order: 1,
    defaults: { x: 12, y: 13, visible: false } };
const projection = { objects: [object], states: {}, state_scoped_element_ids: [] };
strict_1.default.deepEqual((0, sceneCapabilities_js_1.baseObjectRows)(modern, projection), [{ element_id: "box", kind: "outline_rect",
        width: 8, height: 9, z_order: 1, x: 12, y: 13, visible: false }]);
strict_1.default.deepEqual(object.defaults, { x: 12, y: 13, visible: false });
strict_1.default.deepEqual((0, sceneCapabilities_js_1.baseObjectRows)(modern), []);
console.log("Scene capability and projection tests passed");
const editingHost = { ...service, operations: ["project.apply_commands"], scene_object_authoring: {
        ...service.scene_object_authoring, commands: ["object.set_defaults"],
    } };
const editingScene = { ...capability, supported_commands: ["object.set_defaults"] };
(0, strict_1.default)((0, sceneCapabilities_js_1.supportsObjectCommand)(editingHost, editingScene, "object.set_defaults"));
(0, strict_1.default)(!(0, sceneCapabilities_js_1.supportsObjectCommand)(editingHost, capability, "object.set_defaults"));
(0, strict_1.default)(!(0, sceneCapabilities_js_1.supportsObjectCommand)(editingHost, { ...editingScene, host_editing: false }, "object.set_defaults"));
(0, strict_1.default)(!(0, sceneCapabilities_js_1.supportsObjectCommand)({ ...editingHost, scene_object_authoring: { ...editingHost.scene_object_authoring, commands: [] } }, editingScene, "object.set_defaults"));
(0, strict_1.default)(!(0, sceneCapabilities_js_1.supportsObjectCommand)(editingHost, undefined, "object.set_defaults"));
const restrictedHost = { ...editingHost, scene_object_authoring: { ...editingHost.scene_object_authoring,
        status: "restricted_firmware_available", egg_export: true } };
(0, strict_1.default)((0, sceneCapabilities_js_1.canPreviewSceneObjects)(restrictedHost, capability));
(0, strict_1.default)((0, sceneCapabilities_js_1.supportsObjectCommand)(restrictedHost, editingScene, "object.set_defaults"));
(0, strict_1.default)(!(0, sceneCapabilities_js_1.canPreviewSceneObjects)(restrictedHost, { ...capability, host_preview: false }));
(0, strict_1.default)(!(0, sceneCapabilities_js_1.supportsObjectCommand)(restrictedHost, { ...editingScene, host_editing: false }, "object.set_defaults"));
(0, strict_1.default)(!(0, sceneCapabilities_js_1.supportsObjectCommand)(restrictedHost, { ...editingScene, supported_commands: [] }, "object.set_defaults"));
(0, strict_1.default)(!(0, sceneCapabilities_js_1.supportsObjectCommand)({ ...restrictedHost, operations: [] }, editingScene, "object.set_defaults"));
const creationHost = { ...editingHost, scene_creation: {
        project_operation: "project.create", scene_command: "scene.add", entry_scene_command: "project.set_entry_scene",
        version_parameter: "scene_schema_version", supported_versions: [1, 2], default_version: 1,
    }, scene_object_authoring: { ...editingHost.scene_object_authoring,
        commands: ["state.create"], state_management_commands: ["state.create"],
    } };
(0, strict_1.default)((0, sceneCapabilities_js_1.supportsNativeCreation)(creationHost));
(0, strict_1.default)(!(0, sceneCapabilities_js_1.supportsNativeCreation)(editingHost));
(0, strict_1.default)(!(0, sceneCapabilities_js_1.supportsNativeCreation)({ ...creationHost, scene_creation: { ...creationHost.scene_creation, supported_versions: [1] } }));
const stateScene = { ...capability, supported_commands: ["state.create"] };
(0, strict_1.default)((0, sceneCapabilities_js_1.supportsStateManagement)(creationHost, stateScene, "state.create"));
(0, strict_1.default)(!(0, sceneCapabilities_js_1.supportsStateManagement)(creationHost, capability, "state.create"));
(0, strict_1.default)(!(0, sceneCapabilities_js_1.supportsStateManagement)({ ...creationHost, scene_object_authoring: {
        ...creationHost.scene_object_authoring, state_management_commands: [],
    } }, stateScene, "state.create"));
(0, strict_1.default)(!(0, sceneCapabilities_js_1.supportsStateManagement)(creationHost, stateScene, "route.add"));
const graphHost = { ...creationHost, scene_object_authoring: { ...creationHost.scene_object_authoring,
        commands: ["route.create_trigger"], local_graph_commands: ["route.create_trigger"],
        graph_construction_commands: true, scene_connection_commands: false,
    } };
const graphScene = { ...stateScene, supported_commands: ["route.create_trigger"], local_graph_commands: ["route.create_trigger"] };
(0, strict_1.default)((0, sceneCapabilities_js_1.supportsLocalGraphCommand)(graphHost, graphScene, "route.create_trigger"));
(0, strict_1.default)(!(0, sceneCapabilities_js_1.supportsLocalGraphCommand)(graphHost, { ...graphScene, supported_commands: [] }, "route.create_trigger"));
(0, strict_1.default)(!(0, sceneCapabilities_js_1.supportsLocalGraphCommand)(graphHost, { ...graphScene, local_graph_commands: [] }, "route.create_trigger"));
(0, strict_1.default)(!(0, sceneCapabilities_js_1.supportsLocalGraphCommand)({ ...graphHost, scene_object_authoring: { ...graphHost.scene_object_authoring,
        local_graph_commands: [],
    } }, graphScene, "route.create_trigger"));
(0, strict_1.default)(!(0, sceneCapabilities_js_1.supportsLocalGraphCommand)(graphHost, graphScene, "scene_exit.add"));
const connectionFields = { scene_connection_commands: true, connection_commands: ["scene_exit.add"], scene_entry_modes: ["fresh_default"], scene_exit_action_kinds: [] };
const connectionHost = { ...graphHost, scene_object_authoring: { ...graphHost.scene_object_authoring, ...connectionFields, commands: ["scene_exit.add"] } };
const connectionScene = { ...graphScene, ...connectionFields, supported_commands: ["scene_exit.add"] };
(0, strict_1.default)((0, sceneCapabilities_js_1.supportsSceneConnection)(connectionHost, connectionScene, "scene_exit.add"));
for (const removed of [{ scene_connection_commands: false }, { connection_commands: [] }, { scene_entry_modes: [] }, { scene_exit_action_kinds: undefined }]) {
    (0, strict_1.default)(!(0, sceneCapabilities_js_1.supportsSceneConnection)({ ...connectionHost, scene_object_authoring: { ...connectionHost.scene_object_authoring, ...removed } }, connectionScene, "scene_exit.add"));
    (0, strict_1.default)(!(0, sceneCapabilities_js_1.supportsSceneConnection)(connectionHost, { ...connectionScene, ...removed }, "scene_exit.add"));
}
(0, strict_1.default)(!(0, sceneCapabilities_js_1.supportsSceneConnection)(connectionHost, { ...connectionScene, supported_commands: [] }, "scene_exit.add"));
(0, strict_1.default)(!(0, sceneCapabilities_js_1.supportsSceneConnection)(connectionHost, connectionScene, "scene_exit.delete"));
