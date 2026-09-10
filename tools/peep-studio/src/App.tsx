import {
  AlertTriangle,
  ArrowDown,
  ArrowUp,
  ArrowRight,
  Box,
  Check,
  ChevronRight,
  Circle,
  CircleDot,
  Download,
  Eye,
  FileCode2,
  FilePlus2,
  FolderOpen,
  Hammer,
  Image,
  Layers3,
  LoaderCircle,
  Maximize2,
  Minus,
  MonitorDot,
  Network,
  PackageCheck,
  Pause,
  Play,
  Plus,
  RectangleHorizontal,
  RotateCcw,
  Redo2,
  Save,
  SaveAll,
  SquareMousePointer,
  Trash2,
  Type,
  Undo2,
  Volume2,
  X,
} from "lucide-react";
import { useCallback, useEffect, useMemo, useRef, useState, type CSSProperties, type KeyboardEvent as ReactKeyboardEvent, type MouseEvent as ReactMouseEvent, type PointerEvent as ReactPointerEvent } from "react";
import { FramebufferCanvas, FramePreviewCanvas } from "./FramebufferCanvas";
import { EmulatorPanel } from "./EmulatorPanel";
import {
  lineDirectionFromPoints,
  normalizePrimitiveBounds,
  PLACEMENT_HEIGHT,
  PLACEMENT_WIDTH,
  primitiveBoundsFromPoints,
  type PlacementBounds,
  type PlacementLineDirection,
  type PlacementPoint,
  type PlacementPrimitiveKind,
} from "./placementGeometry";
import {
  SceneAuthoringInspector,
  SceneFlowInspector,
  SceneFlowView,
  StateGraphView,
  type NewStateTransitionTarget,
  type SceneSelection,
  type StateTriggerEventKind,
} from "./SceneInspection";
import type { StateGraphEntryHandle, StateGraphEntrySide } from "./stateGraph";
import { baseObjectRows, canEditLegacyScene, canPreviewSceneObjects, supportsNativeCreation, supportsStateManagement, supportsLocalGraphCommand, supportsObjectCommand, usesSceneObjects } from "./sceneCapabilities";
import { SceneObjectInspector } from "./SceneObjectInspector";
import { TimerInspector, type TimerRequest } from "./TimerInspector";
import { SCENE_TIMER, STATE_TIMER, timerBounds, deleteTimerCommands } from "./timerAuthoring";
import type {
  AssetFrameRecord,
  AssetRecord,
  AudioAssetRecord,
  AudioAuditionResult,
  AudioCueRecord,
  CompiledAssetFrame,
  EditorNodePosition,
  EditorRouteRail,
  EditorRouteTokenPositions,
  Framebuffer,
  PackageBuildResult,
  PlacementPreviewSnapshot,
  ProjectCommandResult,
  ProjectHistoryResult,
  ProjectSceneThumbnailsResult,
  ProjectSaveResult,
  PreviewSnapshot,
  ProjectLoadResult,
  SceneDocument,
  ServiceHello,
} from "./types";
import type { RenderElement, RenderModel, StateRecord, StateVariable } from "./types";

const PLACEMENT_PRIMITIVES = [
  { kind: "line", label: "Line" },
  { kind: "outline_rect", label: "Outline rectangle" },
  { kind: "filled_rect", label: "Filled rectangle" },
  { kind: "circle", label: "Circle" },
  { kind: "ellipse", label: "Ellipse" },
] as const;

type PlacementTool = "select" | PlacementPrimitiveKind;
type PlacementPrimitiveDraft = {
  kind: PlacementPrimitiveKind;
  bounds: PlacementBounds;
  lineDirection?: PlacementLineDirection;
};
type WorkspaceMode = "scene-flow" | "logic" | "placement" | "assets";
type PlacementInspectorTab = "object" | "settings";
type AssetSelection =
  | { kind: "sprite"; frameId: string }
  | { kind: "audio"; cueId: string }
  | null;
const SYSTEM_FONT_8X8_BASIC_ID = "peepshow.system.8x8.basic.v1";

type PendingSpriteImport = {
  assetId: string;
  displayName: string;
  sourcePath: string;
  width: number;
  height: number;
  frameWidth: number;
  frameHeight: number;
};

type PreviewStartTarget = {
  sceneId: string;
  stateId?: string;
};

type PreviewStartOptions = {
  revision?: number;
  stateId?: string;
  updateSelection?: boolean;
  rememberStart?: boolean;
};

type ApplyProjectResultOptions = {
  preserveDerivedViews?: boolean;
};

function errorText(error: unknown): string {
  return error instanceof Error ? error.message : String(error);
}

function StatusMark({ ok }: { ok: boolean }) {
  return ok ? <Check size={14} aria-hidden="true" /> : <X size={14} aria-hidden="true" />;
}

export default function App() {
  const bridge = window.peepStudio;
  const [service, setService] = useState<ServiceHello | null>(null);
  const [project, setProject] = useState<ProjectLoadResult | null>(null);
  const [preview, setPreview] = useState<PreviewSnapshot | null>(null);
  const [placementPreview, setPlacementPreview] = useState<PlacementPreviewSnapshot | null>(null);
  const [placementPreviewLoading, setPlacementPreviewLoading] = useState(false);
  const [placementPreviewError, setPlacementPreviewError] = useState<string | null>(null);
  const [selectedScene, setSelectedScene] = useState<string | null>(null);
  const [projectPath, setProjectPath] = useState<string | null>(null);
  const [temporaryProject, setTemporaryProject] = useState(false);
  const [sceneSelection, setSceneSelection] = useState<SceneSelection>({ kind: "project" });
  const [timerRequest, setTimerRequest] = useState<TimerRequest | null>(null);
  const [placementStateId, setPlacementStateId] = useState<string | null>(null);
  const [placementEditStateIds, setPlacementEditStateIds] = useState<string[]>([]);
  const [selectedPlacementElement, setSelectedPlacementElement] = useState<string | null>(null);
  const [build, setBuild] = useState<PackageBuildResult | null>(null);
  const [dirty, setDirty] = useState(false);
  const [canUndo, setCanUndo] = useState(false);
  const [canRedo, setCanRedo] = useState(false);
  const [busy, setBusy] = useState<string | null>(null);
  const [playing, setPlaying] = useState(false);
  const [workspaceMode, setWorkspaceMode] = useState<WorkspaceMode>("scene-flow");
  const [projectHierarchyExpanded, setProjectHierarchyExpanded] = useState(true);
  const [placementInspectorTab, setPlacementInspectorTab] = useState<PlacementInspectorTab>("object");
  const [sceneThumbnails, setSceneThumbnails] = useState<Record<string, Framebuffer>>({});
  const [expandedSceneIds, setExpandedSceneIds] = useState<string[]>([]);
  const [collapsedHierarchyIds, setCollapsedHierarchyIds] = useState<string[]>([]);
  const [assetSelection, setAssetSelection] = useState<AssetSelection>(null);
  const [audioAuditionStatus, setAudioAuditionStatus] = useState("No cue auditioned.");
  const [assetPreviewPlaying, setAssetPreviewPlaying] = useState(false);
  const [assetPreviewStep, setAssetPreviewStep] = useState(0);
  const [sceneFlowLayoutStatus, setSceneFlowLayoutStatus] = useState("No layout move yet");
  const [stateGraphLayoutStatus, setStateGraphLayoutStatus] = useState("No layout move yet");
  const [projectWidth, setProjectWidth] = useState(320);
  const [inspectorWidth, setInspectorWidth] = useState(390);
  const [placementGridVisible, setPlacementGridVisible] = useState(true);
  const [placementMajorGridVisible, setPlacementMajorGridVisible] = useState(true);
  const [placementGridStrength, setPlacementGridStrength] = useState(18);
  const [placementOverlayVisible, setPlacementOverlayVisible] = useState(true);
  const [placementLabelMode, setPlacementLabelMode] = useState<"hover" | "always" | "off">("hover");
  const [placementTool, setPlacementTool] = useState<PlacementTool>("select");
  const [placementPrimitiveDraft, setPlacementPrimitiveDraft] = useState<PlacementPrimitiveDraft | null>(null);
  const [spritePickerOpen, setSpritePickerOpen] = useState(false);
  const [placementDraftPositions, setPlacementDraftPositions] = useState<Record<string, { x: number; y: number }>>({});
  const [placementDraftBounds, setPlacementDraftBounds] = useState<Record<string, { x: number; y: number; width: number; height: number }>>({});
  const [pendingSpriteImport, setPendingSpriteImport] = useState<PendingSpriteImport | null>(null);
  const [assetImportDebug, setAssetImportDebug] = useState("No import attempted.");
  const [message, setMessage] = useState<string | null>(null);
  const previewRef = useRef<PreviewSnapshot | null>(null);
  const selectedSceneRef = useRef<string | null>(null);
  const workspaceModeRef = useRef<WorkspaceMode>(workspaceMode);
  const projectRevisionRef = useRef<number | null>(null);
  const layoutSaveChain = useRef(Promise.resolve());
  const operationLock = useRef(false);
  const audioPlaybackRef = useRef<HTMLAudioElement | null>(null);
  const audioPlaybackRequestRef = useRef(0);
  const previewAudioCacheRef = useRef(new Map<string, string>());
  const previewRestoreAttemptRef = useRef<string | null>(null);
  const previewStartRef = useRef<PreviewStartTarget | null>(null);
  const placementSelectionAnchorRef = useRef<string | null>(null);
  const placementSceneRef = useRef<string | null>(null);
  const placementDrawCancelRef = useRef<(() => boolean) | null>(null);

  const stopAudioPlayback = useCallback(() => {
    audioPlaybackRequestRef.current += 1;
    const audio = audioPlaybackRef.current;
    if (audio !== null) {
      audio.pause();
      audio.currentTime = 0;
      audioPlaybackRef.current = null;
    }
  }, []);

  useEffect(() => {
    previewRef.current = preview;
  }, [preview]);

  useEffect(() => {
    selectedSceneRef.current = selectedScene;
  }, [selectedScene]);

  useEffect(() => {
    workspaceModeRef.current = workspaceMode;
  }, [workspaceMode]);

  useEffect(() => {
    projectRevisionRef.current = project?.project_revision ?? null;
  }, [project?.project_revision]);

  useEffect(() => {
    previewAudioCacheRef.current.clear();
    return stopAudioPlayback;
  }, [project?.project_revision, stopAudioPlayback]);

  useEffect(() => {
    if (bridge === undefined) {
      setMessage("Electron bridge unavailable. Run the desktop shell to connect the authoring service.");
      return;
    }
    bridge
      .serviceRequest<ServiceHello>("service.hello", {})
      .then(setService)
      .catch((error) => setMessage(errorText(error)));
  }, [bridge]);

  const startPreview = useCallback(
    async (sceneId: string, options: PreviewStartOptions = {}) => {
      const revision = options.revision ?? project?.project_revision;
      if (bridge === undefined || revision === undefined) {
        return false;
      }
      setBusy("Starting preview");
      setPlaying(false);
      stopAudioPlayback();
      try {
        const result = await bridge.serviceRequest<PreviewSnapshot>("project.preview_reset", {
          project_revision: revision,
          scene_id: sceneId,
          ...(options.stateId === undefined ? {} : { state_id: options.stateId }),
        });
        if (options.rememberStart !== false) {
          previewStartRef.current = {
            sceneId,
            ...(options.stateId === undefined ? {} : { stateId: options.stateId }),
          };
        }
        setSelectedScene(sceneId);
        if (options.updateSelection !== false) {
          setSceneSelection(options.stateId === undefined
            ? { kind: "scene" }
            : { kind: "state", id: options.stateId });
          setSelectedPlacementElement(null);
        }
        setPreview(result);
        setMessage(null);
        return true;
      } catch (error) {
        setMessage(errorText(error));
        return false;
      } finally {
        setBusy(null);
      }
    },
    [bridge, project?.project_revision, stopAudioPlayback],
  );

  const loadProject = useCallback(
    async (path: string) => {
      if (bridge === undefined) {
        return;
      }
      setBusy("Loading project");
      setPlaying(false);
      setBuild(null);
      setDirty(false);
      setCanUndo(false);
      setCanRedo(false);
      setProjectPath(path);
      setPreview(null);
      setPlacementPreview(null);
      setPlacementPreviewLoading(false);
      setPlacementPreviewError(null);
      setSelectedScene(null);
      previewStartRef.current = null;
      setPlacementStateId(null);
      setSceneThumbnails({});
      setExpandedSceneIds([]);
      setCollapsedHierarchyIds([]);
      setSceneSelection({ kind: "scene" });
      setSelectedPlacementElement(null);
      setAssetSelection(null);
      try {
        const result = await bridge.serviceRequest<ProjectLoadResult>("project.load", { path });
        setProject(result);
        setDirty(result.dirty);
        setCanUndo(result.can_undo);
        setCanRedo(result.can_redo);
        setMessage(result.valid ? null : "Project validation failed. Review the issues panel.");
        if (result.valid) {
          await startPreview(result.summary.entry_scene, { revision: result.project_revision });
        }
      } catch (error) {
        setMessage(errorText(error));
      } finally {
        setBusy(null);
      }
    },
    [bridge, startPreview],
  );

  const openProject = async () => {
    if (bridge === undefined) {
      return;
    }
    const path = await bridge.openProject();
    if (path !== null) {
      setTemporaryProject(false);
      await loadProject(path);
    }
  };

  const newProject = async () => {
    if (bridge === undefined || busy !== null) {
      return;
    }
    const path = await bridge.chooseNewProjectPath();
    if (path === null) {
      return;
    }
    setBusy("Creating project");
    setPlaying(false);
    try {
      const result = await bridge.serviceRequest<ProjectLoadResult>("project.create", {
        path, ...(supportsNativeCreation(service) ? { scene_schema_version: 2 } : {}),
      });
      setBuild(null);
      setPreview(null);
      setPlacementPreview(null);
      setPlacementPreviewLoading(false);
      setPlacementPreviewError(null);
      setSelectedScene(null);
      setPlacementStateId(null);
      setSceneThumbnails({});
      setExpandedSceneIds([]);
      setCollapsedHierarchyIds([]);
      setSceneSelection({ kind: "scene" });
      setSelectedPlacementElement(null);
      setAssetSelection(null);
      setProjectPath(path);
      setTemporaryProject(false);
      setWorkspaceMode("scene-flow");
      setProject(result);
      setDirty(result.dirty);
      setCanUndo(result.can_undo);
      setCanRedo(result.can_redo);
      setMessage(result.valid ? `Created ${result.summary.project_name}.` : "Project validation failed. Review the issues panel.");
      if (result.valid) {
        if (await startPreview(result.summary.entry_scene, { revision: result.project_revision })) {
          setMessage(`Created ${result.summary.project_name}.`);
        }
      }
    } catch (error) {
      setMessage(errorText(error));
    } finally {
      setBusy(null);
    }
  };

  const openExample = async () => {
    if (bridge === undefined) {
      return;
    }
    setTemporaryProject(true);
    await loadProject(await bridge.openExampleProject());
    setMessage("Opened a temporary copy of the example project.");
  };

  const advancePreview = useCallback(
    async (elapsedMs = 250) => {
      const current = previewRef.current;
      if (bridge === undefined || current === null || operationLock.current) {
        return;
      }
      operationLock.current = true;
      try {
        const result = await bridge.serviceRequest<PreviewSnapshot>("project.preview_advance", {
          project_revision: current.project_revision,
          preview_revision: current.preview_revision,
          elapsed_ms: elapsedMs,
        });
        setPreview(result);
        if (result.scene.scene_id !== selectedSceneRef.current && workspaceModeRef.current !== "placement") {
          setSelectedScene(result.scene.scene_id);
          setSceneSelection((current) => current.kind === "project" ? current : { kind: "scene" });
        }
      } catch (error) {
        setPlaying(false);
        setMessage(errorText(error));
      } finally {
        operationLock.current = false;
      }
    },
    [bridge],
  );

  useEffect(() => {
    if (!playing) {
      return undefined;
    }
    const interval = window.setInterval(() => void advancePreview(250), 250);
    return () => window.clearInterval(interval);
  }, [advancePreview, playing]);

  const playPreviewAudioEvents = useCallback(
    async (snapshot: PreviewSnapshot) => {
      const audioEvents = snapshot.input?.audio_events ?? [];
      const event = audioEvents[audioEvents.length - 1];
      if (
        bridge === undefined ||
        event === undefined ||
        service?.operations.includes("project.audio_audition") !== true
      ) {
        return;
      }

      const requestId = audioPlaybackRequestRef.current + 1;
      audioPlaybackRequestRef.current = requestId;
      const cacheKey = `${snapshot.project_revision}:${event.cue_id}`;
      try {
        let dataUrl = previewAudioCacheRef.current.get(cacheKey);
        if (dataUrl === undefined) {
          const result = await bridge.serviceRequest<AudioAuditionResult>("project.audio_audition", {
            project_revision: snapshot.project_revision,
            cue_id: event.cue_id,
          });
          dataUrl = `data:audio/wav;base64,${result.audio.wav_base64}`;
          previewAudioCacheRef.current.set(cacheKey, dataUrl);
        }
        if (audioPlaybackRequestRef.current !== requestId) {
          return;
        }

        const currentAudio = audioPlaybackRef.current;
        if (currentAudio !== null) {
          currentAudio.pause();
          currentAudio.currentTime = 0;
        }
        if (event.volume <= 0) {
          audioPlaybackRef.current = null;
          return;
        }

        const audio = new Audio(dataUrl);
        audio.volume = Math.min(1, Math.max(0, event.volume / 255));
        audioPlaybackRef.current = audio;
        audio.addEventListener("ended", () => {
          if (audioPlaybackRef.current === audio) {
            audioPlaybackRef.current = null;
          }
        }, { once: true });
        await audio.play();
      } catch (error) {
        if (audioPlaybackRequestRef.current === requestId) {
          setMessage(`Preview audio failed: ${errorText(error)}`);
        }
      }
    },
    [bridge, service?.operations],
  );

  const sendInput = async (logicalSource: string) => {
    const current = previewRef.current;
    if (bridge === undefined || current === null || operationLock.current) {
      return;
    }
    operationLock.current = true;
    try {
      const result = await bridge.serviceRequest<PreviewSnapshot>("project.preview_input", {
        project_revision: current.project_revision,
        preview_revision: current.preview_revision,
        logical_source: logicalSource,
      });
      setPreview(result);
      void playPreviewAudioEvents(result);
      if (result.scene.scene_id !== selectedScene) {
        setSelectedScene(result.scene.scene_id);
        setSceneSelection((current) => current.kind === "project" ? current : { kind: "scene" });
      }
      setMessage(null);
    } catch (error) {
      setMessage(errorText(error));
    } finally {
      operationLock.current = false;
    }
  };

  const buildPackage = async () => {
    if (bridge === undefined || project === null || !project.valid) {
      return;
    }
    setBusy("Building package");
    setBuild(null);
    try {
      const result = await bridge.serviceRequest<PackageBuildResult>("project.build_package", {
        project_revision: project.project_revision,
      });
      setBuild(result);
      setMessage(`Built ${result.package.package_id}.egg`);
    } catch (error) {
      setMessage(errorText(error));
    } finally {
      setBusy(null);
    }
  };

  const saveProject = async () => {
    if (bridge === undefined || project === null || busy !== null) {
      return;
    }
    setBusy("Saving project");
    try {
      const result = await bridge.serviceRequest<ProjectSaveResult>("project.save", {
        project_revision: project.project_revision,
      });
      applyProjectResult(result, { preserveDerivedViews: true });
      setMessage(`Saved ${result.saved_sources.length} source file${result.saved_sources.length === 1 ? "" : "s"}.`);
    } catch (error) {
      setMessage(errorText(error));
    } finally {
      setBusy(null);
    }
  };

  const applyProjectResult = (
    result: ProjectHistoryResult | ProjectCommandResult | ProjectSaveResult,
    options: ApplyProjectResultOptions = {},
  ) => {
    setProject((current) => (
      current === null
        ? null
        : {
            ...current,
            project_revision: result.project_revision,
            valid: result.valid,
            issues: result.issues,
            build_issues: result.build_issues,
            document: result.document,
            placement_ownership: result.placement_ownership,
            scene_capabilities: result.scene_capabilities,
            summary: result.summary,
          }
    ));
    setDirty(result.dirty);
    setCanUndo(result.can_undo);
    setCanRedo(result.can_redo);
    if (options.preserveDerivedViews) {
      return;
    }
    setPreview(null);
    setPlacementPreview(null);
    setPlacementPreviewLoading(false);
    setPlacementPreviewError(null);
    setSceneThumbnails({});
    setPlacementDraftPositions({});
    setBuild(null);
  };

  const addScene = async (requestedName: string) => {
    const displayName = requestedName.trim();
    if (bridge === undefined || project === null || busy !== null || displayName.length === 0) {
      return;
    }
    const nativeScene = project.document?.scenes?.find(scene => scene.scene_id === project.summary.entry_scene)?.schema_version === 2;
    if (nativeScene && !supportsNativeCreation(service)) return;
    setBusy("Adding scene");
    setPlaying(false);
    try {
      const result = await bridge.serviceRequest<ProjectCommandResult>("project.apply_commands", {
        project_revision: project.project_revision,
        commands: [{ kind: "scene.add", display_name: displayName,
          ...(nativeScene
            ? { scene_schema_version: 2 } : {}),
        }],
      });
      const applied = result.applied_commands[0];
      const sceneId = typeof applied?.scene_id === "string" ? applied.scene_id : null;
      if (sceneId === null) {
        throw new Error("Authoring service did not return the new scene ID");
      }
      applyProjectResult(result);
      setWorkspaceMode("scene-flow");
      if (await startPreview(sceneId, { revision: result.project_revision })) {
        setMessage(`Added ${displayName}. Save to write it to the project.`);
      }
    } catch (error) {
      setMessage(errorText(error));
    } finally {
      setBusy(null);
    }
  };

  const stepHistory = async (operation: "project.undo" | "project.redo") => {
    if (bridge === undefined || project === null || busy !== null) {
      return;
    }
    setBusy(operation === "project.undo" ? "Undoing" : "Redoing");
    setPlaying(false);
    try {
      const result = await bridge.serviceRequest<ProjectHistoryResult>(operation, {
        project_revision: project.project_revision,
      });
      applyProjectResult(result);
      setSceneSelection({ kind: "scene" });
      setMessage(operation === "project.undo" ? "Undid last edit." : "Redid edit.");
    } catch (error) {
      setMessage(errorText(error));
    } finally {
      setBusy(null);
    }
  };

  const selectProjectRoot = () => {
    placementDrawCancelRef.current?.();
    placementSelectionAnchorRef.current = null;
    setSceneSelection({ kind: "project" });
    setPlacementStateId(null);
    setPlacementEditStateIds([]);
    setSelectedPlacementElement(null);
    setAssetSelection(null);
    setAssetPreviewPlaying(false);
    setSpritePickerOpen(false);
    setPlacementTool("select");
  };

  const selectAssetRecord = (selection: AssetSelection) => {
    setAssetSelection(selection);
    if (selection !== null) {
      setSceneSelection({ kind: "scene" });
    }
  };

  useEffect(() => {
    const handleRootSelectionShortcut = (event: KeyboardEvent) => {
      if (event.key !== "Escape" || event.defaultPrevented || project === null) {
        return;
      }
      if (placementDrawCancelRef.current?.()) {
        event.preventDefault();
        return;
      }
      event.preventDefault();
      if (event.target instanceof HTMLElement && event.target.matches("input, textarea, select")) {
        event.target.blur();
      }
      selectProjectRoot();
    };
    window.addEventListener("keydown", handleRootSelectionShortcut);
    return () => window.removeEventListener("keydown", handleRootSelectionShortcut);
  }, [project]);

  useEffect(() => {
    const handleHistoryShortcut = (event: KeyboardEvent) => {
      const target = event.target;
      if (
        target instanceof HTMLElement &&
        (target.isContentEditable || target.matches("input, textarea, select"))
      ) {
        return;
      }
      if ((!event.ctrlKey && !event.metaKey) || event.altKey) {
        return;
      }
      const key = event.key.toLowerCase();
      const operation = key === "z" && !event.shiftKey
        ? "project.undo"
        : (key === "y" || (key === "z" && event.shiftKey))
          ? "project.redo"
          : null;
      if (operation === null) {
        return;
      }
      const available = operation === "project.undo"
        ? canUndo && service?.operations.includes("project.undo") === true
        : canRedo && service?.operations.includes("project.redo") === true;
      if (!available || project === null || busy !== null) {
        return;
      }
      event.preventDefault();
      void stepHistory(operation);
    };
    window.addEventListener("keydown", handleHistoryShortcut);
    return () => window.removeEventListener("keydown", handleHistoryShortcut);
  }, [busy, canRedo, canUndo, project, service]);

  const existingAssetIds = () => new Set([
    ...assets.map((asset) => asset.asset_id),
    ...audioAssets.map((asset) => asset.asset_id),
    ...compiledAssetFrameGroups.map((group) => group.assetId),
  ]);

  const uniqueImportedAssetId = (baseAssetId: string) => {
    const existing = existingAssetIds();
    if (!existing.has(baseAssetId)) {
      return baseAssetId;
    }
    for (let index = 2; index < 1000; index += 1) {
      const candidate = `${baseAssetId}_${index}`;
      if (!existing.has(candidate)) {
        return candidate;
      }
    }
    return `${baseAssetId}_${existing.size + 1}`;
  };

  const uniqueTextAssetId = () => {
    const existing = existingAssetIds();
    for (let index = 1; index < 1000; index += 1) {
      const candidate = index === 1 ? "label" : `label_${index}`;
      if (!existing.has(candidate)) {
        return candidate;
      }
    }
    return `label_${existing.size + 1}`;
  };

  const uniqueAudioCueId = (baseCueId: string) => {
    const existing = new Set(audioCues.map((cue) => cue.cue_id));
    if (!existing.has(baseCueId)) {
      return baseCueId;
    }
    for (let index = 2; index < 1000; index += 1) {
      const candidate = `${baseCueId}_${index}`;
      if (!existing.has(candidate)) {
        return candidate;
      }
    }
    return `${baseCueId}_${existing.size + 1}`;
  };

  const parseImportFrameSize = (
    sourceWidth: number,
    sourceHeight: number,
    value: string,
  ): { frameWidth: number; frameHeight: number; error?: string } => {
    const trimmed = value.trim();
    if (trimmed === "") {
      if (sourceWidth > 168 || sourceHeight > 144) {
        return {
          frameWidth: sourceWidth,
          frameHeight: sourceHeight,
          error: "This PNG is larger than the screen. Enter the size of one sprite frame.",
        };
      }
      return { frameWidth: sourceWidth, frameHeight: sourceHeight };
    }
    const match = trimmed.match(/^(\d+)\s*[x, ]\s*(\d+)$/i);
    if (match === null) {
      return { frameWidth: sourceWidth, frameHeight: sourceHeight, error: "Use a frame size like 16x16." };
    }
    const frameWidth = Number(match[1]);
    const frameHeight = Number(match[2]);
    if (!Number.isInteger(frameWidth) || !Number.isInteger(frameHeight) || frameWidth < 1 || frameHeight < 1) {
      return { frameWidth, frameHeight, error: "Frame size must use positive whole pixels." };
    }
    if (frameWidth > 168 || frameHeight > 144) {
      return { frameWidth, frameHeight, error: "Each sprite frame must fit inside 168x144." };
    }
    if (sourceWidth % frameWidth !== 0 || sourceHeight % frameHeight !== 0) {
      return { frameWidth, frameHeight, error: "Frame size must divide the PNG evenly." };
    }
    const frameCount = (sourceWidth / frameWidth) * (sourceHeight / frameHeight);
    if (frameCount > 256) {
      return { frameWidth, frameHeight, error: "A sprite asset can contain at most 256 frames." };
    }
    return { frameWidth, frameHeight };
  };

  const createGridFrames = (assetId: string, sourceWidth: number, sourceHeight: number, frameWidth: number, frameHeight: number): AssetFrameRecord[] => {
    const columns = sourceWidth / frameWidth;
    const rows = sourceHeight / frameHeight;
    const frameCount = columns * rows;
    const frames: AssetFrameRecord[] = [];
    for (let row = 0; row < rows; row += 1) {
      for (let column = 0; column < columns; column += 1) {
        const index = frames.length + 1;
        frames.push({
          frame_id: frameCount === 1 ? `${assetId}.frame` : `${assetId}.frame_${index}`,
          display_name: frameCount === 1 ? "Frame" : `Frame ${index}`,
          source_rect: {
            x: column * frameWidth,
            y: row * frameHeight,
            width: frameWidth,
            height: frameHeight,
          },
          pivot_x: 0,
          pivot_y: 0,
        });
      }
    }
    return frames;
  };

  const chooseSpritePng = async () => {
    if (bridge === undefined || project === null || projectPath === null || busy !== null) {
      return;
    }
    setBusy("Choosing sprite");
    setPlaying(false);
    try {
      const imported = await bridge.importSpritePng(projectPath);
      if (imported === null) {
        setAssetImportDebug("PNG picker cancelled.");
        setMessage("Sprite import cancelled.");
        return;
      }
      const assetId = uniqueImportedAssetId(imported.assetId);
      const defaultFrameSize = imported.width > 168 || imported.height > 144
        ? (imported.width % 16 === 0 && imported.height % 16 === 0 ? "16x16" : "")
        : "";
      const parsed = parseImportFrameSize(imported.width, imported.height, defaultFrameSize);
      setPendingSpriteImport({
        ...imported,
        assetId,
        frameWidth: parsed.error === undefined ? parsed.frameWidth : Math.min(imported.width, 168),
        frameHeight: parsed.error === undefined ? parsed.frameHeight : Math.min(imported.height, 144),
      });
      setWorkspaceMode("assets");
      setAssetImportDebug(`Picked ${imported.sourcePath} (${imported.width}x${imported.height}).`);
      setMessage("Choose frame size, then import the sprite.");
    } catch (error) {
      const text = errorText(error);
      setAssetImportDebug(`PNG picker failed: ${text}`);
      setMessage(text);
    } finally {
      setBusy(null);
    }
  };

  const confirmSpriteImport = async () => {
    if (bridge === undefined || project === null || pendingSpriteImport === null || busy !== null) {
      return;
    }
    setBusy("Importing sprite");
    setPlaying(false);
    try {
      const parsed = parseImportFrameSize(
        pendingSpriteImport.width,
        pendingSpriteImport.height,
        `${pendingSpriteImport.frameWidth}x${pendingSpriteImport.frameHeight}`,
      );
      if (parsed.error !== undefined) {
        setAssetImportDebug(`Import blocked: ${parsed.error}`);
        setMessage(parsed.error);
        return;
      }
      const frames = createGridFrames(
        pendingSpriteImport.assetId,
        pendingSpriteImport.width,
        pendingSpriteImport.height,
        parsed.frameWidth,
        parsed.frameHeight,
      );
      const result = await bridge.serviceRequest<ProjectCommandResult>("project.apply_commands", {
        project_revision: project.project_revision,
        commands: [
          {
            kind: "asset.upsert",
            asset: {
              asset_id: pendingSpriteImport.assetId,
              display_name: pendingSpriteImport.displayName,
              asset_type: "masked_1bpp",
              source_path: pendingSpriteImport.sourcePath,
              source_format: "png",
              frames,
            },
          },
        ],
      });
      applyProjectResult(result);
      selectAssetRecord(frames[0] === undefined ? null : { kind: "sprite", frameId: frames[0].frame_id });
      setWorkspaceMode("assets");
      setPendingSpriteImport(null);
      setAssetImportDebug(`Imported ${pendingSpriteImport.sourcePath}: ${frames.length} frame${frames.length === 1 ? "" : "s"} at ${parsed.frameWidth}x${parsed.frameHeight}.`);
      setMessage(`Imported ${pendingSpriteImport.assetId} with ${frames.length} frame${frames.length === 1 ? "" : "s"}. Save to write it to the project.`);
    } catch (error) {
      const text = errorText(error);
      setAssetImportDebug(`Import failed: ${text}`);
      setMessage(text);
    } finally {
      setBusy(null);
    }
  };

  const createTextSpriteAsset = async () => {
    if (bridge === undefined || project === null || busy !== null) {
      return;
    }
    const assetId = uniqueTextAssetId();
    const asset: AssetRecord = {
      asset_id: assetId,
      display_name: "Label",
      asset_type: "masked_1bpp",
      source_format: "system_font_text",
      font_id: SYSTEM_FONT_8X8_BASIC_ID,
      text: "LABEL",
      scale: 1,
      frames: [
        {
          frame_id: `${assetId}.frame`,
          display_name: "Frame",
          pivot_x: 0,
          pivot_y: 0,
        },
      ],
    };
    setBusy("Creating text sprite");
    setPlaying(false);
    try {
      const result = await bridge.serviceRequest<ProjectCommandResult>("project.apply_commands", {
        project_revision: project.project_revision,
        commands: [{ kind: "asset.upsert", asset }],
      });
      applyProjectResult(result);
      selectAssetRecord({ kind: "sprite", frameId: `${assetId}.frame` });
      setWorkspaceMode("assets");
      setMessage("Text sprite created. Save to write it to the project.");
    } catch (error) {
      setMessage(errorText(error));
    } finally {
      setBusy(null);
    }
  };

  const chooseAudioWav = async () => {
    if (bridge === undefined || project === null || projectPath === null || busy !== null) {
      return;
    }
    setBusy("Importing audio");
    setPlaying(false);
    try {
      const imported = await bridge.importAudioWav(projectPath);
      if (imported === null) {
        setAudioAuditionStatus("WAV picker cancelled.");
        setMessage("Audio import cancelled.");
        return;
      }
      const assetId = uniqueImportedAssetId(imported.assetId);
      const cueId = uniqueAudioCueId(`${assetId}.cue`);
      const result = await bridge.serviceRequest<ProjectCommandResult>("project.apply_commands", {
        project_revision: project.project_revision,
        commands: [
          {
            kind: "audio_asset.upsert",
            audio_asset: {
              asset_id: assetId,
              asset_type: "sampled_sfx",
              source_path: imported.sourcePath,
              source_format: "wav",
            },
          },
          {
            kind: "audio_cue.upsert",
            audio_cue: {
              cue_id: cueId,
              display_name: assetId,
              asset_ref: assetId,
              priority: 96,
              volume: 200,
            },
          },
        ],
      });
      applyProjectResult(result);
      selectAssetRecord({ kind: "audio", cueId });
      setWorkspaceMode("assets");
      setAudioAuditionStatus(`Imported ${imported.sourcePath}.`);
      setMessage(`Imported ${assetId} as ${cueId}. Save to write it to the project.`);
    } catch (error) {
      const text = errorText(error);
      setAudioAuditionStatus(`Import failed: ${text}`);
      setMessage(text);
    } finally {
      setBusy(null);
    }
  };

  const auditionAudioCue = async (cueId: string) => {
    if (bridge === undefined || project === null || busy !== null || service?.operations.includes("project.audio_audition") !== true) {
      return;
    }
    setBusy("Auditioning audio");
    setPlaying(false);
    stopAudioPlayback();
    try {
      const result = await bridge.serviceRequest<AudioAuditionResult>("project.audio_audition", {
        project_revision: project.project_revision,
        cue_id: cueId,
      });
      const audio = new Audio(`data:audio/wav;base64,${result.audio.wav_base64}`);
      audioPlaybackRef.current = audio;
      audio.addEventListener("ended", () => {
        if (audioPlaybackRef.current === audio) {
          audioPlaybackRef.current = null;
        }
      }, { once: true });
      await audio.play();
      selectAssetRecord({ kind: "audio", cueId });
      setAudioAuditionStatus(`Played packaged ${result.audio.duration_ms} ms cue at ${result.audio.sample_rate_hz} Hz.`);
      setMessage(`Auditioned ${cueId} from packaged ADPCM bytes.`);
    } catch (error) {
      const text = errorText(error);
      setAudioAuditionStatus(`Audition failed: ${text}`);
      setMessage(text);
    } finally {
      setBusy(null);
    }
  };

  const updateAssetDisplayNames = async (
    asset: AssetRecord,
    nextAssetDisplayName: string,
    nextFrameDisplayName: string | null,
    frameId: string | null,
  ) => {
    if (bridge === undefined || project === null || busy !== null) {
      return;
    }
    const assetDisplayName = nextAssetDisplayName.trim();
    const frameDisplayName = nextFrameDisplayName?.trim() ?? null;
    if (assetDisplayName.length === 0 || assetDisplayName.length > 64 || (frameDisplayName !== null && (frameDisplayName.length === 0 || frameDisplayName.length > 64))) {
      setMessage("Display names must be 1 to 64 characters.");
      return;
    }
    const currentAssetDisplayName = asset.display_name ?? asset.asset_id;
    const targetFrame = frameId === null ? null : asset.frames.find((frame) => frame.frame_id === frameId) ?? null;
    const currentFrameDisplayName = targetFrame === null ? null : targetFrame.display_name ?? targetFrame.frame_id;
    if (assetDisplayName === currentAssetDisplayName && (frameDisplayName === null || frameDisplayName === currentFrameDisplayName)) {
      return;
    }
    const renamed: AssetRecord = {
      ...asset,
      display_name: assetDisplayName,
      frames: asset.frames.map((frame) => (
        frame.frame_id === frameId && frameDisplayName !== null
          ? { ...frame, display_name: frameDisplayName }
          : frame
      )),
    };
    setBusy("Renaming asset");
    setPlaying(false);
    try {
      const result = await bridge.serviceRequest<ProjectCommandResult>("project.apply_commands", {
        project_revision: project.project_revision,
        commands: [{ kind: "asset.upsert", asset: renamed }],
      });
      applyProjectResult(result);
      if (frameId !== null) {
        selectAssetRecord({ kind: "sprite", frameId });
      }
      setMessage("Asset label updated. Save to write it to the project.");
    } catch (error) {
      setMessage(errorText(error));
    } finally {
      setBusy(null);
    }
  };

  const updateAudioCueDisplayName = async (cue: AudioCueRecord, nextDisplayName: string) => {
    if (bridge === undefined || project === null || busy !== null) {
      return;
    }
    const displayName = nextDisplayName.trim();
    if (displayName.length === 0 || displayName.length > 64) {
      setMessage("Audio names must be 1 to 64 characters.");
      return;
    }
    if (displayName === (cue.display_name ?? cue.cue_id)) {
      return;
    }
    setBusy("Renaming audio cue");
    setPlaying(false);
    try {
      const result = await bridge.serviceRequest<ProjectCommandResult>("project.apply_commands", {
        project_revision: project.project_revision,
        commands: [{
          kind: "audio_cue.upsert",
          audio_cue: { ...cue, display_name: displayName },
        }],
      });
      applyProjectResult(result);
      selectAssetRecord({ kind: "audio", cueId: cue.cue_id });
      setMessage("Audio cue renamed. Save to write it to the project.");
    } catch (error) {
      setMessage(errorText(error));
    } finally {
      setBusy(null);
    }
  };

  const updateTextSpriteAsset = async (asset: AssetRecord, nextText: string, nextScale: number) => {
    if (bridge === undefined || project === null || busy !== null || asset.source_format !== "system_font_text") {
      return;
    }
    const text = nextText.replace(/\r\n/g, "\n").replace(/\r/g, "\n");
    if (text.length === 0 || text.length > 96) {
      setMessage("Text must be 1 to 96 characters.");
      return;
    }
    if (!Number.isInteger(nextScale) || nextScale < 1 || nextScale > 8) {
      setMessage("Text scale must be 1 to 8.");
      return;
    }
    if (text === (asset.text ?? "") && nextScale === (asset.scale ?? 1)) {
      return;
    }
    const updated: AssetRecord = {
      ...asset,
      font_id: asset.font_id ?? SYSTEM_FONT_8X8_BASIC_ID,
      text,
      scale: nextScale,
    };
    setBusy("Updating text sprite");
    setPlaying(false);
    try {
      const result = await bridge.serviceRequest<ProjectCommandResult>("project.apply_commands", {
        project_revision: project.project_revision,
        commands: [{ kind: "asset.upsert", asset: updated }],
      });
      applyProjectResult(result);
      selectAssetRecord(asset.frames[0] === undefined ? null : { kind: "sprite", frameId: asset.frames[0].frame_id });
      setMessage("Text sprite updated. Save to write it to the project.");
    } catch (error) {
      setMessage(errorText(error));
    } finally {
      setBusy(null);
    }
  };

  const updateAssetFrameRecord = async (
    asset: AssetRecord,
    frameId: string,
    nextFrame: AssetFrameRecord,
  ) => {
    if (bridge === undefined || project === null || busy !== null) {
      return;
    }
    const currentFrame = asset.frames.find((frame) => frame.frame_id === frameId);
    if (currentFrame === undefined || JSON.stringify(currentFrame) === JSON.stringify(nextFrame)) {
      return;
    }
    const updated: AssetRecord = {
      ...asset,
      frames: asset.frames.map((frame) => (frame.frame_id === frameId ? nextFrame : frame)),
    };
    setBusy("Updating frame");
    setPlaying(false);
    try {
      const result = await bridge.serviceRequest<ProjectCommandResult>("project.apply_commands", {
        project_revision: project.project_revision,
        commands: [{ kind: "asset.upsert", asset: updated }],
      });
      applyProjectResult(result);
      selectAssetRecord({ kind: "sprite", frameId });
      setMessage("Frame updated. Save to write it to the project.");
    } catch (error) {
      setMessage(errorText(error));
    } finally {
      setBusy(null);
    }
  };

  const saveProjectAs = async () => {
    if (bridge === undefined || project === null || projectPath === null || busy !== null) {
      return;
    }
    setBusy("Saving project as");
    try {
      if (dirty) {
        const saved = await bridge.serviceRequest<ProjectSaveResult>("project.save", {
          project_revision: project.project_revision,
        });
        applyProjectResult(saved, { preserveDerivedViews: true });
      }
      const destination = await bridge.saveProjectAs(projectPath, project.source_name);
      if (destination !== null) {
        setTemporaryProject(false);
        await loadProject(destination);
        setMessage(`Saved project as ${destination}`);
      }
    } catch (error) {
      setMessage(errorText(error));
    } finally {
      setBusy(null);
    }
  };

  const renameState = async (sceneId: string, stateId: string, displayName: string) => {
    if (bridge === undefined || project === null || busy !== null) {
      return;
    }
    setBusy("Renaming state");
    setPlaying(false);
    try {
      const result = await bridge.serviceRequest<ProjectCommandResult>("project.apply_commands", {
        project_revision: project.project_revision,
        commands: [
          {
            kind: "state.rename",
            scene_id: sceneId,
            state_id: stateId,
            display_name: displayName,
          },
        ],
      });
      applyProjectResult(result);
      setSceneSelection({ kind: "state", id: stateId });
      setMessage("State renamed. Save to write it to the project.");
    } catch (error) {
      setMessage(errorText(error));
    } finally {
      setBusy(null);
    }
  };

  const renameScene = async (sceneId: string, displayName: string) => {
    if (bridge === undefined || project === null || busy !== null) {
      return;
    }
    setBusy("Renaming scene");
    setPlaying(false);
    try {
      const result = await bridge.serviceRequest<ProjectCommandResult>("project.apply_commands", {
        project_revision: project.project_revision,
        commands: [{ kind: "scene.rename", scene_id: sceneId, display_name: displayName }],
      });
      applyProjectResult(result);
      setSelectedScene(sceneId);
      setSceneSelection({ kind: "scene" });
      setMessage("Scene renamed. Save to write it to the project.");
    } catch (error) {
      setMessage(errorText(error));
    } finally {
      setBusy(null);
    }
  };

  const setPackageEntryScene = async (sceneId: string) => {
    if (bridge === undefined || project === null || busy !== null) {
      return;
    }
    setBusy("Updating package entry");
    setPlaying(false);
    try {
      const result = await bridge.serviceRequest<ProjectCommandResult>("project.apply_commands", {
        project_revision: project.project_revision,
        commands: [{ kind: "project.set_entry_scene", scene_id: sceneId }],
      });
      applyProjectResult(result);
      setSelectedScene(sceneId);
      setSceneSelection({ kind: "packageEntry" });
      setMessage("Package entry connected. Save to write it to the project.");
    } catch (error) {
      setMessage(errorText(error));
    } finally {
      setBusy(null);
    }
  };

  const createState = async (sceneId: string, x: number, y: number) => {
    if (bridge === undefined || project === null || busy !== null) {
      return;
    }
    setBusy("Adding state");
    setPlaying(false);
    try {
      await layoutSaveChain.current;
      const result = await bridge.serviceRequest<ProjectCommandResult>("project.apply_commands", {
        project_revision: projectRevisionRef.current ?? project.project_revision,
        commands: [
          {
            kind: "state.create",
            scene_id: sceneId,
            display_name: "New State",
            x,
            y,
          },
        ],
      });
      const state = result.applied_commands[0]?.state as { state_id?: unknown } | undefined;
      const stateId = typeof state?.state_id === "string" ? state.state_id : null;
      applyProjectResult(result);
      setSelectedScene(sceneId);
      setSceneSelection(stateId === null ? { kind: "scene" } : { kind: "state", id: stateId });
      setMessage("State added. Rename it in the inspector, then save the project.");
    } catch (error) {
      setMessage(errorText(error));
    } finally {
      setBusy(null);
    }
  };

  const setEntryState = async (sceneId: string, stateId: string) => {
    if (bridge === undefined || project === null || busy !== null) {
      return;
    }
    setBusy("Setting start state");
    setPlaying(false);
    try {
      await layoutSaveChain.current;
      const result = await bridge.serviceRequest<ProjectCommandResult>("project.apply_commands", {
        project_revision: projectRevisionRef.current ?? project.project_revision,
        commands: [{ kind: "state.set_entry", scene_id: sceneId, state_id: stateId }],
      });
      applyProjectResult(result);
      setSelectedScene(sceneId);
      setSceneSelection({ kind: "state", id: stateId });
      setMessage("Scene start state updated. Save to write it to the project.");
    } catch (error) {
      setMessage(errorText(error));
    } finally {
      setBusy(null);
    }
  };

  const setEntryConnection = async (
    sceneId: string,
    stateId: string,
    targetHandle: StateGraphEntryHandle,
    targetSide: StateGraphEntrySide,
  ) => {
    if (bridge === undefined || project === null || busy !== null) {
      return;
    }
    setBusy("Moving scene entry");
    setPlaying(false);
    try {
      await layoutSaveChain.current;
      const result = await bridge.serviceRequest<ProjectCommandResult>("project.apply_commands", {
        project_revision: projectRevisionRef.current ?? project.project_revision,
        commands: [
          { kind: "state.set_entry", scene_id: sceneId, state_id: stateId },
          {
            kind: "editor.state_graph.set_entry_layout",
            scene_id: sceneId,
            target_handle: targetHandle,
            target_side: targetSide,
          },
        ],
      });
      applyProjectResult(result);
      setSelectedScene(sceneId);
      setSceneSelection({ kind: "state", id: stateId });
      setMessage("Scene entry moved. Save to write it to the project.");
    } catch (error) {
      setMessage(errorText(error));
    } finally {
      setBusy(null);
    }
  };

  const deleteState = async (sceneId: string, stateId: string) => {
    if (bridge === undefined || project === null || busy !== null) {
      return;
    }
    setBusy("Deleting state");
    setPlaying(false);
    try {
      await layoutSaveChain.current;
      const result = await bridge.serviceRequest<ProjectCommandResult>("project.apply_commands", {
        project_revision: projectRevisionRef.current ?? project.project_revision,
        commands: [{ kind: "state.delete", scene_id: sceneId, state_id: stateId }],
      });
      applyProjectResult(result);
      setSelectedScene(sceneId);
      setSceneSelection({ kind: "scene" });
      setMessage("State deleted. Save to write it to the project.");
    } catch (error) {
      setMessage(errorText(error));
    } finally {
      setBusy(null);
    }
  };

  const deleteSystemExit = async (sceneId: string) => {
    if (bridge === undefined || project === null || busy !== null) {
      return;
    }
    setBusy("Deleting system action");
    try {
      await layoutSaveChain.current;
      const result = await bridge.serviceRequest<ProjectCommandResult>("project.apply_commands", {
        project_revision: projectRevisionRef.current ?? project.project_revision,
        commands: [{ kind: "editor.state_graph.delete_system_exit", scene_id: sceneId }],
      });
      applyProjectResult(result);
      setSceneSelection({ kind: "scene" });
      setMessage("Exit to PeepOS removed. Save to write it to the project.");
    } catch (error) {
      setMessage(errorText(error));
    } finally {
      setBusy(null);
    }
  };

  const createTriggerRoute = async (
    sceneId: string,
    sourceState: string,
    logicalSource: string,
    eventKind: StateTriggerEventKind,
    target: NewStateTransitionTarget,
  ) => {
    if (bridge === undefined || project === null || busy !== null) {
      return;
    }
    setBusy("Adding transition");
    setPlaying(false);
    try {
      const result = await bridge.serviceRequest<ProjectCommandResult>("project.apply_commands", {
        project_revision: project.project_revision,
        commands: [
          {
            kind: "route.create_trigger",
            scene_id: sceneId,
            source_state: sourceState,
            logical_source: logicalSource,
            event_kind: eventKind,
            ...(target.kind === "state"
              ? {
                  target_state: target.stateId,
                  target_handle: target.targetHandle,
                  target_side: target.targetSide,
                }
              : target.kind === "sceneExit"
                ? { scene_exit_ref: target.sceneExitId }
                : { system_exit: true }),
          },
        ],
      });
      const applied = result.applied_commands[0];
      const route = applied?.route as { route_id?: unknown } | undefined;
      const routeId = typeof route?.route_id === "string" ? route.route_id : null;
      applyProjectResult(result);
      setSelectedScene(sceneId);
      setSceneSelection(routeId === null
        ? { kind: "state", id: sourceState }
        : { kind: "route", id: routeId, sourceState });
      setMessage("Transition added. Save to write it to the project.");
    } catch (error) {
      setMessage(errorText(error));
    } finally {
      setBusy(null);
    }
  };

  const rebindTriggerRoute = async (sceneId: string, routeId: string, logicalSource: string) => {
    if (bridge === undefined || project === null || busy !== null) {
      return;
    }
    setBusy("Moving trigger");
    setPlaying(false);
    try {
      const result = await bridge.serviceRequest<ProjectCommandResult>("project.apply_commands", {
        project_revision: projectRevisionRef.current ?? project.project_revision,
        commands: [
          {
            kind: "route.rebind_trigger",
            scene_id: sceneId,
            route_id: routeId,
            logical_source: logicalSource,
          },
        ],
      });
      applyProjectResult(result);
      setSelectedScene(sceneId);
      setMessage("Trigger moved. Save to write it to the project.");
    } catch (error) {
      setMessage(errorText(error));
    } finally {
      setBusy(null);
    }
  };

  const setRouteTarget = async (sceneId: string, routeId: string, targetState: string) => {
    if (bridge === undefined || project === null || busy !== null) {
      return;
    }
    setBusy("Updating route");
    setPlaying(false);
    try {
      const result = await bridge.serviceRequest<ProjectCommandResult>("project.apply_commands", {
        project_revision: project.project_revision,
        commands: [
          {
            kind: "route.set_target",
            scene_id: sceneId,
            route_id: routeId,
            target_state: targetState,
          },
        ],
      });
      applyProjectResult(result);
      setSceneSelection({ kind: "route", id: routeId });
      setMessage("Route target updated. Save to write it to the project.");
    } catch (error) {
      setMessage(errorText(error));
    } finally {
      setBusy(null);
    }
  };

  const setRouteSceneTarget = async (
    sceneId: string,
    routeId: string,
    targetScene: string,
    sceneExitRef?: string,
    sceneFlowReferenceId?: string,
  ) => {
    if (bridge === undefined || project === null || busy !== null) {
      return;
    }
    setBusy("Updating scene exit");
    setPlaying(false);
    try {
      const result = await bridge.serviceRequest<ProjectCommandResult>("project.apply_commands", {
        project_revision: project.project_revision,
        commands: sceneFlowReferenceId === undefined
          ? [
              {
                kind: "route.set_target",
                scene_id: sceneId,
                route_id: routeId,
                target_scene: targetScene,
                ...(sceneExitRef === undefined ? {} : { scene_exit_ref: sceneExitRef }),
              },
              {
                kind: "editor.scene_flow.set_exit_reference",
                scene_id: sceneId,
                endpoint_kind: "route",
                endpoint_id: routeId,
                reference_id: null,
              },
            ]
          : [{
              kind: "editor.scene_flow.set_exit_reference",
              scene_id: sceneId,
              endpoint_kind: "route",
              endpoint_id: routeId,
              reference_id: sceneFlowReferenceId,
            }],
      });
      applyProjectResult(result);
      setSelectedScene(sceneId);
      setSceneSelection({ kind: "route", id: routeId });
      setMessage("Scene exit updated. Save to write it to the project.");
    } catch (error) {
      setMessage(errorText(error));
    } finally {
      setBusy(null);
    }
  };

  const addSceneExit = async (sceneId: string, targetScene: string, referenceId?: string) => {
    if (bridge === undefined || project === null || busy !== null) {
      return;
    }
    setBusy("Adding scene exit");
    setPlaying(false);
    try {
      const result = await bridge.serviceRequest<ProjectCommandResult>("project.apply_commands", {
        project_revision: project.project_revision,
        commands: [
          {
            kind: "scene_exit.add",
            scene_id: sceneId,
            target_scene: targetScene,
            ...(referenceId === undefined ? {} : { scene_flow_reference_id: referenceId }),
          },
        ],
      });
      const applied = result.applied_commands[0];
      applyProjectResult(result);
      setSelectedScene(sceneId);
      const sceneExit = applied?.scene_exit as { scene_exit_id?: unknown } | undefined;
      setSceneSelection({ kind: "sceneExit", id: String(sceneExit?.scene_exit_id ?? "") });
      setMessage("Scene exit added. Save to write it to the project.");
    } catch (error) {
      setMessage(errorText(error));
    } finally {
      setBusy(null);
    }
  };

  const setSceneExitTarget = async (
    sceneId: string,
    sceneExitId: string,
    targetScene: string,
    referenceId?: string,
  ) => {
    if (bridge === undefined || project === null || busy !== null) {
      return;
    }
    setBusy("Updating scene exit");
    setPlaying(false);
    try {
      const result = await bridge.serviceRequest<ProjectCommandResult>("project.apply_commands", {
        project_revision: project.project_revision,
        commands: referenceId === undefined
          ? [
              {
                kind: "scene_exit.set_target",
                scene_id: sceneId,
                scene_exit_id: sceneExitId,
                target_scene: targetScene,
              },
              {
                kind: "editor.scene_flow.set_exit_reference",
                scene_id: sceneId,
                endpoint_kind: "scene_exit",
                endpoint_id: sceneExitId,
                reference_id: null,
              },
            ]
          : [{
              kind: "editor.scene_flow.set_exit_reference",
              scene_id: sceneId,
              endpoint_kind: "scene_exit",
              endpoint_id: sceneExitId,
              reference_id: referenceId,
            }],
      });
      applyProjectResult(result);
      setSelectedScene(sceneId);
      setSceneSelection({ kind: "sceneExit", id: sceneExitId });
      setMessage("Scene exit destination updated. Save to write it to the project.");
    } catch (error) {
      setMessage(errorText(error));
    } finally {
      setBusy(null);
    }
  };

  const deleteSceneExit = async (sceneId: string, sceneExitId: string) => {
    if (bridge === undefined || project === null || busy !== null) {
      return;
    }
    setBusy("Deleting scene exit");
    setPlaying(false);
    try {
      const result = await bridge.serviceRequest<ProjectCommandResult>("project.apply_commands", {
        project_revision: project.project_revision,
        commands: [
          {
            kind: "scene_exit.delete",
            scene_id: sceneId,
            scene_exit_id: sceneExitId,
          },
        ],
      });
      applyProjectResult(result);
      setSelectedScene(sceneId);
      setSceneSelection({ kind: "scene" });
      setMessage("Scene exit deleted. Save to write it to the project.");
    } catch (error) {
      setMessage(errorText(error));
    } finally {
      setBusy(null);
    }
  };

  const deleteLegacySceneRoute = async (sceneId: string, routeId: string) => {
    if (bridge === undefined || project === null || busy !== null) {
      return;
    }
    setBusy("Deleting transition");
    setPlaying(false);
    try {
      const result = await bridge.serviceRequest<ProjectCommandResult>("project.apply_commands", {
        project_revision: project.project_revision,
        commands: (() => {
          const scene = project.document?.scenes?.find(item => item.scene_id === sceneId);
          const eventRef = scene?.routes?.find(route => route.route_id === routeId)?.event_ref;
          return scene && eventRef && scene.routes?.filter(route => route.event_ref === eventRef).length === 1
            && scene.event_bindings?.some(binding => binding.binding_id === eventRef && binding.event_type === STATE_TIMER)
            ? deleteTimerCommands(scene, eventRef) : [{ kind: "route.delete", scene_id: sceneId, route_id: routeId }];
        })(),
      });
      applyProjectResult(result);
      setSelectedScene(sceneId);
      setSceneSelection({ kind: "scene" });
      setMessage("Transition deleted. Save to write it to the project.");
    } catch (error) {
      setMessage(errorText(error));
    } finally {
      setBusy(null);
    }
  };

  const moveSceneNode = async (sceneId: string, x: number, y: number) => {
    if (bridge === undefined || project === null) {
      return;
    }
    setSceneFlowLayoutStatus(`queued ${sceneId} @ ${Math.round(x)}, ${Math.round(y)}`);
    layoutSaveChain.current = layoutSaveChain.current.then(async () => {
      const revision = projectRevisionRef.current;
      if (revision === null) {
        setSceneFlowLayoutStatus(`skipped ${sceneId}: no project revision`);
        return;
      }
      const result = await bridge.serviceRequest<ProjectCommandResult>("project.apply_commands", {
        project_revision: revision,
        commands: [
          {
            kind: "editor.scene_flow.set_node_position",
            scene_id: sceneId,
            x,
            y,
          },
        ],
      });
      projectRevisionRef.current = result.project_revision;
      applyProjectResult(result);
      const applied = result.applied_commands[0];
      setSceneFlowLayoutStatus(
        `saved ${String(applied?.scene_id ?? sceneId)} @ ${String(applied?.x ?? x)}, ${String(applied?.y ?? y)} rev ${result.project_revision}`,
      );
      setMessage("Graph layout updated. Save to write it to the project.");
    }).catch((error) => {
      setSceneFlowLayoutStatus(`save failed: ${errorText(error)}`);
      setMessage(errorText(error));
    });
    await layoutSaveChain.current;
  };

  const setSceneRouteLayout = async (
    sceneId: string,
    endpointKind: "scene_exit" | "route",
    endpointId: string,
    rails: EditorRouteRail[],
  ) => {
    if (bridge === undefined || project === null) {
      return;
    }
    const endpointKey = `${endpointKind}:${endpointId}`;
    const action = rails.length === 0 ? "automatic routing" : "manual routing";
    setSceneFlowLayoutStatus(`queued ${sceneId}.${endpointKey}: ${action}`);
    layoutSaveChain.current = layoutSaveChain.current.then(async () => {
      const revision = projectRevisionRef.current;
      if (revision === null) {
        setSceneFlowLayoutStatus(`skipped ${sceneId}.${endpointKey}: no project revision`);
        return;
      }
      const result = await bridge.serviceRequest<ProjectCommandResult>("project.apply_commands", {
        project_revision: revision,
        commands: [{
          kind: "editor.scene_flow.set_route_layout",
          scene_id: sceneId,
          endpoint_kind: endpointKind,
          endpoint_id: endpointId,
          rails,
        }],
      });
      projectRevisionRef.current = result.project_revision;
      applyProjectResult(result);
      setSceneFlowLayoutStatus(`saved ${sceneId}.${endpointKey}: ${action} rev ${result.project_revision}`);
      setMessage(rails.length === 0
        ? "Scene transition returned to automatic routing. Save to write it to the project."
        : "Scene transition layout updated. Save to write it to the project.");
    }).catch((error) => {
      setSceneFlowLayoutStatus(`save failed: ${errorText(error)}`);
      setMessage(errorText(error));
    });
    await layoutSaveChain.current;
  };

  const movePackageEntryNode = async (x: number, y: number) => {
    if (bridge === undefined || project === null) {
      return;
    }
    setSceneFlowLayoutStatus(`queued package-entry @ ${Math.round(x)}, ${Math.round(y)}`);
    layoutSaveChain.current = layoutSaveChain.current.then(async () => {
      const revision = projectRevisionRef.current;
      if (revision === null) {
        return;
      }
      const result = await bridge.serviceRequest<ProjectCommandResult>("project.apply_commands", {
        project_revision: revision,
        commands: [{ kind: "editor.scene_flow.set_package_entry_position", x, y }],
      });
      projectRevisionRef.current = result.project_revision;
      applyProjectResult(result);
      setSceneFlowLayoutStatus(`saved package-entry @ ${Math.round(x)}, ${Math.round(y)} rev ${result.project_revision}`);
      setMessage("Graph layout updated. Save to write it to the project.");
    }).catch((error) => {
      setSceneFlowLayoutStatus(`save failed: ${errorText(error)}`);
      setMessage(errorText(error));
    });
    await layoutSaveChain.current;
  };

  const moveSceneReferenceNode = async (referenceId: string, x: number, y: number) => {
    if (bridge === undefined || project === null) {
      return;
    }
    setSceneFlowLayoutStatus(`queued ${referenceId} @ ${Math.round(x)}, ${Math.round(y)}`);
    layoutSaveChain.current = layoutSaveChain.current.then(async () => {
      const revision = projectRevisionRef.current;
      if (revision === null) {
        return;
      }
      const result = await bridge.serviceRequest<ProjectCommandResult>("project.apply_commands", {
        project_revision: revision,
        commands: [{
          kind: "editor.scene_flow.set_reference_position",
          reference_id: referenceId,
          x,
          y,
        }],
      });
      projectRevisionRef.current = result.project_revision;
      applyProjectResult(result);
      setSceneFlowLayoutStatus(`saved ${referenceId} @ ${Math.round(x)}, ${Math.round(y)} rev ${result.project_revision}`);
      setMessage("Graph layout updated. Save to write it to the project.");
    }).catch((error) => {
      setSceneFlowLayoutStatus(`save failed: ${errorText(error)}`);
      setMessage(errorText(error));
    });
    await layoutSaveChain.current;
  };

  const addSceneReference = async (targetScene: string, x: number, y: number) => {
    if (bridge === undefined || project === null || busy !== null) {
      return;
    }
    setBusy("Adding scene reference");
    try {
      const result = await bridge.serviceRequest<ProjectCommandResult>("project.apply_commands", {
        project_revision: project.project_revision,
        commands: [{ kind: "editor.scene_flow.add_reference", target_scene: targetScene, x, y }],
      });
      const referenceId = String(result.applied_commands[0]?.reference_id ?? "");
      applyProjectResult(result);
      setSelectedScene(targetScene);
      setSceneSelection({ kind: "sceneReference", id: referenceId });
      setMessage("Go To reference added. Save to write it to the project.");
    } catch (error) {
      setMessage(errorText(error));
    } finally {
      setBusy(null);
    }
  };

  const setSceneReferenceTarget = async (referenceId: string, targetScene: string) => {
    if (bridge === undefined || project === null || busy !== null) {
      return;
    }
    setBusy("Updating scene reference");
    try {
      const result = await bridge.serviceRequest<ProjectCommandResult>("project.apply_commands", {
        project_revision: project.project_revision,
        commands: [{
          kind: "editor.scene_flow.set_reference_target",
          reference_id: referenceId,
          target_scene: targetScene,
        }],
      });
      applyProjectResult(result);
      setSelectedScene(targetScene);
      setSceneSelection({ kind: "sceneReference", id: referenceId });
      setMessage("Go To destination updated. Save to write it to the project.");
    } catch (error) {
      setMessage(errorText(error));
    } finally {
      setBusy(null);
    }
  };

  const deleteSceneReference = async (referenceId: string) => {
    if (bridge === undefined || project === null || busy !== null) {
      return;
    }
    setBusy("Deleting scene reference");
    try {
      const result = await bridge.serviceRequest<ProjectCommandResult>("project.apply_commands", {
        project_revision: project.project_revision,
        commands: [{ kind: "editor.scene_flow.delete_reference", reference_id: referenceId }],
      });
      applyProjectResult(result);
      setSceneSelection({ kind: "scene" });
      setMessage("Go To reference deleted. Scene destinations were preserved.");
    } catch (error) {
      setMessage(errorText(error));
    } finally {
      setBusy(null);
    }
  };

  const moveStateNode = async (sceneId: string, nodeId: string, x: number, y: number) => {
    if (bridge === undefined || project === null) {
      return;
    }
    setStateGraphLayoutStatus(`queued ${sceneId}.${nodeId} @ ${Math.round(x)}, ${Math.round(y)}`);
    layoutSaveChain.current = layoutSaveChain.current.then(async () => {
      const revision = projectRevisionRef.current;
      if (revision === null) {
        setStateGraphLayoutStatus(`skipped ${sceneId}.${nodeId}: no project revision`);
        return;
      }
      const result = await bridge.serviceRequest<ProjectCommandResult>("project.apply_commands", {
        project_revision: revision,
        commands: [
          {
            kind: "editor.state_graph.set_node_position",
            scene_id: sceneId,
            node_id: nodeId,
            x,
            y,
          },
        ],
      });
      projectRevisionRef.current = result.project_revision;
      applyProjectResult(result);
      const applied = result.applied_commands[0];
      setStateGraphLayoutStatus(
        `saved ${String(applied?.scene_id ?? sceneId)}.${String(applied?.node_id ?? nodeId)} @ ${String(applied?.x ?? x)}, ${String(applied?.y ?? y)} rev ${result.project_revision}`,
      );
      setMessage("Logic layout updated. Save to write it to the project.");
    }).catch((error) => {
      setStateGraphLayoutStatus(`save failed: ${errorText(error)}`);
      setMessage(errorText(error));
    });
    await layoutSaveChain.current;
  };

  const setStateRouteLayout = async (
    sceneId: string,
    routeId: string,
    sourceState: string,
    rails: EditorRouteRail[],
    targetHandle: StateGraphEntryHandle | null,
    targetSide: StateGraphEntrySide | null,
    tokenPositions?: EditorRouteTokenPositions,
  ) => {
    if (bridge === undefined || project === null) {
      return;
    }
    const action = rails.length === 0 && targetHandle === null ? "reset" : "manual routing";
    setStateGraphLayoutStatus(`queued ${sceneId}.${routeId}.${sourceState}: ${action}`);
    layoutSaveChain.current = layoutSaveChain.current.then(async () => {
      const revision = projectRevisionRef.current;
      if (revision === null) {
        setStateGraphLayoutStatus(`skipped ${sceneId}.${routeId}: no project revision`);
        return;
      }
      const result = await bridge.serviceRequest<ProjectCommandResult>("project.apply_commands", {
        project_revision: revision,
        commands: [
          {
            kind: "editor.state_graph.set_route_layout",
            scene_id: sceneId,
            route_id: routeId,
            source_state: sourceState,
            rails,
            target_handle: targetHandle,
            target_side: targetSide,
            ...(tokenPositions === undefined ? {} : { token_positions: tokenPositions }),
          },
        ],
      });
      projectRevisionRef.current = result.project_revision;
      applyProjectResult(result);
      setStateGraphLayoutStatus(`saved ${sceneId}.${routeId}.${sourceState}: ${action} rev ${result.project_revision}`);
      setMessage(rails.length === 0 && targetHandle === null
        ? "Transition returned to automatic routing. Save to write it to the project."
        : "Transition layout updated. Save to write it to the project.");
    }).catch((error) => {
      setStateGraphLayoutStatus(`save failed: ${errorText(error)}`);
      setMessage(errorText(error));
    });
    await layoutSaveChain.current;
  };

  const setRouteGuard = async (
    sceneId: string,
    routeId: string,
    guardIndex: number,
    variableRef: string,
    operator: string,
    value: number,
  ) => {
    if (bridge === undefined || project === null || busy !== null) {
      return;
    }
    setBusy("Updating condition");
    setPlaying(false);
    try {
      const result = await bridge.serviceRequest<ProjectCommandResult>("project.apply_commands", {
        project_revision: project.project_revision,
        commands: [
          {
            kind: "route.set_guard",
            scene_id: sceneId,
            route_id: routeId,
            guard_index: guardIndex,
            variable_ref: variableRef,
            operator,
            value,
          },
        ],
      });
      applyProjectResult(result);
      setSceneSelection((current) => current.kind === "route" && current.id === routeId
        ? current
        : { kind: "route", id: routeId });
      setMessage("Condition updated. Save to write it to the project.");
    } catch (error) {
      setMessage(errorText(error));
    } finally {
      setBusy(null);
    }
  };

  const editObjectRouteActions = async (sceneId: string, routeId: string, edit: (actions: Record<string, unknown>[]) => void) => {
    const route = project?.document?.scenes?.find(scene => scene.scene_id === sceneId)?.routes?.find(route => route.route_id === routeId);
    if (route === undefined) return;
    const actions: Record<string, unknown>[] = route.actions.map(action => ({ ...action }));
    edit(actions);
    await applySceneObjectCommands([{ kind: "object_actions.set", scene_id: sceneId,
      owner_kind: "route", owner_id: routeId, actions }]);
  };
  const routeUsesObjects = (sceneId: string) => project?.document?.scenes?.find(scene => scene.scene_id === sceneId)?.schema_version === 2;
  const setRouteAction = async (
    sceneId: string,
    routeId: string,
    actionIndex: number,
    action: Record<string, unknown>,
  ) => {
    if (routeUsesObjects(sceneId)) {
      await editObjectRouteActions(sceneId, routeId, actions => { actions[actionIndex] = action; });
      return;
    }
    if (bridge === undefined || project === null || busy !== null) {
      return;
    }
    setBusy("Updating effect");
    setPlaying(false);
    try {
      const result = await bridge.serviceRequest<ProjectCommandResult>("project.apply_commands", {
        project_revision: project.project_revision,
        commands: [
          {
            kind: "route.set_action",
            scene_id: sceneId,
            route_id: routeId,
            action_index: actionIndex,
            action,
          },
        ],
      });
      applyProjectResult(result);
      setSceneSelection((current) => current.kind === "route" && current.id === routeId
        ? current
        : { kind: "route", id: routeId });
      setMessage("Effect updated. Save to write it to the project.");
    } catch (error) {
      setMessage(errorText(error));
    } finally {
      setBusy(null);
    }
  };

  const addRouteAction = async (
    sceneId: string,
    routeId: string,
    actionIndex: number,
    action: Record<string, unknown>,
  ) => {
    if (routeUsesObjects(sceneId)) {
      await editObjectRouteActions(sceneId, routeId, actions => { actions.splice(actionIndex, 0, action); });
      return;
    }
    if (bridge === undefined || project === null || busy !== null) {
      return;
    }
    setBusy("Adding effect");
    setPlaying(false);
    try {
      const result = await bridge.serviceRequest<ProjectCommandResult>("project.apply_commands", {
        project_revision: project.project_revision,
        commands: [
          {
            kind: "route.action.add",
            scene_id: sceneId,
            route_id: routeId,
            action_index: actionIndex,
            action,
          },
        ],
      });
      applyProjectResult(result);
      setSceneSelection((current) => current.kind === "route" && current.id === routeId
        ? current
        : { kind: "route", id: routeId });
      setMessage("Effect added. Save to write it to the project.");
    } catch (error) {
      setMessage(errorText(error));
    } finally {
      setBusy(null);
    }
  };

  const applyVariableCommand = async (
    busyLabel: string,
    successMessage: string,
    sceneId: string,
    command: Record<string, unknown>,
  ) => {
    if (bridge === undefined || project === null || busy !== null) {
      return;
    }
    setBusy(busyLabel);
    setPlaying(false);
    try {
      const result = await bridge.serviceRequest<ProjectCommandResult>("project.apply_commands", {
        project_revision: project.project_revision,
        commands: [{ scene_id: sceneId, ...command }],
      });
      applyProjectResult(result);
      setSceneSelection({ kind: "scene" });
      setMessage(`${successMessage} Save to write it to the project.`);
    } catch (error) {
      setMessage(errorText(error));
    } finally {
      setBusy(null);
    }
  };

  const addVariable = async (sceneId: string, variable: StateVariable) => applyVariableCommand(
    "Adding variable",
    "Variable added.",
    sceneId,
    { kind: "variable.add", variable },
  );

  const updateVariable = async (sceneId: string, variable: StateVariable) => applyVariableCommand(
    "Updating variable",
    "Variable updated.",
    sceneId,
    { kind: "variable.update", variable },
  );

  const deleteVariable = async (sceneId: string, variableId: string) => applyVariableCommand(
    "Deleting variable",
    "Variable deleted.",
    sceneId,
    { kind: "variable.delete", variable_id: variableId },
  );

  const applyRouteListCommand = async (
    busyLabel: string,
    successMessage: string,
    sceneId: string,
    routeId: string,
    command: Record<string, unknown>,
  ) => {
    if (bridge === undefined || project === null || busy !== null) {
      return;
    }
    setBusy(busyLabel);
    setPlaying(false);
    try {
      const result = await bridge.serviceRequest<ProjectCommandResult>("project.apply_commands", {
        project_revision: project.project_revision,
        commands: [{ scene_id: sceneId, route_id: routeId, ...command }],
      });
      applyProjectResult(result);
      setSceneSelection((current) => current.kind === "route" && current.id === routeId
        ? current
        : { kind: "route", id: routeId });
      setMessage(`${successMessage} Save to write it to the project.`);
    } catch (error) {
      setMessage(errorText(error));
    } finally {
      setBusy(null);
    }
  };

  const addRouteGuard = async (
    sceneId: string,
    routeId: string,
    guardIndex: number,
    guard: Record<string, unknown>,
  ) => applyRouteListCommand(
    "Adding condition",
    "Condition added.",
    sceneId,
    routeId,
    { kind: "route.guard.add", guard_index: guardIndex, guard },
  );

  const deleteRouteGuard = async (sceneId: string, routeId: string, guardIndex: number) =>
    applyRouteListCommand(
      "Deleting condition",
      "Condition deleted.",
      sceneId,
      routeId,
      { kind: "route.guard.delete", guard_index: guardIndex },
    );

  const moveRouteGuard = async (
    sceneId: string,
    routeId: string,
    guardIndex: number,
    targetIndex: number,
  ) => applyRouteListCommand(
    "Reordering conditions",
    "Condition order updated.",
    sceneId,
    routeId,
    { kind: "route.guard.move", guard_index: guardIndex, target_index: targetIndex },
  );

  const deleteRouteAction = async (sceneId: string, routeId: string, actionIndex: number) => {
    if (routeUsesObjects(sceneId)) {
      await editObjectRouteActions(sceneId, routeId, actions => { actions.splice(actionIndex, 1); });
      return;
    }
    await applyRouteListCommand(
      "Deleting effect",
      "Effect deleted.",
      sceneId,
      routeId,
      { kind: "route.action.delete", action_index: actionIndex },
    );
  };

  const moveRouteAction = async (
    sceneId: string,
    routeId: string,
    actionIndex: number,
    targetIndex: number,
  ) => {
    if (routeUsesObjects(sceneId)) {
      await editObjectRouteActions(sceneId, routeId, actions => {
        const [action] = actions.splice(actionIndex, 1);
        if (action !== undefined) actions.splice(targetIndex, 0, action);
      });
      return;
    }
    await applyRouteListCommand(
    "Reordering effects",
    "Effect order updated.",
    sceneId,
    routeId,
    { kind: "route.action.move", action_index: actionIndex, target_index: targetIndex },
  );
  };

  const applyRenderElementCommand = async (
    busyLabel: string,
    sceneId: string,
    renderModelId: string,
    elementId: string | null,
    command: Record<string, unknown>,
    successMessage: string,
    nextSelectedElementId: string | null | undefined = elementId,
  ) => {
    if (bridge === undefined || project === null || busy !== null) {
      return;
    }
    setBusy(busyLabel);
    setPlaying(false);
    try {
      const result = await bridge.serviceRequest<ProjectCommandResult>("project.apply_commands", {
        project_revision: project.project_revision,
        commands: [
          {
            scene_id: sceneId,
            render_model_id: renderModelId,
            ...(elementId === null ? {} : { element_id: elementId }),
            ...command,
          },
        ],
      });
      applyProjectResult(result);
      setSelectedScene(sceneId);
      if (nextSelectedElementId !== undefined) {
        setSelectedPlacementElement(nextSelectedElementId);
      }
      setSceneSelection({ kind: "render", id: renderModelId });
      if (result.valid) {
        const previewResult = await bridge.serviceRequest<PreviewSnapshot>("project.preview_reset", {
          project_revision: result.project_revision,
          scene_id: sceneId,
        });
        setPreview(previewResult);
      }
      setMessage(successMessage);
    } catch (error) {
      setMessage(errorText(error));
    } finally {
      setBusy(null);
    }
  };
  const applyPlacementCommandBatch = async (
    busyLabel: string,
    sceneId: string,
    renderModelId: string,
    elementId: string,
    commands: Record<string, unknown>[],
    successMessage: string,
    nextSelection: SceneSelection = { kind: "render", id: renderModelId },
  ) => {
    if (bridge === undefined || project === null || busy !== null || commands.length === 0) {
      return;
    }
    setBusy(busyLabel);
    setPlaying(false);
    try {
      const result = await bridge.serviceRequest<ProjectCommandResult>("project.apply_commands", {
        project_revision: project.project_revision,
        commands,
      });
      applyProjectResult(result);
      setSelectedScene(sceneId);
      setSelectedPlacementElement(elementId);
      setSceneSelection(nextSelection);
      if (result.valid) {
        const previewResult = await bridge.serviceRequest<PreviewSnapshot>("project.preview_reset", {
          project_revision: result.project_revision,
          scene_id: sceneId,
        });
        setPreview(previewResult);
      }
      setMessage(successMessage);
    } catch (error) {
      setPlacementDraftPositions({});
      setMessage(errorText(error));
    } finally {
      setBusy(null);
    }
  };
  const placementEditStateTargets = () => {
    const states = selectedSceneDocument?.states ?? [];
    const validIds = new Set(states.map((state) => state.state_id));
    return placementEditStateIds.filter((stateId) => validIds.has(stateId));
  };
  const applySceneObjectCommands = async (commands: Record<string, unknown>[]) => {
    if (bridge === undefined || project === null || busy !== null || selectedSceneDocument === null || commands.length === 0
      || commands.some(command => command.scene_id !== selectedSceneDocument.scene_id
        || !supportsObjectCommand(service, selectedSceneCapability, String(command.kind)))) return false;
    setBusy("Updating scene object");
    setPlaying(false);
    stopAudioPlayback();
    try {
      const result = await bridge.serviceRequest<ProjectCommandResult>("project.apply_commands", {
        project_revision: project.project_revision, commands,
      });
      applyProjectResult(result);
      setMessage("Scene object updated. Save to write it to the project.");
      return true;
    } catch (error) {
      setMessage(errorText(error));
      return false;
    } finally { setBusy(null); }
  };
  const sceneObjectPropertyCommands = (elementId: string, properties: Record<string, number | boolean>) => {
    if (selectedSceneDocument === null) return [];
    const common = { scene_id: selectedSceneDocument.scene_id, object_id: elementId, properties };
    const ids = placementEditStateTargets();
    return ids.length === 0 ? [{ kind: "object.set_defaults", ...common }]
      : ids.map(state_id => ({ kind: "object_override.set", ...common, state_id }));
  };
  const placementAnimationStateTargets = () => {
    const explicitStateIds = placementEditStateTargets();
    return explicitStateIds.length > 0
      ? explicitStateIds
      : (selectedSceneDocument?.states ?? []).map((state) => state.state_id);
  };
  const placementBaseElement = (elementId: string) => (
    placementRenderModel?.elements.find((element) => element.element_id === elementId) ?? null
  );
  const placementResolvedElement = (stateId: string, elementId: string) => (
    placementOwnershipScene?.states[stateId]?.resolved_elements.find(
      (element) => element.element_id === elementId,
    ) ?? null
  );
  const placementEditTargetLabel = () => {
    const targetStateIds = placementEditStateTargets();
    if (targetStateIds.length === 0) {
      return "Base Placement";
    }
    if (targetStateIds.length === 1) {
      return (selectedSceneDocument?.states ?? []).find((state) => state.state_id === targetStateIds[0])?.display_name ?? targetStateIds[0];
    }
    return `${targetStateIds.length} states`;
  };
  const placementObjectScope = () => {
    const stateIds = placementEditStateTargets();
    return stateIds.length === 0
      ? { kind: "scene_base" }
      : { kind: "states", state_ids: stateIds };
  };
  const scopedPositionCommands = (
    element: RenderElement,
    renderModelId: string,
    x: number,
    y: number,
  ) => {
    if (selectedSceneDocument === null) {
      return [];
    }
    const baseElement = placementBaseElement(element.element_id) ?? element;
    const targetStateIds = placementEditStateTargets();
    if (targetStateIds.length === 0) {
      if (baseElement.x === x && baseElement.y === y) {
        return [];
      }
      return [
        {
          kind: "render_element.set_position",
          scene_id: selectedSceneDocument.scene_id,
          render_model_id: renderModelId,
          element_id: element.element_id,
          x,
          y,
        },
      ];
    }
    const unchanged = targetStateIds.every((stateId) => {
      const resolved = placementResolvedElement(stateId, element.element_id) ?? baseElement;
      return resolved.x === x && resolved.y === y;
    });
    if (unchanged) {
      return [];
    }
    return targetStateIds.map((stateId) => ({
      kind: "state_placement.set_override",
      scene_id: selectedSceneDocument.scene_id,
      state_id: stateId,
      render_model_id: renderModelId,
      element_id: element.element_id,
      x,
      y,
    }));
  };
  const scopedVisibilityCommands = (
    element: RenderElement,
    renderModelId: string,
    visible: boolean,
  ) => {
    if (selectedSceneDocument === null) {
      return [];
    }
    const baseElement = placementBaseElement(element.element_id) ?? element;
    const targetStateIds = placementEditStateTargets();
    if (targetStateIds.length === 0) {
      if ((baseElement.visible ?? true) === visible) {
        return [];
      }
      return [
        {
          kind: "render_element.set_visibility",
          scene_id: selectedSceneDocument.scene_id,
          render_model_id: renderModelId,
          element_id: element.element_id,
          visible,
        },
      ];
    }
    const unchanged = targetStateIds.every((stateId) => {
      const resolved = placementResolvedElement(stateId, element.element_id) ?? baseElement;
      return (resolved.visible ?? true) === visible;
    });
    if (unchanged) {
      return [];
    }
    return targetStateIds.map((stateId) => ({
      kind: "state_placement.set_override",
      scene_id: selectedSceneDocument.scene_id,
      state_id: stateId,
      render_model_id: renderModelId,
      element_id: element.element_id,
      visible,
    }));
  };
  const movePlacementElement = (
    element: RenderElement,
    renderModelId: string | null,
    x: number,
    y: number,
  ) => {
    if (selectedSceneDocument === null) {
      return;
    }
    const nextX = Math.min(Math.max(0, 168 - element.width), Math.max(0, Math.round(x)));
    const nextY = Math.min(Math.max(0, 144 - element.height), Math.max(0, Math.round(y)));
    if (objectSceneSelected) {
      const properties: Record<string, number> = {};
      if (nextX !== element.x) properties.x = nextX;
      if (nextY !== element.y) properties.y = nextY;
      if (Object.keys(properties).length > 0) void applySceneObjectCommands(sceneObjectPropertyCommands(element.element_id, properties));
      return;
    }
    if (renderModelId === null) return;
    const commands = scopedPositionCommands(element, renderModelId, nextX, nextY);
    if (commands.length === 0) {
      return;
    }
    const targetStateIds = placementEditStateTargets();
    void applyPlacementCommandBatch(
      "Moving object",
      selectedSceneDocument.scene_id,
      renderModelId,
      element.element_id,
      commands,
      `Position updated for ${placementEditTargetLabel()}. Save to write it to the project.`,
      targetStateIds.length === 1 ? { kind: "state", id: targetStateIds[0] } : { kind: "render", id: renderModelId },
    );
  };
  const resizePlacementElement = (
    element: RenderElement,
    renderModelId: string,
    bounds: { x: number; y: number; width: number; height: number },
  ) => {
    if (selectedSceneDocument === null) {
      return;
    }
    const { x: nextX, y: nextY, width: nextWidth, height: nextHeight } = normalizePrimitiveBounds(element.kind as PlacementPrimitiveKind, bounds);
    if (nextX === element.x && nextY === element.y && nextWidth === element.width && nextHeight === element.height) {
      return;
    }
    void applyRenderElementCommand(
      "Resizing element",
      selectedSceneDocument.scene_id,
      renderModelId,
      element.element_id,
      {
        kind: "render_element.set_bounds",
        x: nextX,
        y: nextY,
        width: nextWidth,
        height: nextHeight,
      },
      "Element resized. Save to write it to the project.",
    );
  };
  const deletePlacementElement = (element: RenderElement, renderModelId: string | null) => {
    if (selectedSceneDocument === null) {
      return;
    }
    const targetStateIds = placementEditStateTargets();
    if (objectSceneSelected) {
      void applySceneObjectCommands(targetStateIds.length > 0
        ? sceneObjectPropertyCommands(element.element_id, { visible: false })
        : [{ kind: "object.delete", scene_id: selectedSceneDocument.scene_id, object_id: element.element_id }]);
      return;
    }
    if (renderModelId === null) return;
    if (targetStateIds.length > 0) {
      const commands = scopedVisibilityCommands(element, renderModelId, false);
      if (commands.length === 0) {
        return;
      }
      void applyPlacementCommandBatch(
        "Removing object from states",
        selectedSceneDocument.scene_id,
        renderModelId,
        element.element_id,
        commands,
        `Object removed from ${placementEditTargetLabel()}. Save to write it to the project.`,
        placementState === null
          ? { kind: "render", id: renderModelId }
          : { kind: "state", id: placementState.state_id },
      );
      return;
    }
    void applyRenderElementCommand(
      "Deleting object",
      selectedSceneDocument.scene_id,
      renderModelId,
      element.element_id,
      { kind: "render_element.delete" },
      "Object deleted. Save to write it to the project.",
      null,
    );
  };
  const setPlacementElementZOrder = (element: RenderElement, renderModelId: string, zOrder: number) => {
    if (selectedSceneDocument === null) {
      return;
    }
    const nextZOrder = Math.min(255, Math.max(0, Math.round(zOrder)));
    if (nextZOrder === element.z_order) {
      return;
    }
    void applyRenderElementCommand(
      "Changing draw order",
      selectedSceneDocument.scene_id,
      renderModelId,
      element.element_id,
      { kind: "render_element.set_z_order", z_order: nextZOrder },
      "Draw order updated. Save to write it to the project.",
    );
  };

  const exportPackage = async () => {
    if (bridge === undefined || build === null || busy !== null) {
      return;
    }
    const exported = await bridge.exportEgg(
      `${build.package.package_id}.egg`,
      build.package.blob_base64,
    );
    if (exported !== null) {
      setMessage(`Exported ${exported}`);
    }
  };

  const scenes: SceneDocument[] = project?.document?.scenes ?? [];
  const assets: AssetRecord[] = project?.document?.assets ?? [];
  const audioAssets: AudioAssetRecord[] = project?.document?.audio_assets ?? [];
  const audioCues: AudioCueRecord[] = project?.document?.audio_cues ?? [];
  const compiledAssetFrames: CompiledAssetFrame[] = project?.document?.compiled_asset_frames ?? [];
  const compiledAssetFrameById = useMemo(
    () => new Map(compiledAssetFrames.map((frame) => [frame.frame_id, frame])),
    [compiledAssetFrames],
  );
  const assetById = useMemo(
    () => new Map(assets.map((asset) => [asset.asset_id, asset])),
    [assets],
  );
  const sourceFrameById = useMemo(() => {
    const frames = new Map<string, { asset: AssetRecord; frame: AssetFrameRecord }>();
    for (const asset of assets) {
      for (const frame of asset.frames ?? []) {
        frames.set(frame.frame_id, { asset, frame });
      }
    }
    return frames;
  }, [assets]);
  const compiledAssetFrameGroups = useMemo(() => {
    const groups = new Map<string, CompiledAssetFrame[]>();
    for (const frame of compiledAssetFrames) {
      groups.set(frame.asset_id, [...(groups.get(frame.asset_id) ?? []), frame]);
    }
    return [...groups.entries()].map(([assetId, frames]) => ({ assetId, frames }));
  }, [compiledAssetFrames]);
  const audioAssetById = useMemo(
    () => new Map(audioAssets.map((asset) => [asset.asset_id, asset])),
    [audioAssets],
  );
  const selectedAudioCue = assetSelection?.kind === "audio"
    ? audioCues.find((cue) => cue.cue_id === assetSelection.cueId) ?? null
    : null;
  const selectedAudioAsset = selectedAudioCue === null ? null : audioAssetById.get(selectedAudioCue.asset_ref) ?? null;
  const audioCueDisplayName = (cue: AudioCueRecord) => cue.display_name?.trim() || cue.cue_id;
  const selectedAssetFrame = assetSelection?.kind === "sprite"
    ? compiledAssetFrameById.get(assetSelection.frameId) ?? null
    : null;
  const selectedAssetFrames = selectedAssetFrame === null
    ? []
    : compiledAssetFrameGroups.find((group) => group.assetId === selectedAssetFrame.asset_id)?.frames ?? [];
  const animatedAssetPreviewFrame = selectedAssetFrames.length === 0
    ? selectedAssetFrame
    : selectedAssetFrames[assetPreviewPlaying ? assetPreviewStep % selectedAssetFrames.length : selectedAssetFrames.findIndex((frame) => frame.frame_id === selectedAssetFrame?.frame_id)] ?? selectedAssetFrame;
  const projectRevision = project?.project_revision ?? null;
  const projectValid = project?.valid ?? false;
  const thumbnailsSupported = service?.operations.includes("project.scene_thumbnails") === true;
  const placementPreviewSupported = service?.operations.includes("project.preview_state") === true;
  const placementBasePreviewSupported = service?.operations.includes("project.preview_scene_base") === true;
  const waitingAnimationCommands = service?.state_scene_presentation.waiting_animation.commands ?? [];
  const placementAnimationSupported =
    waitingAnimationCommands.includes("render_element.bind_waiting_animation") &&
    waitingAnimationCommands.includes("render_element.clear_waiting_animation");

  useEffect(() => {
    const validIds = new Set(scenes.map((scene) => scene.scene_id));
    setExpandedSceneIds((current) => {
      const next = current.filter((sceneId) => validIds.has(sceneId));
      if (selectedScene !== null && validIds.has(selectedScene) && !next.includes(selectedScene)) {
        next.push(selectedScene);
      }
      return next.length === current.length && next.every((sceneId, index) => sceneId === current[index])
        ? current
        : next;
    });
  }, [scenes, selectedScene]);

  useEffect(() => {
    if (bridge === undefined || projectRevision === null || !projectValid || !thumbnailsSupported) {
      setSceneThumbnails({});
      return undefined;
    }

    let cancelled = false;
    bridge
      .serviceRequest<ProjectSceneThumbnailsResult>("project.scene_thumbnails", {
        project_revision: projectRevision,
      })
      .then((result) => {
        if (cancelled || result.project_revision !== projectRevision) {
          return;
        }
        setSceneThumbnails(Object.fromEntries(result.thumbnails.map((item) => [item.scene_id, item.framebuffer])));
      })
      .catch((error) => {
        if (!cancelled) {
          setSceneThumbnails({});
          setMessage(errorText(error));
        }
      });

    return () => {
      cancelled = true;
    };
  }, [bridge, projectRevision, projectValid, thumbnailsSupported]);

  useEffect(() => {
    if (assetSelection?.kind === "audio" && !audioCues.some((cue) => cue.cue_id === assetSelection.cueId)) {
      setAssetSelection(null);
    }
  }, [assetSelection, audioCues]);
  const selectedSceneDocument = useMemo(
    () => scenes.find((scene) => scene.scene_id === selectedScene) ?? null,
    [scenes, selectedScene],
  );

  const selectedSceneCapability = selectedScene === null ? undefined : project?.scene_capabilities?.[selectedScene];
  const objectSceneSelected = usesSceneObjects(selectedSceneDocument, selectedSceneCapability);
  const scopedPlacementAddSupported = objectSceneSelected
    ? supportsObjectCommand(service, selectedSceneCapability, "object.add")
    : service?.state_scene_presentation.element_commands.includes("placement_object.add") === true;
  const sceneObjectMoveSupported = objectSceneSelected && supportsObjectCommand(service, selectedSceneCapability,
    placementEditStateIds.length === 0 ? "object.set_defaults" : "object_override.set");
  const canEditSelectedScene = canEditLegacyScene(selectedSceneDocument, selectedSceneCapability)
    && service?.operations.includes("project.apply_commands") === true && busy === null;
  const stateCommandAllowed = (command: string) => objectSceneSelected
    ? busy === null && supportsStateManagement(service, selectedSceneCapability, command)
    : canEditSelectedScene;
  const localCommandAllowed = (command: string) => objectSceneSelected
    ? busy === null && supportsLocalGraphCommand(service, selectedSceneCapability, command)
    : canEditSelectedScene;
  const canEditLocalGraph = ["route.create_trigger", "route.rebind_trigger", "editor.state_graph.set_route_layout",
    "editor.state_graph.delete_system_exit"].every(localCommandAllowed)
    && (!objectSceneSelected || ["state", "system_exit"].every(kind =>
      service?.scene_object_authoring?.route_destination_kinds?.includes(kind)
      && selectedSceneCapability?.route_destination_kinds?.includes(kind)));
  const readOnlySceneIds = useMemo(() => scenes.filter((scene) =>
    !canEditLegacyScene(scene, project?.scene_capabilities?.[scene.scene_id])).map((scene) => scene.scene_id),
  [scenes, project?.scene_capabilities]);
  const hostOnlyProject = scenes.some((scene) => usesSceneObjects(scene, project?.scene_capabilities?.[scene.scene_id])
    && project?.scene_capabilities?.[scene.scene_id]?.egg_export !== true);

  useEffect(() => {
    if (!projectValid || scenes.length === 0) {
      return;
    }
    if (selectedScene !== null && scenes.some((scene) => scene.scene_id === selectedScene)) {
      return;
    }
    const fallbackScene = scenes.find((scene) => scene.scene_id === project?.summary.entry_scene) ?? scenes[0];
    setSelectedScene(fallbackScene.scene_id);
    setSceneSelection({ kind: "scene" });
    setSelectedPlacementElement(null);
  }, [project?.summary.entry_scene, projectValid, scenes, selectedScene]);

  useEffect(() => {
    if (
      bridge === undefined ||
      busy !== null ||
      projectRevision === null ||
      !projectValid ||
      selectedSceneDocument === null
    ) {
      return undefined;
    }
    if (preview !== null) {
      previewRestoreAttemptRef.current = null;
      return undefined;
    }

    const restoreKey = `${projectRevision}:${selectedSceneDocument.scene_id}`;
    if (previewRestoreAttemptRef.current === restoreKey) {
      return undefined;
    }
    const timeout = window.setTimeout(() => {
      if (previewRef.current !== null) {
        return;
      }
      previewRestoreAttemptRef.current = restoreKey;
      const rememberedStart = previewStartRef.current;
      const rememberedStateId = rememberedStart?.sceneId === selectedSceneDocument.scene_id
        && rememberedStart.stateId !== undefined
        && (selectedSceneDocument.states ?? []).some((state) => state.state_id === rememberedStart.stateId)
        ? rememberedStart.stateId
        : undefined;
      void startPreview(selectedSceneDocument.scene_id, {
        revision: projectRevision,
        stateId: rememberedStateId,
        updateSelection: false,
      });
    }, 80);
    return () => window.clearTimeout(timeout);
  }, [bridge, busy, preview, projectRevision, projectValid, selectedSceneDocument, startPreview]);

  const placementState = useMemo<StateRecord | null>(() => {
    if (selectedSceneDocument === null) {
      return null;
    }
    return (selectedSceneDocument.states ?? []).find((state) => state.state_id === placementStateId) ?? null;
  }, [placementStateId, selectedSceneDocument]);
  const placementRenderModel = useMemo<RenderModel | null>(() => {
    if (selectedSceneDocument === null) {
      return null;
    }
    return selectedSceneDocument.render_models?.[0] ?? null;
  }, [selectedSceneDocument]);
  const placementOwnershipScene = useMemo(() => {
    if (selectedSceneDocument === null) {
      return null;
    }
    return project?.placement_ownership?.scenes[selectedSceneDocument.scene_id] ?? null;
  }, [project?.placement_ownership, selectedSceneDocument]);
  const placementStateProjection = useMemo(() => {
    if (placementState === null || placementOwnershipScene === null) {
      return null;
    }
    return placementOwnershipScene.states[placementState.state_id] ?? null;
  }, [placementOwnershipScene, placementState]);
  const effectivePlacementElements = useMemo<RenderElement[]>(() => {
    if (objectSceneSelected && selectedSceneDocument !== null) {
      return placementStateProjection?.resolved_elements ?? baseObjectRows(selectedSceneDocument, placementOwnershipScene);
    }
    if (placementRenderModel === null) {
      return [];
    }
    if (
      placementStateProjection === null ||
      placementOwnershipScene?.render_model_id !== placementRenderModel.visual_id
    ) {
      return placementRenderModel.elements;
    }
    return placementStateProjection.resolved_elements;
  }, [objectSceneSelected, selectedSceneDocument, placementOwnershipScene, placementRenderModel, placementStateProjection]);
  useEffect(() => {
    const sceneId = selectedSceneDocument?.scene_id ?? null;
    const stateId = placementState?.state_id ?? null;
    const previewSupported = (stateId === null ? placementBasePreviewSupported : placementPreviewSupported)
      && (!objectSceneSelected || canPreviewSceneObjects(service, selectedSceneCapability));
    const readyForPlacementPreview =
      bridge !== undefined &&
      projectRevision !== null &&
      projectValid &&
      workspaceMode === "placement" &&
      sceneId !== null;
    if (
      !readyForPlacementPreview ||
      !previewSupported ||
      projectRevision === null ||
      sceneId === null
    ) {
      setPlacementPreview(null);
      setPlacementPreviewLoading(false);
      setPlacementPreviewError(
        readyForPlacementPreview && !previewSupported
          ? "The connected host does not advertise preview for this placement scope."
          : null,
      );
      return undefined;
    }

    let cancelled = false;
    setPlacementPreviewLoading(true);
    setPlacementPreviewError(null);
    const operation = stateId === null ? "project.preview_scene_base" : "project.preview_state";
    bridge
      .serviceRequest<PlacementPreviewSnapshot>(operation, {
        project_revision: projectRevision,
        scene_id: sceneId,
        ...(stateId === null ? {} : { state_id: stateId }),
      })
      .then((result) => {
        const matchesScope = stateId === null
          ? "placement" in result && result.placement.scene_id === sceneId
          : "scene" in result && result.scene.scene_id === sceneId && result.scene.state_id === stateId;
        if (
          cancelled ||
          result.project_revision !== projectRevision ||
          !matchesScope
        ) {
          return;
        }
        setPlacementPreview(result);
        setPlacementPreviewLoading(false);
        setPlacementPreviewError(null);
      })
      .catch((error) => {
        if (!cancelled) {
          const text = errorText(error);
          setPlacementPreview(null);
          setPlacementPreviewLoading(false);
          setMessage(text);
          setPlacementPreviewError(text);
        }
      });

    return () => {
      cancelled = true;
    };
  }, [
    bridge,
    objectSceneSelected,
    selectedSceneCapability,
    service,
    placementBasePreviewSupported,
    placementPreviewSupported,
    placementState?.state_id,
    projectRevision,
    projectValid,
    selectedSceneDocument?.scene_id,
    service?.service_api_version,
    workspaceMode,
  ]);
  useEffect(() => {
    const sceneId = selectedSceneDocument?.scene_id ?? null;
    const sceneKey = sceneId === null ? null : `${project?.summary.project_id ?? ""}:${sceneId}`;
    if (placementSceneRef.current === sceneKey) {
      return;
    }
    placementSceneRef.current = sceneKey;
    placementSelectionAnchorRef.current = null;
    setPlacementStateId(null);
    setPlacementEditStateIds([]);
    setSelectedPlacementElement(null);
  }, [project?.summary.project_id, selectedSceneDocument?.scene_id]);
  useEffect(() => {
    if (selectedSceneDocument === null) {
      if (placementEditStateIds.length > 0) {
        setPlacementEditStateIds([]);
      }
      if (placementStateId !== null) {
        setPlacementStateId(null);
      }
      return;
    }
    const states = selectedSceneDocument.states ?? [];
    const validIds = new Set(states.map((state) => state.state_id));
    setPlacementEditStateIds((current) => {
      const next = current.filter((stateId) => validIds.has(stateId));
      return next.length === current.length && next.every((stateId, index) => stateId === current[index])
        ? current
        : next;
    });
    if (placementStateId !== null && !validIds.has(placementStateId)) {
      setPlacementStateId(placementEditStateIds.find((stateId) => validIds.has(stateId)) ?? null);
    }
  }, [placementEditStateIds, placementStateId, selectedSceneDocument]);
  const selectedPlacementRenderElement = useMemo<RenderElement | null>(() => {
    if (selectedPlacementElement === null) {
      return null;
    }
    return effectivePlacementElements.find((element) => element.element_id === selectedPlacementElement) ?? null;
  }, [effectivePlacementElements, selectedPlacementElement]);
  useEffect(() => {
    if (
      selectedPlacementElement !== null &&
      effectivePlacementElements.some((element) => element.element_id === selectedPlacementElement) !== true
    ) {
      setSelectedPlacementElement(null);
    }
  }, [effectivePlacementElements, selectedPlacementElement]);
  useEffect(() => {
    if (assetSelection?.kind === "sprite" && !compiledAssetFrameById.has(assetSelection.frameId)) {
      setAssetSelection(null);
    }
  }, [assetSelection, compiledAssetFrameById]);
  useEffect(() => {
    setAssetPreviewStep(0);
    setAssetPreviewPlaying(false);
  }, [selectedAssetFrame?.asset_id]);
  useEffect(() => {
    if (!assetPreviewPlaying || workspaceMode !== "assets" || selectedAssetFrames.length <= 1) {
      return undefined;
    }
    const timer = window.setInterval(() => {
      setAssetPreviewStep((step) => step + 1);
    }, 250);
    return () => window.clearInterval(timer);
  }, [assetPreviewPlaying, selectedAssetFrames.length, workspaceMode]);
  useEffect(() => {
    setSpritePickerOpen(false);
  }, [selectedScene, workspaceMode]);
  const connected = service !== null;
  const startProjectResize = (event: ReactPointerEvent<HTMLDivElement>) => {
    event.preventDefault();
    const startX = event.clientX;
    const startWidth = projectWidth;
    const move = (moveEvent: PointerEvent) => {
      const next = Math.min(520, Math.max(260, startWidth + (moveEvent.clientX - startX)));
      setProjectWidth(next);
    };
    const stop = () => {
      window.removeEventListener("pointermove", move);
      window.removeEventListener("pointerup", stop);
    };
    window.addEventListener("pointermove", move);
    window.addEventListener("pointerup", stop);
  };
  const startInspectorResize = (event: ReactPointerEvent<HTMLDivElement>) => {
    event.preventDefault();
    const startX = event.clientX;
    const startWidth = inspectorWidth;
    const move = (moveEvent: PointerEvent) => {
      const next = Math.min(560, Math.max(320, startWidth - (moveEvent.clientX - startX)));
      setInspectorWidth(next);
    };
    const stop = () => {
      window.removeEventListener("pointermove", move);
      window.removeEventListener("pointerup", stop);
    };
    window.addEventListener("pointermove", move);
    window.addEventListener("pointerup", stop);
  };
  const placementDraftKey = (renderModelId: string | null, elementId: string) => `${renderModelId ?? `objects:${selectedScene}`}:${elementId}`;
  const handlePlacementKeyDown = (event: ReactKeyboardEvent<HTMLElement>) => {
    if ((!objectSceneSelected && placementRenderModel === null) || selectedPlacementRenderElement === null || busy !== null) {
      return;
    }
    const target = event.target as HTMLElement | null;
    if (target !== null && target.closest("input, select, textarea, [contenteditable='true']") !== null) {
      return;
    }
    if (event.key === "Delete" || event.key === "Backspace") {
      event.preventDefault();
      deletePlacementElement(selectedPlacementRenderElement, placementRenderModel?.visual_id ?? null);
      return;
    }
    const step = event.shiftKey ? 8 : 1;
    let dx = 0;
    let dy = 0;
    if (event.key === "ArrowLeft") {
      dx = -step;
    } else if (event.key === "ArrowRight") {
      dx = step;
    } else if (event.key === "ArrowUp") {
      dy = -step;
    } else if (event.key === "ArrowDown") {
      dy = step;
    } else {
      return;
    }
    event.preventDefault();
    movePlacementElement(
      selectedPlacementRenderElement,
      placementRenderModel?.visual_id ?? null,
      selectedPlacementRenderElement.x + dx,
      selectedPlacementRenderElement.y + dy,
    );
  };
  const startPlacementDrag = (
    event: ReactPointerEvent<HTMLButtonElement>,
    element: RenderElement,
    renderModelId: string | null,
  ) => {
    if (selectedSceneDocument === null || busy !== null || (objectSceneSelected && !sceneObjectMoveSupported)) {
      return;
    }
    event.preventDefault();
    event.stopPropagation();
    const overlay = event.currentTarget.closest(".placement-screen-overlay");
    if (!(overlay instanceof HTMLElement)) {
      return;
    }
    const rect = overlay.getBoundingClientRect();
    const startClientX = event.clientX;
    const startClientY = event.clientY;
    const key = placementDraftKey(renderModelId, element.element_id);
    const startX = element.x;
    const startY = element.y;
    const maxX = Math.max(0, 168 - element.width);
    const maxY = Math.max(0, 144 - element.height);
    let latestX = startX;
    let latestY = startY;
    setSelectedPlacementElement(element.element_id);
    setPlacementInspectorTab("object");
    if (renderModelId !== null) setSceneSelection({ kind: "render", id: renderModelId });

    const move = (moveEvent: PointerEvent) => {
      const dx = Math.round(((moveEvent.clientX - startClientX) / rect.width) * 168);
      const dy = Math.round(((moveEvent.clientY - startClientY) / rect.height) * 144);
      latestX = Math.min(maxX, Math.max(0, startX + dx));
      latestY = Math.min(maxY, Math.max(0, startY + dy));
      setPlacementDraftPositions((current) => ({ ...current, [key]: { x: latestX, y: latestY } }));
    };
    const stop = () => {
      window.removeEventListener("pointermove", move);
      window.removeEventListener("pointerup", stop);
      setPlacementDraftPositions((current) => {
        const next = { ...current };
        delete next[key];
        return next;
      });
      if (latestX !== startX || latestY !== startY) {
        movePlacementElement(element, renderModelId, latestX, latestY);
      }
    };
    window.addEventListener("pointermove", move);
    window.addEventListener("pointerup", stop);
  };
  const startPlacementResize = (
    event: ReactPointerEvent<HTMLElement>,
    element: RenderElement,
    renderModelId: string,
    handle: "nw" | "ne" | "sw" | "se",
  ) => {
    if (selectedSceneDocument === null || busy !== null || element.kind === "sprite") {
      return;
    }
    event.preventDefault();
    event.stopPropagation();
    const overlay = event.currentTarget.closest(".placement-screen-overlay");
    if (!(overlay instanceof HTMLElement)) {
      return;
    }
    const rect = overlay.getBoundingClientRect();
    const startClientX = event.clientX;
    const startClientY = event.clientY;
    const key = placementDraftKey(renderModelId, element.element_id);
    const startBounds = {
      x: element.x,
      y: element.y,
      width: element.width,
      height: element.height,
    };
    let latestBounds = startBounds;
    setSelectedPlacementElement(element.element_id);
    setPlacementInspectorTab("object");
    setSceneSelection({ kind: "render", id: renderModelId });

    const move = (moveEvent: PointerEvent) => {
      const dx = Math.round(((moveEvent.clientX - startClientX) / rect.width) * 168);
      const dy = Math.round(((moveEvent.clientY - startClientY) / rect.height) * 144);
      let nextX = startBounds.x;
      let nextY = startBounds.y;
      let nextWidth = startBounds.width;
      let nextHeight = startBounds.height;

      if (handle.includes("w")) {
        nextX = Math.min(startBounds.x + startBounds.width - 1, Math.max(0, startBounds.x + dx));
        nextWidth = startBounds.x + startBounds.width - nextX;
      } else {
        nextWidth = Math.min(168 - startBounds.x, Math.max(1, startBounds.width + dx));
      }

      if (handle.includes("n")) {
        nextY = Math.min(startBounds.y + startBounds.height - 1, Math.max(0, startBounds.y + dy));
        nextHeight = startBounds.y + startBounds.height - nextY;
      } else {
        nextHeight = Math.min(144 - startBounds.y, Math.max(1, startBounds.height + dy));
      }

      latestBounds = normalizePrimitiveBounds(element.kind as PlacementPrimitiveKind, { x: nextX, y: nextY, width: nextWidth, height: nextHeight });
      setPlacementDraftBounds((current) => ({ ...current, [key]: latestBounds }));
    };
    const stop = () => {
      window.removeEventListener("pointermove", move);
      window.removeEventListener("pointerup", stop);
      setPlacementDraftBounds((current) => {
        const next = { ...current };
        delete next[key];
        return next;
      });
      resizePlacementElement(element, renderModelId, latestBounds);
    };
    window.addEventListener("pointermove", move);
    window.addEventListener("pointerup", stop);
  };
  const placementPointFromClient = (rect: DOMRect, clientX: number, clientY: number): PlacementPoint => ({
    x: Math.min(PLACEMENT_WIDTH - 1, Math.max(0, Math.floor(((clientX - rect.left) / rect.width) * PLACEMENT_WIDTH))),
    y: Math.min(PLACEMENT_HEIGHT - 1, Math.max(0, Math.floor(((clientY - rect.top) / rect.height) * PLACEMENT_HEIGHT))),
  });
  const startPlacementPrimitiveDraw = (event: ReactPointerEvent<HTMLDivElement>) => {
    if (
      placementTool === "select" ||
      event.button !== 0 ||
      selectedSceneDocument === null ||
      (!objectSceneSelected && placementRenderModel === null) ||
      busy !== null ||
      !scopedPlacementAddSupported
    ) {
      return;
    }
    event.preventDefault();
    event.stopPropagation();
    event.currentTarget.focus();
    placementDrawCancelRef.current?.();
    const kind = placementTool;
    const rect = event.currentTarget.getBoundingClientRect();
    const start = placementPointFromClient(rect, event.clientX, event.clientY);
    let latestBounds = primitiveBoundsFromPoints(kind, start, start);
    let latestLineDirection: PlacementLineDirection = "down_right";
    let finished = false;

    const cleanup = () => {
      window.removeEventListener("pointermove", move);
      window.removeEventListener("pointerup", stop);
      window.removeEventListener("pointercancel", cancel);
      placementDrawCancelRef.current = null;
      setPlacementPrimitiveDraft(null);
    };
    const update = (clientX: number, clientY: number) => {
      const end = placementPointFromClient(rect, clientX, clientY);
      latestBounds = primitiveBoundsFromPoints(kind, start, end);
      latestLineDirection = lineDirectionFromPoints(start, end);
      setPlacementPrimitiveDraft({ kind, bounds: latestBounds, lineDirection: latestLineDirection });
    };
    const move = (moveEvent: PointerEvent) => update(moveEvent.clientX, moveEvent.clientY);
    const stop = (stopEvent: PointerEvent) => {
      if (finished) {
        return;
      }
      finished = true;
      update(stopEvent.clientX, stopEvent.clientY);
      cleanup();
      void addPlacementPrimitive(kind, latestBounds, latestLineDirection);
    };
    const cancel = () => {
      if (finished) {
        return false;
      }
      finished = true;
      cleanup();
      return true;
    };

    setSelectedPlacementElement(null);
    if (placementRenderModel !== null) setSceneSelection({ kind: "render", id: placementRenderModel.visual_id });
    setPlacementPrimitiveDraft({ kind, bounds: latestBounds, lineDirection: latestLineDirection });
    placementDrawCancelRef.current = cancel;
    window.addEventListener("pointermove", move);
    window.addEventListener("pointerup", stop);
    window.addEventListener("pointercancel", cancel);
  };
  const renderPlacementToolPalette = () => (
    <div className="placement-tool-palette" aria-label="Placement tools">
      <button
        type="button"
        className={placementTool === "select" ? "active" : ""}
        onClick={() => {
          placementDrawCancelRef.current?.();
          setPlacementTool("select");
          setSpritePickerOpen(false);
        }}
        title="Select and move objects"
        aria-label="Select and move objects"
      >
        <SquareMousePointer size={18} aria-hidden="true" />
      </button>
      <button
        type="button"
        disabled={!scopedPlacementAddSupported || busy !== null || selectedSceneDocument === null || (!objectSceneSelected && placementRenderModel === null) || compiledAssetFrames.length === 0}
        className={spritePickerOpen ? "active" : ""}
        onClick={() => {
          placementDrawCancelRef.current?.();
          setPlacementTool("select");
          setSpritePickerOpen((open) => !open);
        }}
        title={compiledAssetFrames.length === 0 ? "No sprite assets available" : "Add sprite"}
        aria-label="Add sprite"
      >
        <Image size={18} aria-hidden="true" />
      </button>
      {PLACEMENT_PRIMITIVES.map((primitive) => (
        <button
          key={primitive.kind}
          className={`primitive-${primitive.kind} ${placementTool === primitive.kind ? "active" : ""}`}
          type="button"
          disabled={!scopedPlacementAddSupported || busy !== null || selectedSceneDocument === null || (!objectSceneSelected && placementRenderModel === null)}
          onClick={() => {
            placementDrawCancelRef.current?.();
            setPlacementTool(primitive.kind);
            setSpritePickerOpen(false);
            setSelectedPlacementElement(null);
          }}
          title={`Draw ${primitive.label.toLowerCase()}`}
          aria-label={`Draw ${primitive.label.toLowerCase()}`}
        >
          {placementKindIcon(primitive.kind)}
        </button>
      ))}
    </div>
  );
  const renderSpritePicker = () => (
    <div className="placement-sprite-picker" role="dialog" aria-label="Choose sprite">
      <div className="placement-sprite-picker-heading">
        <strong>Choose sprite</strong>
        <button type="button" onClick={() => setSpritePickerOpen(false)} title="Close sprite picker" aria-label="Close sprite picker">
          <X size={14} aria-hidden="true" />
        </button>
      </div>
      <div className="placement-sprite-picker-groups">
        {compiledAssetFrameGroups.map((group) => (
          <section className="placement-sprite-picker-group" key={group.assetId}>
            <div>
              <strong>{assetDisplayName(group.assetId)}</strong>
              <span>{group.frames.length} frame{group.frames.length === 1 ? "" : "s"}</span>
            </div>
            <div className="placement-sprite-picker-frames">
              {group.frames.map((frame) => (
                <button
                  key={frame.frame_id}
                  type="button"
                  disabled={!scopedPlacementAddSupported || busy !== null || selectedSceneDocument === null || (!objectSceneSelected && placementRenderModel === null)}
                  title={`${placementFrameLabel(frame)} (${frame.width}x${frame.height})`}
                  onClick={() => void addPlacementSprite(frame)}
                >
                  <span className="frame-preview-box">
                    <FramePreviewCanvas frame={frame} />
                  </span>
                  <strong>{placementFrameLabel(frame)}</strong>
                  <small>{frame.width}x{frame.height}</small>
                </button>
              ))}
            </div>
          </section>
        ))}
      </div>
    </div>
  );
  const renderPreviewPanel = (variant: "project" | "placement") => {
    const placementPreviewMatches = placementPreview?.project_revision === projectRevision && (
      placementState === null
        ? "placement" in placementPreview && placementPreview.placement.scene_id === selectedSceneDocument?.scene_id
        : "scene" in placementPreview &&
          placementPreview.scene.scene_id === selectedSceneDocument?.scene_id &&
          placementPreview.scene.state_id === placementState.state_id
    );
    const placementFramebuffer = placementPreviewMatches ? placementPreview.framebuffer : null;
    const matchingLiveFramebuffer = preview?.project_revision === projectRevision &&
      placementState !== null &&
      preview.scene.scene_id === selectedSceneDocument?.scene_id &&
      preview.scene.state_id === placementState.state_id
      ? preview.framebuffer
      : null;
    const framebuffer = variant === "placement" ? placementFramebuffer ?? matchingLiveFramebuffer : preview?.framebuffer ?? null;
    const placementPreviewNotice =
      variant !== "placement" || placementFramebuffer !== null || matchingLiveFramebuffer !== null
        ? null
        : placementPreviewError ?? (
          placementPreviewLoading
            ? `Loading ${placementState === null ? "Base Placement" : "selected state"} preview...`
            : `No ${placementState === null ? "Base Placement" : "selected state"} preview available.`
        );
    return (
      variant === "project" ? <EmulatorPanel
        preview={preview}
        sceneName={scenes.find((scene) => scene.scene_id === preview?.scene.scene_id)?.display_name ?? "No active scene"}
        playing={playing}
        onReset={() => {
          const target = previewStartRef.current;
          if (target !== null) {
            void startPreview(target.sceneId, { stateId: target.stateId, updateSelection: false });
          } else if (selectedScene !== null) {
            void startPreview(selectedScene, { updateSelection: false });
          }
        }}
        onTogglePlaying={() => setPlaying((value) => !value)}
        onAdvance={() => void advancePreview(250)}
        onInput={(source) => void sendInput(source)}
      /> : <section className="preview-pane preview-pane-large">
        <div className="display-stage">
          {variant === "placement" && renderPlacementToolPalette()}
          {variant === "placement" && spritePickerOpen && renderSpritePicker()}
          <div className="panel-bezel">
            <FramebufferCanvas framebuffer={framebuffer} />
          {placementPreviewNotice !== null && (
            <div className="preview-status-card" role="status">
              {placementPreviewNotice}
            </div>
          )}
          {variant === "placement" && (
            <div
              className={`placement-screen-overlay labels-${placementLabelMode} ${placementOverlayVisible ? "" : "boxes-hidden"} ${placementTool === "select" ? "" : `drawing-tool drawing-${placementTool}`}`}
              aria-label="Placement selection overlay"
              tabIndex={0}
              onKeyDown={handlePlacementKeyDown}
              onPointerDown={startPlacementPrimitiveDraw}
              style={{
                "--placement-grid-minor-opacity": placementGridVisible ? placementGridStrength / 100 : 0,
                "--placement-grid-major-opacity": placementGridVisible && placementMajorGridVisible ? (placementGridStrength + 6) / 100 : 0,
              } as CSSProperties}
            >
              <div className="placement-pixel-grid" aria-hidden="true" />
              {placementPrimitiveDraft !== null && (
                <svg className="placement-primitive-draft" viewBox={`0 0 ${PLACEMENT_WIDTH} ${PLACEMENT_HEIGHT}`} aria-hidden="true">
                  {placementPrimitiveDraft.kind === "line" && (
                    <line
                      x1={placementPrimitiveDraft.bounds.x + 0.5}
                      y1={placementPrimitiveDraft.lineDirection === "up_right"
                        ? placementPrimitiveDraft.bounds.y + placementPrimitiveDraft.bounds.height - 0.5
                        : placementPrimitiveDraft.bounds.y + 0.5}
                      x2={placementPrimitiveDraft.bounds.x + placementPrimitiveDraft.bounds.width - 0.5}
                      y2={placementPrimitiveDraft.lineDirection === "up_right"
                        ? placementPrimitiveDraft.bounds.y + 0.5
                        : placementPrimitiveDraft.bounds.y + placementPrimitiveDraft.bounds.height - 0.5}
                    />
                  )}
                  {(placementPrimitiveDraft.kind === "outline_rect" || placementPrimitiveDraft.kind === "filled_rect") && (
                    <rect
                      className={placementPrimitiveDraft.kind === "filled_rect" ? "filled" : ""}
                      x={placementPrimitiveDraft.bounds.x + 0.5}
                      y={placementPrimitiveDraft.bounds.y + 0.5}
                      width={Math.max(0, placementPrimitiveDraft.bounds.width - 1)}
                      height={Math.max(0, placementPrimitiveDraft.bounds.height - 1)}
                    />
                  )}
                  {(placementPrimitiveDraft.kind === "circle" || placementPrimitiveDraft.kind === "ellipse") && (
                    <ellipse
                      cx={placementPrimitiveDraft.bounds.x + placementPrimitiveDraft.bounds.width / 2}
                      cy={placementPrimitiveDraft.bounds.y + placementPrimitiveDraft.bounds.height / 2}
                      rx={Math.max(0.5, placementPrimitiveDraft.bounds.width / 2 - 0.5)}
                      ry={Math.max(0.5, placementPrimitiveDraft.bounds.height / 2 - 0.5)}
                    />
                  )}
                </svg>
              )}
              {[...effectivePlacementElements]
                  .sort((left, right) => left.z_order - right.z_order)
                  .map((element) => {
                    const key = placementDraftKey(placementRenderModel?.visual_id ?? null, element.element_id);
                    const positionDraft = placementDraftPositions[key];
                    const boundsDraft = placementDraftBounds[key];
                    const x = boundsDraft?.x ?? positionDraft?.x ?? element.x;
                    const y = boundsDraft?.y ?? positionDraft?.y ?? element.y;
                    const width = boundsDraft?.width ?? element.width;
                    const height = boundsDraft?.height ?? element.height;
                    const canResize = placementEditStateIds.length === 0 && selectedPlacementElement === element.element_id && element.kind !== "sprite" && placementRenderModel !== null;
                    const isLine = element.kind === "line";
                    return (
                      <button
                        key={element.element_id}
                        className={`placement-element-box ${selectedPlacementElement === element.element_id ? "selected" : ""}`}
                        type="button"
                        style={{
                          left: `${(x / 168) * 100}%`,
                          top: `${(y / 144) * 100}%`,
                          width: `${(width / 168) * 100}%`,
                          height: `${(height / 144) * 100}%`,
                        }}
                        title={`${element.element_id}: ${x},${y} ${width}x${height}`}
                        onPointerDown={(event) => {
                          if (placementRenderModel !== null || sceneObjectMoveSupported) {
                            startPlacementDrag(event, element, placementRenderModel?.visual_id ?? null);
                          }
                        }}
                        onClick={(event) => {
                          event.stopPropagation();
                          event.currentTarget.focus();
                          selectPlacementElement(element.element_id);
                        }}
                      >
                        <span>{element.element_id}</span>
                        {canResize && (
                          <>
                            <i
                              className={`placement-resize-handle ${isLine && element.line_direction === "up_right" ? "handle-sw" : "handle-nw"}`}
                              onPointerDown={(event) => startPlacementResize(
                                event,
                                element,
                                placementRenderModel.visual_id,
                                isLine && element.line_direction === "up_right" ? "sw" : "nw",
                              )}
                            />
                            {!isLine && (
                              <>
                                <i className="placement-resize-handle handle-ne" onPointerDown={(event) => startPlacementResize(event, element, placementRenderModel.visual_id, "ne")} />
                                <i className="placement-resize-handle handle-sw" onPointerDown={(event) => startPlacementResize(event, element, placementRenderModel.visual_id, "sw")} />
                              </>
                            )}
                            <i
                              className={`placement-resize-handle ${isLine && element.line_direction === "up_right" ? "handle-ne" : "handle-se"}`}
                              onPointerDown={(event) => startPlacementResize(
                                event,
                                element,
                                placementRenderModel.visual_id,
                                isLine && element.line_direction === "up_right" ? "ne" : "se",
                              )}
                            />
                          </>
                        )}
                      </button>
                    );
                  })}
            </div>
          )}
        </div>
      </div>

    </section>
    );
  };
  const renderModeTabs = () => (
    <div className="mode-tabs" aria-label="Workspace mode">
      <button
        className={workspaceMode === "scene-flow" ? "active" : ""}
        type="button"
        onClick={() => setWorkspaceMode("scene-flow")}
      >
        <Network size={15} aria-hidden="true" />
        Scene flow
      </button>
      <button
        className={workspaceMode === "logic" ? "active" : ""}
        type="button"
        onClick={() => setWorkspaceMode("logic")}
      >
        <Network size={15} aria-hidden="true" />
        Local logic
      </button>
      <button
        className={workspaceMode === "placement" ? "active" : ""}
        type="button"
        onClick={() => setWorkspaceMode("placement")}
      >
        <Maximize2 size={15} aria-hidden="true" />
        Placement
      </button>
      <button
        className={workspaceMode === "assets" ? "active" : ""}
        type="button"
        onClick={() => setWorkspaceMode("assets")}
      >
        <Image size={15} aria-hidden="true" />
        Assets
      </button>
    </div>
  );
  const renderSpriteImportPanel = (canEditAssets: boolean) => {
    const importCheck = pendingSpriteImport === null
      ? null
      : parseImportFrameSize(
        pendingSpriteImport.width,
        pendingSpriteImport.height,
        `${pendingSpriteImport.frameWidth}x${pendingSpriteImport.frameHeight}`,
      );
    const frameCount = pendingSpriteImport === null || importCheck === null || importCheck.error !== undefined
      ? 0
      : (pendingSpriteImport.width / importCheck.frameWidth) * (pendingSpriteImport.height / importCheck.frameHeight);
    return (
      <div className="asset-import-panel">
        {pendingSpriteImport === null ? (
          <div>
            <strong>PNG import</strong>
            <span>Choose a PNG, then set the frame size if it is a sprite sheet.</span>
          </div>
        ) : (
          <>
            <div className="asset-import-heading">
              <div>
                <strong>{pendingSpriteImport.displayName}</strong>
                <span>{pendingSpriteImport.sourcePath} - {pendingSpriteImport.width}x{pendingSpriteImport.height}</span>
              </div>
              <button className="icon-button" type="button" onClick={() => setPendingSpriteImport(null)} title="Cancel import" aria-label="Cancel import">
                <X size={14} aria-hidden="true" />
              </button>
            </div>
            <div className="asset-import-controls">
              <label>
                Frame width
                <input
                  type="number"
                  min={1}
                  max={168}
                  step={1}
                  value={pendingSpriteImport.frameWidth}
                  disabled={!canEditAssets}
                  onChange={(event) => {
                    const value = Math.max(1, Math.round(Number(event.target.value)));
                    setPendingSpriteImport((current) => current === null ? null : { ...current, frameWidth: value });
                  }}
                />
              </label>
              <label>
                Frame height
                <input
                  type="number"
                  min={1}
                  max={144}
                  step={1}
                  value={pendingSpriteImport.frameHeight}
                  disabled={!canEditAssets}
                  onChange={(event) => {
                    const value = Math.max(1, Math.round(Number(event.target.value)));
                    setPendingSpriteImport((current) => current === null ? null : { ...current, frameHeight: value });
                  }}
                />
              </label>
              <div className={`asset-import-result ${importCheck?.error !== undefined ? "error" : ""}`}>
                {importCheck?.error ?? `${frameCount} frame${frameCount === 1 ? "" : "s"}`}
              </div>
              <button
                className="button primary"
                type="button"
                disabled={!canEditAssets || importCheck?.error !== undefined}
                onClick={() => void confirmSpriteImport()}
              >
                Import
              </button>
            </div>
          </>
        )}
        <div className="asset-import-debug">{assetImportDebug}</div>
      </div>
    );
  };
  const renderAssetsWorkspace = () => {
    const canEditAssets = bridge !== undefined && project !== null && busy === null && service?.operations.includes("project.apply_commands") === true;
    const audioSupported = service?.state_scene_audio.host_package_support === true;
    const auditionSupported = service?.operations.includes("project.audio_audition") === true;
    const hasAnyAssets = compiledAssetFrameGroups.length > 0 || audioCues.length > 0;
    return (
      <section className="asset-workspace-pane">
        <div className="preview-heading graph-heading">
          <div>
            <span className="section-kicker">Assets</span>
            <h2>Project library</h2>
          </div>
          {renderModeTabs()}
        </div>
        <div className="asset-workspace">
          <div className="asset-workspace-summary">
            <div>
              <strong>Project library</strong>
              <span>
                {compiledAssetFrames.length} frame{compiledAssetFrames.length === 1 ? "" : "s"} / {audioCues.length} SFX cue{audioCues.length === 1 ? "" : "s"}
              </span>
            </div>
            <div className="asset-workspace-actions">
              <button
                className="button secondary"
                type="button"
                disabled={!canEditAssets || projectPath === null}
                onClick={() => void chooseSpritePng()}
              >
                <Image size={15} aria-hidden="true" />
                Choose PNG
              </button>
              <button
                className="button secondary"
                type="button"
                disabled={!canEditAssets}
                onClick={() => void createTextSpriteAsset()}
              >
                <Type size={15} aria-hidden="true" />
                Text sprite
              </button>
              <button
                className="button secondary"
                type="button"
                disabled={!canEditAssets || projectPath === null || !audioSupported}
                onClick={() => void chooseAudioWav()}
              >
                <Volume2 size={15} aria-hidden="true" />
                WAV SFX
              </button>
            </div>
          </div>
          {renderSpriteImportPanel(canEditAssets)}
          <div className="asset-group-stack">
            {compiledAssetFrameGroups.length > 0 && (
              <section className="asset-group-panel">
                <div className="asset-group-heading">
                  <strong>Sprites</strong>
                  <span>{compiledAssetFrames.length} compiled frame{compiledAssetFrames.length === 1 ? "" : "s"}</span>
                </div>
                <div className="asset-frame-gallery">
                  {compiledAssetFrameGroups.flatMap((group) => group.frames.map((frame) => (
                    <button
                      key={frame.frame_id}
                      className={selectedAssetFrame?.frame_id === frame.frame_id ? "selected" : ""}
                      type="button"
                      onClick={() => selectAssetRecord({ kind: "sprite", frameId: frame.frame_id })}
                      title="Edit this frame"
                    >
                      <span className="asset-frame-preview">
                        <FramePreviewCanvas frame={frame} />
                      </span>
                      <strong>{assetDisplayName(frame.asset_id)}</strong>
                      <small>{placementFrameLabel(frame)} / {frame.width}x{frame.height}</small>
                    </button>
                  )))}
                </div>
              </section>
            )}
            {audioCues.length > 0 && (
              <section className="asset-group-panel">
                <div className="asset-group-heading">
                  <strong>Sampled SFX</strong>
                  <span>{audioCues.length} cue{audioCues.length === 1 ? "" : "s"}</span>
                </div>
                <div className="audio-cue-gallery">
                  {audioCues.map((cue) => {
                    const asset = audioAssetById.get(cue.asset_ref);
                    return (
                      <div
                        key={cue.cue_id}
                        className={selectedAudioCue?.cue_id === cue.cue_id ? "selected" : ""}
                      >
                        <button
                          className="audio-cue-select"
                          type="button"
                          onClick={() => selectAssetRecord({ kind: "audio", cueId: cue.cue_id })}
                          title="Select this SFX cue"
                        >
                          <Volume2 size={18} aria-hidden="true" />
                          <span>
                            <strong>{audioCueDisplayName(cue)}</strong>
                            <small>
                              {cue.cue_id} / {asset === undefined ? cue.asset_ref : `${asset.duration_ms} ms / ${asset.adpcm_bytes} ADPCM bytes`}
                            </small>
                          </span>
                        </button>
                        <button
                          className="icon-button"
                          type="button"
                          disabled={!auditionSupported || busy !== null || !project?.valid}
                          onClick={(event) => {
                            event.stopPropagation();
                            void auditionAudioCue(cue.cue_id);
                          }}
                          title="Audition packaged cue"
                          aria-label={`Audition ${audioCueDisplayName(cue)}`}
                        >
                          <Play size={15} aria-hidden="true" />
                        </button>
                      </div>
                    );
                  })}
                </div>
              </section>
            )}
            {!hasAnyAssets && (
              <div className="asset-workspace-empty">
                <Box size={28} aria-hidden="true" />
                <strong>No project assets</strong>
              </div>
            )}
          </div>
        </div>
      </section>
    );
  };
  const renderSpriteInspector = () => (
    <section className="inspector-section asset-inspector">
      <h3><Image size={14} aria-hidden="true" /> Sprite</h3>
      {selectedAssetFrame === null ? (
        <p className="muted">Select a sprite to inspect its frames.</p>
      ) : (
        (() => {
          const source = sourceFrameById.get(selectedAssetFrame.frame_id);
          const sourceAsset = source?.asset ?? assetById.get(selectedAssetFrame.asset_id) ?? null;
          return (
            <>
              <div className="asset-inspector-preview">
                <FramePreviewCanvas frame={animatedAssetPreviewFrame ?? selectedAssetFrame} />
                {selectedAssetFrames.length > 1 && (
                  <div className="asset-preview-controls">
                    <button
                      className="icon-button"
                      type="button"
                      onClick={() => setAssetPreviewPlaying((playingNow) => !playingNow)}
                      title={assetPreviewPlaying ? "Pause animation preview" : "Play animation preview"}
                      aria-label={assetPreviewPlaying ? "Pause animation preview" : "Play animation preview"}
                    >
                      {assetPreviewPlaying ? <Pause size={14} aria-hidden="true" /> : <Play size={14} aria-hidden="true" />}
                    </button>
                    <span>
                      Frame {(assetPreviewPlaying ? assetPreviewStep % selectedAssetFrames.length : Math.max(0, selectedAssetFrames.findIndex((frame) => frame.frame_id === selectedAssetFrame.frame_id))) + 1}
                      /{selectedAssetFrames.length}
                    </span>
                  </div>
                )}
              </div>
              {sourceAsset !== null && (
                <div className="asset-name-editor">
                  <label>
                    Sprite name
                    <input
                      key={`${sourceAsset.asset_id}-asset-${sourceAsset.display_name ?? ""}`}
                      type="text"
                      maxLength={64}
                      defaultValue={sourceAsset.display_name ?? sourceAsset.asset_id}
                      disabled={busy !== null}
                      onBlur={(event) => {
                        void updateAssetDisplayNames(sourceAsset, event.target.value, null, null);
                      }}
                      onKeyDown={(event) => {
                        if (event.key === "Enter") {
                          event.currentTarget.blur();
                        }
                      }}
                    />
                  </label>
                </div>
              )}
              {source !== undefined && (
                <div className="asset-frame-strip-panel">
                  <div className="asset-frame-strip-heading">
                    <strong>Frames</strong>
                    <span>{selectedAssetFrames.length} total</span>
                  </div>
                  <div className="asset-frame-strip">
                    {selectedAssetFrames.map((frame, index) => (
                      <button
                        key={frame.frame_id}
                        className={frame.frame_id === selectedAssetFrame.frame_id ? "selected" : ""}
                        type="button"
                        onClick={() => selectAssetRecord({ kind: "sprite", frameId: frame.frame_id })}
                        title={placementFrameLabel(frame)}
                      >
                        <FramePreviewCanvas frame={frame} />
                        <span>{index + 1}</span>
                      </button>
                    ))}
                  </div>
                  <label className="asset-selected-frame-name">
                    Selected frame label
                    <input
                      key={`${source.frame.frame_id}-frame-${source.frame.display_name ?? ""}`}
                      type="text"
                      maxLength={64}
                      defaultValue={source.frame.display_name ?? placementFrameLabel(selectedAssetFrame)}
                      disabled={busy !== null}
                      onBlur={(event) => {
                        void updateAssetFrameRecord(source.asset, source.frame.frame_id, {
                          ...source.frame,
                          display_name: event.target.value.trim(),
                        });
                      }}
                      onKeyDown={(event) => {
                        if (event.key === "Enter") {
                          event.currentTarget.blur();
                        }
                      }}
                    />
                  </label>
                </div>
              )}
              {sourceAsset?.source_format === "system_font_text" && (
                <div className="asset-text-editor">
                  <label>
                    Text
                    <textarea
                      key={`${sourceAsset.asset_id}-text-${sourceAsset.text ?? ""}`}
                      maxLength={96}
                      rows={3}
                      defaultValue={sourceAsset.text ?? ""}
                      disabled={busy !== null}
                      onBlur={(event) => {
                        void updateTextSpriteAsset(sourceAsset, event.target.value, sourceAsset.scale ?? 1);
                      }}
                    />
                  </label>
                  <label>
                    Scale
                    <input
                      key={`${sourceAsset.asset_id}-scale-${sourceAsset.scale ?? 1}`}
                      type="number"
                      min={1}
                      max={8}
                      step={1}
                      defaultValue={sourceAsset.scale ?? 1}
                      disabled={busy !== null}
                      onBlur={(event) => {
                        void updateTextSpriteAsset(sourceAsset, sourceAsset.text ?? "", Number(event.target.value));
                      }}
                      onKeyDown={(event) => {
                        if (event.key === "Enter") {
                          event.currentTarget.blur();
                        }
                      }}
                    />
                  </label>
                </div>
              )}
              <dl className="inspector-list">
                <div><dt>Asset</dt><dd>{assetDisplayName(selectedAssetFrame.asset_id)}</dd></div>
                <div><dt>Frames</dt><dd>{selectedAssetFrames.length}</dd></div>
                <div><dt>Selected</dt><dd>{placementFrameLabel(selectedAssetFrame)}</dd></div>
                <div><dt>Size</dt><dd>{selectedAssetFrame.width}x{selectedAssetFrame.height}</dd></div>
                <div><dt>Mask</dt><dd>{selectedAssetFrame.opaque ? "Opaque" : "Transparent"}</dd></div>
                <div><dt>Source</dt><dd>{sourceAsset?.source_format === "system_font_text" ? "Text sprite" : "PNG sprite"}</dd></div>
                {sourceAsset?.source_format === "system_font_text" && <div><dt>Font</dt><dd>8x8 system</dd></div>}
                <div><dt>Asset ID</dt><dd>{selectedAssetFrame.asset_id}</dd></div>
                <div><dt>Frame ID</dt><dd>{selectedAssetFrame.frame_id}</dd></div>
              </dl>
            </>
          );
        })()
      )}
    </section>
  );
  const renderAudioInspector = () => (
    <section className="inspector-section asset-inspector">
      <h3><Volume2 size={14} aria-hidden="true" /> Sampled SFX</h3>
      {selectedAudioCue === null ? (
        <p className="muted">Import a WAV to create a bounded STATE SFX cue.</p>
      ) : (
        <>
          <div className="asset-name-editor">
            <label>
              SFX name
              <input
                key={`${selectedAudioCue.cue_id}-${selectedAudioCue.display_name ?? ""}`}
                type="text"
                maxLength={64}
                defaultValue={audioCueDisplayName(selectedAudioCue)}
                disabled={busy !== null}
                onBlur={(event) => {
                  const value = event.currentTarget.value.trim();
                  if (value.length === 0) {
                    event.currentTarget.value = audioCueDisplayName(selectedAudioCue);
                    return;
                  }
                  void updateAudioCueDisplayName(selectedAudioCue, value);
                }}
                onKeyDown={(event) => {
                  if (event.key === "Enter") {
                    event.preventDefault();
                    event.currentTarget.blur();
                  } else if (event.key === "Escape") {
                    event.preventDefault();
                    event.stopPropagation();
                    event.currentTarget.value = audioCueDisplayName(selectedAudioCue);
                    event.currentTarget.blur();
                  }
                }}
              />
            </label>
          </div>
          <div className="audio-inspector-controls">
            <button
              className="button primary"
              type="button"
              disabled={busy !== null || !project?.valid || service?.operations.includes("project.audio_audition") !== true}
              onClick={() => void auditionAudioCue(selectedAudioCue.cue_id)}
            >
              <Play size={15} aria-hidden="true" />
              Audition
            </button>
            <span>{audioAuditionStatus}</span>
          </div>
          <dl className="inspector-list">
            <div><dt>Cue ID</dt><dd>{selectedAudioCue.cue_id}</dd></div>
            <div><dt>Asset</dt><dd>{selectedAudioCue.asset_ref}</dd></div>
            <div><dt>Priority</dt><dd>{selectedAudioCue.priority}</dd></div>
            <div><dt>Volume</dt><dd>{selectedAudioCue.volume}</dd></div>
            {selectedAudioAsset !== null && (
              <>
                <div><dt>Source</dt><dd title={selectedAudioAsset.source_path}>{selectedAudioAsset.source_path}</dd></div>
                <div><dt>Source rate</dt><dd>{selectedAudioAsset.source_sample_rate_hz} Hz</dd></div>
                <div><dt>Compiled</dt><dd>{selectedAudioAsset.sample_rate_hz} Hz mono ADPCM</dd></div>
                <div><dt>Duration</dt><dd>{selectedAudioAsset.duration_ms} ms</dd></div>
                <div><dt>Samples</dt><dd>{selectedAudioAsset.sample_count}</dd></div>
                <div><dt>Blocks</dt><dd>{selectedAudioAsset.block_count}</dd></div>
                <div><dt>ADPCM</dt><dd>{selectedAudioAsset.adpcm_bytes} bytes</dd></div>
              </>
            )}
          </dl>
        </>
      )}
    </section>
  );
  const renderAssetInspector = () => {
    if (assetSelection?.kind === "sprite") {
      return renderSpriteInspector();
    }
    if (assetSelection?.kind === "audio") {
      return renderAudioInspector();
    }
    return (
      <section className="inspector-section asset-inspector">
        <h3><Box size={14} aria-hidden="true" /> Asset</h3>
        <p className="muted">Select a sprite or sampled SFX to inspect it.</p>
      </section>
    );
  };
  const renderPlacementViewSettings = () => (
    <section className="inspector-section placement-view-settings-section">
      <h3>Settings</h3>
      <div className="placement-view-settings">
        <label>
          <input
            type="checkbox"
            checked={placementGridVisible}
            onChange={(event) => setPlacementGridVisible(event.target.checked)}
          />
          Pixel grid
        </label>
        <label>
          <input
            type="checkbox"
            checked={placementMajorGridVisible}
            disabled={!placementGridVisible}
            onChange={(event) => setPlacementMajorGridVisible(event.target.checked)}
          />
          Major lines
        </label>
        <label>
          <input
            type="checkbox"
            checked={placementOverlayVisible}
            onChange={(event) => setPlacementOverlayVisible(event.target.checked)}
          />
          Object boxes
        </label>
        <label>
          Grid strength
          <input
            type="range"
            min="4"
            max="30"
            value={placementGridStrength}
            disabled={!placementGridVisible}
            onChange={(event) => setPlacementGridStrength(Number(event.target.value))}
          />
        </label>
        <label>
          Labels
          <select value={placementLabelMode} onChange={(event) => setPlacementLabelMode(event.target.value as "hover" | "always" | "off")}>
            <option value="hover">Hover</option>
            <option value="always">Always</option>
            <option value="off">Off</option>
          </select>
        </label>
      </div>
    </section>
  );
  const placementElements = () => [...(placementRenderModel?.elements ?? [])].sort((left, right) => left.z_order - right.z_order);
  const placementKindLabel = (kind: string) => {
    switch (kind) {
      case "sprite":
        return "Sprite";
      case "line":
        return "Line";
      case "outline_rect":
        return "Outline rectangle";
      case "filled_rect":
        return "Filled rectangle";
      case "circle":
        return "Circle";
      case "ellipse":
        return "Ellipse";
      default:
        return kind.replaceAll("_", " ");
    }
  };
  const placementKindIcon = (kind: string) => {
    switch (kind) {
      case "sprite":
        return <Image size={15} aria-hidden="true" />;
      case "line":
        return <Minus size={15} aria-hidden="true" />;
      case "outline_rect":
      case "filled_rect":
        return <RectangleHorizontal size={15} aria-hidden="true" />;
      case "circle":
      case "ellipse":
        return <CircleDot size={15} aria-hidden="true" />;
      default:
        return <SquareMousePointer size={15} aria-hidden="true" />;
    }
  };
  const placementLayerLabel = (element: RenderElement) => element.layer ?? "SCENE";
  const placementSourceLabel = (element: RenderElement) => element.visual_ref ?? "Native shape";
  const placementFrameLabel = (frame: CompiledAssetFrame) => {
    const source = sourceFrameById.get(frame.frame_id);
    if (source?.frame.display_name !== undefined) {
      return source.frame.display_name;
    }
    const prefixedName = `${frame.asset_id}.`;
    return frame.frame_id.startsWith(prefixedName) ? frame.frame_id.slice(prefixedName.length) : frame.frame_id;
  };
  const assetDisplayName = (assetId: string) => assetById.get(assetId)?.display_name ?? assetId;
  const placementObjectLabelBase = (element: RenderElement) => {
    if (element.kind !== "sprite") {
      return placementKindLabel(element.kind);
    }
    const frame = element.visual_ref === undefined
      ? null
      : compiledAssetFrameById.get(element.visual_ref) ?? null;
    return frame === null ? "Sprite" : assetDisplayName(frame.asset_id);
  };
  const nextPlacementElementId = (kind: string, elements: RenderElement[]) => {
    const prefix = kind.replaceAll("-", "_");
    const existing = new Set(elements.map((element) => element.element_id));
    for (let index = 1; index < 1000; index += 1) {
      const candidate = `${prefix}_${index}`;
      if (!existing.has(candidate)) {
        return candidate;
      }
    }
    return `${prefix}_${elements.length + 1}`;
  };
  const addSceneObject = async (element: RenderElement) => {
    if (selectedSceneDocument === null) return false;
    const { element_id, x, y, visible, visual_ref, ...geometry } = element;
    const stateIds = placementEditStateTargets();
    const applied = await applySceneObjectCommands([{ kind: "object.add", scene_id: selectedSceneDocument.scene_id,
      object: { ...geometry, object_id: element_id,
        defaults: { x, y, visible: visible !== false, ...(visual_ref === undefined ? {} : { visual_ref }) } },
      ...(stateIds.length === 0 ? {} : { visible_in_states: stateIds }) }]);
    if (applied) {
      setSelectedPlacementElement(element_id);
      setPlacementInspectorTab("object");
    }
    return applied;
  };
  const addPlacementSprite = async (frame: CompiledAssetFrame | null) => {
    if (objectSceneSelected && selectedSceneDocument !== null && frame !== null) {
      if (frame.width > PLACEMENT_WIDTH || frame.height > PLACEMENT_HEIGHT) {
        setMessage("This sprite is larger than the placement canvas.");
        return;
      }
      const elements = baseObjectRows(selectedSceneDocument, placementOwnershipScene);
      const added = await addSceneObject({ element_id: nextPlacementElementId(frame.asset_id, elements),
        kind: "sprite", visual_ref: frame.frame_id,
        x: Math.min(48, PLACEMENT_WIDTH - frame.width), y: Math.min(40, PLACEMENT_HEIGHT - frame.height),
        width: frame.width, height: frame.height, z_order: Math.min(255, Math.max(0, ...elements.map(item => item.z_order)) + 1),
        layer: "SCENE", visible: true });
      if (added) setSpritePickerOpen(false);
      return;
    }
    if (selectedSceneDocument === null || placementRenderModel === null || frame === null) {
      if (frame === null) {
        setMessage("Create or import a sprite asset first.");
      }
      return;
    }
    if (!scopedPlacementAddSupported) {
      setMessage(`Scoped placement needs Service API 36. Restart Peep Studio if the top bar still shows Service API ${service?.service_api_version ?? "unknown"}.`);
      return;
    }
    const targetStateIds = placementEditStateTargets();
    const animationStateIds = placementAnimationStateTargets();
    const element: RenderElement = {
      element_id: nextPlacementElementId(frame.asset_id, placementRenderModel.elements),
      kind: "sprite",
      visual_ref: frame.frame_id,
      x: 48,
      y: 40,
      width: frame.width,
      height: frame.height,
      z_order: Math.max(0, ...placementRenderModel.elements.map((item) => item.z_order)) + 1,
      layer: "SCENE",
      visible: true,
    };
    const animationFrames = compiledAssetFrameGroups.find((group) => group.assetId === frame.asset_id)?.frames ?? [];
    const canAutoAnimate =
      placementAnimationSupported &&
      animationStateIds.length > 0 &&
      animationFrames.length >= 2 &&
      animationFrames.length <= 4 &&
      animationFrames.every((item) => item.width === frame.width && item.height === frame.height);
    const commands: Record<string, unknown>[] = [
      {
        kind: "placement_object.add",
        scene_id: selectedSceneDocument.scene_id,
        scope: placementObjectScope(),
        element,
      },
    ];
    if (canAutoAnimate) {
      commands.push(...animationStateIds.map((stateId) => ({
          kind: "render_element.bind_waiting_animation",
          scene_id: selectedSceneDocument.scene_id,
          state_id: stateId,
          render_model_id: placementRenderModel.visual_id,
          element_id: element.element_id,
          phase_visual_refs: animationFrames.map((item) => item.frame_id),
        })));
    }
    if (bridge === undefined || project === null || busy !== null) {
      return;
    }
    setBusy("Adding sprite");
    setPlaying(false);
    try {
      const result = await bridge.serviceRequest<ProjectCommandResult>("project.apply_commands", {
        project_revision: project.project_revision,
        commands,
      });
      applyProjectResult(result);
      setSelectedScene(selectedSceneDocument.scene_id);
      setSelectedPlacementElement(element.element_id);
      setPlacementInspectorTab("object");
      setSceneSelection(placementState === null
        ? { kind: "render", id: placementRenderModel.visual_id }
        : { kind: "state", id: placementState.state_id });
      if (result.valid) {
        const previewResult = await bridge.serviceRequest<PreviewSnapshot>("project.preview_reset", {
          project_revision: result.project_revision,
          scene_id: selectedSceneDocument.scene_id,
        });
        setPreview(previewResult);
      }
      setMessage(`${canAutoAnimate ? "Animated sprite" : "Sprite"} added to ${placementEditTargetLabel()}. Save to write it to the project.`);
      setSpritePickerOpen(false);
    } catch (error) {
      setMessage(errorText(error));
    } finally {
      setBusy(null);
    }
  };
  const bindPlacementSpriteAnimation = async (
    element: RenderElement,
    renderModelId: string,
    frames: CompiledAssetFrame[],
  ) => {
    const animationStateIds = placementAnimationStateTargets();
    if (selectedSceneDocument === null || animationStateIds.length === 0 || frames.length < 2) {
      return;
    }
    await applyPlacementCommandBatch(
      "Adding animation",
      selectedSceneDocument.scene_id,
      renderModelId,
      element.element_id,
      animationStateIds.map((stateId) => ({
        kind: "render_element.bind_waiting_animation",
        scene_id: selectedSceneDocument.scene_id,
        state_id: stateId,
        render_model_id: renderModelId,
        element_id: element.element_id,
        phase_visual_refs: frames.map((frame) => frame.frame_id),
      })),
      `Sprite animation enabled for ${placementEditTargetLabel()}. Save to write it to the project.`,
      placementState === null
        ? { kind: "render", id: renderModelId }
        : { kind: "state", id: placementState.state_id },
    );
  };
  const clearPlacementSpriteAnimation = async (element: RenderElement, renderModelId: string) => {
    const animationStateIds = placementAnimationStateTargets();
    if (selectedSceneDocument === null || animationStateIds.length === 0) {
      return;
    }
    await applyPlacementCommandBatch(
      "Removing animation",
      selectedSceneDocument.scene_id,
      renderModelId,
      element.element_id,
      animationStateIds.map((stateId) => ({
        kind: "render_element.clear_waiting_animation",
        scene_id: selectedSceneDocument.scene_id,
        state_id: stateId,
        render_model_id: renderModelId,
        element_id: element.element_id,
      })),
      `Sprite animation removed for ${placementEditTargetLabel()}. Save to write it to the project.`,
      placementState === null
        ? { kind: "render", id: renderModelId }
        : { kind: "state", id: placementState.state_id },
    );
  };
  const addPlacementPrimitive = async (
    kind: PlacementPrimitiveKind,
    requestedBounds: PlacementBounds,
    lineDirection: PlacementLineDirection = "down_right",
  ) => {
    if (objectSceneSelected && selectedSceneDocument !== null) {
      const elements = baseObjectRows(selectedSceneDocument, placementOwnershipScene);
      await addSceneObject({ element_id: nextPlacementElementId(kind, elements), kind,
        ...normalizePrimitiveBounds(kind, requestedBounds),
        z_order: Math.min(255, Math.max(0, ...elements.map(item => item.z_order)) + 1), layer: "SCENE", visible: true,
        ...(kind === "line" ? { line_direction: lineDirection } : {}) });
      return;
    }
    if (selectedSceneDocument === null || placementRenderModel === null) {
      return;
    }
    const bounds = normalizePrimitiveBounds(kind, requestedBounds);
    const element: RenderElement = {
      element_id: nextPlacementElementId(kind, placementRenderModel.elements),
      kind,
      ...bounds,
      z_order: Math.max(0, ...placementRenderModel.elements.map((item) => item.z_order)) + 1,
      layer: "SCENE",
      visible: true,
      ...(kind === "line" ? { line_direction: lineDirection } : {}),
    };
    if (!scopedPlacementAddSupported) {
      setMessage(`Scoped placement needs Service API 36. Restart Peep Studio if the top bar still shows Service API ${service?.service_api_version ?? "unknown"}.`);
      return;
    }
    await applyPlacementCommandBatch(
      "Adding object",
      selectedSceneDocument.scene_id,
      placementRenderModel.visual_id,
      element.element_id,
      [{
        kind: "placement_object.add",
        scene_id: selectedSceneDocument.scene_id,
        scope: placementObjectScope(),
        element,
      }],
      `Object added to ${placementEditTargetLabel()}. Save to write it to the project.`,
      placementState === null
        ? { kind: "render", id: placementRenderModel.visual_id }
        : { kind: "state", id: placementState.state_id },
    );
    setSelectedPlacementElement(element.element_id);
    setPlacementInspectorTab("object");
  };
  const selectPlacementElement = (elementId: string) => {
    setSelectedPlacementElement(elementId);
    setPlacementInspectorTab("object");
    if (placementRenderModel !== null) {
      setSceneSelection({ kind: "render", id: placementRenderModel.visual_id });
    }
  };
  const selectPlacementBase = (elementId?: string) => {
    placementSelectionAnchorRef.current = null;
    setPlacementEditStateIds([]);
    setPlacementStateId(null);
    if (elementId !== undefined) {
      setSelectedPlacementElement(elementId);
      setPlacementInspectorTab("object");
    }
    if (placementRenderModel !== null) {
      setSceneSelection({ kind: "render", id: placementRenderModel.visual_id });
    }
  };
  const selectPlacementState = (stateId: string, event?: ReactMouseEvent<HTMLElement>) => {
    const states = selectedSceneDocument?.states ?? [];
    const state = (selectedSceneDocument?.states ?? []).find((item) => item.state_id === stateId) ?? null;
    if (state === null) {
      return;
    }
    const orderedIds = states.map((item) => item.state_id);
    const additive = event?.ctrlKey === true || event?.metaKey === true;
    const range = event?.shiftKey === true;
    let nextIds: string[];
    if (range && placementSelectionAnchorRef.current !== null) {
      const anchorIndex = orderedIds.indexOf(placementSelectionAnchorRef.current);
      const stateIndex = orderedIds.indexOf(stateId);
      if (anchorIndex >= 0 && stateIndex >= 0) {
        const start = Math.min(anchorIndex, stateIndex);
        const end = Math.max(anchorIndex, stateIndex);
        const rangeIds = orderedIds.slice(start, end + 1);
        nextIds = additive
          ? orderedIds.filter((id) => placementEditStateIds.includes(id) || rangeIds.includes(id))
          : rangeIds;
      } else {
        nextIds = [stateId];
      }
    } else if (additive) {
      const selectedIds = new Set(placementEditStateIds);
      if (selectedIds.has(stateId)) {
        selectedIds.delete(stateId);
      } else {
        selectedIds.add(stateId);
      }
      nextIds = orderedIds.filter((id) => selectedIds.has(id));
      placementSelectionAnchorRef.current = stateId;
    } else {
      nextIds = [stateId];
      placementSelectionAnchorRef.current = stateId;
    }
    const nextPrimary = nextIds.includes(stateId)
      ? stateId
      : nextIds.includes(placementStateId ?? "")
        ? placementStateId
        : nextIds.at(-1) ?? null;
    setPlacementEditStateIds(nextIds);
    setPlacementStateId(nextPrimary);
    if (nextPrimary === null) {
      if (placementRenderModel !== null) {
        setSceneSelection({ kind: "render", id: placementRenderModel.visual_id });
      }
    } else {
      setSceneSelection({ kind: "state", id: nextPrimary });
    }
  };
  const selectPlacementStateObject = (stateId: string, elementId: string) => {
    placementSelectionAnchorRef.current = stateId;
    setPlacementEditStateIds([stateId]);
    setPlacementStateId(stateId);
    setSelectedPlacementElement(elementId);
    setPlacementInspectorTab("object");
    setSceneSelection({ kind: "state", id: stateId });
  };
  const handlePlacementHierarchyKeyDown = (event: ReactKeyboardEvent<HTMLDivElement>) => {
    if (!(event.ctrlKey || event.metaKey) || event.key.toLowerCase() !== "a") {
      return;
    }
    const stateIds = (selectedSceneDocument?.states ?? []).map((state) => state.state_id);
    if (stateIds.length === 0) {
      return;
    }
    event.preventDefault();
    const primary = placementStateId !== null && stateIds.includes(placementStateId)
      ? placementStateId
      : stateIds[0];
    placementSelectionAnchorRef.current = primary;
    setPlacementEditStateIds(stateIds);
    setPlacementStateId(primary);
    setSceneSelection({ kind: "state", id: primary });
  };
  const renderPlacementEditScope = () => {
    const targetStates = (selectedSceneDocument?.states ?? []).filter((state) => placementEditStateIds.includes(state.state_id));
    return (
      <section className="inspector-section placement-edit-scope-section">
        <h3><SquareMousePointer size={14} aria-hidden="true" /> Edit target</h3>
        {selectedSceneDocument === null ? (
          <p className="muted">Select a scene to choose where placement edits apply.</p>
        ) : (
          <div className="placement-edit-scope">
            <div className="placement-scope-header">
              <span>{targetStates.length === 0 ? <Layers3 size={16} aria-hidden="true" /> : <Network size={16} aria-hidden="true" />}</span>
              <span>
                <strong>{placementEditTargetLabel()}</strong>
                <small>{targetStates.length === 0 ? "Scene base" : `${targetStates.length} state${targetStates.length === 1 ? "" : "s"}`}</small>
              </span>
              <code>{targetStates.length === 0 ? "BASE" : targetStates.length}</code>
            </div>
            {targetStates.length > 1 && (
              <div className="placement-target-chips">
                {targetStates.map((state) => (
                  <span className={placementStateId === state.state_id ? "primary" : ""} key={state.state_id}>
                    {state.display_name}
                  </span>
                ))}
              </div>
            )}
            <p className="muted">Preview: {placementState?.display_name ?? "Base Placement"}</p>
          </div>
        )}
      </section>
    );
  };
  const toggleHierarchyScene = (sceneId: string) => {
    setExpandedSceneIds((current) => (
      current.includes(sceneId)
        ? current.filter((id) => id !== sceneId)
        : [...current, sceneId]
    ));
  };
  const toggleHierarchyGroup = (groupId: string) => {
    setCollapsedHierarchyIds((current) => (
      current.includes(groupId)
        ? current.filter((id) => id !== groupId)
        : [...current, groupId]
    ));
  };
  const selectHierarchyScene = (sceneId: string) => {
    setSelectedScene(sceneId);
    setSceneSelection({ kind: "scene" });
  };
  const openHierarchyPlacementTarget = async (
    scene: SceneDocument,
    stateId: string | null,
    elementId?: string,
  ) => {
    setWorkspaceMode("placement");
    if (selectedScene !== scene.scene_id) {
      const started = await startPreview(scene.scene_id, {
        ...(stateId === null ? {} : { stateId }),
        updateSelection: false,
      });
      if (!started) {
        return;
      }
    }
    placementSelectionAnchorRef.current = stateId;
    setPlacementEditStateIds(stateId === null ? [] : [stateId]);
    setPlacementStateId(stateId);
    setSelectedPlacementElement(elementId ?? null);
    if (elementId !== undefined) {
      setPlacementInspectorTab("object");
    }
    const renderModel = scene.render_models?.[0] ?? null;
    setSceneSelection(stateId === null
      ? renderModel === null ? { kind: "scene" } : { kind: "render", id: renderModel.visual_id }
      : { kind: "state", id: stateId });
  };
  const openHierarchyState = (
    scene: SceneDocument,
    state: StateRecord,
    event: ReactMouseEvent<HTMLElement>,
  ) => {
    if (workspaceMode === "placement") {
      if (selectedScene === scene.scene_id) {
        selectPlacementState(state.state_id, event);
      } else {
        void openHierarchyPlacementTarget(scene, state.state_id);
      }
      return;
    }
    setSelectedScene(scene.scene_id);
    setSelectedPlacementElement(null);
    setSceneSelection({ kind: "state", id: state.state_id });
  };
  const openHierarchyVariable = async (scene: SceneDocument) => {
    setWorkspaceMode("logic");
    if (selectedScene !== scene.scene_id) {
      const started = await startPreview(scene.scene_id, { updateSelection: false });
      if (!started) {
        return;
      }
    }
    setSceneSelection({ kind: "scene" });
  };
  const renderSceneHierarchy = () => (
    <nav
      className="scene-hierarchy"
      aria-label="Project scene hierarchy"
      onKeyDown={handlePlacementHierarchyKeyDown}
    >
      {scenes.map((scene) => {
        const renderModel = scene.render_models?.[0] ?? null;
        const ownership = project?.placement_ownership?.scenes[scene.scene_id] ?? null;
        const elements = [...baseObjectRows(scene, ownership)].sort((left, right) => left.z_order - right.z_order);
        const objectLabelCounts = new Map<string, number>();
        const objectLabelById = new Map<string, string>();
        for (const element of elements) {
          const baseLabel = placementObjectLabelBase(element);
          const occurrence = (objectLabelCounts.get(baseLabel) ?? 0) + 1;
          objectLabelCounts.set(baseLabel, occurrence);
          objectLabelById.set(
            element.element_id,
            occurrence === 1 ? baseLabel : `${baseLabel} ${occurrence}`,
          );
        }
        const stateScopedElementIds = new Set(ownership?.state_scoped_element_ids ?? []);
        const stateIds = (scene.states ?? []).map((state) => state.state_id);
        const fullyStateControlledElementIds = new Set(
          elements
            .filter((element) => (
              stateIds.length > 0
              && stateIds.every((stateId) => (
                (ownership?.states[stateId]?.changes[element.element_id]?.local_properties.length ?? 0) > 0
              ))
            ))
            .map((element) => element.element_id),
        );
        const baseElements = usesSceneObjects(scene) ? elements : elements.filter((element) => (
          !stateScopedElementIds.has(element.element_id)
          && !fullyStateControlledElementIds.has(element.element_id)
        ));
        const renderObjectRow = (
          element: RenderElement,
          key: string,
          onClick: () => void,
          badges: string[],
          selected: boolean,
        ) => (
          <button
            key={key}
            className={`placement-tree-object ${selected ? "selected" : ""}`}
            type="button"
            onClick={onClick}
          >
            <span className="placement-object-kind">
              {placementKindIcon(element.kind)}
              <span>
                <strong>{objectLabelById.get(element.element_id) ?? placementObjectLabelBase(element)}</strong>
                <small>{placementKindLabel(element.kind)}</small>
              </span>
            </span>
            <span className="placement-object-badges">
              {badges.map((badge) => <code key={badge}>{badge}</code>)}
            </span>
          </button>
        );
        const sceneSelected = sceneSelection.kind !== "project" && selectedScene === scene.scene_id;
        const expanded = expandedSceneIds.includes(scene.scene_id);
        const baseGroupId = `${scene.scene_id}:base`;
        const statesGroupId = `${scene.scene_id}:states`;
        const variablesGroupId = `${scene.scene_id}:variables`;
        const baseExpanded = !collapsedHierarchyIds.includes(baseGroupId);
        const statesExpanded = !collapsedHierarchyIds.includes(statesGroupId);
        const variablesExpanded = !collapsedHierarchyIds.includes(variablesGroupId);
        return (
          <section
            className={`scene-hierarchy-node ${sceneSelected ? "selected" : ""}`}
            key={scene.scene_id}
          >
            <div className="scene-hierarchy-row">
              <button
                className="hierarchy-disclosure-control"
                type="button"
                aria-expanded={expanded}
                aria-label={`${expanded ? "Collapse" : "Expand"} ${scene.display_name}`}
                title={`${expanded ? "Collapse" : "Expand"} ${scene.display_name}`}
                onClick={() => toggleHierarchyScene(scene.scene_id)}
              >
                <ChevronRight className={expanded ? "expanded" : ""} size={15} aria-hidden="true" />
              </button>
              <button
                className="scene-hierarchy-select"
                type="button"
                onClick={() => selectHierarchyScene(scene.scene_id)}
                onDoubleClick={() => toggleHierarchyScene(scene.scene_id)}
              >
                <FileCode2 size={16} aria-hidden="true" />
                <span>
                  <strong>{scene.display_name}</strong>
                  <small>{scene.scene_type}</small>
                </span>
              </button>
            </div>
            {expanded && <div className="scene-hierarchy-children">
              <section className="hierarchy-branch base-branch">
                <div className="hierarchy-branch-row">
                  <button
                    className="hierarchy-disclosure-control"
                    type="button"
                    aria-expanded={baseExpanded}
                    aria-label={`${baseExpanded ? "Collapse" : "Expand"} Base objects`}
                    title={`${baseExpanded ? "Collapse" : "Expand"} Base objects`}
                    onClick={() => toggleHierarchyGroup(baseGroupId)}
                  >
                    <ChevronRight className={baseExpanded ? "expanded" : ""} size={14} aria-hidden="true" />
                  </button>
                  <button
                    className={`hierarchy-branch-select ${sceneSelected && workspaceMode === "placement" && placementEditStateIds.length === 0 ? "selected" : ""}`}
                    type="button"
                    onClick={() => {
                      setSelectedScene(scene.scene_id);
                      setPlacementStateId(null);
                      setPlacementEditStateIds([]);
                      setSceneSelection(renderModel ? { kind: "render", id: renderModel.visual_id } : { kind: "scene" });
                    }}
                    onDoubleClick={() => toggleHierarchyGroup(baseGroupId)}
                  >
                    <Layers3 size={15} aria-hidden="true" />
                    <span>
                      <strong>Base objects</strong>
                      <small>Scene-owned placement</small>
                    </span>
                    <span className="hierarchy-state-meta">
                      {sceneSelected && workspaceMode === "placement" && placementStateId === null && (
                        <span className="hierarchy-placement-indicator" title="Placement preview" aria-label="Placement preview">
                          <Eye size={13} aria-hidden="true" />
                        </span>
                      )}
                      <code>{baseElements.length}</code>
                    </span>
                  </button>
                </div>
                {baseExpanded && baseElements.length > 0 && (
                  <div className="placement-object-children">
                    {baseElements.map((element) => renderObjectRow(
                      element,
                      `${scene.scene_id}:base:${element.element_id}`,
                      () => void openHierarchyPlacementTarget(scene, null, element.element_id),
                      [
                        placementLayerLabel(element),
                        ...(element.visible === false ? ["Hidden"] : []),
                        `z${element.z_order}`,
                      ],
                      sceneSelected
                        && workspaceMode === "placement"
                        && placementEditStateIds.length === 0
                        && selectedPlacementElement === element.element_id,
                    ))}
                  </div>
                )}
              </section>
              <section className="hierarchy-branch states-branch">
                <div className="hierarchy-branch-row">
                  <button
                    className="hierarchy-disclosure-control"
                    type="button"
                    aria-expanded={statesExpanded}
                    aria-label={`${statesExpanded ? "Collapse" : "Expand"} States`}
                    title={`${statesExpanded ? "Collapse" : "Expand"} States`}
                    onClick={() => toggleHierarchyGroup(statesGroupId)}
                  >
                    <ChevronRight className={statesExpanded ? "expanded" : ""} size={14} aria-hidden="true" />
                  </button>
                  <button
                    className="hierarchy-branch-select"
                    type="button"
                    onClick={() => selectHierarchyScene(scene.scene_id)}
                    onDoubleClick={() => toggleHierarchyGroup(statesGroupId)}
                  >
                    <Network size={15} aria-hidden="true" />
                    <span>
                      <strong>States</strong>
                      <small>Local object changes</small>
                    </span>
                    <code>{scene.states?.length ?? 0}</code>
                  </button>
                </div>
                {statesExpanded && <div className="hierarchy-nested-branches">
                  {(scene.states ?? []).map((state) => {
                    const projection = ownership?.states[state.state_id] ?? null;
                    const changes = projection?.changes ?? {};
                    const resolvedById = new Map(
                      (projection?.resolved_elements ?? []).map((element) => [element.element_id, element]),
                    );
                    const changedElements = elements
                      .filter((element) => changes[element.element_id] !== undefined)
                      .map((element) => resolvedById.get(element.element_id) ?? element);
                    const selected = sceneSelected && (
                      workspaceMode === "placement"
                        ? placementEditStateIds.includes(state.state_id)
                        : sceneSelection.kind === "state" && sceneSelection.id === state.state_id
                    );
                    const primary = sceneSelected
                      && workspaceMode === "placement"
                      && placementStateId === state.state_id;
                    const active = preview?.scene.scene_id === scene.scene_id && preview.scene.state_id === state.state_id;
                    const stateGroupId = `${scene.scene_id}:state:${state.state_id}`;
                    const stateExpanded = !collapsedHierarchyIds.includes(stateGroupId);
                    return (
                      <section className="hierarchy-branch state-branch" key={state.state_id}>
                        <div className="hierarchy-branch-row">
                          <button
                            className="hierarchy-disclosure-control"
                            type="button"
                            aria-expanded={stateExpanded}
                            aria-label={`${stateExpanded ? "Collapse" : "Expand"} ${state.display_name}`}
                            title={`${stateExpanded ? "Collapse" : "Expand"} ${state.display_name}`}
                            onClick={() => toggleHierarchyGroup(stateGroupId)}
                          >
                            <ChevronRight className={stateExpanded ? "expanded" : ""} size={14} aria-hidden="true" />
                          </button>
                          <button
                            className={`hierarchy-branch-select ${selected ? "selected" : ""}`}
                            type="button"
                            aria-selected={selected}
                            aria-current={active ? "step" : undefined}
                            onClick={(event) => { if (event.detail < 2) openHierarchyState(scene, state, event); }}
                            onDoubleClick={() => toggleHierarchyGroup(stateGroupId)}
                          >
                            <span className="hierarchy-state-icon">
                              <Network size={15} aria-hidden="true" />
                              {active && (
                                <span className="hierarchy-runtime-indicator" title="Emulator active" aria-label="Emulator active" />
                              )}
                            </span>
                            <span>
                              <strong>{state.display_name}</strong>
                              <small>
                                {active && primary
                                  ? "Emulator active / Placement preview"
                                  : active
                                    ? "Emulator active"
                                    : primary
                                      ? "Placement preview"
                                      : state.state_id}
                              </small>
                            </span>
                            <span className="hierarchy-state-meta">
                              {primary && (
                                <span className="hierarchy-placement-indicator" title="Placement preview" aria-label="Placement preview">
                                  <Eye size={13} aria-hidden="true" />
                                </span>
                              )}
                              <code>{changedElements.length}</code>
                            </span>
                          </button>
                          <button
                            className="hierarchy-emulator-load"
                            type="button"
                            disabled={!projectValid || busy !== null}
                            aria-label={`Load ${state.display_name} in emulator`}
                            title={`Load ${state.display_name} in emulator`}
                            onClick={() => void startPreview(scene.scene_id, {
                              stateId: state.state_id,
                              updateSelection: false,
                            })}
                          >
                            <MonitorDot size={15} aria-hidden="true" />
                          </button>
                        </div>
                        {stateExpanded && changedElements.length > 0 && (
                          <div className="placement-object-children">
                            {changedElements.map((element) => {
                              const change = changes[element.element_id];
                              const badges = [
                                ...(change?.local_properties.includes("position") ? ["Position"] : []),
                                ...(change?.local_properties.includes("visible") ? ["Visibility"] : []),
                                ...(change?.local_properties.includes("visual_ref") ? ["Frame"] : []),
                                ...(change?.animated ? ["Anim"] : []),
                              ];
                              return renderObjectRow(
                                element,
                                `${scene.scene_id}:${state.state_id}:${element.element_id}`,
                                () => void openHierarchyPlacementTarget(scene, state.state_id, element.element_id),
                                badges,
                                sceneSelected && primary && selectedPlacementElement === element.element_id,
                              );
                            })}
                          </div>
                        )}
                      </section>
                    );
                  })}
                </div>}
              </section>
              {(scene.variables?.length ?? 0) > 0 && (
                <section className="hierarchy-branch variables-branch">
                  <div className="hierarchy-branch-row">
                    <button
                      className="hierarchy-disclosure-control"
                      type="button"
                      aria-expanded={variablesExpanded}
                      aria-label={`${variablesExpanded ? "Collapse" : "Expand"} Variables`}
                      title={`${variablesExpanded ? "Collapse" : "Expand"} Variables`}
                      onClick={() => toggleHierarchyGroup(variablesGroupId)}
                    >
                      <ChevronRight className={variablesExpanded ? "expanded" : ""} size={14} aria-hidden="true" />
                    </button>
                    <button
                    className="hierarchy-branch-select"
                    type="button"
                    onClick={() => selectHierarchyScene(scene.scene_id)}
                    onDoubleClick={() => toggleHierarchyGroup(variablesGroupId)}
                    >
                      <Type size={15} aria-hidden="true" />
                      <span>
                        <strong>Variables</strong>
                        <small>Scene-local data</small>
                      </span>
                      <code>{scene.variables?.length ?? 0}</code>
                    </button>
                  </div>
                  {variablesExpanded && <div className="hierarchy-reference-rows">
                    {scene.variables?.map((variable) => (
                      <button
                        className="scene-reference-row"
                        key={variable.variable_id}
                        type="button"
                        onClick={() => void openHierarchyVariable(scene)}
                      >
                        <Type size={14} aria-hidden="true" />
                        <span>
                          <strong>{variable.variable_id}</strong>
                          <small>{variable.value_type}</small>
                        </span>
                      </button>
                    ))}
                  </div>}
                </section>
              )}
            </div>}
          </section>
        );
      })}
    </nav>
  );
  const renderPlacementInspector = () => {
    const elements = [...effectivePlacementElements].sort((left, right) => left.z_order - right.z_order);
    const selectedElement = selectedPlacementElement === null
      ? null
      : elements.find((element) => element.element_id === selectedPlacementElement) ?? null;
    if (objectSceneSelected) {
      const object = placementOwnershipScene?.objects?.find((item) => item.object_id === selectedPlacementElement);
      if (selectedSceneDocument === null) return null;
      return <>{renderPlacementEditScope()}<SceneObjectInspector
        key={`${selectedSceneDocument.scene_id}:${selectedPlacementElement}:${placementEditStateIds.join(",")}`}
        scene={selectedSceneDocument} object={object}
        label={selectedElement === null ? "" : placementObjectLabelBase(selectedElement)}
        stateIds={placementEditStateTargets()} ownership={placementOwnershipScene}
        frames={compiledAssetFrames} clips={project?.document?.animations ?? []} busy={busy !== null}
        supports={kind => supportsObjectCommand(service, selectedSceneCapability, kind)}
        onApply={applySceneObjectCommands}
      /></>;
    }
    const selectedElementIsShape = selectedElement !== null && selectedElement.kind !== "sprite";
    const selectedSpriteFrame = selectedElement?.kind === "sprite" && selectedElement.visual_ref !== undefined
      ? compiledAssetFrameById.get(selectedElement.visual_ref) ?? null
      : null;
    const selectedSpriteFrames = selectedSpriteFrame === null
      ? []
      : compiledAssetFrameGroups.find((group) => group.assetId === selectedSpriteFrame.asset_id)?.frames ?? [];
    const selectedSpriteFramesFit =
      selectedElement !== null &&
      selectedSpriteFrames.length >= 2 &&
      selectedSpriteFrames.length <= 4 &&
      selectedSpriteFrames.every((frame) => frame.width === selectedElement.width && frame.height === selectedElement.height);
    const targetStateIds = placementEditStateTargets();
    const animationStateIds = placementAnimationStateTargets();
    const placementPropertyOverrideCount = (property: "position" | "visible" | "visual_ref") => {
      if (selectedElement === null) {
        return 0;
      }
      return targetStateIds.filter((stateId) => (
        placementOwnershipScene?.states[stateId]?.changes[selectedElement.element_id]
          ?.local_properties.includes(property) === true
      )).length;
    };
    const placementPropertyStatus = (property: "position" | "visible" | "visual_ref") => {
      if (targetStateIds.length === 0) {
        return "Scene base";
      }
      const count = placementPropertyOverrideCount(property);
      if (count === 0) {
        return "Inherited";
      }
      return count === targetStateIds.length ? "Local" : "Mixed";
    };
    const clearPlacementOverrides = (properties: Array<"position" | "visible" | "visual_ref">) => {
      if (selectedSceneDocument === null || placementRenderModel === null || selectedElement === null || targetStateIds.length === 0) {
        return;
      }
      void applyPlacementCommandBatch(
        "Restoring inherited placement",
        selectedSceneDocument.scene_id,
        placementRenderModel.visual_id,
        selectedElement.element_id,
        targetStateIds.map((stateId) => ({
          kind: "state_placement.clear_override",
          scene_id: selectedSceneDocument.scene_id,
          state_id: stateId,
          element_id: selectedElement.element_id,
          properties,
        })),
        `Inherited ${properties.join(", ")} restored for ${placementEditTargetLabel()}. Save to write it to the project.`,
        placementState === null
          ? { kind: "render", id: placementRenderModel.visual_id }
          : { kind: "state", id: placementState.state_id },
      );
    };
    const selectedSpriteAnimationCount = selectedElement === null
      ? 0
      : animationStateIds.filter((stateId) => (
          placementOwnershipScene?.states[stateId]?.changes[selectedElement.element_id]?.animated === true
        )).length;
    const commitPositionInput = (axis: "x" | "y", value: string) => {
      if (selectedElement === null || placementRenderModel === null) {
        return;
      }
      const parsed = Number(value);
      if (!Number.isFinite(parsed)) {
        return;
      }
      movePlacementElement(
        selectedElement,
        placementRenderModel.visual_id,
        axis === "x" ? parsed : selectedElement.x,
        axis === "y" ? parsed : selectedElement.y,
      );
    };
    const commitBoundsInput = (axis: "width" | "height", value: string) => {
      if (selectedSceneDocument === null || selectedElement === null || placementRenderModel === null || targetStateIds.length > 0) {
        return;
      }
      const parsed = Math.round(Number(value));
      if (!Number.isFinite(parsed)) {
        return;
      }
      const nextWidth = axis === "width" ? parsed : selectedElement.width;
      const nextHeight = axis === "height" ? parsed : selectedElement.height;
      resizePlacementElement(selectedElement, placementRenderModel.visual_id, {
        x: selectedElement.x,
        y: selectedElement.y,
        width: nextWidth,
        height: nextHeight,
      });
    };
    const setElementLayer = (layer: "BACKGROUND" | "SCENE" | "UI") => {
      if (selectedSceneDocument === null || selectedElement === null || placementRenderModel === null || targetStateIds.length > 0) {
        return;
      }
      void applyRenderElementCommand(
        "Changing layer",
        selectedSceneDocument.scene_id,
        placementRenderModel.visual_id,
        selectedElement.element_id,
        { kind: "render_element.set_layer", layer },
        "Layer updated. Save to write it to the project.",
      );
    };
    const setElementVisibility = (visible: boolean) => {
      if (selectedSceneDocument === null || selectedElement === null || placementRenderModel === null) {
        return;
      }
      const commands = scopedVisibilityCommands(selectedElement, placementRenderModel.visual_id, visible);
      if (commands.length === 0) {
        return;
      }
      const targetStateIds = placementEditStateTargets();
      void applyPlacementCommandBatch(
        "Changing visibility",
        selectedSceneDocument.scene_id,
        placementRenderModel.visual_id,
        selectedElement.element_id,
        commands,
        `Visibility updated for ${placementEditTargetLabel()}. Save to write it to the project.`,
        targetStateIds.length === 1 ? { kind: "state", id: targetStateIds[0] } : { kind: "render", id: placementRenderModel.visual_id },
      );
    };
    return (
      <>
        {renderPlacementEditScope()}
        <section className="inspector-section placement-inspector">
        <h3><Layers3 size={14} aria-hidden="true" /> Object</h3>
        {selectedSceneDocument === null ? (
          <p className="muted">Select a scene to inspect placement.</p>
        ) : placementRenderModel === null ? (
          <p className="muted">This state has no placed-object view.</p>
        ) : selectedElement === null ? (
          <p className="muted">Select an object from the canvas or object hierarchy.</p>
        ) : (
          <>
            <dl className="inspector-list">
              <div><dt>Object</dt><dd>{placementKindLabel(selectedElement.kind)}</dd></div>
              <div><dt>Layer</dt><dd>{placementLayerLabel(selectedElement)}</dd></div>
              <div><dt>Visible</dt><dd>{selectedElement.visible === false ? "No" : "Yes"}</dd></div>
              <div><dt>Source</dt><dd>{placementSourceLabel(selectedElement)}</dd></div>
              <div><dt>Width</dt><dd>{selectedElement.width}</dd></div>
              <div><dt>Height</dt><dd>{selectedElement.height}</dd></div>
              <div><dt>Draw order</dt><dd>{selectedElement.z_order}</dd></div>
              <div><dt>Internal ID</dt><dd>{selectedElement.element_id}</dd></div>
            </dl>
            {targetStateIds.length > 0 && (
              <div className="placement-override-status">
                {([
                  ["Position", "position"],
                  ["Visibility", "visible"],
                  ...(selectedElement.kind === "sprite" ? [["Frame", "visual_ref"]] : []),
                ] as Array<[string, "position" | "visible" | "visual_ref"]>).map(([label, property]) => {
                  const overrideCount = placementPropertyOverrideCount(property);
                  return (
                    <div key={property}>
                      <span>{label}</span>
                      <code>{placementPropertyStatus(property)}</code>
                      <button
                        type="button"
                        disabled={busy !== null || overrideCount === 0}
                        onClick={() => clearPlacementOverrides([property])}
                        title={`Restore inherited ${label.toLowerCase()}`}
                        aria-label={`Restore inherited ${label.toLowerCase()}`}
                      >
                        <RotateCcw size={13} aria-hidden="true" />
                      </button>
                    </div>
                  );
                })}
              </div>
            )}
            <div className="placement-position-editor">
              <label>
                X
                <input
                  key={`${selectedElement.element_id}-x-${selectedElement.x}`}
                  type="number"
                  min="0"
                  max={Math.max(0, 168 - selectedElement.width)}
                  defaultValue={selectedElement.x}
                  onBlur={(event) => commitPositionInput("x", event.target.value)}
                  onKeyDown={(event) => {
                    if (event.key === "Enter") {
                      event.currentTarget.blur();
                    }
                  }}
                />
              </label>
              <label>
                Y
                <input
                  key={`${selectedElement.element_id}-y-${selectedElement.y}`}
                  type="number"
                  min="0"
                  max={Math.max(0, 144 - selectedElement.height)}
                  defaultValue={selectedElement.y}
                  onBlur={(event) => commitPositionInput("y", event.target.value)}
                  onKeyDown={(event) => {
                    if (event.key === "Enter") {
                      event.currentTarget.blur();
                    }
                  }}
                />
              </label>
              {selectedElementIsShape && (
                <>
                  <label>
                    Width
                    <input
                      key={`${selectedElement.element_id}-width-${selectedElement.width}`}
                      type="number"
                      min="1"
                      max={168 - selectedElement.x}
                      defaultValue={selectedElement.width}
                      disabled={busy !== null || targetStateIds.length > 0}
                      onBlur={(event) => commitBoundsInput("width", event.target.value)}
                      onKeyDown={(event) => {
                        if (event.key === "Enter") {
                          event.currentTarget.blur();
                        }
                      }}
                    />
                  </label>
                  <label>
                    Height
                    <input
                      key={`${selectedElement.element_id}-height-${selectedElement.height}`}
                      type="number"
                      min="1"
                      max={144 - selectedElement.y}
                      defaultValue={selectedElement.height}
                      disabled={busy !== null || targetStateIds.length > 0}
                      onBlur={(event) => commitBoundsInput("height", event.target.value)}
                      onKeyDown={(event) => {
                        if (event.key === "Enter") {
                          event.currentTarget.blur();
                        }
                      }}
                    />
                  </label>
                </>
              )}
              <label>
                Layer
                <select
                  value={placementLayerLabel(selectedElement)}
                  disabled={busy !== null || targetStateIds.length > 0}
                  onChange={(event) => setElementLayer(event.target.value as "BACKGROUND" | "SCENE" | "UI")}
                >
                  <option value="BACKGROUND">Background</option>
                  <option value="SCENE">Scene</option>
                  <option value="UI">UI</option>
                </select>
              </label>
              <div className="placement-order-controls">
                <span>Draw order</span>
                <div>
                  <button
                    type="button"
                    disabled={busy !== null || targetStateIds.length > 0 || selectedElement.z_order <= 0}
                    onClick={() => setPlacementElementZOrder(selectedElement, placementRenderModel.visual_id, selectedElement.z_order - 1)}
                    title="Send backward"
                  >
                    <ArrowDown size={14} aria-hidden="true" />
                    Back
                  </button>
                  <button
                    type="button"
                    disabled={busy !== null || targetStateIds.length > 0 || selectedElement.z_order >= 255}
                    onClick={() => setPlacementElementZOrder(selectedElement, placementRenderModel.visual_id, selectedElement.z_order + 1)}
                    title="Bring forward"
                  >
                    <ArrowUp size={14} aria-hidden="true" />
                    Forward
                  </button>
                </div>
              </div>
              <label className="placement-toggle-row">
                <input
                  type="checkbox"
                  checked={selectedElement.visible !== false}
                  disabled={busy !== null}
                  onChange={(event) => setElementVisibility(event.target.checked)}
                />
                Visible
              </label>
            </div>
            {selectedElement.kind === "sprite" && (
              <div className="placement-animation-editor">
                <div className="placement-animation-heading">
                  <span>Animation</span>
                  <code>{selectedSpriteAnimationCount === 0
                    ? "Static"
                    : selectedSpriteAnimationCount === animationStateIds.length
                      ? "Animated"
                      : "Mixed"}</code>
                </div>
                {selectedSpriteFrames.length > 0 && (
                  <div className="placement-animation-strip" aria-label="Sprite frames">
                    {selectedSpriteFrames.map((frame) => (
                      <span key={frame.frame_id} title={`${placementFrameLabel(frame)} (${frame.width}x${frame.height})`}>
                        <FramePreviewCanvas frame={frame} />
                      </span>
                    ))}
                  </div>
                )}
                {animationStateIds.length === 0 ? (
                  <p className="muted">This scene has no states available for animation.</p>
                ) : selectedSpriteFrame === null ? (
                  <p className="muted">This sprite's frame is not available.</p>
                ) : !placementAnimationSupported ? (
                  <p className="muted">Restart Peep Studio for sprite animation editing.</p>
                ) : selectedSpriteFrames.length <= 1 ? (
                  <p className="muted">This sprite asset has one frame.</p>
                ) : selectedSpriteFrames.length > 4 ? (
                  <p className="muted">STATE sprite animation supports 2 to 4 frames for now.</p>
                ) : !selectedSpriteFramesFit ? (
                  <p className="muted">All animation frames must match this object's size.</p>
                ) : (
                  <label className="placement-animation-toggle">
                    <input
                      type="checkbox"
                      checked={selectedSpriteAnimationCount === animationStateIds.length}
                      disabled={busy !== null}
                      onChange={(event) => {
                        if (event.target.checked) {
                          void bindPlacementSpriteAnimation(selectedElement, placementRenderModel.visual_id, selectedSpriteFrames);
                        } else {
                          void clearPlacementSpriteAnimation(selectedElement, placementRenderModel.visual_id);
                        }
                      }}
                    />
                    Animated
                  </label>
                )}
              </div>
            )}
            <div className="placement-danger-zone">
              <button
                type="button"
                disabled={busy !== null}
                onClick={() => deletePlacementElement(selectedElement, placementRenderModel.visual_id)}
              >
                <Trash2 size={14} aria-hidden="true" />
                {targetStateIds.length === 0 ? "Delete object" : `Remove from ${targetStateIds.length} state${targetStateIds.length === 1 ? "" : "s"}`}
              </button>
            </div>
          </>
        )}
        </section>
      </>
    );
  };
  const projectRootSelected = sceneSelection.kind === "project";
  const renderProjectInspector = () => (
    <>
      <section className="inspector-section project-inspector">
        <h3><Box size={14} aria-hidden="true" /> Project</h3>
        {project === null ? (
          <p className="muted">No project open.</p>
        ) : (
          <>
            <div className="project-inspector-title">
              <strong>{project.summary.project_name}</strong>
              <span className={`validation-state ${project.valid ? "valid" : "invalid"}`}>
                <StatusMark ok={project.valid} /> {project.valid ? "Valid" : "Invalid"}
              </span>
            </div>
            <dl className="inspector-list">
              <div><dt>Package</dt><dd>{project.summary.package_id}</dd></div>
              <div><dt>Target</dt><dd>{project.summary.target_profile}</dd></div>
              <div><dt>Entry scene</dt><dd>{project.summary.entry_scene}</dd></div>
              <div><dt>Source</dt><dd>{temporaryProject ? "Example copy" : "Project"}</dd></div>
              <div><dt>Edits</dt><dd>{dirty ? "Unsaved" : "Clean"}</dd></div>
              <div><dt>Path</dt><dd title={projectPath ?? undefined}>{projectPath ?? "-"}</dd></div>
              <div><dt>Scenes</dt><dd>{project.summary.scene_count}</dd></div>
              <div><dt>Frames</dt><dd>{project.summary.asset_frame_count}</dd></div>
              <div><dt>Animations</dt><dd>{project.summary.animation_count}</dd></div>
              <div><dt>SFX</dt><dd>{project.summary.audio_cue_count}</dd></div>
            </dl>
            <label className="select-field">Entry scene
              <select aria-label="Project entry scene" value={project.summary.entry_scene}
                disabled={busy !== null || !(service?.scene_creation?.entry_scene_command === "project.set_entry_scene"
                  || service?.state_scene_graph.scene_commands.includes("project.set_entry_scene") === true)}
                onChange={event => void setPackageEntryScene(event.target.value)}>
                {scenes.map(scene => <option key={scene.scene_id} value={scene.scene_id}
                  disabled={scene.schema_version === 2 && !supportsObjectCommand(service, project.scene_capabilities?.[scene.scene_id], "project.set_entry_scene")}>
                  {scene.display_name}
                </option>)}
              </select>
            </label>
          </>
        )}
      </section>
      <section className="inspector-section">
        <h3>Validation</h3>
        {project === null ? (
          <p className="muted">No validation result.</p>
        ) : project.issues.length === 0 && (project.build_issues ?? []).length === 0 ? (
          <div className="success-note"><PackageCheck size={17} aria-hidden="true" /> No source validation issues.</div>
        ) : (
          <div className="issue-list">
            {[...project.issues, ...(project.build_issues ?? [])].map((issue, index) => (
              <div className="issue" key={`${issue.code}-${index}`}>
                <AlertTriangle size={16} aria-hidden="true" />
                <span><strong>{issue.code}</strong>{issue.message}<small>{issue.path}</small></span>
                {issue.scene_id && (
                  <button className="icon-button" title="Locate scene" onClick={() => {
                    setSelectedScene(issue.scene_id!);
                    setWorkspaceMode("placement");
                    setPlacementStateId(issue.state_id ?? null);
                    setPlacementEditStateIds(issue.state_id ? [issue.state_id] : []);
                    setSelectedPlacementElement(null);
                    setSceneSelection(issue.state_id ? { kind: "state", id: issue.state_id } : { kind: "scene" });
                  }}><ArrowRight size={16} aria-hidden="true" /></button>
                )}
              </div>
            ))}
          </div>
        )}
      </section>
      {build !== null && (
        <section className="inspector-section build-result">
          <h3>Last build</h3>
          <dl className="inspector-list">
            <div><dt>Size</dt><dd>{build.package.size_bytes} B</dd></div>
            <div><dt>Chunks</dt><dd>{build.package.chunk_count}</dd></div>
            <div><dt>Scenes</dt><dd>{build.package.scene_count}</dd></div>
          </dl>
          <code>{build.package.sha256.slice(0, 16)}...</code>
        </section>
      )}
    </>
  );

  return (
    <main className="studio-shell">
      <header className="app-toolbar">
        <div className="brand-block">
          <div className="brand-mark" aria-hidden="true">P</div>
          <div>
            <h1>Peep Studio</h1>
            <p>{project?.summary.project_name ?? "STATE authoring workbench"}</p>
          </div>
        </div>

        <div className="toolbar-actions">
          <span className={`service-state ${connected ? "connected" : "disconnected"}`}>
            <MonitorDot size={15} aria-hidden="true" />
            {connected ? `Service API ${service.service_api_version}` : "Service offline"}
          </span>
          <button className="button secondary" onClick={newProject} disabled={bridge === undefined || busy !== null || service?.operations.includes("project.create") !== true}>
            <FilePlus2 size={16} aria-hidden="true" />
            New project
          </button>
          <button className="button secondary example-button" onClick={openExample} disabled={bridge === undefined || busy !== null}>
            <FileCode2 size={16} aria-hidden="true" />
            Open example
          </button>
          <button className="button secondary" onClick={openProject} disabled={bridge === undefined || busy !== null}>
            <FolderOpen size={16} aria-hidden="true" />
            Open project
          </button>
          <button className="button primary" onClick={buildPackage} disabled={!project?.valid || busy !== null || hostOnlyProject}>
            <Hammer size={16} aria-hidden="true" />
            Build
          </button>
          <button className="icon-button" onClick={() => void stepHistory("project.undo")} disabled={!canUndo || project === null || busy !== null || service?.operations.includes("project.undo") !== true} title="Undo">
            <Undo2 size={18} aria-hidden="true" />
          </button>
          <button className="icon-button" onClick={() => void stepHistory("project.redo")} disabled={!canRedo || project === null || busy !== null || service?.operations.includes("project.redo") !== true} title="Redo">
            <Redo2 size={18} aria-hidden="true" />
          </button>
          <button className="button secondary" onClick={saveProject} disabled={!dirty || project === null || busy !== null || service?.operations.includes("project.save") !== true}>
            <Save size={16} aria-hidden="true" />
            Save
          </button>
          <button className="button secondary" onClick={saveProjectAs} disabled={project === null || projectPath === null || busy !== null || service?.operations.includes("project.save") !== true}>
            <SaveAll size={16} aria-hidden="true" />
            Save as
          </button>
          <button className="icon-button" onClick={exportPackage} disabled={build === null || busy !== null || hostOnlyProject} title="Export .egg">
            <Download size={18} aria-hidden="true" />
          </button>
        </div>
      </header>

      {hostOnlyProject && <div className="host-preview-notice" role="status">
        Host preview only: this project contains scene objects. Version-2 .egg export and device support are unavailable.
      </div>}

      <section
        className={`workspace-grid ${workspaceMode === "placement" ? "placement-mode" : workspaceMode === "scene-flow" ? "scene-flow-mode" : workspaceMode === "assets" ? "assets-mode" : "logic-mode"}`}
        style={{ "--project-width": `${projectWidth}px`, "--inspector-width": `${inspectorWidth}px` } as CSSProperties}
      >
        <aside className="project-pane">
          {renderPreviewPanel("project")}

          {project !== null && (
            <div className="project-hierarchy-root">
            <button
              className="hierarchy-disclosure-control"
              type="button"
              aria-expanded={projectHierarchyExpanded}
              aria-label={`${projectHierarchyExpanded ? "Collapse" : "Expand"} project`}
              title={`${projectHierarchyExpanded ? "Collapse" : "Expand"} project`}
              onClick={() => setProjectHierarchyExpanded((expanded) => !expanded)}
            >
              <ChevronRight className={projectHierarchyExpanded ? "expanded" : ""} size={15} aria-hidden="true" />
            </button>
            <button
              className={`project-root-row ${projectRootSelected ? "selected" : ""}`}
              type="button"
              onClick={selectProjectRoot}
              onDoubleClick={() => setProjectHierarchyExpanded((expanded) => !expanded)}
            >
              <Box size={17} aria-hidden="true" />
              <span className="project-root-copy">
                <strong>{project.summary.project_name}</strong>
                <small>Project</small>
              </span>
              <span className={`validation-state ${project.valid ? "valid" : "invalid"}`}>
                <StatusMark ok={project.valid} /> {project.valid ? "Valid" : "Invalid"}
              </span>
            </button>
            </div>
          )}

          {project === null ? (
            <div className="empty-pane">
              <Box size={25} aria-hidden="true" />
              <strong>No project open</strong>
              <span>Open a .peepproj folder to inspect scenes and package assets.</span>
            </div>
          ) : (
            projectHierarchyExpanded && renderSceneHierarchy()
          )}
        </aside>

        <div
          className="project-resize-handle"
          role="separator"
          aria-orientation="vertical"
          aria-label="Resize project bar"
          title="Drag to resize project bar"
          onPointerDown={startProjectResize}
        />

        {workspaceMode === "placement" && (
          <section className="placement-pane">
            <div className="preview-heading graph-heading">
              <div>
                <span className="section-kicker">Placement mode</span>
                <h2>
                  {selectedSceneDocument === null
                    ? "No scene selected"
                    : `${selectedSceneDocument.display_name} / ${placementState?.display_name ?? "Base Placement"}`}
                </h2>
              </div>
              {renderModeTabs()}
            </div>
            {renderPreviewPanel("placement")}
          </section>
        )}

        {workspaceMode === "scene-flow" && (
        <section className="scene-flow-pane">
          <div className="preview-heading graph-heading">
            <div>
              <span className="section-kicker">Scene flow</span>
              <h2>{project?.summary.project_name ?? "No project open"}</h2>
            </div>
            {renderModeTabs()}
          </div>
          <div className="graph-surface">
            <SceneFlowView
              scenes={scenes}
              entrySceneId={project?.summary.entry_scene ?? null}
              thumbnails={sceneThumbnails}
              editor={project?.document?.project?.editor}
              layoutStatus={sceneFlowLayoutStatus}
              selectedSceneId={sceneSelection.kind === "project" || sceneSelection.kind === "sceneReference" || sceneSelection.kind === "packageEntry" ? null : selectedScene}
              selectedSceneExitId={sceneSelection.kind === "sceneExit" ? sceneSelection.id : null}
              selectedRouteId={sceneSelection.kind === "route" ? sceneSelection.id : null}
              selectedReferenceId={sceneSelection.kind === "sceneReference" ? sceneSelection.id : null}
              packageEntrySelected={sceneSelection.kind === "packageEntry"}
              onAddScene={(displayName) => void addScene(displayName)}
              onSelectScene={(sceneId) => {
                setSelectedScene(sceneId);
                setSceneSelection({ kind: "scene" });
                void startPreview(sceneId);
              }}
              onOpenScene={(sceneId) => {
                setSelectedScene(sceneId);
                setSceneSelection({ kind: "scene" });
                setWorkspaceMode("logic");
              }}
              onSelectSceneRoute={(sceneId, routeId) => {
                setSelectedScene(sceneId);
                setSceneSelection({ kind: "route", id: routeId });
              }}
              onSelectSceneExit={(sceneId, sceneExitId) => {
                setSelectedScene(sceneId);
                setSceneSelection({ kind: "sceneExit", id: sceneExitId });
              }}
              onSelectPackageEntry={() => {
                const entryScene = project?.summary.entry_scene ?? null;
                if (entryScene !== null) {
                  setSelectedScene(entryScene);
                }
                setSceneSelection({ kind: "packageEntry" });
              }}
              onSelectSceneReference={(referenceId, targetScene) => {
                setSelectedScene(targetScene);
                setSceneSelection({ kind: "sceneReference", id: referenceId });
              }}
              onAddSceneExit={(sceneId, targetScene, referenceId) => {
                void addSceneExit(sceneId, targetScene, referenceId);
              }}
              onAddSceneReference={(targetScene, x, y) => void addSceneReference(targetScene, x, y)}
              onDeleteSceneExit={(sceneId, sceneExitId) => {
                void deleteSceneExit(sceneId, sceneExitId);
              }}
              onDeleteLegacyRoute={(sceneId, routeId) => {
                void deleteLegacySceneRoute(sceneId, routeId);
              }}
              onDeleteSceneReference={(referenceId) => void deleteSceneReference(referenceId)}
              onMoveSceneNode={(sceneId, x, y) => {
                void moveSceneNode(sceneId, x, y);
              }}
              onMovePackageEntry={(x, y) => void movePackageEntryNode(x, y)}
              onMoveSceneReference={(referenceId, x, y) => void moveSceneReferenceNode(referenceId, x, y)}
              onSetRouteLayout={(sceneId, endpointKind, endpointId, rails) => {
                void setSceneRouteLayout(sceneId, endpointKind, endpointId, rails);
              }}
              onSetEntryScene={(sceneId) => void setPackageEntryScene(sceneId)}
              onConnectSceneExit={(sceneId, routeId, targetScene, referenceId) => {
                void setRouteSceneTarget(sceneId, routeId, targetScene, undefined, referenceId);
              }}
              onSetSceneExitTarget={(sceneId, sceneExitId, targetScene, referenceId) => {
                void setSceneExitTarget(sceneId, sceneExitId, targetScene, referenceId);
              }}
              canEdit={service?.operations.includes("project.apply_commands") === true && busy === null}
              canAddScene={busy === null && (project?.document?.scenes?.find(scene => scene.scene_id === project.summary.entry_scene)?.schema_version === 2
                ? supportsNativeCreation(service) : service?.state_scene_graph.scene_commands?.includes("scene.add") === true)}
              readOnlySceneIds={readOnlySceneIds}
            />
          </div>
        </section>
        )}

        {workspaceMode === "logic" && (
        <section className="state-graph-pane">
          <div className="preview-heading graph-heading">
            <div>
              <span className="section-kicker">Logic graph</span>
              <h2>{selectedSceneDocument?.display_name ?? "No scene selected"}</h2>
            </div>
            {renderModeTabs()}
          </div>
          <div className="graph-surface">
            <StateGraphView
              scene={selectedSceneDocument}
              activeStateId={preview !== null && preview.scene.scene_id === selectedSceneDocument?.scene_id
                ? preview.scene.state_id
                : null}
              editor={project?.document?.project?.editor}
              layoutStatus={stateGraphLayoutStatus}
              selected={sceneSelection}
              physicalEventKinds={service?.state_scene_presentation.logical_input_events ?? ["press"]}
              peepOSTriggers={service?.state_scene_graph.peepos_trigger_catalog ?? []}
              timerTypes={objectSceneSelected ? [SCENE_TIMER, STATE_TIMER].filter(type =>
                timerBounds(service, project?.summary.target_profile ?? "", type)
                && ["event_binding.add", "scene.set_reactive_wait_default", type === SCENE_TIMER ? "event_handler.add" : "route.add"].every(localCommandAllowed)) : []}
              onRequestTimer={(stateId, eventType) => {
                setSceneSelection(eventType === SCENE_TIMER ? { kind: "scene" } : { kind: "state", id: stateId });
                setTimerRequest({ serial: Date.now(), sceneId: selectedSceneDocument!.scene_id, eventType, stateId });
              }}
              onSelect={setSceneSelection}
              onCreateState={(sceneId, x, y) => {
                void createState(sceneId, x, y);
              }}
              onDeleteState={(sceneId, stateId) => {
                void deleteState(sceneId, stateId);
              }}
              onMoveStateNode={(sceneId, stateId, x, y) => {
                void moveStateNode(sceneId, stateId, x, y);
              }}
              onSetEntryConnection={(sceneId, stateId, targetHandle, targetSide) => {
                void setEntryConnection(sceneId, stateId, targetHandle, targetSide);
              }}
              onSetRouteLayout={(sceneId, routeId, sourceState, rails, targetHandle, targetSide, tokenPositions) => {
                void setStateRouteLayout(sceneId, routeId, sourceState, rails, targetHandle, targetSide, tokenPositions);
              }}
              onCreateTriggerRoute={(sceneId, sourceState, logicalSource, eventKind, target) => {
                void createTriggerRoute(sceneId, sourceState, logicalSource, eventKind, target);
              }}
              onRebindTriggerRoute={(sceneId, routeId, logicalSource) => {
                void rebindTriggerRoute(sceneId, routeId, logicalSource);
              }}
              onConnectRouteToSceneExit={(sceneId, routeId, sceneExitId, targetScene) => {
                void setRouteSceneTarget(sceneId, routeId, targetScene, sceneExitId);
              }}
              onDeleteSystemExit={(sceneId) => {
                void deleteSystemExit(sceneId);
              }}
              canCreateState={stateCommandAllowed("state.create") && (objectSceneSelected
                || service?.state_scene_graph.state_commands.includes("state.create") === true)}
              canMoveStates={stateCommandAllowed("editor.state_graph.set_node_position")}
              canDeleteStates={stateCommandAllowed("state.delete")}
              canEditEntry={stateCommandAllowed("state.set_entry") && stateCommandAllowed("editor.state_graph.set_entry_layout")}
              canEdit={canEditLocalGraph}
              canConnectScenes={canEditSelectedScene}
            />
          </div>
        </section>
        )}

        {workspaceMode === "assets" && renderAssetsWorkspace()}

        <div
          className="inspector-resize-handle"
          role="separator"
          aria-orientation="vertical"
          aria-label="Resize inspector"
          title="Drag to resize inspector"
          onPointerDown={startInspectorResize}
        />

        <aside className="inspector-pane">
          <div className="pane-heading inspector-heading">
            <span>Inspector</span>
            {workspaceMode === "placement" && !projectRootSelected && (
              <div className="inspector-tabs" aria-label="Placement inspector tabs">
                <button
                  type="button"
                  className={placementInspectorTab === "object" ? "active" : ""}
                  onClick={() => setPlacementInspectorTab("object")}
                >
                  Object
                </button>
                <button
                  type="button"
                  className={placementInspectorTab === "settings" ? "active" : ""}
                  onClick={() => setPlacementInspectorTab("settings")}
                >
                  Settings
                </button>
              </div>
            )}
          </div>

          {projectRootSelected && renderProjectInspector()}
          {!projectRootSelected && workspaceMode === "placement" && (placementInspectorTab === "settings" ? renderPlacementViewSettings() : renderPlacementInspector())}
          {!projectRootSelected && workspaceMode === "assets" && renderAssetInspector()}

          {!projectRootSelected && workspaceMode === "logic" && (
            <section className="inspector-section">
              <h3>Runtime</h3>
              {preview === null ? (
                <p className="muted">Start a scene preview to inspect its runtime state.</p>
              ) : (
                <dl className="inspector-list">
                  <div><dt>Scene</dt><dd>{preview.scene.scene_id}</dd></div>
                  <div><dt>State</dt><dd>{preview.scene.state_id}</dd></div>
                  {preview.timeline.ownership === "scene_objects" ? (
                    <div><dt>Scene time</dt><dd>{preview.timeline.elapsed_ms} ms</dd></div>
                  ) : <>
                    <div><dt>Presentation</dt><dd>{preview.timeline.presentation_id}</dd></div>
                    <div><dt>Quantum</dt><dd>{preview.timeline.phase_quantum_ms} ms</dd></div>
                  </>}
                  <div><dt>Black pixels</dt><dd>{preview.framebuffer.black_pixel_count}</dd></div>
                </dl>
              )}
            </section>
          )}

          {!projectRootSelected && workspaceMode === "scene-flow" && (
            <SceneFlowInspector
              canRenameScene={canEditSelectedScene || (busy === null && supportsObjectCommand(service, selectedSceneCapability, "scene.rename"))}
              scene={selectedSceneDocument}
              scenes={scenes}
              editor={project?.document?.project?.editor}
              entrySceneId={project?.summary.entry_scene ?? null}
              selection={sceneSelection}
              onSelect={setSceneSelection}
              onRenameScene={renameScene}
              onSetSceneExitTarget={setSceneExitTarget}
              onSetLegacyRouteTarget={setRouteSceneTarget}
              onSetReferenceTarget={setSceneReferenceTarget}
              onDeleteReference={deleteSceneReference}
              canEdit={canEditSelectedScene}
            />
          )}

          {!projectRootSelected && workspaceMode === "logic" && (
            <SceneAuthoringInspector
              timerActionKinds={objectSceneSelected ? service?.state_scene_graph.scene_timers?.actions ?? [] : []}
              onDeleteRoute={deleteLegacySceneRoute}
              localCommandAllowed={localCommandAllowed}
              stateCommandAllowed={stateCommandAllowed}
              objectActionsEditable={busy === null && supportsObjectCommand(service, selectedSceneCapability, "object_actions.set")}
              scene={selectedSceneDocument}
              scenes={scenes}
              editor={project?.document?.project?.editor}
              selection={sceneSelection}
              onSelect={setSceneSelection}
              onPreviewState={(sceneId, stateId) => startPreview(sceneId, { stateId, updateSelection: false })}
              onRenameState={renameState}
              onSetEntryState={setEntryState}
              onDeleteState={deleteState}
              onSetRouteTarget={setRouteTarget}
              onSetRouteSceneTarget={setRouteSceneTarget}
              onSetSceneExitTarget={setSceneExitTarget}
              onSetRouteGuard={setRouteGuard}
              onAddRouteGuard={addRouteGuard}
              onDeleteRouteGuard={deleteRouteGuard}
              onMoveRouteGuard={moveRouteGuard}
              onSetRouteAction={setRouteAction}
              onAddRouteAction={addRouteAction}
              onDeleteRouteAction={deleteRouteAction}
              onMoveRouteAction={moveRouteAction}
              onAddVariable={addVariable}
              onUpdateVariable={updateVariable}
              onDeleteVariable={deleteVariable}
              onResetRouteLayout={(sceneId, routeId, sourceState) =>
                setStateRouteLayout(sceneId, routeId, sourceState, [], null, null)}
              placementOwnership={project?.placement_ownership ?? null}
              assets={assets}
              audioCues={audioCues}
              variableLimit={service?.state_scene_graph.limits.variables ?? 0}
              guardLimit={service?.state_scene_graph.limits.guards_per_route ?? 0}
              actionLimit={service?.state_scene_graph.limits.actions_per_route ?? 0}
              canEdit={canEditSelectedScene}
              canPreview={projectValid && busy === null}
            />
          )}

          {!projectRootSelected && workspaceMode === "logic" && objectSceneSelected && selectedSceneDocument && (
            <TimerInspector key={selectedSceneDocument.scene_id} scene={selectedSceneDocument} service={service}
              profileId={project?.summary.target_profile ?? ""} request={timerRequest}
              stateId={sceneSelection.kind === "state" ? sceneSelection.id : undefined}
              routeId={sceneSelection.kind === "route" ? sceneSelection.id : undefined}
              supports={kind => kind === "object_actions.set"
                ? busy === null && supportsObjectCommand(service, selectedSceneCapability, kind) : localCommandAllowed(kind)}
              onApply={applySceneObjectCommands} ownership={project?.placement_ownership?.scenes[selectedSceneDocument.scene_id] ?? null}
              assets={assets} audioCues={audioCues} />
          )}
          {!projectRootSelected && workspaceMode === "logic" && (
            <section className="inspector-section">
              <h3>Variables</h3>
              {preview === null || Object.keys(preview.variables).length === 0 ? (
                <p className="muted">No runtime variables.</p>
              ) : (
                <dl className="inspector-list">
                  {Object.entries(preview.variables).map(([name, value]) => (
                    <div key={name}><dt>{name}</dt><dd>{value}</dd></div>
                  ))}
                </dl>
              )}
            </section>
          )}

        </aside>
      </section>

      <footer className="status-bar">
        <span>{busy !== null ? <><LoaderCircle className="spin" size={14} aria-hidden="true" /> {busy}</> : message ?? "Ready"}</span>
        <span>{project?.source_name ?? "No project"}</span>
      </footer>
    </main>
  );
}
