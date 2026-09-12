import type { PlacementOwnership, RenderElement, SceneCapabilities, SceneDocument, ServiceHello } from "./types.js";

export function usesSceneObjects(scene: SceneDocument | null, capability?: SceneCapabilities): boolean {
  return scene?.schema_version === 2 || capability?.execution_model === "scene_objects";
}

export function canEditLegacyScene(scene: SceneDocument | null, capability?: SceneCapabilities): boolean {
  return scene !== null && !usesSceneObjects(scene, capability)
    && (capability === undefined || (capability.host_editing && capability.legacy_command_catalog));
}

export function canPreviewSceneObjects(service: ServiceHello | null, capability?: SceneCapabilities): boolean {
  return service?.scene_object_authoring?.execution_model === "scene_objects"
    && capability?.execution_model === "scene_objects" && capability.host_preview;
}

export function supportsObjectCommand(service: ServiceHello | null, capability: SceneCapabilities | undefined, command: string): boolean {
  return service?.operations.includes("project.apply_commands") === true
    && service.scene_object_authoring?.execution_model === "scene_objects"
    && service.scene_object_authoring.commands.includes(command)
    && capability?.execution_model === "scene_objects" && capability.host_editing
    && capability.supported_commands?.includes(command) === true;
}

export function supportsNativeCreation(service: ServiceHello | null): boolean {
  return service?.scene_creation?.supported_versions.includes(2) === true
    && service.scene_creation.version_parameter === "scene_schema_version"
    && service.scene_creation.project_operation === "project.create"
    && service.scene_creation.scene_command === "scene.add";
}

export function supportsStateManagement(service: ServiceHello | null, capability: SceneCapabilities | undefined, command: string): boolean {
  return supportsObjectCommand(service, capability, command)
    && service?.scene_object_authoring?.state_management_commands?.includes(command) === true;
}

export function supportsLocalGraphCommand(service: ServiceHello | null, capability: SceneCapabilities | undefined, command: string): boolean {
  return supportsObjectCommand(service, capability, command)
    && service?.scene_object_authoring?.local_graph_commands?.includes(command) === true
    && capability?.local_graph_commands?.includes(command) === true;
}

export function supportsSceneConnection(service: ServiceHello | null, capability: SceneCapabilities | undefined, command: string): boolean {
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
export function baseObjectRows(scene: SceneDocument, ownership?: PlacementOwnership["scenes"][string] | null): RenderElement[] {
  if (!usesSceneObjects(scene)) return scene.render_models?.[0]?.elements ?? [];
  return (ownership?.objects ?? []).map(({ object_id, defaults, animation_ref: _clip, ...geometry }) => ({
    ...geometry, ...defaults, element_id: object_id,
  }));
}
