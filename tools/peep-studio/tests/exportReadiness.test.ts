import assert from "node:assert/strict";
import { canBuildProject } from "../src/exportReadiness.js";
import type { ProjectLoadResult, ServiceHello } from "../src/types.js";

const service = {
  operations: ["project.build_package"],
  package_export: {operation:"project.build_package",container_versions:[1,2],v2_profile:{profile_id:"restricted"}},
  scene_object_authoring: {egg_export:true,export_requires_project_readiness:true},
} as ServiceHello;
const project = {
  valid:true, document:{scenes:[{scene_id:"main",schema_version:2}]}, build_issues:[],
  scene_capabilities:{main:{egg_export:true,export_ready:true,export_readiness_scope:"whole_project",export_profile_id:"restricted"}},
} as unknown as ProjectLoadResult;
assert(canBuildProject(service,project));
assert(!canBuildProject(null,project));
assert(!canBuildProject(service,null));
assert(!canBuildProject({...service,package_export:undefined},project));
assert(!canBuildProject({...service,operations:[]},project));
assert(!canBuildProject(service,{...project,valid:false}));
for (const patch of [{egg_export:false},{export_ready:false},{export_ready:undefined},
  {export_readiness_scope:"scene"},{export_profile_id:"other"}]) {
  assert(!canBuildProject(service,{...project,scene_capabilities:{main:{...project.scene_capabilities!.main,...patch}}}));
}
assert(!canBuildProject(service,{...project,build_issues:[{code:"V2_TEST",message:"Blocked",path:"project"}]}));
assert(!canBuildProject(service,{...project,scene_capabilities:{}}));
const legacy = {...project,document:{...project.document!,scenes:[{...project.document!.scenes![0],schema_version:1}]}};
assert(canBuildProject({...service,package_export:undefined},legacy));
console.log("Export capability and whole-project readiness checks passed");
