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
};

export type SceneDocument = {
  scene_id: string;
  display_name: string;
  scene_type: string;
  states?: unknown[];
  routes?: unknown[];
  render_models?: unknown[];
};

export type ProjectLoadResult = {
  project_revision: number;
  source_name: string;
  valid: boolean;
  issues: ValidationIssue[];
  document: {
    scenes?: SceneDocument[];
  } | null;
  summary: ProjectSummary;
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
  project_loaded: boolean;
  project_revision: number | null;
};
