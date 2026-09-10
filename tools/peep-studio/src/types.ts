export type ValidationIssue = {
  scene_id?: string;
  state_id?: string;
  render_model_ref?: string;
  code: string;
  path: string;
  message: string;
};

export type ProjectSummary = {
  project_id: string;
  project_name: string;
  package_id: string;
  target_profile: string;
  entry_scene: string;
  scene_count: number;
  asset_frame_count: number;
  animation_count: number;
  audio_asset_count: number;
  audio_cue_count: number;
};

export type SceneCapabilities = {
  local_graph_commands?: string[];
  scene_connection_commands?: boolean;
  route_destination_kinds?: string[];
  schema_version: number;
  execution_model: string;
  host_editing: boolean;
  host_preview: boolean;
  egg_export: boolean;
  supported_commands: string[] | null;
  legacy_command_catalog: boolean;
};

export type SceneObject = Omit<RenderElement, "element_id" | "x" | "y" | "visible" | "visual_ref"> & {
  object_id: string;
  defaults: { x: number; y: number; visible: boolean; visual_ref?: string };
  animation_ref?: string;
};

export type SceneDocument = {
  schema_version?: number;
  objects?: SceneObject[];
  scene_id: string;
  display_name: string;
  scene_type: string;
  entry_state?: string;
  variables?: StateVariable[];
  input_actions?: InputAction[];
  event_bindings?: EventBinding[];
  event_handlers?: EventHandler[];
  scene_exits?: SceneExitRecord[];
  states?: StateRecord[];
  routes?: StateRoute[];
  render_models?: RenderModel[];
  waiting_visuals?: WaitingVisual[];
  reactive_wait_default?: ReactiveWaitPolicy;
  interaction_policy?: InteractionPolicy;
  joystick_policy?: "four_way" | "eight_way";
};

export type SceneExitRecord = {
  scene_exit_id: string;
  display_name: string;
  target_scene: string;
};

export type StateVariable = {
  variable_id: string;
  value_type: string;
  initial: number;
  minimum: number;
  maximum: number;
};

export type InputAction = {
  action_id: string;
  logical_source: string;
  event_kind?: "press" | "release" | "hold" | "repeat";
};

export type StateRecord = {
  state_id: string;
  display_name: string;
  waiting_visual_ref?: string;
  object_overrides?: Array<{ object_ref: string; x?: number; y?: number; visible?: boolean; visual_ref?: string }>;
  render_model_ref?: string;
  placement_overrides?: StatePlacementOverride[];
};

export type StatePlacementOverride = {
  element_ref: string;
  x?: number;
  y?: number;
  visible?: boolean;
  visual_ref?: string;
};

export type StateGuard = {
  variable_ref: string;
  operator: string;
  value: number;
};

export type StateAction = {
  timer_ref?: string;
  object_ref?: string;
  dx?: number;
  dy?: number;
  kind: string;
  variable_ref?: string;
  operation?: string;
  value?: number;
  cue_ref?: string;
  element_ref?: string;
  visible?: boolean;
  x?: number;
  y?: number;
  frame_ref?: string;
  waiting_visual_ref?: string;
  waiting_element_ref?: string;
  timeline_policy?: string;
};

export type StateRoute = {
  route_id: string;
  action_ref?: string;
  event_ref?: string;
  from_states: string[];
  guards: StateGuard[];
  actions: StateAction[];
  target_state?: string;
  target_scene?: string;
  scene_exit_ref?: string;
};

export type EventBinding = {
  binding_id: string;
  event_type: string;
  configuration: { delay_ms?: number; start_policy?: string; [key: string]: unknown };
};
export type EventHandler = {
  handler_id: string;
  event_ref: string;
  guards: StateGuard[];
  actions: StateAction[];
  target_state?: string;
  target_scene?: string;
};

export type RenderElement = {
  element_id: string;
  kind: string;
  visual_ref?: string;
  x: number;
  y: number;
  width: number;
  height: number;
  z_order: number;
  focus_role?: string;
  layer?: "BACKGROUND" | "SCENE" | "UI";
  visible?: boolean;
  line_direction?: "down_right" | "up_right";
};

export type RenderModel = {
  visual_id: string;
  focus_index: number;
  elements: RenderElement[];
};

export type PlacementProperty = "position" | "visible" | "visual_ref";

export type PlacementStateProjection = {
  changes: Record<string, {
    local_properties: Array<PlacementProperty | "x" | "y">;
    animated: boolean;
  }>;
  resolved_elements: RenderElement[];
};

export type PlacementOwnership = {
  scenes: Record<string, {
    render_model_id?: string;
    execution_model?: string;
    derived_read_only?: boolean;
    objects?: SceneObject[];
    state_scoped_element_ids: string[];
    states: Record<string, PlacementStateProjection>;
  }>;
};

export type WaitingVisualElement = {
  element_id: string;
  source_element_ref: string;
  phase_visual_refs: string[];
  step_phase_indices: number[];
};

export type WaitingVisual = {
  waiting_visual_id: string;
  presentation_id: string;
  phase_quantum_ms: number;
  combined_step_count: number;
  settled_step: number;
  cycle_policy: string;
  elements: WaitingVisualElement[];
};

export type ReactiveWaitPolicy = {
  policy_id: string;
  waiting_visual_ref: string;
  hold_fallback_allowed: boolean;
  event_interests: string[];
};

export type InteractionPolicy = {
  policy_id: string;
  mode: "continuous" | "timeout";
  meaningful_activity_actions: string[];
  inactive_route?: "preserve_scene" | "exit_to_shell";
  bounded_deferrals?: never[];
};

export type EditorNodePosition = {
  x: number;
  y: number;
};

export type EditorRouteRail = {
  axis: "x" | "y";
  value: number;
};

export type EditorRouteTokenPositions = {
  condition?: number;
  actions?: number[];
};

export type EditorRouteLayout = {
  sources?: Record<string, {
    routing_version?: number;
    target_handle?: "entry-top-left" | "entry-top-right" | "entry-bottom-left" | "entry-bottom-right";
    target_side?: "left" | "right" | "top" | "bottom";
    rails?: EditorRouteRail[];
    token_positions?: EditorRouteTokenPositions;
    waypoints?: EditorNodePosition[];
  }>;
};

export type EditorSceneFlowReference = EditorNodePosition & {
  target_scene: string;
};

export type ProjectEditorData = {
  scene_flow?: {
    nodes?: Record<string, EditorNodePosition>;
    package_entry?: EditorNodePosition;
    references?: Record<string, EditorSceneFlowReference>;
    exit_references?: Record<string, Record<string, string>>;
    routes?: Record<string, Record<string, {
      routing_version?: number;
      rails?: EditorRouteRail[];
    }>>;
  };
  state_graph?: {
    scenes?: Record<string, {
      entry?: {
        target_handle: "entry-top-left" | "entry-top-right" | "entry-bottom-left" | "entry-bottom-right";
        target_side: "left" | "right" | "top" | "bottom";
      };
      nodes?: Record<string, EditorNodePosition>;
      routes?: Record<string, EditorRouteLayout>;
    }>;
  };
};

export type CompiledAssetFrame = {
  asset_id: string;
  frame_id: string;
  width: number;
  height: number;
  row_stride_bytes: number;
  pivot_x: number;
  pivot_y: number;
  opaque: boolean;
  pixels_base64: string;
  mask_base64: string;
  pixels_sha256: string;
  mask_sha256: string;
};

export type AssetFrameRecord = {
  frame_id: string;
  display_name?: string;
  source_rect?: { x: number; y: number; width: number; height: number };
  pivot_x: number;
  pivot_y: number;
};

export type AssetRecord = {
  asset_id: string;
  display_name?: string;
  asset_type: string;
  source_path?: string;
  source_format: string;
  font_id?: string;
  text?: string;
  scale?: number;
  frames: AssetFrameRecord[];
};

export type AudioAssetRecord = {
  asset_id: string;
  source_path: string;
  source_sample_rate_hz: number;
  source_channels: number;
  sample_rate_hz: number;
  channels: number;
  sample_count: number;
  duration_ms: number;
  decoded_pcm_bytes: number;
  block_samples: number;
  block_count: number;
  adpcm_bytes: number;
  adpcm_sha256: string;
};

export type AudioCueRecord = {
  cue_id: string;
  display_name?: string;
  asset_ref: string;
  priority: number;
  volume: number;
};

export type ProjectDocument = {
  animations?: AuthoredClip[];
  project?: {
    editor?: ProjectEditorData;
  };
  scenes?: SceneDocument[];
  assets?: AssetRecord[];
  audio_assets?: AudioAssetRecord[];
  audio_cues?: AudioCueRecord[];
  compiled_asset_frames?: CompiledAssetFrame[];
};

export type AuthoredClip = {
  animation_id: string;
  frame_refs: string[];
  frame_duration_ms: number[];
  loop_policy: string;
};

export type ProjectLoadResult = {
  scene_capabilities?: Record<string, SceneCapabilities>;
  build_issues?: ValidationIssue[];
  project_revision: number;
  source_name: string;
  valid: boolean;
  issues: ValidationIssue[];
  document: ProjectDocument | null;
  placement_ownership: PlacementOwnership | null;
  summary: ProjectSummary;
  dirty: boolean;
  can_undo: boolean;
  can_redo: boolean;
  undo_limit: number;
};

export type ProjectCommandResult = {
  scene_capabilities?: Record<string, SceneCapabilities>;
  build_issues?: ValidationIssue[];
  project_revision: number;
  valid: boolean;
  issues: ValidationIssue[];
  document: ProjectDocument | null;
  placement_ownership: PlacementOwnership | null;
  summary: ProjectSummary;
  applied_commands: Array<Record<string, unknown>>;
  dirty: boolean;
  can_undo: boolean;
  can_redo: boolean;
  undo_limit: number;
};

export type ProjectSaveResult = {
  scene_capabilities?: Record<string, SceneCapabilities>;
  build_issues?: ValidationIssue[];
  project_revision: number;
  valid: boolean;
  issues: ValidationIssue[];
  document: ProjectDocument | null;
  placement_ownership: PlacementOwnership | null;
  summary: ProjectSummary;
  dirty: boolean;
  can_undo: boolean;
  can_redo: boolean;
  undo_limit: number;
  saved_sources: string[];
};

export type ProjectHistoryResult = {
  scene_capabilities?: Record<string, SceneCapabilities>;
  build_issues?: ValidationIssue[];
  project_revision: number;
  valid: boolean;
  issues: ValidationIssue[];
  document: ProjectDocument | null;
  placement_ownership: PlacementOwnership | null;
  summary: ProjectSummary;
  dirty: boolean;
  can_undo: boolean;
  can_redo: boolean;
  undo_limit: number;
};

export type Framebuffer = {
  width: number;
  height: number;
  row_stride_bytes: number;
  encoding: string;
  size_bytes: number;
  black_pixel_count: number;
  sha256: string;
  data_base64: string;
};

export type ProjectSceneThumbnailsResult = {
  project_revision: number;
  thumbnails: Array<{
    scene_id: string;
    framebuffer: Framebuffer;
  }>;
};

export type PreviewSnapshot = {
  project_revision: number;
  preview_revision: number;
  scene: {
    scene_id: string;
    state_index: number;
    state_id: string;
    display_name: string;
  };
  timeline: {
    elapsed_ms: number;
    ownership?: "scene_objects";
    presentation_id?: number;
    step_index?: number;
    step_elapsed_ms?: number;
    phase_quantum_ms?: number;
    step_count?: number;
  };
  variables: Record<string, number>;
  input: {
    logical_source: string;
    action_id: string;
    accepted: boolean;
    route_id: string | null;
    audio_events?: Array<{
      cue_id: string;
      priority: number;
      volume: number;
    }>;
  } | null;
  framebuffer: Framebuffer;
};

export type SceneBasePreviewSnapshot = {
  project_revision: number;
  preview_revision: number;
  placement: {
    kind: "scene_base";
    scene_id: string;
    display_name: string;
  };
  framebuffer: Framebuffer;
};

export type PlacementPreviewSnapshot = PreviewSnapshot | SceneBasePreviewSnapshot;

export type PackageBuildResult = {
  project_revision: number;
  package: {
    package_id: string;
    target_profile: string;
    entry_scene: string;
    scene_count: number;
    asset_frame_count: number;
    animation_count: number;
    audio_asset_count: number;
    audio_cue_count: number;
    chunk_count: number;
    size_bytes: number;
    sha256: string;
    blob_base64: string;
  };
  compatibility_report: Record<string, unknown>;
};

export type PeepOSTriggerCapability = {
  kind: string;
  label: string;
  detail: string;
  support: "available" | "contract_only";
  requires: string[];
};

export type ServiceHello = {
  target_profiles?: { available: Array<{ profile_id: string; state_scene_events?: {
    sources: Array<{ event_type: string; status: string; configuration_schema: {
      delay_ms?: { minimum: number; maximum: number };
    } }>;
  } }> };
  scene_creation?: {
    project_operation: string; scene_command: string; entry_scene_command: string;
    version_parameter: string; supported_versions: number[]; default_version: number;
  };
  scene_object_authoring?: {
    local_graph_commands?: string[];
    scene_connection_commands?: boolean;
    route_destination_kinds?: string[];
    state_management_commands?: string[];
    status: string;
    schema_version: number;
    execution_model: string;
    egg_export: boolean;
    firmware_available: boolean;
    commands: string[];
    graph_construction_commands: boolean;
  };
  service: string;
  service_api_version: number;
  protocol_version: number;
  operations: string[];
  state_scene_presentation: {
    record_format: string;
    load_compatible_formats: string[];
    package_layers: string[];
    system_layers: string[];
    element_kinds: string[];
    visibility: boolean;
    z_order: boolean;
    element_commands: string[];
    state_override_commands: string[];
    asset_commands: string[];
    general_frame_animation: {
      state_placeable: boolean;
      purpose: string;
      commands: string[];
    };
    waiting_animation: {
      sprite_only: boolean;
      phase_count: { minimum: number; maximum: number };
      combined_step_count: { minimum: number; maximum: number };
      element_count_maximum: number;
      quantum_ms: { minimum: number; maximum: number };
      cycle_policies: string[];
      commands: string[];
    };
    logical_inputs: string[];
    logical_input_events: Array<"press" | "release" | "hold" | "repeat">;
    joystick_policies: Array<"four_way" | "eight_way">;
    runtime_text: boolean;
    build_time_text: {
      source_format: string;
      font_ids: string[];
      character_set: string;
      glyph_cell: { width: number; height: number };
      scale: { minimum: number; maximum: number; integer_only: boolean };
      ink: string;
      background: string;
      frames_per_asset: number;
      commands: string[];
    };
    element_actions: boolean;
  };
  state_scene_graph: {
    scene_timers?: { event_type: string; start_policies: string[]; actions: string[] };
    command_batch_maximum: number;
    scene_commands: string[];
    scene_flow_commands: string[];
    peepos_trigger_commands: string[];
    peepos_trigger_catalog: PeepOSTriggerCapability[];
    limits: {
      states: number;
      render_models: number;
      variables: number;
      input_actions: number;
      scene_exits: number;
      routes: number;
      guards_per_route: number;
      actions_per_route: number;
    };
    state_commands: string[];
    state_placement_commands: string[];
    render_model_commands: string[];
    variable_commands: string[];
    input_action_commands: string[];
    route_commands: string[];
    scene_exit_commands: string[];
    guard_commands: string[];
    action_commands: string[];
    policy_commands: string[];
    generic_delete_policy: string;
  };
  state_scene_audio: {
    host_package_support: boolean;
    target_playback_status: string;
    source_format: string;
    compiled_format: string;
    sample_rate_hz: number;
    channels: number;
    block_samples: number;
    maximum_duration_ms: number | null;
    maximum_assets: number;
    maximum_cues: number;
    maximum_bank_bytes: number;
    voice_limit: number;
    route_action: string;
    asset_commands: string[];
    cue_commands: string[];
    audition_operation: string;
    unsupported: string[];
  };
  project_loaded: boolean;
  project_revision: number | null;
};

export type AudioAuditionResult = {
  project_revision: number;
  cue: {
    cue_id: string;
    asset_id: string;
    priority: number;
    volume: number;
  };
  audio: {
    encoding: string;
    sample_rate_hz: number;
    channels: number;
    sample_count: number;
    duration_ms: number;
    wav_base64: string;
  };
};
