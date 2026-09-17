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
    const multiScene = scenes.length > 1;
    const requiresAudio = (project.document?.audio_assets?.length ?? 0) > 0
        || (project.document?.audio_cues?.length ?? 0) > 0;
    const packageAudio = advertised?.v2_profile?.audio_profile;
    const authoringAudio = service.scene_object_authoring?.audio_export;
    const audioExportReady = !requiresAudio || (advertised?.v2_profile?.audio === true
        && packageAudio?.supported === true
        && packageAudio.capability === "audio.sampled_sfx"
        && authoringAudio?.supported === true
        && authoringAudio.capability === packageAudio.capability);
    return service.scene_object_authoring?.egg_export === true
        && service.scene_object_authoring.export_requires_project_readiness === true
        && (!multiScene || service.scene_object_authoring.multi_scene_export === true)
        && advertised?.operation === "project.build_package"
        && advertised.container_versions.includes(2)
        && !!advertised.v2_profile?.profile_id
        && audioExportReady
        && !project.build_issues?.length
        && scenes.every(scene => {
            const capability = project.scene_capabilities?.[scene.scene_id];
            return capability?.egg_export === true && capability.export_ready === true
                && (!multiScene || capability.multi_scene_export === true)
                && capability.export_readiness_scope === "whole_project"
                && capability.export_profile_id === advertised.v2_profile?.profile_id
                && (!requiresAudio || (capability.audio_export?.supported === true
                    && capability.audio_export.capability === packageAudio?.capability));
        });
}
