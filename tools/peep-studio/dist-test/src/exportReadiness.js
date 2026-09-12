"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.canBuildProject = canBuildProject;
function canBuildProject(service, project) {
    if (!project?.valid || !service?.operations.includes("project.build_package"))
        return false;
    const scenes = project.document?.scenes ?? [];
    if (!scenes.some(scene => scene.schema_version === 2))
        return true;
    const advertised = service.package_export;
    return service.scene_object_authoring?.egg_export === true
        && service.scene_object_authoring.export_requires_project_readiness === true
        && advertised?.operation === "project.build_package"
        && advertised.container_versions.includes(2)
        && !!advertised.v2_profile?.profile_id
        && !project.build_issues?.length
        && scenes.every(scene => {
            const capability = project.scene_capabilities?.[scene.scene_id];
            return capability?.egg_export === true && capability.export_ready === true
                && capability.export_readiness_scope === "whole_project"
                && capability.export_profile_id === advertised.v2_profile?.profile_id;
        });
}
