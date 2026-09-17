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
const multiSceneService = {
  ...service,
  scene_object_authoring: {...service.scene_object_authoring,multi_scene_export:true},
} as ServiceHello;
const multiSceneProject = {
  ...project,
  document:{...project.document!,scenes:[...project.document!.scenes!,{scene_id:"garden",schema_version:2}]},
  scene_capabilities:{
    main:{...project.scene_capabilities!.main,multi_scene_export:true},
    garden:{...project.scene_capabilities!.main,multi_scene_export:true},
  },
};
assert(canBuildProject(multiSceneService,multiSceneProject as ProjectLoadResult));
assert(!canBuildProject(service,multiSceneProject as ProjectLoadResult));
assert(!canBuildProject(multiSceneService,{
  ...multiSceneProject,
  scene_capabilities:{...multiSceneProject.scene_capabilities,garden:{...multiSceneProject.scene_capabilities.garden,multi_scene_export:false}},
} as ProjectLoadResult));
const audioProfile = {
  supported:true, capability:"audio.sampled_sfx", action_kinds:["play_sfx"],
  action_contexts:["local_transition","local_timer_handler"], compiled_format:"ima_adpcm",
  sample_rate_hz:16000, channels:1, block_samples:256, maximum_assets:32, maximum_cues:64,
  voice_limit:5, cue_volume:true, cue_priority:true, residency:"whole_package",
  shared_package_limit_bytes:65536, survives_local_state_change:true,
  survives_same_package_scene_replacement:true, package_suspend:"stop_and_discard",
  package_resume:"new_requests_only", package_exit_or_replacement:"stop_and_discard",
  unsupported:[],
};
const audioService = {
  ...multiSceneService,
  package_export:{...multiSceneService.package_export!,v2_profile:{
    ...multiSceneService.package_export!.v2_profile!, audio:true, audio_profile:audioProfile,
  }},
  scene_object_authoring:{...multiSceneService.scene_object_authoring!,audio_export:audioProfile},
} as ServiceHello;
const audioProject = {
  ...multiSceneProject,
  document:{...multiSceneProject.document!,audio_assets:[{asset_id:"short"}],audio_cues:[{cue_id:"short.cue"}]},
  scene_capabilities:Object.fromEntries(Object.entries(multiSceneProject.scene_capabilities).map(([id, capability]) => [id, {
    ...capability, audio_export:audioProfile,
  }])),
} as unknown as ProjectLoadResult;
assert(canBuildProject(audioService,audioProject));
assert(!canBuildProject(multiSceneService,audioProject));
assert(!canBuildProject({...audioService,scene_object_authoring:{...audioService.scene_object_authoring!,audio_export:undefined}},audioProject));
assert(!canBuildProject(audioService,{
  ...audioProject,
  scene_capabilities:{...audioProject.scene_capabilities!,garden:{...audioProject.scene_capabilities!.garden,audio_export:null}},
}));
const legacy = {...project,document:{...project.document!,scenes:[{...project.document!.scenes![0],schema_version:1}]}};
assert(canBuildProject({...service,package_export:undefined},legacy));
console.log("Export capability and whole-project readiness checks passed");
