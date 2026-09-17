"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
const strict_1 = __importDefault(require("node:assert/strict"));
const exportReadiness_js_1 = require("../src/exportReadiness.js");
const service = {
    operations: ["project.build_package"],
    package_export: { operation: "project.build_package", container_versions: [1, 2], v2_profile: { profile_id: "restricted" } },
    scene_object_authoring: { egg_export: true, export_requires_project_readiness: true },
};
const project = {
    valid: true, document: { scenes: [{ scene_id: "main", schema_version: 2 }] }, build_issues: [],
    scene_capabilities: { main: { egg_export: true, export_ready: true, export_readiness_scope: "whole_project", export_profile_id: "restricted" } },
};
(0, strict_1.default)((0, exportReadiness_js_1.canBuildProject)(service, project));
(0, strict_1.default)(!(0, exportReadiness_js_1.canBuildProject)(null, project));
(0, strict_1.default)(!(0, exportReadiness_js_1.canBuildProject)(service, null));
(0, strict_1.default)(!(0, exportReadiness_js_1.canBuildProject)({ ...service, package_export: undefined }, project));
(0, strict_1.default)(!(0, exportReadiness_js_1.canBuildProject)({ ...service, operations: [] }, project));
(0, strict_1.default)(!(0, exportReadiness_js_1.canBuildProject)(service, { ...project, valid: false }));
for (const patch of [{ egg_export: false }, { export_ready: false }, { export_ready: undefined },
    { export_readiness_scope: "scene" }, { export_profile_id: "other" }]) {
    (0, strict_1.default)(!(0, exportReadiness_js_1.canBuildProject)(service, { ...project, scene_capabilities: { main: { ...project.scene_capabilities.main, ...patch } } }));
}
(0, strict_1.default)(!(0, exportReadiness_js_1.canBuildProject)(service, { ...project, build_issues: [{ code: "V2_TEST", message: "Blocked", path: "project" }] }));
(0, strict_1.default)(!(0, exportReadiness_js_1.canBuildProject)(service, { ...project, scene_capabilities: {} }));
const multiSceneService = {
    ...service,
    scene_object_authoring: { ...service.scene_object_authoring, multi_scene_export: true },
};
const multiSceneProject = {
    ...project,
    document: { ...project.document, scenes: [...project.document.scenes, { scene_id: "garden", schema_version: 2 }] },
    scene_capabilities: {
        main: { ...project.scene_capabilities.main, multi_scene_export: true },
        garden: { ...project.scene_capabilities.main, multi_scene_export: true },
    },
};
(0, strict_1.default)((0, exportReadiness_js_1.canBuildProject)(multiSceneService, multiSceneProject));
(0, strict_1.default)(!(0, exportReadiness_js_1.canBuildProject)(service, multiSceneProject));
(0, strict_1.default)(!(0, exportReadiness_js_1.canBuildProject)(multiSceneService, {
    ...multiSceneProject,
    scene_capabilities: { ...multiSceneProject.scene_capabilities, garden: { ...multiSceneProject.scene_capabilities.garden, multi_scene_export: false } },
}));
const audioProfile = {
    supported: true, capability: "audio.sampled_sfx", action_kinds: ["play_sfx"],
    action_contexts: ["local_transition", "local_timer_handler"], compiled_format: "ima_adpcm",
    sample_rate_hz: 16000, channels: 1, block_samples: 256, maximum_assets: 32, maximum_cues: 64,
    voice_limit: 5, cue_volume: true, cue_priority: true, residency: "whole_package",
    shared_package_limit_bytes: 65536, survives_local_state_change: true,
    survives_same_package_scene_replacement: true, package_suspend: "stop_and_discard",
    package_resume: "new_requests_only", package_exit_or_replacement: "stop_and_discard",
    unsupported: [],
};
const audioService = {
    ...multiSceneService,
    package_export: { ...multiSceneService.package_export, v2_profile: {
            ...multiSceneService.package_export.v2_profile, audio: true, audio_profile: audioProfile,
        } },
    scene_object_authoring: { ...multiSceneService.scene_object_authoring, audio_export: audioProfile },
};
const audioProject = {
    ...multiSceneProject,
    document: { ...multiSceneProject.document, audio_assets: [{ asset_id: "short" }], audio_cues: [{ cue_id: "short.cue" }] },
    scene_capabilities: Object.fromEntries(Object.entries(multiSceneProject.scene_capabilities).map(([id, capability]) => [id, {
            ...capability, audio_export: audioProfile,
        }])),
};
(0, strict_1.default)((0, exportReadiness_js_1.canBuildProject)(audioService, audioProject));
(0, strict_1.default)(!(0, exportReadiness_js_1.canBuildProject)(multiSceneService, audioProject));
(0, strict_1.default)(!(0, exportReadiness_js_1.canBuildProject)({ ...audioService, scene_object_authoring: { ...audioService.scene_object_authoring, audio_export: undefined } }, audioProject));
(0, strict_1.default)(!(0, exportReadiness_js_1.canBuildProject)(audioService, {
    ...audioProject,
    scene_capabilities: { ...audioProject.scene_capabilities, garden: { ...audioProject.scene_capabilities.garden, audio_export: null } },
}));
const legacy = { ...project, document: { ...project.document, scenes: [{ ...project.document.scenes[0], schema_version: 1 }] } };
(0, strict_1.default)((0, exportReadiness_js_1.canBuildProject)({ ...service, package_export: undefined }, legacy));
console.log("Export capability and whole-project readiness checks passed");
