"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.usesSceneObjects = usesSceneObjects;
exports.canEditLegacyScene = canEditLegacyScene;
exports.canPreviewSceneObjects = canPreviewSceneObjects;
exports.supportsObjectCommand = supportsObjectCommand;
exports.supportsNativeCreation = supportsNativeCreation;
exports.supportsStateManagement = supportsStateManagement;
exports.supportsLocalGraphCommand = supportsLocalGraphCommand;
exports.supportsSceneConnection = supportsSceneConnection;
exports.baseObjectRows = baseObjectRows;
function usesSceneObjects(scene, capability) {
    return scene?.schema_version === 2 || capability?.execution_model === "scene_objects";
}
function canEditLegacyScene(scene, capability) {
    return scene !== null && !usesSceneObjects(scene, capability)
        && (capability === undefined || (capability.host_editing && capability.legacy_command_catalog));
}
function canPreviewSceneObjects(service, capability) {
    return service?.scene_object_authoring?.execution_model === "scene_objects"
        && capability?.execution_model === "scene_objects" && capability.host_preview;
}
function supportsObjectCommand(service, capability, command) {
    return service?.operations.includes("project.apply_commands") === true
        && service.scene_object_authoring?.execution_model === "scene_objects"
        && service.scene_object_authoring.commands.includes(command)
        && capability?.execution_model === "scene_objects" && capability.host_editing
        && capability.supported_commands?.includes(command) === true;
}
function supportsNativeCreation(service) {
    return service?.scene_creation?.supported_versions.includes(2) === true
        && service.scene_creation.version_parameter === "scene_schema_version"
        && service.scene_creation.project_operation === "project.create"
        && service.scene_creation.scene_command === "scene.add";
}
function supportsStateManagement(service, capability, command) {
    return supportsObjectCommand(service, capability, command)
        && service?.scene_object_authoring?.state_management_commands?.includes(command) === true;
}
function supportsLocalGraphCommand(service, capability, command) {
    return supportsObjectCommand(service, capability, command)
        && service?.scene_object_authoring?.local_graph_commands?.includes(command) === true
        && capability?.local_graph_commands?.includes(command) === true;
}
function supportsSceneConnection(service, capability, command) {
    return supportsObjectCommand(service, capability, command)
        && service?.scene_object_authoring?.scene_connection_commands === true
        && capability?.scene_connection_commands === true
        && service.scene_object_authoring.connection_commands?.includes(command) === true
        && capability.connection_commands?.includes(command) === true
        && service.scene_object_authoring.scene_entry_modes?.includes("fresh_default") === true
        && capability.scene_entry_modes?.includes("fresh_default") === true
        && Array.isArray(service.scene_object_authoring.scene_exit_action_kinds)
        && Array.isArray(capability.scene_exit_action_kinds);
}
// Presentation adapter only: defaults are displayed verbatim; state resolution stays in the host.
function baseObjectRows(scene, ownership) {
    if (!usesSceneObjects(scene))
        return scene.render_models?.[0]?.elements ?? [];
    return (ownership?.objects ?? []).map(({ object_id, defaults, animation_ref: _clip, ...geometry }) => ({
        ...geometry, ...defaults, element_id: object_id,
    }));
}
