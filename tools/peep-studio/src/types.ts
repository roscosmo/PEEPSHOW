export type ValidationIssue = {
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

export type SceneDocument = {
  scene_id: string;
  display_name: string;
  scene_type: string;
  entry_state?: string;
  variables?: StateVariable[];
  input_actions?: InputAction[];
  states?: StateRecord[];
  routes?: StateRoute[];
  render_models?: RenderModel[];
  waiting_visuals?: WaitingVisual[];
  reactive_wait_default?: ReactiveWaitPolicy;
  interaction_policy?: InteractionPolicy;
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
};

export type StateRecord = {
  state_id: string;
  display_name: string;
  waiting_visual_ref: string;
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
  action_ref: string;
  from_states: string[];
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
};

export type RenderModel = {
  visual_id: string;
  focus_index: number;
  elements: RenderElement[];
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

export type EditorRouteLayout = {
  sources?: Record<string, {
    routing_version?: number;
    target_handle?: "entry-top-left" | "entry-top-right" | "entry-bottom-left" | "entry-bottom-right";
    target_side?: "left" | "right" | "top" | "bottom";
    rails?: EditorRouteRail[];
    waypoints?: EditorNodePosition[];
  }>;
};

export type ProjectEditorData = {
  scene_flow?: {
    nodes?: Record<string, EditorNodePosition>;
  };
  state_graph?: {
    scenes?: Record<string, {
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
  asset_ref: string;
  priority: number;
  volume: number;
};

export type ProjectDocument = {
  project?: {
    editor?: ProjectEditorData;
  };
  scenes?: SceneDocument[];
  assets?: AssetRecord[];
  audio_assets?: AudioAssetRecord[];
  audio_cues?: AudioCueRecord[];
  compiled_asset_frames?: CompiledAssetFrame[];
};

export type ProjectLoadResult = {
  project_revision: number;
  source_name: string;
  valid: boolean;
  issues: ValidationIssue[];
  document: ProjectDocument | null;
  summary: ProjectSummary;
  dirty: boolean;
  can_undo: boolean;
  can_redo: boolean;
  undo_limit: number;
};

export type ProjectCommandResult = {
  project_revision: number;
  valid: boolean;
  issues: ValidationIssue[];
  document: ProjectDocument | null;
  summary: ProjectSummary;
  applied_commands: Array<Record<string, unknown>>;
  dirty: boolean;
  can_undo: boolean;
  can_redo: boolean;
  undo_limit: number;
};

export type ProjectSaveResult = {
  project_revision: number;
  valid: boolean;
  issues: ValidationIssue[];
  document: ProjectDocument | null;
  summary: ProjectSummary;
  dirty: boolean;
  can_undo: boolean;
  can_redo: boolean;
  undo_limit: number;
  saved_sources: string[];
};

export type ProjectHistoryResult = {
  project_revision: number;
  valid: boolean;
  issues: ValidationIssue[];
  document: ProjectDocument | null;
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
    presentation_id: number;
    step_index: number;
    step_elapsed_ms: number;
    phase_quantum_ms: number;
    step_count: number;
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

export type ServiceHello = {
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
    command_batch_maximum: number;
    limits: {
      states: number;
      render_models: number;
      variables: number;
      input_actions: number;
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
