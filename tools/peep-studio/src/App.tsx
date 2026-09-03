import {
  AlertTriangle,
  ArrowDown,
  ArrowLeft,
  ArrowRight,
  ArrowUp,
  Box,
  Check,
  ChevronRight,
  Circle,
  CircleDot,
  Download,
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
  StepForward,
  SquareMousePointer,
  Trash2,
  Type,
  Undo2,
  Volume2,
  X,
} from "lucide-react";
import { useCallback, useEffect, useMemo, useRef, useState, type CSSProperties, type KeyboardEvent as ReactKeyboardEvent, type PointerEvent as ReactPointerEvent } from "react";
import { FramebufferCanvas, FramePreviewCanvas } from "./FramebufferCanvas";
import {
  SceneAuthoringInspector,
  SceneFlowView,
  StateGraphView,
  type NewStateTransitionTarget,
  type SceneSelection,
  type StateTriggerEventKind,
} from "./SceneInspection";
import type { StateGraphEntryHandle, StateGraphEntrySide } from "./stateGraph";
import type {
  AssetFrameRecord,
  AssetRecord,
  AudioAssetRecord,
  AudioAuditionResult,
  AudioCueRecord,
  CompiledAssetFrame,
  EditorNodePosition,
  EditorRouteRail,
  Framebuffer,
  PackageBuildResult,
  ProjectCommandResult,
  ProjectHistoryResult,
  ProjectSceneThumbnailsResult,
  ProjectSaveResult,
  PreviewSnapshot,
  ProjectLoadResult,
  SceneDocument,
  ServiceHello,
  WaitingVisual,
} from "./types";
import type { RenderElement, RenderModel, StateRecord } from "./types";

const INPUTS = [
  { source: "BUTTON_L", label: "L", icon: Circle },
  { source: "BUTTON_R", label: "R", icon: Circle },
  { source: "BUTTON_A", label: "A", icon: Circle },
  { source: "BUTTON_B", label: "B", icon: Circle },
] as const;

const JOYSTICK_INPUTS = [
  { source: "JOY_UP", label: "Up", icon: ArrowUp, className: "joy-up" },
  { source: "JOY_LEFT", label: "Left", icon: ArrowLeft, className: "joy-left" },
  { source: "JOY_RIGHT", label: "Right", icon: ArrowRight, className: "joy-right" },
  { source: "JOY_DOWN", label: "Down", icon: ArrowDown, className: "joy-down" },
] as const;

const PLACEMENT_PRIMITIVES = [
  { kind: "line", label: "Line" },
  { kind: "outline_rect", label: "Outline rectangle" },
  { kind: "filled_rect", label: "Filled rectangle" },
  { kind: "circle", label: "Circle" },
  { kind: "ellipse", label: "Ellipse" },
] as const;

type PlacementPrimitiveKind = (typeof PLACEMENT_PRIMITIVES)[number]["kind"];
type WorkspaceMode = "scene-flow" | "logic" | "placement" | "assets";
type PlacementInspectorTab = "object" | "settings";
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
  const [placementPreview, setPlacementPreview] = useState<PreviewSnapshot | null>(null);
  const [placementPreviewLoading, setPlacementPreviewLoading] = useState(false);
  const [placementPreviewError, setPlacementPreviewError] = useState<string | null>(null);
  const [selectedScene, setSelectedScene] = useState<string | null>(null);
  const [projectPath, setProjectPath] = useState<string | null>(null);
  const [temporaryProject, setTemporaryProject] = useState(false);
  const [sceneSelection, setSceneSelection] = useState<SceneSelection>({ kind: "scene" });
  const [placementStateId, setPlacementStateId] = useState<string | null>(null);
  const [placementEditAllStates, setPlacementEditAllStates] = useState(false);
  const [placementEditStateIds, setPlacementEditStateIds] = useState<string[]>([]);
  const [selectedPlacementElement, setSelectedPlacementElement] = useState<string | null>(null);
  const [build, setBuild] = useState<PackageBuildResult | null>(null);
  const [dirty, setDirty] = useState(false);
  const [canUndo, setCanUndo] = useState(false);
  const [canRedo, setCanRedo] = useState(false);
  const [busy, setBusy] = useState<string | null>(null);
  const [playing, setPlaying] = useState(false);
  const [workspaceMode, setWorkspaceMode] = useState<WorkspaceMode>("scene-flow");
  const [placementInspectorTab, setPlacementInspectorTab] = useState<PlacementInspectorTab>("object");
  const [sceneThumbnails, setSceneThumbnails] = useState<Record<string, Framebuffer>>({});
  const [sceneCreatorOpen, setSceneCreatorOpen] = useState(false);
  const [newSceneName, setNewSceneName] = useState("");
  const [selectedAssetFrameId, setSelectedAssetFrameId] = useState<string | null>(null);
  const [selectedAudioCueId, setSelectedAudioCueId] = useState<string | null>(null);
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
    async (sceneId: string, revision = project?.project_revision) => {
      if (bridge === undefined || revision === undefined) {
        return false;
      }
      setBusy("Starting preview");
      setPlaying(false);
      try {
        const result = await bridge.serviceRequest<PreviewSnapshot>("project.preview_reset", {
          project_revision: revision,
          scene_id: sceneId,
        });
        setSelectedScene(sceneId);
        setSceneSelection({ kind: "scene" });
        setSelectedPlacementElement(null);
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
    [bridge, project?.project_revision],
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
      setPlacementStateId(null);
      setSceneThumbnails({});
      setSceneCreatorOpen(false);
      setNewSceneName("");
      setSceneSelection({ kind: "scene" });
      setSelectedPlacementElement(null);
      try {
        const result = await bridge.serviceRequest<ProjectLoadResult>("project.load", { path });
        setProject(result);
        setDirty(result.dirty);
        setCanUndo(result.can_undo);
        setCanRedo(result.can_redo);
        setMessage(result.valid ? null : "Project validation failed. Review the issues panel.");
        if (result.valid) {
          await startPreview(result.summary.entry_scene, result.project_revision);
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
      const result = await bridge.serviceRequest<ProjectLoadResult>("project.create", { path });
      setBuild(null);
      setPreview(null);
      setPlacementPreview(null);
      setPlacementPreviewLoading(false);
      setPlacementPreviewError(null);
      setSelectedScene(null);
      setPlacementStateId(null);
      setSceneThumbnails({});
      setSceneCreatorOpen(false);
      setNewSceneName("");
      setSceneSelection({ kind: "scene" });
      setSelectedPlacementElement(null);
      setProjectPath(path);
      setTemporaryProject(false);
      setWorkspaceMode("scene-flow");
      setProject(result);
      setDirty(result.dirty);
      setCanUndo(result.can_undo);
      setCanRedo(result.can_redo);
      setMessage(result.valid ? `Created ${result.summary.project_name}.` : "Project validation failed. Review the issues panel.");
      if (result.valid) {
        if (await startPreview(result.summary.entry_scene, result.project_revision)) {
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
          setSceneSelection({ kind: "scene" });
          setSelectedPlacementElement(null);
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
      if (result.scene.scene_id !== selectedScene) {
        setSelectedScene(result.scene.scene_id);
        setSceneSelection({ kind: "scene" });
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
      applyProjectResult(result);
      setMessage(`Saved ${result.saved_sources.length} source file${result.saved_sources.length === 1 ? "" : "s"}.`);
    } catch (error) {
      setMessage(errorText(error));
    } finally {
      setBusy(null);
    }
  };

  const applyProjectResult = (result: ProjectHistoryResult | ProjectCommandResult | ProjectSaveResult) => {
    setProject((current) => (
      current === null
        ? null
        : {
            ...current,
            project_revision: result.project_revision,
            valid: result.valid,
            issues: result.issues,
            document: result.document,
            summary: result.summary,
          }
    ));
    setDirty(result.dirty);
    setCanUndo(result.can_undo);
    setCanRedo(result.can_redo);
    setPreview(null);
    setPlacementPreview(null);
    setPlacementPreviewLoading(false);
    setPlacementPreviewError(null);
    setSceneThumbnails({});
    setPlacementDraftPositions({});
    setBuild(null);
  };

  const addScene = async () => {
    const displayName = newSceneName.trim();
    if (bridge === undefined || project === null || busy !== null || displayName.length === 0) {
      return;
    }
    setBusy("Adding scene");
    setPlaying(false);
    try {
      const result = await bridge.serviceRequest<ProjectCommandResult>("project.apply_commands", {
        project_revision: project.project_revision,
        commands: [{ kind: "scene.add", display_name: displayName }],
      });
      const applied = result.applied_commands[0];
      const sceneId = typeof applied?.scene_id === "string" ? applied.scene_id : null;
      if (sceneId === null) {
        throw new Error("Authoring service did not return the new scene ID");
      }
      applyProjectResult(result);
      setSceneCreatorOpen(false);
      setNewSceneName("");
      setWorkspaceMode("scene-flow");
      if (await startPreview(sceneId, result.project_revision)) {
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
      setSelectedAssetFrameId(frames[0]?.frame_id ?? null);
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
      setSelectedAssetFrameId(`${assetId}.frame`);
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
              asset_ref: assetId,
              priority: 96,
              volume: 200,
            },
          },
        ],
      });
      applyProjectResult(result);
      setSelectedAudioCueId(cueId);
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
    try {
      const result = await bridge.serviceRequest<AudioAuditionResult>("project.audio_audition", {
        project_revision: project.project_revision,
        cue_id: cueId,
      });
      const audio = new Audio(`data:audio/wav;base64,${result.audio.wav_base64}`);
      await audio.play();
      setSelectedAudioCueId(cueId);
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
        setSelectedAssetFrameId(frameId);
      }
      setMessage("Asset label updated. Save to write it to the project.");
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
      setSelectedAssetFrameId(asset.frames[0]?.frame_id ?? null);
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
      setSelectedAssetFrameId(frameId);
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
        applyProjectResult(saved);
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
              : { scene_exit_ref: target.sceneExitId }),
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
  ) => {
    if (bridge === undefined || project === null || busy !== null) {
      return;
    }
    setBusy("Updating scene exit");
    setPlaying(false);
    try {
      const result = await bridge.serviceRequest<ProjectCommandResult>("project.apply_commands", {
        project_revision: project.project_revision,
        commands: [
          {
            kind: "route.set_target",
            scene_id: sceneId,
            route_id: routeId,
            target_scene: targetScene,
            ...(sceneExitRef === undefined ? {} : { scene_exit_ref: sceneExitRef }),
          },
        ],
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

  const addSceneExit = async (sceneId: string, targetScene: string) => {
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

  const setSceneExitTarget = async (sceneId: string, sceneExitId: string, targetScene: string) => {
    if (bridge === undefined || project === null || busy !== null) {
      return;
    }
    setBusy("Updating scene exit");
    setPlaying(false);
    try {
      const result = await bridge.serviceRequest<ProjectCommandResult>("project.apply_commands", {
        project_revision: project.project_revision,
        commands: [{
          kind: "scene_exit.set_target",
          scene_id: sceneId,
          scene_exit_id: sceneExitId,
          target_scene: targetScene,
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
    setBusy("Deleting legacy scene transition");
    setPlaying(false);
    try {
      const result = await bridge.serviceRequest<ProjectCommandResult>("project.apply_commands", {
        project_revision: project.project_revision,
        commands: [{ kind: "route.delete", scene_id: sceneId, route_id: routeId }],
      });
      applyProjectResult(result);
      setSelectedScene(sceneId);
      setSceneSelection({ kind: "scene" });
      setMessage("Legacy scene transition deleted. Save to write it to the project.");
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
    setBusy("Updating guard");
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
      setSceneSelection({ kind: "route", id: routeId });
      setMessage("Guard updated. Save to write it to the project.");
    } catch (error) {
      setMessage(errorText(error));
    } finally {
      setBusy(null);
    }
  };

  const setRouteAction = async (
    sceneId: string,
    routeId: string,
    actionIndex: number,
    action: Record<string, unknown>,
  ) => {
    if (bridge === undefined || project === null || busy !== null) {
      return;
    }
    setBusy("Updating action");
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
      setSceneSelection({ kind: "route", id: routeId });
      setMessage("Action updated. Save to write it to the project.");
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
    if (bridge === undefined || project === null || busy !== null) {
      return;
    }
    setBusy("Adding action");
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
      setSceneSelection({ kind: "route", id: routeId });
      setMessage("Action added. Save to write it to the project.");
    } catch (error) {
      setMessage(errorText(error));
    } finally {
      setBusy(null);
    }
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
  const applyStatePlacementOverride = async (
    busyLabel: string,
    sceneId: string,
    stateId: string,
    renderModelId: string,
    elementId: string,
    override: Record<string, unknown>,
    successMessage: string,
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
            kind: "state_placement.set_override",
            scene_id: sceneId,
            state_id: stateId,
            render_model_id: renderModelId,
            element_id: elementId,
            ...override,
          },
        ],
      });
      applyProjectResult(result);
      setSelectedScene(sceneId);
      setSelectedPlacementElement(elementId);
      setSceneSelection({ kind: "state", id: stateId });
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
    const checked = placementEditStateIds.filter((stateId) => validIds.has(stateId));
    if (checked.length > 0) {
      return checked;
    }
    return placementState === null ? [] : [placementState.state_id];
  };
  const placementBaseElement = (elementId: string) => (
    placementRenderModel?.elements.find((element) => element.element_id === elementId) ?? null
  );
  const statePlacementOverride = (state: StateRecord, elementId: string) => (
    state.placement_overrides?.find((override) => override.element_ref === elementId) ?? null
  );
  const placementEditTargetLabel = () => {
    if (placementEditAllStates) {
      return "all states";
    }
    const count = placementEditStateTargets().length;
    return count === 1 ? "1 state" : `${count} states`;
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
    const states = selectedSceneDocument.states ?? [];
    if (placementEditAllStates) {
      const hasStatePositionOverride = states.some((state) => {
        const override = statePlacementOverride(state, element.element_id);
        return override?.x !== undefined || override?.y !== undefined;
      });
      if (baseElement.x === x && baseElement.y === y && !hasStatePositionOverride) {
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
        ...states.map((state) => ({
          kind: "state_placement.set_override",
          scene_id: selectedSceneDocument.scene_id,
          state_id: state.state_id,
          render_model_id: renderModelId,
          element_id: element.element_id,
          x,
          y,
        })),
      ];
    }
    const targetStateIds = placementEditStateTargets();
    const unchanged = targetStateIds.every((stateId) => {
      const state = states.find((item) => item.state_id === stateId);
      if (state === undefined) {
        return true;
      }
      const override = statePlacementOverride(state, element.element_id);
      return (override?.x ?? baseElement.x) === x && (override?.y ?? baseElement.y) === y;
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
    const states = selectedSceneDocument.states ?? [];
    if (placementEditAllStates) {
      const hasStateVisibilityOverride = states.some((state) => (
        statePlacementOverride(state, element.element_id)?.visible !== undefined
      ));
      if ((baseElement.visible ?? true) === visible && !hasStateVisibilityOverride) {
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
        ...states.map((state) => ({
          kind: "state_placement.set_override",
          scene_id: selectedSceneDocument.scene_id,
          state_id: state.state_id,
          render_model_id: renderModelId,
          element_id: element.element_id,
          visible,
        })),
      ];
    }
    const targetStateIds = placementEditStateTargets();
    const unchanged = targetStateIds.every((stateId) => {
      const state = states.find((item) => item.state_id === stateId);
      if (state === undefined) {
        return true;
      }
      const override = statePlacementOverride(state, element.element_id);
      return (override?.visible ?? baseElement.visible ?? true) === visible;
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
    renderModelId: string,
    x: number,
    y: number,
  ) => {
    if (selectedSceneDocument === null) {
      return;
    }
    const nextX = Math.min(Math.max(0, 168 - element.width), Math.max(0, Math.round(x)));
    const nextY = Math.min(Math.max(0, 144 - element.height), Math.max(0, Math.round(y)));
    const commands = scopedPositionCommands(element, renderModelId, nextX, nextY);
    if (commands.length === 0) {
      return;
    }
    const targetStateIds = placementEditAllStates ? [] : placementEditStateTargets();
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
  const oddDimension = (value: number, maximum: number) => {
    const bounded = Math.min(maximum, Math.max(3, Math.round(value)));
    if (bounded % 2 === 1) {
      return bounded;
    }
    return bounded < maximum ? bounded + 1 : bounded - 1;
  };
  const normalizePrimitiveBounds = (
    element: Pick<RenderElement, "kind">,
    bounds: { x: number; y: number; width: number; height: number },
  ) => {
    const nextX = Math.min(167, Math.max(0, Math.round(bounds.x)));
    const nextY = Math.min(143, Math.max(0, Math.round(bounds.y)));
    if (element.kind === "circle") {
      const maximum = Math.min(168 - nextX, 144 - nextY);
      const size = oddDimension(Math.max(bounds.width, bounds.height), maximum);
      return { x: nextX, y: nextY, width: size, height: size };
    }
    const nextWidth = element.kind === "ellipse"
      ? oddDimension(bounds.width, 168 - nextX)
      : Math.min(168 - nextX, Math.max(1, Math.round(bounds.width)));
    const nextHeight = element.kind === "ellipse"
      ? oddDimension(bounds.height, 144 - nextY)
      : Math.min(144 - nextY, Math.max(1, Math.round(bounds.height)));
    return { x: nextX, y: nextY, width: nextWidth, height: nextHeight };
  };
  const resizePlacementElement = (
    element: RenderElement,
    renderModelId: string,
    bounds: { x: number; y: number; width: number; height: number },
  ) => {
    if (selectedSceneDocument === null) {
      return;
    }
    const { x: nextX, y: nextY, width: nextWidth, height: nextHeight } = normalizePrimitiveBounds(element, bounds);
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
  const deletePlacementElement = (element: RenderElement, renderModelId: string) => {
    if (selectedSceneDocument === null) {
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
    if (bridge === undefined || build === null) {
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
  const selectedAudioCue = selectedAudioCueId === null
    ? audioCues[0] ?? null
    : audioCues.find((cue) => cue.cue_id === selectedAudioCueId) ?? audioCues[0] ?? null;
  const selectedAudioAsset = selectedAudioCue === null ? null : audioAssetById.get(selectedAudioCue.asset_ref) ?? null;
  const selectedAssetFrame = selectedAssetFrameId === null
    ? compiledAssetFrames[0] ?? null
    : compiledAssetFrameById.get(selectedAssetFrameId) ?? compiledAssetFrames[0] ?? null;
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
  const waitingAnimationCommands = service?.state_scene_presentation.waiting_animation.commands ?? [];
  const placementAnimationSupported =
    waitingAnimationCommands.includes("render_element.bind_waiting_animation") &&
    waitingAnimationCommands.includes("render_element.clear_waiting_animation");

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
    if (audioCues.length === 0) {
      setSelectedAudioCueId(null);
      return;
    }
    if (selectedAudioCueId === null || !audioCues.some((cue) => cue.cue_id === selectedAudioCueId)) {
      setSelectedAudioCueId(audioCues[0].cue_id);
    }
  }, [audioCues, selectedAudioCueId]);
  const selectedSceneDocument = useMemo(
    () => scenes.find((scene) => scene.scene_id === selectedScene) ?? null,
    [scenes, selectedScene],
  );
  const placementState = useMemo<StateRecord | null>(() => {
    if (selectedSceneDocument === null) {
      return null;
    }
    const states = selectedSceneDocument.states ?? [];
    return (
      states.find((state) => state.state_id === placementStateId) ??
      states.find((state) => state.state_id === selectedSceneDocument.entry_state) ??
      states[0] ??
      null
    );
  }, [placementStateId, selectedSceneDocument]);
  const placementRenderModel = useMemo<RenderModel | null>(() => {
    if (selectedSceneDocument === null) {
      return null;
    }
    return selectedSceneDocument.render_models?.[0] ?? null;
  }, [selectedSceneDocument]);
  const placementOverrideByElement = useMemo(() => {
    const overrides = new Map<string, NonNullable<StateRecord["placement_overrides"]>[number]>();
    for (const override of placementState?.placement_overrides ?? []) {
      overrides.set(override.element_ref, override);
    }
    return overrides;
  }, [placementState?.placement_overrides]);
  const effectivePlacementElements = useMemo<RenderElement[]>(() => {
    if (placementRenderModel === null) {
      return [];
    }
    return placementRenderModel.elements.map((element) => {
      const override = placementOverrideByElement.get(element.element_id);
      if (override === undefined) {
        return element;
      }
      return {
        ...element,
        x: override.x ?? element.x,
        y: override.y ?? element.y,
        visible: override.visible ?? element.visible,
        visual_ref: override.visual_ref ?? element.visual_ref,
      };
    });
  }, [placementOverrideByElement, placementRenderModel]);
  useEffect(() => {
    const sceneId = selectedSceneDocument?.scene_id ?? null;
    const stateId = placementState?.state_id ?? null;
    const readyForPlacementPreview =
      bridge !== undefined &&
      projectRevision !== null &&
      projectValid &&
      workspaceMode === "placement" &&
      sceneId !== null &&
      stateId !== null;
    if (
      !readyForPlacementPreview ||
      !placementPreviewSupported ||
      projectRevision === null ||
      sceneId === null ||
      stateId === null
    ) {
      setPlacementPreview(null);
      setPlacementPreviewLoading(false);
      setPlacementPreviewError(
        readyForPlacementPreview && !placementPreviewSupported
          ? `Placement preview needs Service API 16. Restart Peep Studio if the top bar still shows Service API ${service?.service_api_version ?? "unknown"}.`
          : null,
      );
      return undefined;
    }

    let cancelled = false;
    setPlacementPreviewLoading(true);
    setPlacementPreviewError(null);
    bridge
      .serviceRequest<PreviewSnapshot>("project.preview_state", {
        project_revision: projectRevision,
        scene_id: sceneId,
        state_id: stateId,
      })
      .then((result) => {
        if (
          cancelled ||
          result.project_revision !== projectRevision ||
          result.scene.scene_id !== sceneId ||
          result.scene.state_id !== stateId
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
    placementPreviewSupported,
    placementState?.state_id,
    projectRevision,
    projectValid,
    selectedSceneDocument?.scene_id,
    service?.service_api_version,
    workspaceMode,
  ]);
  useEffect(() => {
    if (selectedSceneDocument === null) {
      if (placementStateId !== null) {
        setPlacementStateId(null);
      }
      return;
    }
    const states = selectedSceneDocument.states ?? [];
    if (placementStateId !== null && states.some((state) => state.state_id === placementStateId)) {
      return;
    }
    setPlacementStateId(
      states.find((state) => state.state_id === selectedSceneDocument.entry_state)?.state_id ??
      states[0]?.state_id ??
      null,
    );
    setSelectedPlacementElement(null);
  }, [placementStateId, selectedSceneDocument]);
  useEffect(() => {
    if (selectedSceneDocument === null) {
      if (placementEditAllStates) {
        setPlacementEditAllStates(false);
      }
      if (placementEditStateIds.length > 0) {
        setPlacementEditStateIds([]);
      }
      return;
    }
    const states = selectedSceneDocument.states ?? [];
    const validIds = new Set(states.map((state) => state.state_id));
    const fallbackStateId =
      (placementStateId !== null && validIds.has(placementStateId) ? placementStateId : null) ??
      states.find((state) => state.state_id === selectedSceneDocument.entry_state)?.state_id ??
      states[0]?.state_id ??
      null;
    setPlacementEditStateIds((current) => {
      const next = current.filter((stateId) => validIds.has(stateId));
      const resolved = next.length > 0 ? next : fallbackStateId === null ? [] : [fallbackStateId];
      return resolved.length === current.length && resolved.every((stateId, index) => stateId === current[index])
        ? current
        : resolved;
    });
  }, [placementEditAllStates, placementEditStateIds.length, placementStateId, selectedSceneDocument]);
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
    if (compiledAssetFrames.length === 0) {
      setSelectedAssetFrameId(null);
      return;
    }
    if (selectedAssetFrameId === null || !compiledAssetFrameById.has(selectedAssetFrameId)) {
      setSelectedAssetFrameId(compiledAssetFrames[0].frame_id);
    }
  }, [compiledAssetFrameById, compiledAssetFrames, selectedAssetFrameId]);
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
  const renderInputControls = () => (
    <div className="input-controls">
      <div className="input-control-group">
        <span className="input-group-label">Joystick</span>
        <div className="joystick-pad" aria-label="Joystick cardinal inputs">
          {JOYSTICK_INPUTS.map(({ source, label, icon: Icon, className }) => (
            <button key={source} className={`joystick-button ${className}`} onClick={() => void sendInput(source)} disabled={preview === null} title={`Send ${source}`}>
              <Icon size={15} aria-hidden="true" />
              <span>{label}</span>
            </button>
          ))}
          <button className="joystick-nub" type="button" disabled title="Analog joystick preview is reserved for PROGRAM scenes" aria-label="Analog joystick preview reserved for PROGRAM scenes" />
        </div>
      </div>
      <div className="input-control-group start-control-group">
        <span className="input-group-label">Start</span>
        <button className="input-button start-button" onClick={() => void sendInput("BUTTON_START")} disabled={preview === null} title="Send BUTTON_START">
          <Circle size={13} aria-hidden="true" />
          Start
        </button>
      </div>
      <div className="input-control-group">
        <span className="input-group-label">Buttons</span>
        <div className="trigger-buttons" aria-label="Trigger buttons">
          {INPUTS.map(({ source, label, icon: Icon }) => (
            <button key={source} className="input-button" onClick={() => void sendInput(source)} disabled={preview === null} title={`Send ${source}`}>
              <Icon size={13} aria-hidden="true" />
              {label}
            </button>
          ))}
        </div>
      </div>
    </div>
  );
  const placementDraftKey = (renderModelId: string, elementId: string) => `${renderModelId}:${elementId}`;
  const handlePlacementKeyDown = (event: ReactKeyboardEvent<HTMLElement>) => {
    if (placementRenderModel === null || selectedPlacementRenderElement === null || busy !== null) {
      return;
    }
    const target = event.target as HTMLElement | null;
    if (target !== null && target.closest("input, select, textarea, [contenteditable='true']") !== null) {
      return;
    }
    if (event.key === "Delete" || event.key === "Backspace") {
      event.preventDefault();
      deletePlacementElement(selectedPlacementRenderElement, placementRenderModel.visual_id);
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
      placementRenderModel.visual_id,
      selectedPlacementRenderElement.x + dx,
      selectedPlacementRenderElement.y + dy,
    );
  };
  const startPlacementDrag = (
    event: ReactPointerEvent<HTMLButtonElement>,
    element: RenderElement,
    renderModelId: string,
  ) => {
    if (selectedSceneDocument === null || busy !== null) {
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
    setSceneSelection({ kind: "render", id: renderModelId });

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

      latestBounds = normalizePrimitiveBounds(element, { x: nextX, y: nextY, width: nextWidth, height: nextHeight });
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
  const renderPlacementToolPalette = () => (
    <div className="placement-tool-palette" aria-label="Placement tools">
      <button type="button" className="active" title="Select and move objects" aria-label="Select and move objects">
        <SquareMousePointer size={18} aria-hidden="true" />
      </button>
      <button
        type="button"
        disabled={busy !== null || selectedSceneDocument === null || placementRenderModel === null || compiledAssetFrames.length === 0}
        onClick={() => setSpritePickerOpen((open) => !open)}
        title={compiledAssetFrames.length === 0 ? "No sprite assets available" : "Add sprite"}
        aria-label="Add sprite"
      >
        <Image size={18} aria-hidden="true" />
      </button>
      {PLACEMENT_PRIMITIVES.map((primitive) => (
        <button
          key={primitive.kind}
          className={`primitive-${primitive.kind}`}
          type="button"
          disabled={busy !== null || selectedSceneDocument === null || placementRenderModel === null}
          onClick={() => void addPlacementPrimitive(primitive.kind)}
          title={`Add ${primitive.label.toLowerCase()}`}
          aria-label={`Add ${primitive.label.toLowerCase()}`}
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
                  disabled={busy !== null || selectedSceneDocument === null || placementRenderModel === null}
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
    const placementFramebuffer = placementPreview?.project_revision === projectRevision &&
      placementPreview.scene.scene_id === selectedSceneDocument?.scene_id &&
      placementPreview.scene.state_id === placementState?.state_id
      ? placementPreview.framebuffer
      : null;
    const matchingLiveFramebuffer = preview?.project_revision === projectRevision &&
      preview.scene.scene_id === selectedSceneDocument?.scene_id &&
      preview.scene.state_id === placementState?.state_id
      ? preview.framebuffer
      : null;
    const framebuffer = variant === "placement" ? placementFramebuffer ?? matchingLiveFramebuffer : preview?.framebuffer ?? null;
    const placementPreviewNotice =
      variant !== "placement" || placementFramebuffer !== null || matchingLiveFramebuffer !== null
        ? null
        : placementPreviewError ?? (placementPreviewLoading ? "Loading selected state preview..." : "No selected state preview available.");
    return (
    <section className={`preview-pane ${variant === "placement" ? "preview-pane-large" : "preview-pane-compact"}`}>
        {variant === "project" && (
          <div className="preview-heading emulator-heading">
            <div className="preview-title">
              <span className="section-kicker">State</span>
              <h2>{preview?.scene.display_name ?? "State preview"}</h2>
            </div>
            <div className="playback-controls" aria-label="Preview playback">
              <button className="icon-button" onClick={() => selectedScene !== null && void startPreview(selectedScene)} disabled={preview === null} title="Reset preview">
                <RotateCcw size={18} aria-hidden="true" />
              </button>
              <button className="icon-button transport-play" onClick={() => setPlaying((value) => !value)} disabled={preview === null} title={playing ? "Pause preview" : "Play preview"}>
                {playing ? <Pause size={19} aria-hidden="true" /> : <Play size={19} aria-hidden="true" />}
              </button>
              <button className="icon-button" onClick={() => void advancePreview(250)} disabled={preview === null} title="Advance 250 ms">
                <StepForward size={18} aria-hidden="true" />
              </button>
            </div>
            {preview !== null ? (
              <div className="timeline-readout">
                <span>Step {preview.timeline.step_index + 1}/{preview.timeline.step_count}</span>
                <strong>{preview.timeline.elapsed_ms} ms</strong>
              </div>
            ) : (
              <div className="timeline-readout timeline-readout-empty" aria-hidden="true" />
            )}
          </div>
        )}

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
              className={`placement-screen-overlay labels-${placementLabelMode}`}
              aria-label="Placement selection overlay"
              tabIndex={0}
              onKeyDown={handlePlacementKeyDown}
              style={{
                "--placement-grid-minor-opacity": placementGridVisible ? placementGridStrength / 100 : 0,
                "--placement-grid-major-opacity": placementGridVisible && placementMajorGridVisible ? (placementGridStrength + 6) / 100 : 0,
              } as CSSProperties}
            >
              <div className="placement-pixel-grid" aria-hidden="true" />
              {placementOverlayVisible &&
                [...effectivePlacementElements]
                  .sort((left, right) => left.z_order - right.z_order)
                  .map((element) => {
                    const key = placementDraftKey(placementRenderModel?.visual_id ?? "", element.element_id);
                    const positionDraft = placementDraftPositions[key];
                    const boundsDraft = placementDraftBounds[key];
                    const x = boundsDraft?.x ?? positionDraft?.x ?? element.x;
                    const y = boundsDraft?.y ?? positionDraft?.y ?? element.y;
                    const width = boundsDraft?.width ?? element.width;
                    const height = boundsDraft?.height ?? element.height;
                    const canResize = selectedPlacementElement === element.element_id && element.kind !== "sprite" && placementRenderModel !== null;
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
                          if (placementRenderModel !== null) {
                            startPlacementDrag(event, element, placementRenderModel.visual_id);
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
                              className={`placement-resize-handle ${isLine ? "handle-line-start" : "handle-nw"}`}
                              onPointerDown={(event) => startPlacementResize(event, element, placementRenderModel.visual_id, "nw")}
                            />
                            {!isLine && (
                              <>
                                <i className="placement-resize-handle handle-ne" onPointerDown={(event) => startPlacementResize(event, element, placementRenderModel.visual_id, "ne")} />
                                <i className="placement-resize-handle handle-sw" onPointerDown={(event) => startPlacementResize(event, element, placementRenderModel.visual_id, "sw")} />
                              </>
                            )}
                            <i
                              className={`placement-resize-handle ${isLine ? "handle-line-end" : "handle-se"}`}
                              onPointerDown={(event) => startPlacementResize(event, element, placementRenderModel.visual_id, "se")}
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

      {variant === "project" && <div className="transport-bar">{renderInputControls()}</div>}
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
                      onClick={() => setSelectedAssetFrameId(frame.frame_id)}
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
                          onClick={() => setSelectedAudioCueId(cue.cue_id)}
                          title="Select this SFX cue"
                        >
                          <Volume2 size={18} aria-hidden="true" />
                          <span>
                            <strong>{cue.cue_id}</strong>
                            <small>
                              {asset === undefined ? cue.asset_ref : `${asset.duration_ms} ms / ${asset.adpcm_bytes} ADPCM bytes`}
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
                          aria-label={`Audition ${cue.cue_id}`}
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
                <FramePreviewCanvas frame={animatedAssetPreviewFrame} />
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
                        onClick={() => setSelectedAssetFrameId(frame.frame_id)}
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
            <div><dt>Cue</dt><dd>{selectedAudioCue.cue_id}</dd></div>
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
  const renderAssetInspector = () => (
    <>
      {renderSpriteInspector()}
      {renderAudioInspector()}
    </>
  );
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
  const placementElements = () => [...effectivePlacementElements].sort((left, right) => left.z_order - right.z_order);
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
  const addPlacementSprite = async (frame: CompiledAssetFrame | null) => {
    if (selectedSceneDocument === null || placementRenderModel === null || frame === null) {
      if (frame === null) {
        setMessage("Create or import a sprite asset first.");
      }
      return;
    }
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
      placementState !== null &&
      animationFrames.length >= 2 &&
      animationFrames.length <= 4 &&
      animationFrames.every((item) => item.width === frame.width && item.height === frame.height);
    const commands: Record<string, unknown>[] = [
      {
        kind: "render_element.add",
        scene_id: selectedSceneDocument.scene_id,
        render_model_id: placementRenderModel.visual_id,
        element,
      },
    ];
    if (canAutoAnimate) {
      commands.push({
        kind: "render_element.bind_waiting_animation",
        scene_id: selectedSceneDocument.scene_id,
        state_id: placementState.state_id,
        render_model_id: placementRenderModel.visual_id,
        element_id: element.element_id,
        phase_visual_refs: animationFrames.map((item) => item.frame_id),
      });
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
      setSceneSelection({ kind: "render", id: placementRenderModel.visual_id });
      if (result.valid) {
        const previewResult = await bridge.serviceRequest<PreviewSnapshot>("project.preview_reset", {
          project_revision: result.project_revision,
          scene_id: selectedSceneDocument.scene_id,
        });
        setPreview(previewResult);
      }
      setMessage(`${canAutoAnimate ? "Animated sprite" : "Sprite"} added. Save to write it to the project.`);
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
    if (selectedSceneDocument === null || placementState === null || frames.length < 2) {
      return;
    }
    await applyRenderElementCommand(
      "Adding animation",
      selectedSceneDocument.scene_id,
      renderModelId,
      element.element_id,
      {
        kind: "render_element.bind_waiting_animation",
        state_id: placementState.state_id,
        phase_visual_refs: frames.map((frame) => frame.frame_id),
      },
      "Sprite animation enabled. Save to write it to the project.",
    );
  };
  const clearPlacementSpriteAnimation = async (element: RenderElement, renderModelId: string) => {
    if (selectedSceneDocument === null || placementState === null) {
      return;
    }
    await applyRenderElementCommand(
      "Removing animation",
      selectedSceneDocument.scene_id,
      renderModelId,
      element.element_id,
      {
        kind: "render_element.clear_waiting_animation",
        state_id: placementState.state_id,
      },
      "Sprite animation removed. Save to write it to the project.",
    );
  };
  const addPlacementPrimitive = async (kind: PlacementPrimitiveKind) => {
    if (selectedSceneDocument === null || placementRenderModel === null) {
      return;
    }
    const element: RenderElement = {
      element_id: nextPlacementElementId(kind, placementRenderModel.elements),
      kind,
      x: 48,
      y: 40,
      width: kind === "line" ? 48 : kind === "circle" ? 31 : kind === "ellipse" ? 33 : 32,
      height: kind === "line" ? 1 : kind === "circle" ? 31 : kind === "ellipse" ? 25 : 24,
      z_order: Math.max(0, ...placementRenderModel.elements.map((item) => item.z_order)) + 1,
      layer: "SCENE",
      visible: true,
    };
    await applyRenderElementCommand(
      "Adding object",
      selectedSceneDocument.scene_id,
      placementRenderModel.visual_id,
      null,
      {
        kind: "render_element.add",
        element,
      },
      "Object added. Save to write it to the project.",
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
  const selectPlacementState = (stateId: string) => {
    const state = (selectedSceneDocument?.states ?? []).find((item) => item.state_id === stateId) ?? null;
    setPlacementStateId(stateId);
    if (state !== null && placementRenderModel !== null) {
      setSceneSelection({ kind: "state", id: state.state_id });
    }
  };
  const setAllPlacementStatesTargeted = (targeted: boolean) => {
    setPlacementEditAllStates(targeted);
    if (!targeted && placementEditStateTargets().length === 0 && placementState !== null) {
      setPlacementEditStateIds([placementState.state_id]);
    }
  };
  const togglePlacementEditState = (stateId: string) => {
    const currentlyChecked = placementEditStateIds.includes(stateId);
    setPlacementEditAllStates(false);
    if (!currentlyChecked) {
      setPlacementStateId(stateId);
    }
    setPlacementEditStateIds((current) => {
      if (current.includes(stateId)) {
        return current.length <= 1 ? current : current.filter((item) => item !== stateId);
      }
      return [...current, stateId];
    });
  };
  const renderPlacementEditScope = () => {
    const editStateIds = new Set(placementEditStateIds);
    return (
      <section className="inspector-section placement-edit-scope-section">
        <h3><SquareMousePointer size={14} aria-hidden="true" /> Edit target</h3>
        {selectedSceneDocument === null ? (
          <p className="muted">Select a scene to choose where placement edits apply.</p>
        ) : (
          <div className="placement-edit-scope">
            <div className="placement-scope-header">
              <span>Applying edits to</span>
              <code>{placementEditAllStates ? "All states" : placementEditTargetLabel()}</code>
            </div>
            <label className="placement-scope-all">
              <input
                type="checkbox"
                checked={placementEditAllStates}
                disabled={busy !== null}
                onChange={(event) => setAllPlacementStatesTargeted(event.target.checked)}
              />
              <span>All states</span>
            </label>
            <div className="placement-state-scope-list">
              {(selectedSceneDocument.states ?? []).map((state) => (
                <div
                  key={state.state_id}
                  className={`placement-state-scope-row ${placementState?.state_id === state.state_id ? "previewing" : ""}`}
                >
                  <label>
                    <input
                      type="checkbox"
                      checked={placementEditAllStates || editStateIds.has(state.state_id)}
                      disabled={busy !== null || placementEditAllStates}
                      onChange={() => togglePlacementEditState(state.state_id)}
                    />
                    <span>
                      <strong>{state.display_name}</strong>
                      <small>{state.state_id}</small>
                    </span>
                  </label>
                  <button
                    className="placement-state-preview-button"
                    type="button"
                    disabled={busy !== null}
                    onClick={() => selectPlacementState(state.state_id)}
                    title="Preview this state"
                  >
                    {placementState?.state_id === state.state_id ? "Preview" : "View"}
                  </button>
                </div>
              ))}
            </div>
            <p className="muted">Previewing {placementState?.display_name ?? "no state"}.</p>
          </div>
        )}
      </section>
    );
  };
  const renderPlacementObjectHierarchy = () => {
    const elements = placementElements();
    const activeWaitingVisual = selectedSceneDocument?.waiting_visuals?.find(
      (waiting) => waiting.waiting_visual_id === placementState?.waiting_visual_ref,
    ) ?? null;
    const animatedSourceIds = new Set(
      activeWaitingVisual?.elements.map((element) => element.source_element_ref) ?? [],
    );
    const changedSourceIds = new Set((placementState?.placement_overrides ?? []).map((override) => override.element_ref));
    return (
      <details className="project-section placement-object-section" open>
        <summary>Hierarchy</summary>
        {selectedSceneDocument === null ? (
          <p className="muted project-section-empty">Select a scene to inspect objects.</p>
        ) : (
          <div className="placement-object-tree">
            {placementRenderModel === null ? (
              <p className="muted project-section-empty">No placed-object view for this state.</p>
            ) : elements.length === 0 ? (
              <p className="muted project-section-empty">No placed objects.</p>
            ) : (
              <div className="placement-object-children">
                {elements.map((element: RenderElement) => (
                  <button
                    key={element.element_id}
                    className={selectedPlacementElement === element.element_id ? "selected" : ""}
                    type="button"
                    onClick={() => selectPlacementElement(element.element_id)}
                  >
                    <span className="placement-object-kind">
                      {placementKindIcon(element.kind)}
                      <span>
                        <strong>{placementKindLabel(element.kind)}</strong>
                        <small>{placementSourceLabel(element)}</small>
                      </span>
                    </span>
                    <span className="placement-object-badges">
                      <code>{placementLayerLabel(element)}</code>
                      {changedSourceIds.has(element.element_id) && <code>Changed</code>}
                      {animatedSourceIds.has(element.element_id) && <code>Anim</code>}
                      {element.visible === false && <code>Hidden</code>}
                      <code>z{element.z_order}</code>
                    </span>
                  </button>
                ))}
              </div>
            )}
          </div>
        )}
      </details>
    );
  };
  const renderPlacementInspector = () => {
    const elements = [...effectivePlacementElements].sort((left, right) => left.z_order - right.z_order);
    const selectedElement = selectedPlacementElement === null
      ? null
      : elements.find((element) => element.element_id === selectedPlacementElement) ?? null;
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
    const selectedStateWaitingVisual: WaitingVisual | null = selectedSceneDocument?.waiting_visuals?.find(
      (waiting) => waiting.waiting_visual_id === placementState?.waiting_visual_ref,
    ) ?? null;
    const selectedSpriteAnimation = selectedElement === null
      ? null
      : selectedStateWaitingVisual?.elements.find((element) => element.source_element_ref === selectedElement.element_id) ?? null;
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
      if (selectedSceneDocument === null || selectedElement === null || placementRenderModel === null) {
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
      if (selectedSceneDocument === null || selectedElement === null || placementRenderModel === null) {
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
      const targetStateIds = placementEditAllStates ? [] : placementEditStateTargets();
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
                      disabled={busy !== null}
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
                      disabled={busy !== null}
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
                  disabled={busy !== null}
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
                    disabled={busy !== null || selectedElement.z_order <= 0}
                    onClick={() => setPlacementElementZOrder(selectedElement, placementRenderModel.visual_id, selectedElement.z_order - 1)}
                    title="Send backward"
                  >
                    <ArrowDown size={14} aria-hidden="true" />
                    Back
                  </button>
                  <button
                    type="button"
                    disabled={busy !== null || selectedElement.z_order >= 255}
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
              <p className="muted">Arrow keys nudge 1 px. Shift + arrow nudges 8 px.</p>
            </div>
            {selectedElement.kind === "sprite" && (
              <div className="placement-animation-editor">
                <div className="placement-animation-heading">
                  <span>Animation</span>
                  <code>{selectedSpriteAnimation === null ? "Static" : "Animated"}</code>
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
                {selectedSpriteFrame === null ? (
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
                      checked={selectedSpriteAnimation !== null}
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
                Delete object
              </button>
            </div>
          </>
        )}
        </section>
      </>
    );
  };

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
          <button className="button primary" onClick={buildPackage} disabled={!project?.valid || busy !== null}>
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
          <button className="icon-button" onClick={exportPackage} disabled={build === null} title="Export .egg">
            <Download size={18} aria-hidden="true" />
          </button>
        </div>
      </header>

      <section
        className={`workspace-grid ${workspaceMode === "placement" ? "placement-mode" : workspaceMode === "scene-flow" ? "scene-flow-mode" : workspaceMode === "assets" ? "assets-mode" : "logic-mode"}`}
        style={{ "--project-width": `${projectWidth}px`, "--inspector-width": `${inspectorWidth}px` } as CSSProperties}
      >
        <aside className="project-pane">
          <div className="pane-heading project-heading">
            <details className="project-heading-details">
              <summary>
                <span>Project</span>
                {project !== null && (
                  <span className={`validation-state ${project.valid ? "valid" : "invalid"}`}>
                    <StatusMark ok={project.valid} /> {project.valid ? "Valid" : "Invalid"}
                  </span>
                )}
              </summary>
              {project !== null && (
                <dl className="project-facts project-facts-dropdown">
                  <div><dt>Package</dt><dd>{project.summary.package_id}</dd></div>
                  <div><dt>Target</dt><dd>{project.summary.target_profile}</dd></div>
                  <div><dt>Source</dt><dd>{temporaryProject ? "Example copy" : "Project"}</dd></div>
                  <div><dt>Edits</dt><dd>{dirty ? "Unsaved" : "Clean"}</dd></div>
                  <div><dt>Path</dt><dd title={projectPath ?? undefined}>{projectPath ?? "-"}</dd></div>
                  <div><dt>Frames</dt><dd>{project.summary.asset_frame_count}</dd></div>
                  <div><dt>Animations</dt><dd>{project.summary.animation_count}</dd></div>
                  <div><dt>SFX</dt><dd>{project.summary.audio_cue_count}</dd></div>
                </dl>
              )}
            </details>
          </div>

          {renderPreviewPanel("project")}

          {project === null ? (
            <div className="empty-pane">
              <Box size={25} aria-hidden="true" />
              <strong>No project open</strong>
              <span>Open a .peepproj folder to inspect scenes and package assets.</span>
            </div>
          ) : (
            <>
              <details className="project-section" open>
                <summary>Scenes</summary>
                {sceneCreatorOpen ? (
                  <form
                    className="scene-create-form"
                    onSubmit={(event) => {
                      event.preventDefault();
                      void addScene();
                    }}
                  >
                    <input
                      autoFocus
                      aria-label="New scene name"
                      value={newSceneName}
                      maxLength={96}
                      placeholder="Scene name"
                      onChange={(event) => setNewSceneName(event.target.value)}
                      onKeyDown={(event) => {
                        if (event.key === "Escape") {
                          setSceneCreatorOpen(false);
                          setNewSceneName("");
                        }
                      }}
                    />
                    <button
                      type="submit"
                      className="icon-button"
                      disabled={newSceneName.trim().length === 0 || busy !== null}
                      title="Create scene"
                    >
                      <Check size={16} aria-hidden="true" />
                    </button>
                    <button
                      type="button"
                      className="icon-button"
                      disabled={busy !== null}
                      title="Cancel"
                      onClick={() => {
                        setSceneCreatorOpen(false);
                        setNewSceneName("");
                      }}
                    >
                      <X size={16} aria-hidden="true" />
                    </button>
                  </form>
                ) : (
                  <button
                    type="button"
                    className="button secondary scene-create-button"
                    disabled={busy !== null || service?.state_scene_graph.scene_commands?.includes("scene.add") !== true}
                    onClick={() => setSceneCreatorOpen(true)}
                  >
                    <Plus size={15} aria-hidden="true" />
                    Add scene
                  </button>
                )}
                <nav className="scene-list" aria-label="Project scenes">
                  {scenes.map((scene) => (
                    <button
                      key={scene.scene_id}
                      className={`scene-row ${selectedScene === scene.scene_id ? "selected" : ""}`}
                      onClick={() => void startPreview(scene.scene_id)}
                    >
                      <FileCode2 size={16} aria-hidden="true" />
                      <span>
                        <strong>{scene.display_name}</strong>
                        <small>{scene.scene_type}</small>
                      </span>
                      <ChevronRight size={15} aria-hidden="true" />
                    </button>
                  ))}
                </nav>
              </details>
            </>
          )}

          {workspaceMode === "placement" && renderPlacementObjectHierarchy()}
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
                    : `${selectedSceneDocument.display_name} / ${placementState?.display_name ?? "No state"}`}
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
              selectedSceneId={selectedScene}
              selectedSceneExitId={sceneSelection.kind === "sceneExit" ? sceneSelection.id : null}
              selectedRouteId={sceneSelection.kind === "route" ? sceneSelection.id : null}
              onSelectScene={(sceneId) => void startPreview(sceneId)}
              onSelectSceneRoute={(sceneId, routeId) => {
                setSelectedScene(sceneId);
                setSceneSelection({ kind: "route", id: routeId });
              }}
              onSelectSceneExit={(sceneId, sceneExitId) => {
                setSelectedScene(sceneId);
                setSceneSelection({ kind: "sceneExit", id: sceneExitId });
              }}
              onAddSceneExit={(sceneId, targetScene) => {
                void addSceneExit(sceneId, targetScene);
              }}
              onDeleteSceneExit={(sceneId, sceneExitId) => {
                void deleteSceneExit(sceneId, sceneExitId);
              }}
              onDeleteLegacyRoute={(sceneId, routeId) => {
                void deleteLegacySceneRoute(sceneId, routeId);
              }}
              onMoveSceneNode={(sceneId, x, y) => {
                void moveSceneNode(sceneId, x, y);
              }}
              onConnectSceneExit={(sceneId, routeId, targetScene) => {
                void setRouteSceneTarget(sceneId, routeId, targetScene);
              }}
              onSetSceneExitTarget={(sceneId, sceneExitId, targetScene) => {
                void setSceneExitTarget(sceneId, sceneExitId, targetScene);
              }}
              canEdit={service?.operations.includes("project.apply_commands") === true && busy === null}
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
              editor={project?.document?.project?.editor}
              layoutStatus={stateGraphLayoutStatus}
              selected={sceneSelection}
              physicalEventKinds={service?.state_scene_presentation.logical_input_events ?? ["press"]}
              peepOSTriggers={service?.state_scene_graph.peepos_trigger_catalog ?? []}
              onSelect={setSceneSelection}
              onMoveStateNode={(sceneId, stateId, x, y) => {
                void moveStateNode(sceneId, stateId, x, y);
              }}
              onSetRouteLayout={(sceneId, routeId, sourceState, rails, targetHandle, targetSide) => {
                void setStateRouteLayout(sceneId, routeId, sourceState, rails, targetHandle, targetSide);
              }}
              onCreateTriggerRoute={(sceneId, sourceState, logicalSource, eventKind, target) => {
                void createTriggerRoute(sceneId, sourceState, logicalSource, eventKind, target);
              }}
              onConnectRouteToSceneExit={(sceneId, routeId, sceneExitId, targetScene) => {
                void setRouteSceneTarget(sceneId, routeId, targetScene, sceneExitId);
              }}
              canEdit={service?.operations.includes("project.apply_commands") === true && busy === null}
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
            {workspaceMode === "placement" && (
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

          {workspaceMode === "placement" && (placementInspectorTab === "settings" ? renderPlacementViewSettings() : renderPlacementInspector())}
          {workspaceMode === "assets" && renderAssetInspector()}

          {workspaceMode !== "placement" && workspaceMode !== "assets" && (
            <section className="inspector-section">
              <h3>Runtime</h3>
              {preview === null ? (
                <p className="muted">Start a scene preview to inspect its runtime state.</p>
              ) : (
                <dl className="inspector-list">
                  <div><dt>Scene</dt><dd>{preview.scene.scene_id}</dd></div>
                  <div><dt>State</dt><dd>{preview.scene.state_id}</dd></div>
                  <div><dt>Presentation</dt><dd>{preview.timeline.presentation_id}</dd></div>
                  <div><dt>Quantum</dt><dd>{preview.timeline.phase_quantum_ms} ms</dd></div>
                  <div><dt>Black pixels</dt><dd>{preview.framebuffer.black_pixel_count}</dd></div>
                </dl>
              )}
            </section>
          )}

          {workspaceMode !== "placement" && workspaceMode !== "assets" && (
            <SceneAuthoringInspector
              scene={selectedSceneDocument}
              scenes={scenes}
              editor={project?.document?.project?.editor}
              selection={sceneSelection}
              onSelect={setSceneSelection}
              onRenameState={renameState}
              onSetRouteTarget={setRouteTarget}
              onSetRouteSceneTarget={setRouteSceneTarget}
              onSetSceneExitTarget={setSceneExitTarget}
              onSetRouteGuard={setRouteGuard}
              onSetRouteAction={setRouteAction}
              onAddRouteAction={addRouteAction}
              onResetRouteLayout={(sceneId, routeId, sourceState) =>
                setStateRouteLayout(sceneId, routeId, sourceState, [], null, null)}
              audioCues={audioCues}
              canEdit={service?.operations.includes("project.apply_commands") === true && busy === null}
            />
          )}

          {workspaceMode !== "placement" && workspaceMode !== "assets" && (
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

          {workspaceMode !== "placement" && workspaceMode !== "assets" && (
            <section className="inspector-section">
              <h3>Validation</h3>
              {project === null ? (
                <p className="muted">No validation result.</p>
              ) : project.issues.length === 0 ? (
                <div className="success-note"><PackageCheck size={17} aria-hidden="true" /> Project is package-ready.</div>
              ) : (
                <div className="issue-list">
                  {project.issues.map((issue, index) => (
                    <div className="issue" key={`${issue.code}-${index}`}>
                      <AlertTriangle size={16} aria-hidden="true" />
                      <span><strong>{issue.code}</strong>{issue.message}<small>{issue.path}</small></span>
                    </div>
                  ))}
                </div>
              )}
            </section>
          )}

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
        </aside>
      </section>

      <footer className="status-bar">
        <span>{busy !== null ? <><LoaderCircle className="spin" size={14} aria-hidden="true" /> {busy}</> : message ?? "Ready"}</span>
        <span>{project?.source_name ?? "No project"}</span>
      </footer>
    </main>
  );
}
