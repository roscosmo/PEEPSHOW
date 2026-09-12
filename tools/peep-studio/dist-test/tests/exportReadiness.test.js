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
const legacy = { ...project, document: { ...project.document, scenes: [{ ...project.document.scenes[0], schema_version: 1 }] } };
(0, strict_1.default)((0, exportReadiness_js_1.canBuildProject)({ ...service, package_export: undefined }, legacy));
console.log("Export capability and whole-project readiness checks passed");
