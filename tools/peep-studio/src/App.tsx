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
  Eye,
  FileCode2,
  FolderOpen,
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
  Search,
  SquareMousePointer,
  Trash2,
  Type,
  Volume2,
  X,
  ZoomIn,
  ZoomOut,
} from "lucide-react";
import { useCallback, useEffect, useLayoutEffect, useMemo, useRef, useState, type CSSProperties, type KeyboardEvent as ReactKeyboardEvent, type MouseEvent as ReactMouseEvent, type PointerEvent as ReactPointerEvent, type WheelEvent as ReactWheelEvent } from "react";
import { FramebufferCanvas, FramePreviewCanvas } from "./FramebufferCanvas";
import { parseSpriteSheetGrid } from "./spriteSheetImport";
import { convertSpritePixels, spriteImportOutputScale, type SpriteImportAlphaMode, type SpriteImportConversion, type SpriteImportConversionMode } from "./spriteImportConversion";
import { useEditorPreferences } from "./editorPreferences";
import { SpriteAssetCard } from "./SpriteAssetCard";
import { AnimationClipEditor } from "./AnimationClipEditor";
import { canBuildProject } from "./exportReadiness";
import { AudioWaveform } from "./AudioWaveform";
import { AudioImportWaveform } from "./AudioImportWaveform";
import { AssetTagEditor } from "./AssetTagEditor";
import { EmulatorPanel } from "./EmulatorPanel";
import type { EmulatorPopoutState } from "./EmulatorPopoutApp";
import {
  isFillableShapeKind,
  lineDirectionFromPoints,
  normalizePrimitiveBounds,
  PLACEMENT_HEIGHT,
  PLACEMENT_WIDTH,
  primitiveBoundsFromPoints,
  shapeKindWithFill,
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
import { baseObjectRows, canEditLegacyScene, canPreviewSceneObjects, supportsNativeCreation, supportsStateManagement, supportsLocalGraphCommand, supportsObjectCommand, supportsSceneConnection, usesSceneObjects } from "./sceneCapabilities";
import { SceneObjectInspector } from "./SceneObjectInspector";
import { TimerInspector } from "./TimerInspector";
import { ObjectActionContext } from "./ObjectActionContext";
import { SCENE_TIMER, STATE_TIMER, timerBounds, deleteTimerCommands } from "./timerAuthoring";
import type {
  AssetFrameRecord,
  AssetRecord,
  AuthoredClip,
  AudioAssetRecord,
  AudioAuditionResult,
  AudioCueRecord,
  CompiledAssetFrame,
  EditorHandlerLayout,
  EditorNodePosition,
  EditorRouteRail,
  EditorRouteTokenPositions,
  Framebuffer,
  PackageBuildResult,
  ProjectSettings,
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

type PlacementDrawingTool = "line" | "rectangle" | "circle" | "text";
type PlacementTool = "select" | "sprite" | "animation" | PlacementDrawingTool;
type PlacementPrimitiveDraft = {
  kind: PlacementPrimitiveKind | "text";
  bounds: PlacementBounds;
  lineDirection?: PlacementLineDirection;
};
type WorkspaceMode = "scene-flow" | "logic" | "placement" | "assets";
const TOPBAR_ICONS = {
  assets: "/topbar-icons/assets.png",
  build: "/topbar-icons/build.png",
  egg: "/topbar-icons/egg.png",
  emulator: "/topbar-icons/emulator.png",
  emulatorOpen: "/topbar-icons/emulator_open.png",
  emulatorSleep: "/topbar-icons/emulator_sleep.png",
  logic: "/topbar-icons/local_logic.png",
  newProject: "/topbar-icons/new_project.png",
  open: "/topbar-icons/open.png",
  studioIcon: "/topbar-icons/peep_studio_icon.png",
  studioName: "/topbar-icons/peep_studio_name_logo.png",
  placement: "/topbar-icons/placement.png",
  save: "/topbar-icons/save.png",
  saveAs: "/topbar-icons/save_as.png",
  sceneFlow: "/topbar-icons/Scene_flow.png",
  settings: "/topbar-icons/settings.png",
  undo: "/topbar-icons/undo.png",
  redo: "/topbar-icons/redo.png",
} as const;
const WORKSPACE_MODES: Array<{ mode: WorkspaceMode; label: string; icon: string }> = [
  { mode: "scene-flow", label: "Scene flow", icon: TOPBAR_ICONS.sceneFlow },
  { mode: "logic", label: "Logic", icon: TOPBAR_ICONS.logic },
  { mode: "assets", label: "Assets", icon: TOPBAR_ICONS.assets },
  { mode: "placement", label: "Placement", icon: TOPBAR_ICONS.placement },
];
const EMULATOR_POPOUT_DRAG_THRESHOLD = 34;
const UI_ICONS = {
  animation: "/ui-icons/animation.png",
  circle: "/ui-icons/circle.png",
  line: "/ui-icons/line.png",
  rectangle: "/ui-icons/rectangle.png",
  scene: "/ui-icons/scene.png",
  sfx: "/ui-icons/sfx.png",
  sprite: "/ui-icons/sprite.png",
  state: "/ui-icons/state.png",
  text: "/ui-icons/text.png",
  textSprite: "/ui-icons/text_sprite.png",
} as const;
type StudioIconName = keyof typeof UI_ICONS;
function StudioIcon({ name, className = "" }: { name: StudioIconName; className?: string }) {
  return <img className={`studio-ui-icon ${className}`.trim()} src={UI_ICONS[name]} alt="" aria-hidden="true" />;
}

function readableTextForColor(hex: string): string {
  const match = /^#([0-9a-fA-F]{6})$/.exec(hex);
  if (match === null) return "#f5fbff";
  const value = match[1];
  const red = parseInt(value.slice(0, 2), 16) / 255;
  const green = parseInt(value.slice(2, 4), 16) / 255;
  const blue = parseInt(value.slice(4, 6), 16) / 255;
  const luminance = 0.2126 * red + 0.7152 * green + 0.0722 * blue;
  return luminance > 0.62 ? "#111613" : "#f5fbff";
}
type AssetTab = "sprite" | "audio" | "font";
type SpriteAssetFilter = "all" | "static" | "animation" | "text";
type AssetSelection =
  | { kind: "animation"; clipId: string }
  | { kind: "animation-draft"; clip: AuthoredClip }
  | { kind: "sprite"; frameId: string }
  | { kind: "audio"; cueId: string }
  | { kind: "font"; fontId: string }
  | null;
type EmulatorPopoutCommand =
  | { kind: "reset" }
  | { kind: "togglePlaying" }
  | { kind: "advance"; elapsedMs?: number }
  | { kind: "input"; source?: string };
type SpriteImportPreset = {
  id: string;
  label: string;
  threshold: string;
  alphaCutoff: string;
  alphaMode: SpriteImportAlphaMode;
  conversionMode: SpriteImportConversionMode;
  ditherStrength: string;
  invert: boolean;
};
type SpriteImportPreview = {
  dataUrl: string;
  width: number;
  height: number;
  visiblePixels: number;
  blackPixels: number;
  whitePixels: number;
  transparentPixels: number;
  sourceColors: number;
  sourceColorsLimited: boolean;
  error: string | null;
};
type AnimationAnchorX = "left" | "center" | "right";
type AnimationAnchorY = "top" | "middle" | "bottom";
type AnimationNormalizeDraft = {
  frameIds: string[];
  horizontal: AnimationAnchorX;
  vertical: AnimationAnchorY;
  cadence: string;
  loopPolicy: string;
  error: string | null;
};
type CompiledAssetFrameGroup = {
  assetId: string;
  frames: CompiledAssetFrame[];
};
type AssetLibraryGroup<T> = {
  key: string;
  label: string;
  detail: string;
  items: T[];
};
type AudioAuditionProgress = {
  cueId: string;
  progress: number;
};
type PendingAudioImport = {
  sourcePath: string;
  sourceName: string;
  channels: number;
  sampleRateHz: number;
  bitsPerSample: number;
  durationMs: number;
  peakDbfs: number | null;
  waveformPeaks: number[];
  suggestedTrimStartMs: number;
  suggestedTrimEndMs: number;
  trimStartMs: number;
  trimEndMs: number;
  appliedTrimStartMs?: number;
  appliedTrimEndMs?: number;
};

function audioCueDisplayName(cue: Pick<AudioCueRecord, "cue_id" | "display_name">): string {
  return cue.display_name?.trim() || cue.cue_id.replace(/\.cue(?:_\d+)?$/i, "");
}

function AudioCueSettings({ cue, disabled, onApply }: {
  cue: AudioCueRecord;
  disabled: boolean;
  onApply: (next: AudioCueRecord) => void;
}) {
  const [volume, setVolume] = useState(cue.volume);
  const [priority, setPriority] = useState(cue.priority);
  useEffect(() => {
    setVolume(cue.volume);
    setPriority(cue.priority);
  }, [cue.cue_id, cue.priority, cue.volume]);
  const commit = (nextVolume = volume, nextPriority = priority) => {
    const boundedVolume = Number.isFinite(nextVolume) ? Math.max(0, Math.min(255, Math.round(nextVolume))) : cue.volume;
    const boundedPriority = Number.isFinite(nextPriority) ? Math.max(0, Math.min(255, Math.round(nextPriority))) : cue.priority;
    setVolume(boundedVolume);
    setPriority(boundedPriority);
    if (boundedVolume !== cue.volume || boundedPriority !== cue.priority) {
      onApply({ ...cue, volume: boundedVolume, priority: boundedPriority });
    }
  };
  return <div className="audio-cue-settings">
    <label className="audio-volume-field">
      <span><strong>Volume</strong><output>{volume}</output></span>
      <input
        type="range"
        min={0}
        max={255}
        step={1}
        value={volume}
        disabled={disabled}
        aria-label="SFX cue volume"
        onChange={(event) => setVolume(Number(event.target.value))}
        onPointerUp={(event) => event.currentTarget.blur()}
        onBlur={() => commit()}
      />
      <small>0 is muted; 255 is full authored level.</small>
    </label>
    <label className="audio-priority-field">
      <span>Priority</span>
      <input
        type="number"
        min={0}
        max={255}
        step={1}
        value={priority}
        disabled={disabled}
        aria-label="SFX cue priority"
        onChange={(event) => setPriority(Number(event.target.value))}
        onBlur={() => commit(volume, priority)}
        onKeyDown={(event) => {
          if (event.key === "Enter") event.currentTarget.blur();
          if (event.key === "Escape") {
            setPriority(cue.priority);
            event.currentTarget.blur();
          }
        }}
      />
      <small>Higher-priority sounds may replace lower-priority voices.</small>
    </label>
  </div>;
}
const SYSTEM_FONT_8X8_BASIC_ID = "peepshow.system.8x8.basic.v1";
const PLACEMENT_VIEWPORT_MIN_ZOOM = 0.5;
const PLACEMENT_VIEWPORT_MAX_ZOOM = 6;
const PLACEMENT_VIEWPORT_ZOOM_STEP = 1.25;
const PLACEMENT_GRID_MINOR_X = Array.from({ length: PLACEMENT_WIDTH + 1 }, (_, index) => index)
  .filter((value) => value % 8 !== 0);
const PLACEMENT_GRID_MINOR_Y = Array.from({ length: PLACEMENT_HEIGHT + 1 }, (_, index) => index)
  .filter((value) => value % 8 !== 0);
const PLACEMENT_GRID_MAJOR_X = Array.from({ length: PLACEMENT_WIDTH / 8 + 1 }, (_, index) => index * 8);
const PLACEMENT_GRID_MAJOR_Y = Array.from({ length: PLACEMENT_HEIGHT / 8 + 1 }, (_, index) => index * 8);
const BAKED_TEXT_MIN_FONT_SIZE = 6;
const BAKED_TEXT_MAX_FONT_SIZE = 128;
const BAKED_TEXT_MAX_SOURCE_DIMENSION = 4096;
const NORMALIZED_ANIMATION_MAX_SOURCE_DIMENSION = 4096;
const ASSET_LIBRARY_MIN_ZOOM = 0.6;
const ASSET_LIBRARY_MAX_ZOOM = 1.8;
const ASSET_LIBRARY_ZOOM_STEP = 0.1;
const SPRITE_IMPORT_MAX_OUTPUT_DIMENSION = 4096;
const SPRITE_IMPORT_MAX_OUTPUT_PIXELS = 16_777_216;
const DEFAULT_FONT_PREVIEW_TEXT = "PEEP STUDIO 0123456789 START SETTINGS CREDITS";
const NORMALIZED_ANIMATION_BACKING_DISPLAY_NAME = "Animation backing frames";
const LEGACY_NORMALIZED_ANIMATION_BACKING_DISPLAY_NAME = "Padded animation frames";
const NORMALIZED_ANIMATION_BACKING_ID_PREFIXES = ["animation_backing_frames", "padded_animation_frames"];
const SPRITE_IMPORT_ALPHA_MODES: SpriteImportAlphaMode[] = ["respect_alpha", "ignore_alpha", "transparent_as_white"];
const SPRITE_IMPORT_PRESETS: SpriteImportPreset[] = [
  { id: "standard", label: "Standard B/W", conversionMode: "threshold_1bpp", threshold: "128", ditherStrength: "100", alphaCutoff: "1", alphaMode: "respect_alpha", invert: false },
  { id: "indexed_4", label: "4-colour index 2x2", conversionMode: "ordered_2x2", threshold: "128", ditherStrength: "100", alphaCutoff: "1", alphaMode: "respect_alpha", invert: false },
  { id: "indexed_detail", label: "Fine shades 4x4", conversionMode: "ordered_4x4", threshold: "128", ditherStrength: "100", alphaCutoff: "1", alphaMode: "respect_alpha", invert: false },
  { id: "diffusion", label: "Smooth gradients", conversionMode: "floyd_steinberg", threshold: "128", ditherStrength: "100", alphaCutoff: "1", alphaMode: "respect_alpha", invert: false },
  { id: "white_art", label: "White art on transparent", conversionMode: "threshold_1bpp", threshold: "64", ditherStrength: "100", alphaCutoff: "1", alphaMode: "respect_alpha", invert: false },
  { id: "faint_dark", label: "Faint dark lines", conversionMode: "threshold_1bpp", threshold: "192", ditherStrength: "100", alphaCutoff: "1", alphaMode: "respect_alpha", invert: false },
  { id: "inverted", label: "Invert source", conversionMode: "threshold_1bpp", threshold: "128", ditherStrength: "100", alphaCutoff: "1", alphaMode: "respect_alpha", invert: true },
  { id: "opaque_sheet", label: "Opaque sheet", conversionMode: "threshold_1bpp", threshold: "128", ditherStrength: "100", alphaCutoff: "1", alphaMode: "ignore_alpha", invert: false },
  { id: "transparent_white", label: "Transparent as white", conversionMode: "threshold_1bpp", threshold: "128", ditherStrength: "100", alphaCutoff: "1", alphaMode: "transparent_as_white", invert: false },
];

const normalizeTextLines = (value: string) => value.replace(/\r\n/g, "\n").replace(/\r/g, "\n").split("\n");
const stableAssetIdFromLabel = (value: string, fallback: string) => {
  const stem = value.trim().toLowerCase().replace(/[^a-z0-9_]+/g, "_").replace(/^_+|_+$/g, "");
  const normalized = stem === "" ? fallback : stem;
  const prefixed = /^[a-z]/.test(normalized) ? normalized : `${fallback}_${normalized}`;
  return prefixed.slice(0, 48);
};
const isNormalizedAnimationBackingAsset = (asset: AssetRecord | undefined) => {
  if (asset === undefined || asset.source_format !== "png") {
    return false;
  }
  return asset.display_name === NORMALIZED_ANIMATION_BACKING_DISPLAY_NAME
    || asset.display_name === LEGACY_NORMALIZED_ANIMATION_BACKING_DISPLAY_NAME
    || NORMALIZED_ANIMATION_BACKING_ID_PREFIXES.some((prefix) => asset.asset_id === prefix || asset.asset_id.startsWith(`${prefix}_`));
};
const parseBakedTextFontSize = (value: string) => {
  const size = Number(value);
  return Number.isInteger(size) && size >= BAKED_TEXT_MIN_FONT_SIZE && size <= BAKED_TEXT_MAX_FONT_SIZE
    ? size
    : null;
};
const clampAssetLibraryZoom = (zoom: number) => (
  Math.min(ASSET_LIBRARY_MAX_ZOOM, Math.max(ASSET_LIBRARY_MIN_ZOOM, Math.round(zoom * 10) / 10))
);
const spriteSheetPreviewMetrics = (columns: number, frames: CompiledAssetFrame[], zoom: number) => {
  const safeColumns = Math.max(1, columns);
  const rows = Math.max(1, Math.ceil(frames.length / safeColumns));
  const maxFrameWidth = Math.max(1, ...frames.map(frame => frame.width));
  const maxFrameHeight = Math.max(1, ...frames.map(frame => frame.height));
  const maxFrameDimension = Math.max(maxFrameWidth, maxFrameHeight);
  const frameScale = Math.max(1, Math.round((32 * zoom) / maxFrameDimension));
  const cellWidth = maxFrameWidth * frameScale;
  const cellHeight = maxFrameHeight * frameScale;
  const gap = Math.max(3, Math.round(5 * zoom));
  const padding = Math.round(8 * zoom);
  const cardPadding = Math.round(10 * zoom);
  const previewWidth = safeColumns * cellWidth + Math.max(0, safeColumns - 1) * gap + padding * 2;
  const previewHeight = rows * cellHeight + Math.max(0, rows - 1) * gap + padding * 2;
  const cardWidth = previewWidth + cardPadding * 2 + 2;
  const compactCardWidth = Math.round(178 * zoom);
  const compactPreviewHeight = Math.round(120 * zoom);
  return {
    cellWidth,
    cellHeight,
    frameScale,
    width: Math.max(cardWidth, 178),
    height: previewHeight,
    expanded: cardWidth > compactCardWidth + cardPadding || previewHeight > compactPreviewHeight,
  };
};

const parseSpriteImportConversion = (
  draft: Pick<PendingSpriteImport, "conversionMode" | "threshold" | "ditherStrength" | "alphaCutoff" | "alphaMode" | "invert">,
): (SpriteImportConversion & { error?: undefined }) | { error: string } => {
  if (!["threshold_1bpp", "ordered_2x2", "ordered_4x4", "floyd_steinberg"].includes(draft.conversionMode)) {
    return { error: "Choose a supported conversion mode." };
  }
  const threshold = Number(draft.threshold);
  if (!Number.isInteger(threshold) || threshold < 0 || threshold > 255) {
    return { error: "Threshold must be a whole number from 0 to 255." };
  }
  const ditherStrength = Number(draft.ditherStrength);
  if (!Number.isInteger(ditherStrength) || ditherStrength < 0 || ditherStrength > 100) {
    return { error: "Dither strength must be a whole number from 0 to 100." };
  }
  if (!SPRITE_IMPORT_ALPHA_MODES.includes(draft.alphaMode)) {
    return { error: "Choose a supported transparency mode." };
  }
  const alphaCutoff = Number(draft.alphaCutoff);
  if (draft.alphaMode !== "ignore_alpha" && (!Number.isInteger(alphaCutoff) || alphaCutoff < 1 || alphaCutoff > 255)) {
    return { error: "Alpha cutoff must be a whole number from 1 to 255." };
  }
  return { mode: draft.conversionMode, threshold, ditherStrength, alphaCutoff: draft.alphaMode === "ignore_alpha" ? 1 : alphaCutoff, alphaMode: draft.alphaMode, invert: draft.invert };
};

const parseSpriteImportGrid = (draft: Pick<PendingSpriteImport, "width" | "height" | "columns" | "rows" | "conversionMode">) => {
  const parsed = parseSpriteSheetGrid(draft.width, draft.height, draft.columns, draft.rows);
  if (parsed.error !== undefined) return { ...parsed, scale: 1, outputWidth: 0, outputHeight: 0 };
  const scale = spriteImportOutputScale(draft.conversionMode);
  const outputWidth = draft.width * scale;
  const outputHeight = draft.height * scale;
  if (parsed.frameWidth * scale > 168 || parsed.frameHeight * scale > 144) {
    return {
      ...parsed,
      scale,
      outputWidth,
      outputHeight,
      error: `Expanded frames are ${parsed.frameWidth * scale}x${parsed.frameHeight * scale}; each frame must fit inside 168x144.`,
    };
  }
  if (outputWidth > SPRITE_IMPORT_MAX_OUTPUT_DIMENSION || outputHeight > SPRITE_IMPORT_MAX_OUTPUT_DIMENSION
    || outputWidth * outputHeight > SPRITE_IMPORT_MAX_OUTPUT_PIXELS) {
    return {
      ...parsed,
      scale,
      outputWidth,
      outputHeight,
      error: `Expanded sheet is ${outputWidth}x${outputHeight}; reduce its dimensions or use a smaller pattern.`,
    };
  }
  return { ...parsed, scale, outputWidth, outputHeight };
};

const spriteImportPresetId = (draft: PendingSpriteImport) => (
  SPRITE_IMPORT_PRESETS.find(preset => preset.conversionMode === draft.conversionMode
    && preset.threshold === draft.threshold
    && preset.ditherStrength === draft.ditherStrength
    && preset.alphaCutoff === draft.alphaCutoff
    && preset.alphaMode === draft.alphaMode
    && preset.invert === draft.invert)?.id ?? "custom"
);

const spriteImportAlphaLabel = (alphaMode: SpriteImportAlphaMode) => {
  switch (alphaMode) {
    case "ignore_alpha":
      return "Ignore alpha";
    case "transparent_as_white":
      return "Transparent as white";
    case "respect_alpha":
    default:
      return "Respect alpha";
  }
};

async function loadImageFromDataUrl(dataUrl: string): Promise<HTMLImageElement> {
  return new Promise((resolve, reject) => {
    const image = new window.Image();
    image.onload = () => resolve(image);
    image.onerror = () => reject(new Error("Sprite source preview could not be loaded."));
    image.src = dataUrl;
  });
}

async function renderSpriteImportPng(
  sourceDataUrl: string,
  conversion: SpriteImportConversion,
): Promise<SpriteImportPreview> {
  const source = await loadImageFromDataUrl(sourceDataUrl);
  const width = source.naturalWidth;
  const height = source.naturalHeight;
  if (width <= 0 || height <= 0) {
    throw new Error("Sprite source preview is empty.");
  }
  const canvas = document.createElement("canvas");
  canvas.width = width;
  canvas.height = height;
  const context = canvas.getContext("2d");
  if (context === null) {
    throw new Error("Canvas rendering is unavailable.");
  }
  context.clearRect(0, 0, width, height);
  context.drawImage(source, 0, 0);
  const image = context.getImageData(0, 0, width, height);
  const converted = convertSpritePixels(image.data, width, height, conversion);
  const outputCanvas = document.createElement("canvas");
  outputCanvas.width = converted.width;
  outputCanvas.height = converted.height;
  const outputContext = outputCanvas.getContext("2d");
  if (outputContext === null) {
    throw new Error("Canvas rendering is unavailable.");
  }
  const outputImage = outputContext.createImageData(converted.width, converted.height);
  outputImage.data.set(converted.data);
  outputContext.putImageData(outputImage, 0, 0);
  return {
    dataUrl: outputCanvas.toDataURL("image/png"),
    width: converted.width,
    height: converted.height,
    visiblePixels: converted.visiblePixels,
    blackPixels: converted.blackPixels,
    whitePixels: converted.whitePixels,
    transparentPixels: converted.transparentPixels,
    sourceColors: converted.sourceColors,
    sourceColorsLimited: converted.sourceColorsLimited,
    error: null,
  };
}

async function renderBakedTextPng(
  draft: Pick<BakedTextDraft, "text" | "fontSize">,
  fontFamily: string,
): Promise<{ dataUrl: string; width: number; height: number }> {
  const text = draft.text.replace(/\r\n/g, "\n").replace(/\r/g, "\n");
  if (text.trim().length === 0) {
    throw new Error("Text must contain at least one visible character.");
  }
  const fontSize = parseBakedTextFontSize(draft.fontSize);
  if (fontSize === null) {
    throw new Error(`Font size must be ${BAKED_TEXT_MIN_FONT_SIZE} to ${BAKED_TEXT_MAX_FONT_SIZE} px.`);
  }
  await document.fonts.ready;
  const lines = normalizeTextLines(text);
  const measure = document.createElement("canvas");
  const measureContext = measure.getContext("2d");
  if (measureContext === null) {
    throw new Error("Canvas rendering is unavailable.");
  }
  measureContext.font = `${fontSize}px "${fontFamily}"`;
  const measured = lines.map(line => measureContext.measureText(line.length === 0 ? " " : line));
  const ascent = Math.max(fontSize, ...measured.map(item => item.actualBoundingBoxAscent || Math.ceil(fontSize * 0.8)));
  const descent = Math.max(Math.ceil(fontSize * 0.25), ...measured.map(item => item.actualBoundingBoxDescent || Math.ceil(fontSize * 0.2)));
  const lineHeight = Math.ceil((ascent + descent) * 1.15);
  const padding = Math.max(2, Math.ceil(fontSize / 6));
  const width = Math.ceil(Math.max(1, ...measured.map(item => item.width))) + padding * 2;
  const height = Math.ceil(ascent + descent + lineHeight * Math.max(0, lines.length - 1)) + padding * 2;
  if (width > BAKED_TEXT_MAX_SOURCE_DIMENSION || height > BAKED_TEXT_MAX_SOURCE_DIMENSION) {
    throw new Error(`Rendered text must fit within ${BAKED_TEXT_MAX_SOURCE_DIMENSION}x${BAKED_TEXT_MAX_SOURCE_DIMENSION} px.`);
  }
  const canvas = document.createElement("canvas");
  canvas.width = width;
  canvas.height = height;
  const context = canvas.getContext("2d");
  if (context === null) {
    throw new Error("Canvas rendering is unavailable.");
  }
  context.clearRect(0, 0, width, height);
  context.font = `${fontSize}px "${fontFamily}"`;
  context.fillStyle = "#000000";
  context.textBaseline = "alphabetic";
  let y = padding + ascent;
  for (const line of lines) {
    context.fillText(line, padding, y);
    y += lineHeight;
  }
  const image = context.getImageData(0, 0, width, height);
  for (let index = 0; index < image.data.length; index += 4) {
    const alpha = image.data[index + 3] ?? 0;
    if (alpha >= 96) {
      image.data[index] = 0;
      image.data[index + 1] = 0;
      image.data[index + 2] = 0;
      image.data[index + 3] = 255;
    } else {
      image.data[index] = 255;
      image.data[index + 1] = 255;
      image.data[index + 2] = 255;
      image.data[index + 3] = 0;
    }
  }
  context.putImageData(image, 0, 0);
  return { dataUrl: canvas.toDataURL("image/png"), width, height };
}

const animationAnchorOffset = (container: number, content: number, anchor: AnimationAnchorX | AnimationAnchorY) => {
  if (anchor === "right" || anchor === "bottom") {
    return container - content;
  }
  if (anchor === "center" || anchor === "middle") {
    return Math.floor((container - content) / 2);
  }
  return 0;
};

function renderNormalizedAnimationPng(
  frames: CompiledAssetFrame[],
  horizontal: AnimationAnchorX,
  vertical: AnimationAnchorY,
): { dataUrl: string; frameWidth: number; frameHeight: number; columns: number; rows: number } {
  if (frames.length === 0) {
    throw new Error("Select at least one frame before normalizing.");
  }
  const frameWidth = Math.max(...frames.map(frame => frame.width));
  const frameHeight = Math.max(...frames.map(frame => frame.height));
  const columns = Math.max(1, Math.min(frames.length, Math.floor(NORMALIZED_ANIMATION_MAX_SOURCE_DIMENSION / frameWidth)));
  const rows = Math.ceil(frames.length / columns);
  const width = columns * frameWidth;
  const height = rows * frameHeight;
  if (width > NORMALIZED_ANIMATION_MAX_SOURCE_DIMENSION || height > NORMALIZED_ANIMATION_MAX_SOURCE_DIMENSION) {
    throw new Error(`Normalized animation sheet must fit within ${NORMALIZED_ANIMATION_MAX_SOURCE_DIMENSION}x${NORMALIZED_ANIMATION_MAX_SOURCE_DIMENSION} px.`);
  }
  const canvas = document.createElement("canvas");
  canvas.width = width;
  canvas.height = height;
  const context = canvas.getContext("2d");
  if (context === null) {
    throw new Error("Canvas rendering is unavailable.");
  }
  context.clearRect(0, 0, width, height);
  frames.forEach((frame, index) => {
    const image = context.createImageData(frame.width, frame.height);
    const pixels = atob(frame.pixels_base64);
    const mask = atob(frame.mask_base64);
    for (let y = 0; y < frame.height; y += 1) {
      for (let x = 0; x < frame.width; x += 1) {
        const byteIndex = y * frame.row_stride_bytes + (x >> 3);
        const bit = 0x80 >> (x & 7);
        const visible = frame.opaque || (mask.charCodeAt(byteIndex) & bit) !== 0;
        const black = (pixels.charCodeAt(byteIndex) & bit) !== 0;
        const target = (y * frame.width + x) * 4;
        const value = black ? 0 : 255;
        image.data[target] = value;
        image.data[target + 1] = value;
        image.data[target + 2] = value;
        image.data[target + 3] = visible ? 255 : 0;
      }
    }
    const frameX = (index % columns) * frameWidth + animationAnchorOffset(frameWidth, frame.width, horizontal);
    const frameY = Math.floor(index / columns) * frameHeight + animationAnchorOffset(frameHeight, frame.height, vertical);
    context.putImageData(image, frameX, frameY);
  });
  return { dataUrl: canvas.toDataURL("image/png"), frameWidth, frameHeight, columns, rows };
}

type PlacementViewport = {
  x: number;
  y: number;
  zoom: number;
};

type PlacementGridFrame = {
  left: number;
  top: number;
  width: number;
  height: number;
};

const clampPlacementZoom = (zoom: number) => (
  Math.min(PLACEMENT_VIEWPORT_MAX_ZOOM, Math.max(PLACEMENT_VIEWPORT_MIN_ZOOM, zoom))
);
const snapPlacementGridOffset = (offset: number, origin: number, span: number) => {
  const snapped = Math.round(origin + offset) - origin + 0.5;
  return Math.min(Math.max(snapped, 0.5), Math.max(0.5, span - 0.5));
};

type PendingSpriteImport = {
  assetId: string;
  displayName: string;
  sourceName: string;
  sourceDataUrl: string;
  width: number;
  height: number;
  columns: string;
  rows: string;
  conversionMode: SpriteImportConversionMode;
  threshold: string;
  ditherStrength: string;
  alphaCutoff: string;
  alphaMode: SpriteImportAlphaMode;
  invert: boolean;
};

type BakedTextDraft = {
  assetName: string;
  text: string;
  fontSize: string;
  fontId: string;
  previewDataUrl: string | null;
  previewWidth: number;
  previewHeight: number;
  status: string;
  error: string | null;
};

type FontAssetRecord = {
  font_id: string;
  display_name: string;
  source_path: string;
  source_format: "ttf" | "otf";
};

type BakedTextSourceRecord = {
  asset_id: string;
  frame_id: string;
  display_name: string;
  font_id: string;
  text: string;
  font_size_px: number;
  source_path: string;
  width: number;
  height: number;
  updated_at: string;
};

type BakedTextEditDraft = {
  assetId: string;
  displayName: string;
  text: string;
  fontSize: string;
  fontId: string;
};

type LoadedBakedTextFont = {
  family: string;
  fontId: string;
  displayName: string;
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

function SpriteSheetAssetCard({
  frames,
  name,
  selected,
  onSelect,
  columns,
  frameScale,
  textPreview = false,
  animationSelection,
}: {
  frames: CompiledAssetFrame[];
  name: string;
  selected: boolean;
  onSelect: () => void;
  columns?: number;
  frameScale?: number;
  textPreview?: boolean;
  animationSelection?: {
    selectedFrameIds: string[];
    onToggleFrame: (frameId: string) => void;
  };
}) {
  if (frames.length === 0) {
    return null;
  }
  const resolvedColumns = columns ?? Math.min(4, Math.max(1, frames.length));
  const sheetColumnSize = textPreview ? "minmax(0, 1fr)" : "var(--asset-sheet-cell-width, var(--asset-sheet-cell-size, var(--asset-library-sheet-cell-size, 32px)))";
  const preview = (
    <span className={`asset-sheet-preview ${textPreview ? "text-sprite-preview" : ""}`} style={{ gridTemplateColumns: `repeat(${resolvedColumns}, ${sheetColumnSize})` }}>
      {frames.map((frame, index) => {
        const selectedForAnimation = animationSelection?.selectedFrameIds.includes(frame.frame_id) === true;
        const frameStyle = frameScale === undefined ? undefined : {
          width: `${frame.width * frameScale}px`,
          height: `${frame.height * frameScale}px`,
        };
        return animationSelection === undefined ? (
          <span className="asset-sheet-cell" key={frame.frame_id}>
            <FramePreviewCanvas frame={frame} style={frameStyle} />
          </span>
        ) : (
          <button
            type="button"
            className="asset-sheet-cell asset-sheet-cell-toggle"
            key={frame.frame_id}
            aria-pressed={selectedForAnimation}
            aria-label={`Include ${name} frame ${index + 1} in animation`}
            title={`Frame ${index + 1}`}
            onClick={() => animationSelection.onToggleFrame(frame.frame_id)}
          >
            <FramePreviewCanvas frame={frame} style={frameStyle} />
            <span className="asset-sheet-frame-index">{index + 1}</span>
          </button>
        );
      })}
    </span>
  );
  return (
    <div className={`asset-sheet-card ${selected ? "selected" : ""}`} title={name}>
      {animationSelection === undefined ? (
        <button type="button" className="asset-sheet-preview-button" onClick={onSelect}>{preview}</button>
      ) : preview}
      <button type="button" className="asset-sheet-card-label" onClick={onSelect}>
        <strong>{name}</strong>
        <small>{frames.length} frame{frames.length === 1 ? "" : "s"} / {frames[0].width}x{frames[0].height}</small>
      </button>
    </div>
  );
}

export default function App() {
  const bridge = window.peepStudio;
  const [service, setService] = useState<ServiceHello | null>(null);
  const [project, setProject] = useState<ProjectLoadResult | null>(null);
  const [projectReplacing, setProjectReplacing] = useState(false);
  // Synchronous invalidation also covers replies arriving before React effect cleanup.
  const projectReplacementRef = useRef({ pending: false, generation: 0 });
  const [preview, setPreview] = useState<PreviewSnapshot | null>(null);
  const [placementPreview, setPlacementPreview] = useState<PlacementPreviewSnapshot | null>(null);
  const [placementPreviewLoading, setPlacementPreviewLoading] = useState(false);
  const [placementPreviewError, setPlacementPreviewError] = useState<string | null>(null);
  const [selectedScene, setSelectedScene] = useState<string | null>(null);
  const [projectPath, setProjectPath] = useState<string | null>(null);
  const [temporaryProject, setTemporaryProject] = useState(false);
  const [sceneSelection, setSceneSelection] = useState<SceneSelection>({ kind: "project" });
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
  const [emulatorDockVisible, setEmulatorDockVisible] = useState(false);
  const [emulatorPoppedOut, setEmulatorPoppedOut] = useState(false);
  const [nativeWindowInteracting, setNativeWindowInteracting] = useState(false);
  useEffect(() => {
    if (workspaceMode !== "logic") setSceneSelection(current => current.kind === "timerDraft" ? { kind: "scene" } : current);
  }, [workspaceMode]);
  useEffect(() => {
    if (sceneSelection.kind === "timer" && !project?.document?.scenes?.find(scene => scene.scene_id === selectedScene)
      ?.event_bindings?.some(binding => binding.binding_id === sceneSelection.id)) setSceneSelection({ kind: "scene" });
  }, [project?.document, selectedScene, sceneSelection]);
  const [projectHierarchyExpanded, setProjectHierarchyExpanded] = useState(true);
  const [settingsOpen, setSettingsOpen] = useState(false);
  const [sceneThumbnails, setSceneThumbnails] = useState<Record<string, Framebuffer>>({});
  const [expandedSceneIds, setExpandedSceneIds] = useState<string[]>([]);
  const [collapsedHierarchyIds, setCollapsedHierarchyIds] = useState<string[]>([]);
  const [assetSelection, setAssetSelection] = useState<AssetSelection>(null);
  const [combineFrameIds, setCombineFrameIds] = useState<string[]>([]);
  useEffect(() => setCombineFrameIds([]), [projectPath]);
  const [animationNormalizeDraft, setAnimationNormalizeDraft] = useState<AnimationNormalizeDraft | null>(null);
  const [assetTab, setAssetTab] = useState<AssetTab>("sprite");
  const [assetSearchQuery, setAssetSearchQuery] = useState("");
  const [spriteAssetFilter, setSpriteAssetFilter] = useState<SpriteAssetFilter>("all");
  const [assetTagFilter, setAssetTagFilter] = useState("");
  const [fontAssets, setFontAssets] = useState<FontAssetRecord[]>([]);
  const [fontPreviewFamilies, setFontPreviewFamilies] = useState<Record<string, string>>({});
  const [bakedTextSources, setBakedTextSources] = useState<BakedTextSourceRecord[]>([]);
  const [audioAuditionStatus, setAudioAuditionStatus] = useState("No cue auditioned.");
  const [normalizeAudioImports, setNormalizeAudioImports] = useState(true);
  const [audioImportPeakDbfs, setAudioImportPeakDbfs] = useState(-6);
  const [pendingAudioImport, setPendingAudioImport] = useState<PendingAudioImport | null>(null);
  const [selectedAudioTrim, setSelectedAudioTrim] = useState<PendingAudioImport | null>(null);
  const [selectedAudioTrimError, setSelectedAudioTrimError] = useState<string | null>(null);
  const [audioAuditionProgress, setAudioAuditionProgress] = useState<AudioAuditionProgress | null>(null);
  const [assetPreviewPlaying, setAssetPreviewPlaying] = useState(false);
  const [assetPreviewStep, setAssetPreviewStep] = useState(0);
  const [sceneFlowLayoutStatus, setSceneFlowLayoutStatus] = useState("No layout move yet");
  const [stateGraphLayoutStatus, setStateGraphLayoutStatus] = useState("No layout move yet");
  const [projectWidth, setProjectWidth] = useState(320);
  const [inspectorWidth, setInspectorWidth] = useState(390);
  const { preferences, update: updatePreference } = useEditorPreferences();
  const [systemPrefersDark, setSystemPrefersDark] = useState(() => (
    typeof window !== "undefined" && window.matchMedia("(prefers-color-scheme: dark)").matches
  ));
  useEffect(() => {
    const query = window.matchMedia("(prefers-color-scheme: dark)");
    const updateSystemTheme = () => setSystemPrefersDark(query.matches);
    updateSystemTheme();
    query.addEventListener("change", updateSystemTheme);
    return () => query.removeEventListener("change", updateSystemTheme);
  }, []);
  const resolvedTheme = preferences.theme === "system"
    ? systemPrefersDark ? "dark" : "light"
    : preferences.theme;
  const chromeTextColor = readableTextForColor(preferences.chromeColor);
  const { gridVisible: placementGridVisible, majorGridVisible: placementMajorGridVisible,
    gridStrength: placementGridStrength, objectBoxes: placementOverlayVisible,
    labelMode: placementLabelMode } = preferences;
  const setPlacementGridVisible = (value: boolean) => updatePreference("gridVisible", value);
  const setPlacementMajorGridVisible = (value: boolean) => updatePreference("majorGridVisible", value);
  const setPlacementGridStrength = (value: number) => updatePreference("gridStrength", value);
  const setPlacementOverlayVisible = (value: boolean) => updatePreference("objectBoxes", value);
  const setPlacementLabelMode = (value: "hover" | "always" | "off") => updatePreference("labelMode", value);
  const setAssetLibraryZoom = (value: number) => updatePreference("assetLibraryZoom", clampAssetLibraryZoom(value));
  const [placementTool, setPlacementTool] = useState<PlacementTool>("select");
  const [placementPrimitiveDraft, setPlacementPrimitiveDraft] = useState<PlacementPrimitiveDraft | null>(null);
  const [spritePickerOpen, setSpritePickerOpen] = useState(false);
  const [placementViewport, setPlacementViewport] = useState<PlacementViewport>({ x: 0, y: 0, zoom: 1 });
  const [placementViewportPanning, setPlacementViewportPanning] = useState(false);
  const [placementGridFrame, setPlacementGridFrame] = useState<PlacementGridFrame | null>(null);
  const [placementDraftPositions, setPlacementDraftPositions] = useState<Record<string, { x: number; y: number }>>({});
  const [placementDraftBounds, setPlacementDraftBounds] = useState<Record<string, { x: number; y: number; width: number; height: number }>>({});
  const [pendingSpriteImport, setPendingSpriteImport] = useState<PendingSpriteImport | null>(null);
  const [spriteImportPreview, setSpriteImportPreview] = useState<SpriteImportPreview | null>(null);
  const [bakedTextDraft, setBakedTextDraft] = useState<BakedTextDraft | null>(null);
  const [bakedTextEditDraft, setBakedTextEditDraft] = useState<BakedTextEditDraft | null>(null);
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
  const emulatorIconDragRef = useRef<{ pointerId: number; startX: number; startY: number } | null>(null);
  const suppressEmulatorToggleClickRef = useRef(false);
  const placementSelectionAnchorRef = useRef<string | null>(null);
  const placementSceneRef = useRef<string | null>(null);
  const placementDrawCancelRef = useRef<(() => boolean) | null>(null);
  const placementStageRef = useRef<HTMLDivElement | null>(null);
  const placementScreenOverlayRef = useRef<HTMLDivElement | null>(null);
  const bakedTextFontFacesRef = useRef(new Map<string, LoadedBakedTextFont>());
  const bakedTextPreviewRequestRef = useRef(0);
  const spriteImportPreviewRequestRef = useRef(0);

  useEffect(() => {
    setPendingSpriteImport(null);
    setSpriteImportPreview(null);
    setBakedTextDraft(null);
    setBakedTextEditDraft(null);
    setFontAssets([]);
    setFontPreviewFamilies({});
    setBakedTextSources([]);
    bakedTextFontFacesRef.current.clear();
  }, [projectPath]);

  const refreshFontAssets = useCallback(async () => {
    if (bridge === undefined || projectPath === null || bridge.readFontAssets === undefined) {
      setFontAssets([]);
      return;
    }
    try {
      setFontAssets(await bridge.readFontAssets(projectPath));
    } catch (error) {
      setFontAssets([]);
      setAssetImportDebug(`Font catalog load failed: ${errorText(error)}`);
    }
  }, [bridge, projectPath]);

  useEffect(() => {
    void refreshFontAssets();
  }, [refreshFontAssets]);

  const refreshBakedTextSources = useCallback(async () => {
    if (bridge === undefined || projectPath === null || bridge.readBakedTextSources === undefined) {
      setBakedTextSources([]);
      return;
    }
    try {
      setBakedTextSources(await bridge.readBakedTextSources(projectPath));
    } catch (error) {
      setBakedTextSources([]);
      setAssetImportDebug(`Text source catalog load failed: ${errorText(error)}`);
    }
  }, [bridge, projectPath]);

  useEffect(() => {
    void refreshBakedTextSources();
  }, [refreshBakedTextSources]);

  useEffect(() => {
    if (pendingSpriteImport === null) {
      setSpriteImportPreview(null);
      return;
    }
    const requestId = spriteImportPreviewRequestRef.current + 1;
    spriteImportPreviewRequestRef.current = requestId;
    const conversion = parseSpriteImportConversion(pendingSpriteImport);
    if (conversion.error !== undefined) {
      setSpriteImportPreview({
        dataUrl: "",
        width: pendingSpriteImport.width * spriteImportOutputScale(pendingSpriteImport.conversionMode),
        height: pendingSpriteImport.height * spriteImportOutputScale(pendingSpriteImport.conversionMode),
        visiblePixels: 0,
        blackPixels: 0,
        whitePixels: 0,
        transparentPixels: 0,
        sourceColors: 0,
        sourceColorsLimited: false,
        error: conversion.error,
      });
      return;
    }
    void renderSpriteImportPng(pendingSpriteImport.sourceDataUrl, conversion)
      .then(preview => {
        if (spriteImportPreviewRequestRef.current !== requestId) {
          return;
        }
        setSpriteImportPreview(preview);
      })
      .catch(error => {
        if (spriteImportPreviewRequestRef.current !== requestId) {
          return;
        }
        setSpriteImportPreview({
          dataUrl: "",
          width: pendingSpriteImport.width * spriteImportOutputScale(pendingSpriteImport.conversionMode),
          height: pendingSpriteImport.height * spriteImportOutputScale(pendingSpriteImport.conversionMode),
          visiblePixels: 0,
          blackPixels: 0,
          whitePixels: 0,
          transparentPixels: 0,
          sourceColors: 0,
          sourceColorsLimited: false,
          error: errorText(error),
        });
      });
  }, [
    pendingSpriteImport?.alphaCutoff,
    pendingSpriteImport?.alphaMode,
    pendingSpriteImport?.conversionMode,
    pendingSpriteImport?.ditherStrength,
    pendingSpriteImport?.height,
    pendingSpriteImport?.invert,
    pendingSpriteImport?.sourceDataUrl,
    pendingSpriteImport?.threshold,
    pendingSpriteImport?.width,
  ]);

  const loadBakedTextFontFace = useCallback(async (font: FontAssetRecord): Promise<LoadedBakedTextFont> => {
    const cached = bakedTextFontFacesRef.current.get(font.font_id);
    if (cached !== undefined) {
      return cached;
    }
    if (bridge === undefined || projectPath === null || bridge.fontAssetSource === undefined) {
      throw new Error("Restart Peep Studio to enable imported font loading.");
    }
    const source = await bridge.fontAssetSource(projectPath, font.source_path);
    const family = `peep_font_${font.font_id.replace(/[^a-zA-Z0-9_-]+/g, "_")}_${bakedTextFontFacesRef.current.size + 1}`;
    const face = new FontFace(family, `url("${source.data}")`);
    await face.load();
    document.fonts.add(face);
    const loaded = { fontId: font.font_id, family, displayName: font.display_name };
    bakedTextFontFacesRef.current.set(font.font_id, loaded);
    return loaded;
  }, [bridge, projectPath]);

  useEffect(() => {
    let cancelled = false;
    setFontPreviewFamilies(current => {
      const next: Record<string, string> = {};
      for (const font of fontAssets) {
        if (current[font.font_id] !== undefined) {
          next[font.font_id] = current[font.font_id];
        }
      }
      return next;
    });
    for (const font of fontAssets) {
      void loadBakedTextFontFace(font)
        .then(loaded => {
          if (cancelled) {
            return;
          }
          setFontPreviewFamilies(current => current[font.font_id] === loaded.family
            ? current
            : { ...current, [font.font_id]: loaded.family });
        })
        .catch(() => {
          if (cancelled) {
            return;
          }
          setFontPreviewFamilies(current => current[font.font_id] === ""
            ? current
            : { ...current, [font.font_id]: "" });
        });
    }
    return () => {
      cancelled = true;
    };
  }, [fontAssets, loadBakedTextFontFace]);

  useEffect(() => {
    if (bakedTextDraft === null) {
      return;
    }
    const requestId = bakedTextPreviewRequestRef.current + 1;
    bakedTextPreviewRequestRef.current = requestId;
    const font = fontAssets.find(item => item.font_id === bakedTextDraft.fontId);
    if (font === undefined) {
      setBakedTextDraft(current => current === null ? null : {
        ...current,
        previewDataUrl: null,
        previewWidth: 0,
        previewHeight: 0,
        error: null,
        status: fontAssets.length === 0
          ? "Import a font asset before creating baked text sprites."
          : "Choose a font asset to render this sprite.",
      });
      return;
    }
    void loadBakedTextFontFace(font)
      .then(loaded => renderBakedTextPng(bakedTextDraft, loaded.family)
        .then(preview => ({ preview, loaded })))
      .then(preview => {
        if (bakedTextPreviewRequestRef.current !== requestId) {
          return;
        }
        setBakedTextDraft(current => current === null ? null : {
          ...current,
          previewDataUrl: preview.preview.dataUrl,
          previewWidth: preview.preview.width,
          previewHeight: preview.preview.height,
          error: null,
          status: `${preview.loaded.displayName} - ${preview.preview.width}x${preview.preview.height} px generated sprite`,
        });
      })
      .catch(error => {
        if (bakedTextPreviewRequestRef.current !== requestId) {
          return;
        }
        setBakedTextDraft(current => current === null ? null : {
          ...current,
          previewDataUrl: null,
          previewWidth: 0,
          previewHeight: 0,
          error: errorText(error),
          status: "Preview blocked.",
        });
      });
  }, [bakedTextDraft?.fontId, bakedTextDraft?.fontSize, bakedTextDraft?.text, fontAssets, loadBakedTextFontFace]);

  const stopAudioPlayback = useCallback(() => {
    audioPlaybackRequestRef.current += 1;
    setAudioAuditionProgress(null);
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

  useEffect(() => {
    if (bridge?.getEmulatorPopoutStatus === undefined) {
      return undefined;
    }
    let cancelled = false;
    void bridge.getEmulatorPopoutStatus()
      .then((status) => {
        if (!cancelled) {
          setEmulatorPoppedOut(status.open);
        }
      })
      .catch(() => {
        if (!cancelled) {
          setEmulatorPoppedOut(false);
        }
      });
    const removeClosedListener = bridge.onEmulatorPopoutClosed?.(() => setEmulatorPoppedOut(false));
    return () => {
      cancelled = true;
      removeClosedListener?.();
    };
  }, [bridge]);

  useEffect(() => {
    if (bridge?.onNativeWindowInteraction === undefined) {
      return undefined;
    }
    return bridge.onNativeWindowInteraction(setNativeWindowInteracting);
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

  const resetProjectPreview = useCallback(() => {
    const target = previewStartRef.current;
    if (target !== null) {
      void startPreview(target.sceneId, { stateId: target.stateId, updateSelection: false });
      return;
    }
    if (selectedSceneRef.current !== null) {
      void startPreview(selectedSceneRef.current, { updateSelection: false });
    }
  }, [startPreview]);

  const loadProject = useCallback(
    async (path: string) => {
      if (bridge === undefined || projectReplacementRef.current.pending) {
        return;
      }
      projectReplacementRef.current = { pending: true, generation: projectReplacementRef.current.generation + 1 };
      setProjectReplacing(true);
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
        projectReplacementRef.current.pending = false;
        setProjectReplacing(false);
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
    if (bridge === undefined || busy !== null || projectReplacementRef.current.pending) {
      return;
    }
    const path = await bridge.chooseNewProjectPath();
    if (path === null) {
      return;
    }
    if (projectReplacementRef.current.pending) return;
    projectReplacementRef.current = { pending: true, generation: projectReplacementRef.current.generation + 1 };
    setProjectReplacing(true);
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
      projectReplacementRef.current.pending = false;
      setProjectReplacing(false);
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
    if (!playing || nativeWindowInteracting) {
      return undefined;
    }
    const interval = window.setInterval(() => void advancePreview(250), 250);
    return () => window.clearInterval(interval);
  }, [advancePreview, nativeWindowInteracting, playing]);

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
        setAudioAuditionProgress(null);
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

  const sendInput = useCallback(
    async (logicalSource: string) => {
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
        if (result.scene.scene_id !== selectedSceneRef.current) {
          setSelectedScene(result.scene.scene_id);
          setSceneSelection((current) => current.kind === "project" ? current : { kind: "scene" });
        }
        setMessage(null);
      } catch (error) {
        setMessage(errorText(error));
      } finally {
        operationLock.current = false;
      }
    },
    [bridge, playPreviewAudioEvents],
  );

  useEffect(() => {
    if (bridge?.onEmulatorPopoutCommand === undefined) {
      return undefined;
    }
    return bridge.onEmulatorPopoutCommand((value) => {
      if (value === null || typeof value !== "object") {
        return;
      }
      const command = value as EmulatorPopoutCommand;
      if (command.kind === "reset") {
        resetProjectPreview();
      } else if (command.kind === "togglePlaying") {
        setPlaying((current) => !current);
      } else if (command.kind === "advance") {
        void advancePreview(command.elapsedMs ?? 250);
      } else if (command.kind === "input" && typeof command.source === "string") {
        void sendInput(command.source);
      }
    });
  }, [advancePreview, bridge, resetProjectPreview, sendInput]);

  const buildPackage = async () => {
    if (bridge === undefined || project === null || busy !== null || !canBuildProject(service, project)) {
      return;
    }
    setBusy("Building package");
    setBuild(null);
    try {
      const result = await bridge.serviceRequest<PackageBuildResult>("project.build_package", {
        project_revision: project.project_revision,
      });
      if (result.project_revision !== projectRevisionRef.current || projectReplacementRef.current.pending) {
        setMessage("Project changed during build. Build the current revision before exporting.");
        return;
      }
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

  const updateProjectSettings = async (settings: ProjectSettings) => {
    const capability = service?.project_settings;
    if (bridge === undefined || project === null || busy !== null || capability?.persisted !== true
      || capability.edit_command !== "project.settings.set"
      || service?.operations.includes("project.apply_commands") !== true) return false;
    setBusy("Updating project settings");
    setPlaying(false);
    stopAudioPlayback();
    try {
      const result = await bridge.serviceRequest<ProjectCommandResult>("project.apply_commands", {
        project_revision: project.project_revision,
        commands: [{ kind: capability.edit_command, settings }],
      });
      applyProjectResult(result);
      setMessage("Project preferences updated. Save to write them to the project.");
      return true;
    } catch (error) {
      setMessage(errorText(error));
      return false;
    } finally {
      setBusy(null);
    }
  };

  const applyProjectResult = (
    result: ProjectHistoryResult | ProjectCommandResult | ProjectSaveResult,
    options: ApplyProjectResultOptions = {},
  ) => {
    projectRevisionRef.current = result.project_revision;
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
      setAssetTab(selection.kind === "audio" ? "audio" : selection.kind === "font" ? "font" : "sprite");
      setSceneSelection({ kind: "scene" });
    }
  };
  const selectAssetTab = (tab: AssetTab) => {
    if (tab === assetTab) return;
    setAssetTab(tab);
    setAssetSelection(null);
    setAssetTagFilter("");
    setAssetPreviewPlaying(false);
    stopAudioPlayback();
  };
  const clearAssetLibrarySelection = () => {
    setAssetSelection(null);
    setCombineFrameIds([]);
    setAnimationNormalizeDraft(null);
    setAssetPreviewPlaying(false);
    stopAudioPlayback();
  };
  const handleAssetWorkspaceBackgroundClick = (event: ReactMouseEvent<HTMLElement>) => {
    const target = event.target;
    if (!(target instanceof Element)) {
      return;
    }
    if (target.closest("button, input, select, textarea, label, summary, a, .asset-import-panel")) {
      return;
    }
    clearAssetLibrarySelection();
  };
  const handleAssetWorkspaceWheel = (event: ReactWheelEvent<HTMLDivElement>) => {
    if (!event.ctrlKey && !event.metaKey) {
      return;
    }
    event.preventDefault();
    setAssetLibraryZoom(preferences.assetLibraryZoom + (event.deltaY < 0 ? ASSET_LIBRARY_ZOOM_STEP : -ASSET_LIBRARY_ZOOM_STEP));
  };
  const clearSceneFlowSelection = () => {
    setSceneSelection({ kind: "project" });
    clearAssetLibrarySelection();
  };
  const clearLogicSelection = () => {
    setSceneSelection({ kind: "scene" });
    clearAssetLibrarySelection();
  };

  useEffect(() => {
    const handleRootSelectionShortcut = (event: KeyboardEvent) => {
      if (event.key !== "Escape" || event.defaultPrevented) {
        return;
      }
      if (settingsOpen) {
        event.preventDefault();
        setSettingsOpen(false);
        return;
      }
      if (project === null) return;
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
  }, [project, settingsOpen]);

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
      const suggestTiles = (imported.width > 168 || imported.height > 144)
        && imported.width % 16 === 0 && imported.height % 16 === 0;
      setPendingSpriteImport({
        ...imported,
        assetId,
        columns: String(suggestTiles ? imported.width / 16 : 1),
        rows: String(suggestTiles ? imported.height / 16 : 1),
        conversionMode: "threshold_1bpp",
        threshold: "128",
        ditherStrength: "100",
        alphaCutoff: "1",
        alphaMode: "respect_alpha",
        invert: false,
      });
      setWorkspaceMode("assets");
      setAssetImportDebug(`Picked ${imported.sourceName} (${imported.width}x${imported.height}).`);
      setMessage("Adjust conversion and frame split, then import the sprite.");
    } catch (error) {
      const text = errorText(error);
      setAssetImportDebug(`PNG picker failed: ${text}`);
      setMessage(text);
    } finally {
      setBusy(null);
    }
  };

  const confirmSpriteImport = async () => {
    if (bridge === undefined || project === null || projectPath === null || pendingSpriteImport === null || busy !== null) {
      return;
    }
    setBusy("Importing sprite");
    setPlaying(false);
    try {
      if (bridge.writeGeneratedSpritePng === undefined) {
        setAssetImportDebug("Import blocked: restart Peep Studio to enable staged sprite writing.");
        setMessage("Restart Peep Studio to enable staged sprite writing.");
        return;
      }
      const parsed = parseSpriteImportGrid(pendingSpriteImport);
      if (parsed.error !== undefined) {
        setAssetImportDebug(`Import blocked: ${parsed.error}`);
        setMessage(parsed.error);
        return;
      }
      const conversion = parseSpriteImportConversion(pendingSpriteImport);
      if (conversion.error !== undefined) {
        setAssetImportDebug(`Import blocked: ${conversion.error}`);
        setMessage(conversion.error);
        return;
      }
      const converted = await renderSpriteImportPng(pendingSpriteImport.sourceDataUrl, conversion);
      const written = await bridge.writeGeneratedSpritePng(projectPath, pendingSpriteImport.assetId, converted.dataUrl);
      const frames = createGridFrames(
        written.assetId,
        written.width,
        written.height,
        parsed.frameWidth * parsed.scale,
        parsed.frameHeight * parsed.scale,
      );
      const result = await bridge.serviceRequest<ProjectCommandResult>("project.apply_commands", {
        project_revision: project.project_revision,
        commands: [
          {
            kind: "asset.upsert",
            asset: {
              asset_id: written.assetId,
              display_name: pendingSpriteImport.displayName,
              asset_type: "masked_1bpp",
              source_path: written.sourcePath,
              source_format: "png",
              frames,
            },
          },
        ],
      });
      applyProjectResult(result);
      selectAssetRecord(frames[0] === undefined ? null : { kind: "sprite", frameId: frames[0].frame_id });
      setCombineFrameIds([]);
      setWorkspaceMode("assets");
      setPendingSpriteImport(null);
      const conversionLabel = conversion.mode === "threshold_1bpp"
        ? `threshold ${conversion.threshold}`
        : `${conversion.mode.replaceAll("_", " ")}, threshold ${conversion.threshold}, strength ${conversion.ditherStrength}%`;
      setAssetImportDebug(`Imported ${pendingSpriteImport.sourceName}: ${frames.length} frame${frames.length === 1 ? "" : "s"} at ${parsed.frameWidth}x${parsed.frameHeight}, ${conversionLabel}, ${spriteImportAlphaLabel(conversion.alphaMode).toLowerCase()}${conversion.alphaMode === "ignore_alpha" ? "" : ` ${conversion.alphaCutoff}`}${conversion.invert ? ", inverted" : ""}.`);
      setMessage(`Imported ${written.assetId} with ${frames.length} frame${frames.length === 1 ? "" : "s"}. Save to write it to the project.`);
    } catch (error) {
      const text = errorText(error);
      setAssetImportDebug(`Import failed: ${text}`);
      setMessage(text);
    } finally {
      setBusy(null);
    }
  };

  const startBakedTextSprite = () => {
    setPendingSpriteImport(null);
    const selectedFontId = assetSelection?.kind === "font"
      ? assetSelection.fontId
      : fontAssets[0]?.font_id ?? "";
    setBakedTextDraft({
      assetName: "Text sprite",
      text: "LABEL",
      fontSize: "16",
      fontId: selectedFontId,
      previewDataUrl: null,
      previewWidth: 0,
      previewHeight: 0,
      status: selectedFontId === ""
        ? "Import a font asset before creating baked text sprites."
        : "Ready to render text.",
      error: null,
    });
    setWorkspaceMode("assets");
    setAssetTab("sprite");
  };

  const importFontAsset = async () => {
    if (bridge === undefined || projectPath === null || busy !== null) {
      return;
    }
    if (bridge.importFontAsset === undefined) {
      setMessage("Restart Peep Studio to enable font asset import.");
      return;
    }
    setBusy("Importing font");
    setPlaying(false);
    try {
      const imported = await bridge.importFontAsset(projectPath);
      if (imported === null) {
        setMessage("Font import cancelled.");
        return;
      }
      await refreshFontAssets();
      setFontAssets(current => current.some(item => item.font_id === imported.font_id) ? current : [...current, imported]);
      selectAssetRecord({ kind: "font", fontId: imported.font_id });
      setAssetTab("font");
      setWorkspaceMode("assets");
      setMessage(`Imported font ${imported.display_name}.`);
    } catch (error) {
      setMessage(errorText(error));
    } finally {
      setBusy(null);
    }
  };

  const renameFontAsset = async (font: FontAssetRecord, nextDisplayName: string) => {
    if (bridge === undefined || projectPath === null || busy !== null) {
      return;
    }
    const displayName = nextDisplayName.trim();
    if (displayName.length === 0 || displayName.length > 64) {
      setMessage("Font name must be 1 to 64 characters.");
      return;
    }
    if (displayName === font.display_name) {
      return;
    }
    if (bridge.renameFontAsset === undefined) {
      setMessage("Restart Peep Studio to enable font rename.");
      return;
    }
    setBusy("Renaming font");
    try {
      const renamed = await bridge.renameFontAsset(projectPath, font.font_id, displayName);
      setFontAssets(current => current.map(item => item.font_id === renamed.font_id ? renamed : item));
      selectAssetRecord({ kind: "font", fontId: renamed.font_id });
      setMessage("Font renamed.");
    } catch (error) {
      setMessage(errorText(error));
    } finally {
      setBusy(null);
    }
  };

  const deleteFontAsset = async (font: FontAssetRecord) => {
    if (bridge === undefined || projectPath === null || busy !== null) {
      return;
    }
    const references = bakedTextSources.filter(source => source.font_id === font.font_id);
    if (references.length > 0) {
      setMessage(`Cannot delete ${font.display_name}; ${references.length} baked text sprite${references.length === 1 ? "" : "s"} still use it.`);
      return;
    }
    if (bridge.deleteFontAsset === undefined) {
      setMessage("Restart Peep Studio to enable font delete.");
      return;
    }
    setBusy("Deleting font");
    try {
      await bridge.deleteFontAsset(projectPath, font.font_id);
      setFontAssets(current => current.filter(item => item.font_id !== font.font_id));
      setFontPreviewFamilies(current => {
        const next = { ...current };
        delete next[font.font_id];
        return next;
      });
      if (assetSelection?.kind === "font" && assetSelection.fontId === font.font_id) {
        setAssetSelection(null);
      }
      setMessage("Font removed from the project catalog.");
    } catch (error) {
      setMessage(errorText(error));
    } finally {
      setBusy(null);
    }
  };

  const confirmBakedTextSprite = async () => {
    if (bridge === undefined || project === null || projectPath === null || bakedTextDraft === null || busy !== null) {
      return;
    }
    if (bridge.writeGeneratedSpritePng === undefined) {
      setBakedTextDraft(current => current === null ? null : {
        ...current,
        error: "Restart Peep Studio to enable generated sprite writing.",
      });
      return;
    }
    const font = fontAssets.find(item => item.font_id === bakedTextDraft.fontId);
    if (font === undefined) {
      setBakedTextDraft(current => current === null ? null : {
        ...current,
        error: "Choose an imported font asset before creating a baked text sprite.",
      });
      return;
    }
    const displayName = bakedTextDraft.assetName.trim();
    if (displayName.length === 0 || displayName.length > 64) {
      setBakedTextDraft(current => current === null ? null : {
        ...current,
        error: "Asset name must be 1 to 64 characters.",
      });
      return;
    }
    setBusy("Creating text sprite");
    setPlaying(false);
    try {
      const loadedFont = await loadBakedTextFontFace(font);
      const rendered = await renderBakedTextPng(bakedTextDraft, loadedFont.family);
      const requestedAssetId = uniqueImportedAssetId(stableAssetIdFromLabel(`text_${displayName}`, "text"));
      const written = await bridge.writeGeneratedSpritePng(projectPath, requestedAssetId, rendered.dataUrl);
      const frame: AssetFrameRecord = {
        frame_id: `${written.assetId}.frame`,
        display_name: "Frame",
        source_rect: { x: 0, y: 0, width: written.width, height: written.height },
        pivot_x: 0,
        pivot_y: 0,
      };
      const result = await bridge.serviceRequest<ProjectCommandResult>("project.apply_commands", {
        project_revision: project.project_revision,
        commands: [
          {
            kind: "asset.upsert",
            asset: {
              asset_id: written.assetId,
              display_name: displayName,
              asset_type: "masked_1bpp",
              source_path: written.sourcePath,
              source_format: "png",
              frames: [frame],
            },
          },
        ],
      });
      applyProjectResult(result);
      const sourceRecord: BakedTextSourceRecord = {
        asset_id: written.assetId,
        frame_id: frame.frame_id,
        display_name: displayName,
        font_id: font.font_id,
        text: bakedTextDraft.text.replace(/\r\n/g, "\n").replace(/\r/g, "\n"),
        font_size_px: parseBakedTextFontSize(bakedTextDraft.fontSize) ?? BAKED_TEXT_MIN_FONT_SIZE,
        source_path: written.sourcePath,
        width: written.width,
        height: written.height,
        updated_at: new Date().toISOString(),
      };
      if (bridge.upsertBakedTextSource !== undefined) {
        try {
          await bridge.upsertBakedTextSource(projectPath, sourceRecord);
          setBakedTextSources(current => [
            ...current.filter(item => item.asset_id !== sourceRecord.asset_id),
            sourceRecord,
          ].sort((left, right) => left.asset_id.localeCompare(right.asset_id)));
        } catch (metadataError) {
          setAssetImportDebug(`Created ${written.sourcePath}, but text source metadata was not saved: ${errorText(metadataError)}`);
        }
      } else {
        setAssetImportDebug(`Created ${written.sourcePath}. Restart Peep Studio to persist editable text source metadata.`);
      }
      selectAssetRecord({ kind: "sprite", frameId: frame.frame_id });
      setCombineFrameIds([frame.frame_id]);
      setBakedTextDraft(null);
      if (bridge.upsertBakedTextSource !== undefined) {
        setAssetImportDebug(`Created baked text sprite ${written.sourcePath} from ${font.display_name}.`);
      }
      setMessage(`Created ${displayName} as a baked text sprite. Save to write it to the project.`);
    } catch (error) {
      const text = errorText(error);
      setBakedTextDraft(current => current === null ? null : {
        ...current,
        error: text,
        status: "Text sprite creation failed.",
      });
      setMessage(text);
    } finally {
      setBusy(null);
    }
  };

  const regenerateBakedTextSprite = async (
    source: BakedTextSourceRecord,
    asset: AssetRecord,
    draft: BakedTextEditDraft,
  ) => {
    if (bridge === undefined || project === null || projectPath === null || busy !== null) {
      return;
    }
    if (bridge.overwriteGeneratedSpritePng === undefined || bridge.upsertBakedTextSource === undefined) {
      setMessage("Restart Peep Studio to enable baked text regeneration.");
      return;
    }
    const font = fontAssets.find(item => item.font_id === draft.fontId);
    if (font === undefined) {
      setMessage("Choose an imported font asset before regenerating this text sprite.");
      return;
    }
    const displayName = draft.displayName.trim();
    if (displayName.length === 0 || displayName.length > 64) {
      setMessage("Sprite name must be 1 to 64 characters.");
      return;
    }
    const fontSize = parseBakedTextFontSize(draft.fontSize);
    if (fontSize === null) {
      setMessage(`Font size must be ${BAKED_TEXT_MIN_FONT_SIZE} to ${BAKED_TEXT_MAX_FONT_SIZE} px.`);
      return;
    }
    setBusy("Regenerating text sprite");
    setPlaying(false);
    try {
      const loadedFont = await loadBakedTextFontFace(font);
      const normalizedText = draft.text.replace(/\r\n/g, "\n").replace(/\r/g, "\n");
      const rendered = await renderBakedTextPng({ text: normalizedText, fontSize: String(fontSize) }, loadedFont.family);
      const written = await bridge.overwriteGeneratedSpritePng(projectPath, source.source_path, rendered.dataUrl);
      const existingFrame = asset.frames.find(frame => frame.frame_id === source.frame_id) ?? asset.frames[0];
      const frameId = existingFrame?.frame_id ?? source.frame_id;
      const nextFrame: AssetFrameRecord = {
        ...(existingFrame ?? {}),
        frame_id: frameId,
        display_name: existingFrame?.display_name ?? "Frame",
        source_rect: { x: 0, y: 0, width: written.width, height: written.height },
        pivot_x: existingFrame?.pivot_x ?? 0,
        pivot_y: existingFrame?.pivot_y ?? 0,
      };
      const hasTargetFrame = asset.frames.some(frame => frame.frame_id === frameId);
      const frames = asset.frames.length === 0
        ? [nextFrame]
        : hasTargetFrame
          ? asset.frames.map(frame => frame.frame_id === frameId ? nextFrame : frame)
          : [nextFrame, ...asset.frames];
      const updatedAsset: AssetRecord = {
        ...asset,
        display_name: displayName,
        source_path: written.sourcePath,
        source_format: "png",
        frames,
      };
      const result = await bridge.serviceRequest<ProjectCommandResult>("project.apply_commands", {
        project_revision: project.project_revision,
        commands: [{ kind: "asset.upsert", asset: updatedAsset }],
      });
      applyProjectResult(result);
      const nextSource: BakedTextSourceRecord = {
        ...source,
        frame_id: frameId,
        display_name: displayName,
        font_id: font.font_id,
        text: normalizedText,
        font_size_px: fontSize,
        source_path: written.sourcePath,
        width: written.width,
        height: written.height,
        updated_at: new Date().toISOString(),
      };
      await bridge.upsertBakedTextSource(projectPath, nextSource);
      setBakedTextSources(current => [
        ...current.filter(item => item.asset_id !== nextSource.asset_id),
        nextSource,
      ].sort((left, right) => left.asset_id.localeCompare(right.asset_id)));
      selectAssetRecord({ kind: "sprite", frameId });
      setBakedTextEditDraft({
        assetId: nextSource.asset_id,
        displayName: nextSource.display_name,
        text: nextSource.text,
        fontSize: String(nextSource.font_size_px),
        fontId: nextSource.font_id,
      });
      const sizeChanged = source.width !== written.width || source.height !== written.height;
      setMessage(sizeChanged
        ? `Regenerated ${displayName}. Size changed ${source.width}x${source.height} to ${written.width}x${written.height}; check placements.`
        : `Regenerated ${displayName}. Save to write it to the project.`);
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
    setBusy("Reading audio");
    setPlaying(false);
    try {
      const selected = await bridge.chooseAudioWav(projectPath);
      if (selected === null) {
        setAudioAuditionStatus("WAV picker cancelled.");
        setMessage("Audio import cancelled.");
        return;
      }
      setPendingAudioImport({ ...selected, trimStartMs: 0, trimEndMs: selected.durationMs });
      setAssetSelection(null);
      setMessage(`Review ${selected.sourceName}, then trim and import it.`);
    } catch (error) {
      const text = errorText(error);
      setAudioAuditionStatus(`Import failed: ${text}`);
      setMessage(text);
    } finally {
      setBusy(null);
    }
  };

  const importPendingAudioWav = async () => {
    if (bridge === undefined || project === null || projectPath === null || pendingAudioImport === null || busy !== null) return;
    setBusy("Importing audio");
    setPlaying(false);
    try {
      const imported = await bridge.importAudioWav(projectPath, pendingAudioImport.sourcePath, {
        normalize: normalizeAudioImports,
        targetPeakDbfs: audioImportPeakDbfs,
        trimStartMs: pendingAudioImport.trimStartMs,
        trimEndMs: pendingAudioImport.trimEndMs,
      });
      if (imported === null) throw new Error("Audio import did not produce a project asset");
      const assetId = uniqueImportedAssetId(imported.assetId);
      const cueId = uniqueAudioCueId(`${assetId}.cue`);
      const commands: Array<Record<string, unknown>> = [{
        kind: "audio_asset.upsert",
        audio_asset: {
          asset_id: assetId,
          asset_type: "sampled_sfx",
          source_path: imported.sourcePath,
          source_format: "wav",
        },
      }];
      commands.push({
        kind: "audio_cue.upsert",
        audio_cue: {
          cue_id: cueId,
          display_name: assetId,
          asset_ref: assetId,
          priority: 96,
          volume: 200,
        },
      });
      const result = await bridge.serviceRequest<ProjectCommandResult>("project.apply_commands", {
        project_revision: project.project_revision,
        commands,
      });
      applyProjectResult(result);
      setPendingAudioImport(null);
      selectAssetRecord({ kind: "audio", cueId });
      setWorkspaceMode("assets");
      const gainLabel = imported.analysis.inputPeakDbfs === null
          ? "(silent source; no gain applied)"
          : `${imported.analysis.inputPeakDbfs.toFixed(1)} to ${imported.analysis.outputPeakDbfs?.toFixed(1) ?? "-inf"} dBFS (${imported.analysis.gainDb >= 0 ? "+" : ""}${imported.analysis.gainDb.toFixed(1)} dB)`;
      setAudioAuditionStatus(`Imported ${imported.sourcePath} ${gainLabel}.`);
      const trimmedMs = imported.analysis.originalDurationMs - imported.analysis.outputDurationMs;
      setMessage(`Imported ${assetId}${trimmedMs >= 1 ? ` and removed ${Math.round(trimmedMs)} ms` : ""}. Save to write it to the project.`);
    } catch (error) {
      const text = errorText(error);
      setAudioAuditionStatus(`Import failed: ${text}`);
      setMessage(text);
    } finally {
      setBusy(null);
    }
  };

  const playAudioData = async (
    cueId: string,
    wavBase64: string,
    durationMs: number,
    status: string,
    message: string,
  ) => {
    const audio = new Audio(`data:audio/wav;base64,${wavBase64}`);
    audioPlaybackRef.current = audio;
    let progressFrame = 0;
    const updateProgress = () => {
      if (audioPlaybackRef.current !== audio) return;
      const fallbackDuration = Math.max(0.001, durationMs / 1000);
      const duration = Number.isFinite(audio.duration) && audio.duration > 0 ? audio.duration : fallbackDuration;
      setAudioAuditionProgress({ cueId, progress: Math.min(1, Math.max(0, audio.currentTime / duration)) });
      if (!audio.paused && !audio.ended) progressFrame = window.requestAnimationFrame(updateProgress);
    };
    audio.addEventListener("ended", () => {
      if (audioPlaybackRef.current === audio) {
        audioPlaybackRef.current = null;
        setAudioAuditionProgress({ cueId, progress: 1 });
        window.setTimeout(() => {
          setAudioAuditionProgress((current) => current?.cueId === cueId && current.progress >= 1 ? null : current);
        }, 250);
      }
      window.cancelAnimationFrame(progressFrame);
    }, { once: true });
    await audio.play();
    progressFrame = window.requestAnimationFrame(updateProgress);
    setAudioAuditionStatus(status);
    setMessage(message);
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
      const cueLabel = audioCueDisplayName(audioCues.find((cue) => cue.cue_id === cueId) ?? { cue_id: cueId });
      await playAudioData(
        cueId,
        result.audio.wav_base64,
        result.audio.duration_ms,
        `Played packaged ${result.audio.duration_ms} ms cue at ${result.audio.sample_rate_hz} Hz.`,
        `Auditioned ${cueLabel} from packaged ADPCM bytes.`,
      );
      selectAssetRecord({ kind: "audio", cueId });
    } catch (error) {
      const text = errorText(error);
      setAudioAuditionStatus(`Audition failed: ${text}`);
      setMessage(text);
    } finally {
      setBusy(null);
    }
  };

  const auditionSelectedAudioTrim = async () => {
    if (bridge === undefined || projectPath === null || selectedAudioCue === null || selectedAudioTrim === null || busy !== null) return;
    setBusy("Auditioning selection");
    setPlaying(false);
    stopAudioPlayback();
    try {
      const result = await bridge.previewAudioWav(projectPath, selectedAudioTrim.sourcePath, {
        normalize: normalizeAudioImports,
        targetPeakDbfs: audioImportPeakDbfs,
        trimStartMs: selectedAudioTrim.trimStartMs,
        trimEndMs: selectedAudioTrim.trimEndMs,
      });
      await playAudioData(
        selectedAudioCue.cue_id,
        result.wavBase64,
        result.durationMs,
        `Auditioning ${Math.round(selectedAudioTrim.trimStartMs)}-${Math.round(selectedAudioTrim.trimEndMs)} ms.`,
        `Auditioned the current trim selection for ${audioCueDisplayName(selectedAudioCue)}.`,
      );
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
    if (displayName === audioCueDisplayName(cue)) {
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

  const updateAudioCueSettings = async (nextCue: AudioCueRecord) => {
    if (bridge === undefined || project === null || busy !== null) return;
    setBusy("Updating audio cue");
    setPlaying(false);
    try {
      const result = await bridge.serviceRequest<ProjectCommandResult>("project.apply_commands", {
        project_revision: project.project_revision,
        commands: [{ kind: "audio_cue.upsert", audio_cue: nextCue }],
      });
      applyProjectResult(result);
      selectAssetRecord({ kind: "audio", cueId: nextCue.cue_id });
      setMessage("Audio cue settings updated. Save to write them to the project.");
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
    const source = scenes.find(scene => scene.scene_id === sceneId);
    if (source?.schema_version === 2 && source.routes?.find(route => route.route_id === routeId)?.actions.length) {
      setMessage("Remove the transition's actions before connecting it to a scene exit.");
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
      applyProjectResult(result, { preserveDerivedViews: true });
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
      applyProjectResult(result, { preserveDerivedViews: true });
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
      applyProjectResult(result, { preserveDerivedViews: true });
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
      applyProjectResult(result, { preserveDerivedViews: true });
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
      applyProjectResult(result, { preserveDerivedViews: true });
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
      applyProjectResult(result, { preserveDerivedViews: true });
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

  const setTimerHandlerLayout = async (
    sceneId: string,
    handlerId: string,
    layout: EditorHandlerLayout,
  ) => {
    if (bridge === undefined || project === null) {
      return;
    }
    setStateGraphLayoutStatus(`queued ${sceneId}.${handlerId} timer layout`);
    layoutSaveChain.current = layoutSaveChain.current.then(async () => {
      const revision = projectRevisionRef.current;
      if (revision === null) {
        setStateGraphLayoutStatus(`skipped ${sceneId}.${handlerId}: no project revision`);
        return;
      }
      const result = await bridge.serviceRequest<ProjectCommandResult>("project.apply_commands", {
        project_revision: revision,
        commands: [{
          kind: "editor.state_graph.set_handler_layout",
          scene_id: sceneId,
          handler_id: handlerId,
          layout,
        }],
      });
      projectRevisionRef.current = result.project_revision;
      applyProjectResult(result, { preserveDerivedViews: true });
      setStateGraphLayoutStatus(`saved ${sceneId}.${handlerId} timer layout rev ${result.project_revision}`);
      setMessage("Timer path layout updated. Save to write it to the project.");
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
    const supportedAnimationCommands = service?.state_scene_presentation.general_frame_animation.commands ?? [];
    if (bridge === undefined || project === null || busy !== null || selectedSceneDocument === null || commands.length === 0
      || commands.some(command => ["animation.upsert", "animation.delete"].includes(String(command.kind))
        ? command.scene_id !== undefined || !supportedAnimationCommands.includes(String(command.kind))
        : command.scene_id !== selectedSceneDocument.scene_id
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
    if (objectSceneSelected && element.kind === "text") {
      const definition = placementOwnershipScene?.objects?.find(object => object.object_id === element.element_id);
      if (definition?.kind !== "text") return;
      const width = Math.max(1, Math.min(PLACEMENT_WIDTH, Math.round(bounds.width)));
      const height = Math.max(1, Math.min(PLACEMENT_HEIGHT, Math.round(bounds.height)));
      const x = Math.max(0, Math.min(PLACEMENT_WIDTH - width, Math.round(bounds.x)));
      const y = Math.max(0, Math.min(PLACEMENT_HEIGHT - height, Math.round(bounds.y)));
      const commands: Record<string, unknown>[] = [];
      if (width !== definition.width || height !== definition.height) commands.push({
        kind: "object.set_text", scene_id: selectedSceneDocument.scene_id, object_id: definition.object_id,
        text: definition.text, font_id: definition.font_id, scale: definition.scale,
        alignment: definition.alignment, width, height,
      });
      if (x !== element.x || y !== element.y) commands.push(...sceneObjectPropertyCommands(element.element_id, { x, y }));
      if (commands.length > 0) void applySceneObjectCommands(commands);
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
    if (bridge === undefined || build === null || busy !== null || !canBuildProject(service, project)
      || build.project_revision !== project?.project_revision || projectReplacementRef.current.pending) {
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
  const animationClips = project?.document?.animations ?? [];
  const animationPolicies = service?.scene_object_authoring?.clip_loop_policies ?? [];
  const generalAnimationCommands = service?.state_scene_presentation.general_frame_animation.commands ?? [];
  const canUpsertAnimations = service?.operations.includes("project.apply_commands") === true
    && generalAnimationCommands.includes("animation.upsert");
  const canDeleteAnimations = service?.operations.includes("project.apply_commands") === true
    && generalAnimationCommands.includes("animation.delete");
  const canAuthorAnimations = canUpsertAnimations && animationPolicies.length > 0;
  const animationFallbackLabel = (clip: AuthoredClip) => {
    const animationIndex = Math.max(0, animationClips.indexOf(clip)) + 1;
    const asset = assets.find(item => item.frames.some(frame => frame.frame_id === clip.frame_refs[0]));
    if (isNormalizedAnimationBackingAsset(asset)) {
      return `Animation ${animationIndex}`;
    }
    return `${asset?.display_name ?? asset?.text ?? "Animation"} - Animation ${animationIndex}`;
  };
  const animationLabel = (clip: AuthoredClip) => clip.display_name?.trim() || animationFallbackLabel(clip);
  const nextAnimationId = () => {
    let index = 1;
    while (animationClips.some(clip => clip.animation_id === `animation_${index}`)) index++;
    return `animation_${index}`;
  };
  const uniqueAnimationDisplayName = (base: string) => {
    const normalizedBase = base.trim() || "Animation";
    const existingNames = new Set(animationClips.map(animationLabel));
    let candidate = normalizedBase.slice(0, 64);
    let index = 2;
    while (existingNames.has(candidate)) {
      const suffix = ` ${index}`;
      candidate = `${normalizedBase.slice(0, 64 - suffix.length)}${suffix}`;
      index++;
    }
    return candidate;
  };
  const duplicateAnimationClip = async (clip: AuthoredClip) => {
    if (!canUpsertAnimations || busy !== null) return;
    const animationId = nextAnimationId();
    const displayName = uniqueAnimationDisplayName(`${animationLabel(clip)} copy`);
    const applied = await applySceneObjectCommands([{ kind: "animation.upsert", animation: { ...clip, animation_id: animationId, display_name: displayName } }]);
    if (applied) {
      setCombineFrameIds([]);
      selectAssetRecord({ kind: "animation", clipId: animationId });
      setMessage(`Duplicated ${animationLabel(clip)}. Save to write it to the project.`);
    }
  };
  const deleteAnimationClip = async (clip: AuthoredClip) => {
    if (!canDeleteAnimations || busy !== null) return;
    const used = scenes.some(scene => (scene.objects ?? []).some(object => object.animation_ref === clip.animation_id));
    if (used) {
      setMessage("Clear this animation from scene objects before deleting it.");
      return;
    }
    const applied = await applySceneObjectCommands([{ kind: "animation.delete", animation_id: clip.animation_id }]);
    if (applied) {
      setCombineFrameIds([]);
      selectAssetRecord(null);
      setMessage(`Deleted ${animationLabel(clip)}. Save to write it to the project.`);
    }
  };
  const toggleAnimationFrameSelection = (frameId: string) => {
    if (compiledAssetFrameById.has(frameId)) {
      selectAssetRecord({ kind: "sprite", frameId });
    }
    setCombineFrameIds(current => current.includes(frameId)
      ? current.filter(id => id !== frameId)
      : [...current, frameId]);
  };
  const setAnimationFrameSelectionGroup = (frameIds: string[], include: boolean) => {
    if (include && frameIds[0] !== undefined && compiledAssetFrameById.has(frameIds[0])) {
      selectAssetRecord({ kind: "sprite", frameId: frameIds[0] });
    }
    setCombineFrameIds(current => include
      ? [...current, ...frameIds.filter(frameId => !current.includes(frameId))]
      : current.filter(frameId => !frameIds.includes(frameId)));
  };
  const startAssetAnimation = () => {
    const frameIds = combineFrameIds.filter(id => compiledAssetFrameById.has(id));
    if (!frameIds.length) return;
    const frames = frameIds.flatMap(id => compiledAssetFrameById.get(id) ?? []);
    const sizes = new Set(frames.map(frame => `${frame.width}x${frame.height}`));
    if (sizes.size > 1) {
      setAnimationNormalizeDraft({ frameIds, horizontal: "center", vertical: "middle", cadence: "400", loopPolicy: animationPolicies[0], error: null });
      setAssetSelection(null);
      setMessage("Selected frames have mixed sizes. Choose padding alignment to create a normalized animation asset.");
      return;
    }
    setAnimationNormalizeDraft(null);
    selectAssetRecord({ kind: "animation-draft", clip: { animation_id: nextAnimationId(), display_name: uniqueAnimationDisplayName(`Animation ${animationClips.length + 1}`), frame_refs: frameIds,
      frame_duration_ms: frameIds.map(() => 400), loop_policy: animationPolicies[0] } });
  };
  const createNormalizedAnimation = async () => {
    if (bridge === undefined || project === null || projectPath === null || animationNormalizeDraft === null || busy !== null) {
      return;
    }
    if (bridge.writeGeneratedSpritePng === undefined) {
      setAnimationNormalizeDraft(current => current === null ? null : { ...current, error: "Restart Peep Studio to enable generated sprite writing." });
      return;
    }
    const selectedFrames = animationNormalizeDraft.frameIds.flatMap(id => compiledAssetFrameById.get(id) ?? []);
    if (selectedFrames.length !== animationNormalizeDraft.frameIds.length || selectedFrames.length === 0) {
      setAnimationNormalizeDraft(current => current === null ? null : { ...current, error: "Selected animation frames are no longer available." });
      return;
    }
    const cadenceMs = Number(animationNormalizeDraft.cadence);
    if (!Number.isInteger(cadenceMs) || cadenceMs < 1 || cadenceMs > 60000) {
      setAnimationNormalizeDraft(current => current === null ? null : { ...current, error: "Cadence must be a whole number from 1 to 60000 ms." });
      return;
    }
    if (!animationPolicies.includes(animationNormalizeDraft.loopPolicy)) {
      setAnimationNormalizeDraft(current => current === null ? null : { ...current, error: "Choose a supported playback mode." });
      return;
    }
    setBusy("Normalizing animation");
    setPlaying(false);
    try {
      const rendered = renderNormalizedAnimationPng(selectedFrames, animationNormalizeDraft.horizontal, animationNormalizeDraft.vertical);
      const displayName = NORMALIZED_ANIMATION_BACKING_DISPLAY_NAME;
      const requestedAssetId = uniqueImportedAssetId(stableAssetIdFromLabel(displayName, "animation_frames"));
      const written = await bridge.writeGeneratedSpritePng(projectPath, requestedAssetId, rendered.dataUrl);
      const normalizedFrames: AssetFrameRecord[] = selectedFrames.map((_, index) => ({
        frame_id: `${written.assetId}.frame_${index + 1}`,
        display_name: `Frame ${index + 1}`,
        source_rect: {
          x: (index % rendered.columns) * rendered.frameWidth,
          y: Math.floor(index / rendered.columns) * rendered.frameHeight,
          width: rendered.frameWidth,
          height: rendered.frameHeight,
        },
        pivot_x: 0,
        pivot_y: 0,
      }));
      const frameRefs = normalizedFrames.map(frame => frame.frame_id);
      const animationId = nextAnimationId();
      const animationDisplayName = uniqueAnimationDisplayName(`Animation ${animationClips.length + 1}`);
      const result = await bridge.serviceRequest<ProjectCommandResult>("project.apply_commands", {
        project_revision: project.project_revision,
        commands: [
          {
            kind: "asset.upsert",
            asset: {
              asset_id: written.assetId,
              display_name: displayName,
              asset_type: "masked_1bpp",
              source_path: written.sourcePath,
              source_format: "png",
              frames: normalizedFrames,
            },
          },
          {
            kind: "animation.upsert",
            animation: {
              animation_id: animationId,
              display_name: animationDisplayName,
              frame_refs: frameRefs,
              frame_duration_ms: frameRefs.map(() => cadenceMs),
              loop_policy: animationNormalizeDraft.loopPolicy,
            },
          },
        ],
      });
      applyProjectResult(result);
      setCombineFrameIds([]);
      setAnimationNormalizeDraft(null);
      selectAssetRecord({ kind: "animation", clipId: animationId });
      setMessage(`Created animation from ${rendered.frameWidth}x${rendered.frameHeight} padded frames. Save to write it to the project.`);
    } catch (error) {
      setAnimationNormalizeDraft(current => current === null ? null : { ...current, error: errorText(error) });
      setMessage(errorText(error));
    } finally {
      setBusy(null);
    }
  };
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
  const animationEditorAssets = useMemo(
    () => assets.map((asset) => (isNormalizedAnimationBackingAsset(asset) ? { ...asset, display_name: "Animation frames" } : asset)),
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
  const compiledAssetFrameGroups = useMemo<CompiledAssetFrameGroup[]>(() => {
    const groups = new Map<string, CompiledAssetFrame[]>();
    for (const frame of compiledAssetFrames) {
      groups.set(frame.asset_id, [...(groups.get(frame.asset_id) ?? []), frame]);
    }
    return [...groups.entries()].map(([assetId, frames]) => ({ assetId, frames }));
  }, [compiledAssetFrames]);
  const visibleCompiledAssetFrameGroups = useMemo(
    () => compiledAssetFrameGroups.filter((group) => !isNormalizedAnimationBackingAsset(assetById.get(group.assetId))),
    [assetById, compiledAssetFrameGroups],
  );
  const audioAssetById = useMemo(
    () => new Map(audioAssets.map((asset) => [asset.asset_id, asset])),
    [audioAssets],
  );
  const assetTagCapability = service?.asset_metadata?.tags;
  const updateAssetTags = async (kind: "sprite" | "audio", assetId: string, tags: string[]) => {
    const commandKind = kind === "sprite" ? "asset.set_tags" : "audio_asset.set_tags";
    if (bridge === undefined || project === null || busy !== null
      || assetTagCapability?.supported !== true || !assetTagCapability.commands.includes(commandKind)) return false;
    setBusy("Updating asset tags");
    setPlaying(false);
    stopAudioPlayback();
    try {
      const result = await bridge.serviceRequest<ProjectCommandResult>("project.apply_commands", {
        project_revision: project.project_revision,
        commands: [{ kind: commandKind, asset_id: assetId, tags }],
      });
      applyProjectResult(result);
      setMessage("Asset tags updated. Save to write them to the project.");
      return true;
    } catch (error) {
      setMessage(errorText(error));
      return false;
    } finally {
      setBusy(null);
    }
  };
  const bakedTextSourceByAssetId = useMemo(
    () => new Map(bakedTextSources.map((source) => [source.asset_id, source])),
    [bakedTextSources],
  );
  useEffect(() => {
    const assetId = assetSelection?.kind === "sprite"
      ? compiledAssetFrameById.get(assetSelection.frameId)?.asset_id ?? null
      : null;
    if (assetId === null) {
      setBakedTextEditDraft(null);
      return;
    }
    const source = bakedTextSourceByAssetId.get(assetId);
    if (source === undefined) {
      setBakedTextEditDraft(null);
      return;
    }
    const asset = assetById.get(assetId);
    setBakedTextEditDraft({
      assetId: source.asset_id,
      displayName: asset?.display_name ?? source.display_name,
      text: source.text,
      fontSize: String(source.font_size_px),
      fontId: source.font_id,
    });
  }, [assetById, assetSelection, bakedTextSourceByAssetId, compiledAssetFrameById]);
  const spriteAssetKind = (asset: AssetRecord | undefined, frameCount: number) => {
    if (asset?.text !== undefined || asset?.font_id !== undefined || asset?.asset_type === "text") {
      return "Text";
    }
    if (asset?.source_format === "png" && (asset.asset_id.startsWith("text_") || asset.source_path?.includes("/text_") === true)) {
      return "Text sprite";
    }
    if (frameCount > 1) {
      return "Sprite sheet";
    }
    if (asset?.source_path !== undefined) {
      return "Single frame";
    }
    return "Generated";
  };
  const isTextSpriteAsset = (asset: AssetRecord | undefined) => (
    asset?.source_format === "system_font_text"
    || asset?.text !== undefined
    || asset?.font_id !== undefined
    || asset?.asset_type === "text"
    || (asset !== undefined && bakedTextSourceByAssetId.has(asset.asset_id))
    || (asset?.source_format === "png" && (asset.asset_id.startsWith("text_") || asset.source_path?.includes("/text_") === true))
  );
  const spriteSheetColumns = (asset: AssetRecord | undefined, frameCount: number) => {
    const rects = asset?.frames.map((frame) => frame.source_rect).filter((rect) => rect !== undefined) ?? [];
    if (rects.length === frameCount && rects.length > 0) {
      const columns = new Set(rects.map((rect) => rect.x)).size;
      if (columns > 0) {
        return Math.min(24, Math.max(1, columns));
      }
    }
    return Math.min(4, Math.max(1, frameCount));
  };
  const sourceSpriteGroups = useMemo<AssetLibraryGroup<CompiledAssetFrameGroup>[]>(() => {
    const order = ["Sprite sheet", "Text sprite", "Single frame", "Text", "Generated"];
    const grouped = new Map<string, CompiledAssetFrameGroup[]>();
    for (const group of visibleCompiledAssetFrameGroups) {
      const kind = spriteAssetKind(assetById.get(group.assetId), group.frames.length);
      grouped.set(kind, [...(grouped.get(kind) ?? []), group]);
    }
    return order.flatMap((label) => {
      const items = grouped.get(label) ?? [];
      if (items.length === 0) {
        return [];
      }
      return [{
        key: label.toLowerCase().replace(/\s+/g, "-"),
        label,
        detail: `${items.length} asset${items.length === 1 ? "" : "s"}`,
        items,
      }];
    });
  }, [assetById, bakedTextSourceByAssetId, visibleCompiledAssetFrameGroups]);
  const audioCueGroups = useMemo<AssetLibraryGroup<AudioCueRecord>[]>(() => {
    const ready = audioCues.filter((cue) => audioAssetById.has(cue.asset_ref));
    const missingSource = audioCues.filter((cue) => !audioAssetById.has(cue.asset_ref));
    return [
      ready.length === 0 ? null : {
        key: "ready",
        label: "Ready SFX",
        detail: `${ready.length} cue${ready.length === 1 ? "" : "s"}`,
        items: ready,
      },
      missingSource.length === 0 ? null : {
        key: "missing-source",
        label: "Missing source",
        detail: `${missingSource.length} cue${missingSource.length === 1 ? "" : "s"}`,
        items: missingSource,
      },
    ].filter((group): group is AssetLibraryGroup<AudioCueRecord> => group !== null);
  }, [audioAssetById, audioCues]);
  const selectedAudioCue = assetSelection?.kind === "audio"
    ? audioCues.find((cue) => cue.cue_id === assetSelection.cueId) ?? null
    : null;
  const selectedAudioAsset = selectedAudioCue === null ? null : audioAssetById.get(selectedAudioCue.asset_ref) ?? null;
  useEffect(() => {
    let cancelled = false;
    setSelectedAudioTrim(null);
    setSelectedAudioTrimError(null);
    if (bridge === undefined || projectPath === null || selectedAudioAsset === null) return () => { cancelled = true; };
    void bridge.inspectProjectAudioWav(projectPath, selectedAudioAsset.source_path)
      .then((selected) => {
        if (!cancelled) setSelectedAudioTrim({
          ...selected,
          appliedTrimStartMs: selected.trimStartMs,
          appliedTrimEndMs: selected.trimEndMs,
        });
      })
      .catch((error) => {
        if (!cancelled) setSelectedAudioTrimError(errorText(error));
      });
    return () => { cancelled = true; };
  }, [bridge, projectPath, selectedAudioAsset?.asset_id, selectedAudioAsset?.source_path]);

  const applySelectedAudioTrim = async () => {
    if (bridge === undefined || project === null || projectPath === null || selectedAudioCue === null
      || selectedAudioAsset === null || selectedAudioTrim === null || busy !== null) return;
    setBusy("Applying audio trim");
    setPlaying(false);
    stopAudioPlayback();
    try {
      const imported = await bridge.importAudioWav(projectPath, selectedAudioTrim.sourcePath, {
        normalize: normalizeAudioImports,
        targetPeakDbfs: audioImportPeakDbfs,
        trimStartMs: selectedAudioTrim.trimStartMs,
        trimEndMs: selectedAudioTrim.trimEndMs,
      });
      if (imported === null) throw new Error("Audio trim did not produce a project asset");
      const result = await bridge.serviceRequest<ProjectCommandResult>("project.apply_commands", {
        project_revision: project.project_revision,
        commands: [{
          kind: "audio_asset.upsert",
          audio_asset: {
            asset_id: selectedAudioAsset.asset_id,
            asset_type: "sampled_sfx",
            source_path: imported.sourcePath,
            source_format: "wav",
            tags: selectedAudioAsset.tags ?? [],
          },
        }],
      });
      applyProjectResult(result);
      selectAssetRecord({ kind: "audio", cueId: selectedAudioCue.cue_id });
      const removedMs = imported.analysis.originalDurationMs - imported.analysis.outputDurationMs;
      setAudioAuditionStatus(`Updated ${audioCueDisplayName(selectedAudioCue)} from ${imported.sourcePath}.`);
      setMessage(`Applied trim${removedMs >= 1 ? ` and removed ${Math.round(removedMs)} ms` : ""}. Undo restores the previous source.`);
    } catch (error) {
      const text = errorText(error);
      setSelectedAudioTrimError(text);
      setMessage(text);
    } finally {
      setBusy(null);
    }
  };
  const selectedFontAsset = assetSelection?.kind === "font"
    ? fontAssets.find((font) => font.font_id === assetSelection.fontId) ?? null
    : null;
  const selectedAssetFrame = assetSelection?.kind === "sprite"
    ? compiledAssetFrameById.get(assetSelection.frameId) ?? null
    : null;
  const selectedAssetFrames = selectedAssetFrame === null
    ? []
    : (() => {
      const frames = compiledAssetFrameGroups.find((group) => group.assetId === selectedAssetFrame.asset_id)?.frames ?? [];
      const authoredOrder = new Map((assetById.get(selectedAssetFrame.asset_id)?.frames ?? []).map((frame, index) => [frame.frame_id, index]));
      return [...frames].sort((left, right) => (authoredOrder.get(left.frame_id) ?? Infinity) - (authoredOrder.get(right.frame_id) ?? Infinity));
    })();
  const selectedAnimationFrames = combineFrameIds.flatMap(id => compiledAssetFrameById.get(id) ?? []);
  const assetInspectorPreviewFrames = selectedAnimationFrames.length > 0 ? selectedAnimationFrames : selectedAssetFrames;
  const assetInspectorPreviewIndex = assetInspectorPreviewFrames.length === 0
    ? 0
    : assetPreviewPlaying
      ? assetPreviewStep % assetInspectorPreviewFrames.length
      : selectedAnimationFrames.length > 0
        ? 0
        : Math.max(0, assetInspectorPreviewFrames.findIndex((frame) => frame.frame_id === selectedAssetFrame?.frame_id));
  const animatedAssetPreviewFrame = assetInspectorPreviewFrames.length === 0
    ? selectedAssetFrame
    : assetInspectorPreviewFrames[assetInspectorPreviewIndex] ?? selectedAssetFrame;
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
  const runtimeTextProfile = service?.state_scene_presentation.runtime_text_profile;
  const runtimeTextSupported = objectSceneSelected
    && service?.state_scene_presentation.runtime_text === true
    && runtimeTextProfile?.supported === true
    && service.scene_object_authoring?.runtime_text?.supported === true
    && selectedSceneCapability?.runtime_text === true
    && selectedSceneCapability.runtime_text_profile?.supported === true
    && supportsObjectCommand(service, selectedSceneCapability, "object.add")
    && supportsObjectCommand(service, selectedSceneCapability, "object.set_text");
  const canEditSelectedScene = canEditLegacyScene(selectedSceneDocument, selectedSceneCapability)
    && service?.operations.includes("project.apply_commands") === true && busy === null;
  const stateCommandAllowed = (command: string) => objectSceneSelected
    ? busy === null && supportsStateManagement(service, selectedSceneCapability, command)
    : canEditSelectedScene;
  const localCommandAllowed = (command: string) => objectSceneSelected
    ? busy === null && supportsLocalGraphCommand(service, selectedSceneCapability, command)
    : canEditSelectedScene;
  const audioRouteActionKinds = service?.state_scene_audio.host_package_support === true && service.state_scene_audio.route_action
    ? [service.state_scene_audio.route_action]
    : [];
  const targetSceneActionKinds = service?.state_scene_graph.target_scene_actions ?? audioRouteActionKinds;
  const canEditLocalGraph = ["route.create_trigger", "route.rebind_trigger", "editor.state_graph.set_route_layout",
    "editor.state_graph.delete_system_exit"].every(localCommandAllowed)
    && (!objectSceneSelected || ["state", "system_exit"].every(kind =>
      service?.scene_object_authoring?.route_destination_kinds?.includes(kind)
      && selectedSceneCapability?.route_destination_kinds?.includes(kind)));
  const sceneConnectionsEditable = (scene: SceneDocument) => !usesSceneObjects(scene)
    ? canEditLegacyScene(scene, project?.scene_capabilities?.[scene.scene_id])
    : ["scene_exit.add", "scene_exit.set_target", "scene_exit.delete", "editor.scene_flow.set_node_position",
      "editor.scene_flow.set_route_layout", "editor.scene_flow.set_package_entry_position", "editor.scene_flow.add_reference",
      "editor.scene_flow.set_reference_position", "editor.scene_flow.set_reference_target", "editor.scene_flow.set_exit_reference",
      "editor.scene_flow.delete_reference"].every(command => supportsSceneConnection(service, project?.scene_capabilities?.[scene.scene_id], command));
  const canConnectSelectedScene = busy === null && selectedSceneDocument !== null && sceneConnectionsEditable(selectedSceneDocument);
  const readOnlySceneIds = scenes.filter(scene => !sceneConnectionsEditable(scene)).map(scene => scene.scene_id);
  const buildReady = canBuildProject(service, project);
  const hostOnlyProject = scenes.some(scene => usesSceneObjects(scene, project?.scene_capabilities?.[scene.scene_id])) && !buildReady;
  const emulatorPopoutState = useMemo<EmulatorPopoutState>(() => ({
    preview,
    sceneName: scenes.find((scene) => scene.scene_id === preview?.scene.scene_id)?.display_name ?? "No active scene",
    playing,
  }), [playing, preview, scenes]);

  useEffect(() => {
    if (!emulatorPoppedOut || nativeWindowInteracting || bridge?.syncEmulatorPopout === undefined) {
      return;
    }
    void bridge.syncEmulatorPopout(emulatorPopoutState);
  }, [bridge, emulatorPoppedOut, emulatorPopoutState, nativeWindowInteracting]);

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
      !projectReplacing && !projectReplacementRef.current.pending &&
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
    const generation = projectReplacementRef.current.generation;
    const obsolete = () => cancelled || projectReplacementRef.current.pending ||
      generation !== projectReplacementRef.current.generation;
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
          obsolete() ||
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
        if (!obsolete()) {
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
    projectReplacing,
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
    if (assetSelection?.kind !== "sprite") {
      return;
    }
    const frame = compiledAssetFrameById.get(assetSelection.frameId);
    if (frame === undefined || isNormalizedAnimationBackingAsset(assetById.get(frame.asset_id))) {
      setAssetSelection(null);
    }
  }, [assetById, assetSelection, compiledAssetFrameById]);
  useEffect(() => {
    setAssetPreviewStep(0);
    setAssetPreviewPlaying(false);
  }, [selectedAssetFrame?.asset_id]);
  useEffect(() => {
    setAssetPreviewStep(0);
    if (combineFrameIds.length > 1) {
      setAssetPreviewPlaying(true);
    } else {
      setAssetPreviewPlaying(false);
    }
  }, [combineFrameIds.length]);
  useEffect(() => {
    if (!assetPreviewPlaying || workspaceMode !== "assets" || assetInspectorPreviewFrames.length <= 1) {
      return undefined;
    }
    const timer = window.setInterval(() => {
      setAssetPreviewStep((step) => step + 1);
    }, 250);
    return () => window.clearInterval(timer);
  }, [assetPreviewPlaying, assetInspectorPreviewFrames.length, workspaceMode]);
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
  const zoomPlacementViewport = (factor: number) => {
    setPlacementViewport((current) => ({
      ...current,
      zoom: clampPlacementZoom(current.zoom * factor),
    }));
  };
  const resetPlacementViewport = () => {
    setPlacementViewport({ x: 0, y: 0, zoom: 1 });
  };
  const updatePlacementGridFrame = useCallback(() => {
    const stage = placementStageRef.current;
    const overlay = placementScreenOverlayRef.current;
    if (stage === null || overlay === null) {
      setPlacementGridFrame(null);
      return;
    }
    const stageRect = stage.getBoundingClientRect();
    const overlayRect = overlay.getBoundingClientRect();
    const next = {
      left: overlayRect.left - stageRect.left,
      top: overlayRect.top - stageRect.top,
      width: overlayRect.width,
      height: overlayRect.height,
    };
    setPlacementGridFrame((current) => (
      current !== null &&
      Math.abs(current.left - next.left) < 0.25 &&
      Math.abs(current.top - next.top) < 0.25 &&
      Math.abs(current.width - next.width) < 0.25 &&
      Math.abs(current.height - next.height) < 0.25
        ? current
        : next
    ));
  }, []);
  useLayoutEffect(() => {
    if (workspaceMode !== "placement") {
      setPlacementGridFrame(null);
      return;
    }
    updatePlacementGridFrame();
    if (typeof ResizeObserver === "undefined") {
      window.addEventListener("resize", updatePlacementGridFrame);
      return () => window.removeEventListener("resize", updatePlacementGridFrame);
    }
    const observer = new ResizeObserver(updatePlacementGridFrame);
    if (placementStageRef.current !== null) observer.observe(placementStageRef.current);
    if (placementScreenOverlayRef.current !== null) observer.observe(placementScreenOverlayRef.current);
    window.addEventListener("resize", updatePlacementGridFrame);
    return () => {
      observer.disconnect();
      window.removeEventListener("resize", updatePlacementGridFrame);
    };
  }, [placementViewport, updatePlacementGridFrame, workspaceMode]);
  const startPlacementViewportPan = (event: ReactPointerEvent<HTMLDivElement>) => {
    if (event.button !== 0 && event.button !== 1) {
      return;
    }
    const target = event.target;
    if (!(target instanceof Element)) {
      return;
    }
    if (target.closest(".placement-tool-palette, .placement-sprite-picker, .placement-viewport-controls, .preview-status-card")) {
      return;
    }
    const insideScreen = target.closest(".panel-bezel") !== null;
    const canPanFromScreen = event.button === 1 || (event.button === 0 && event.altKey);
    if (insideScreen && !canPanFromScreen) {
      return;
    }

    event.preventDefault();
    const startClientX = event.clientX;
    const startClientY = event.clientY;
    const startX = placementViewport.x;
    const startY = placementViewport.y;
    let dragged = false;
    setPlacementViewportPanning(true);

    const move = (moveEvent: PointerEvent) => {
      dragged = dragged || Math.abs(moveEvent.clientX - startClientX) > 2 || Math.abs(moveEvent.clientY - startClientY) > 2;
      setPlacementViewport((current) => ({
        ...current,
        x: startX + moveEvent.clientX - startClientX,
        y: startY + moveEvent.clientY - startClientY,
      }));
    };
    const stop = () => {
      window.removeEventListener("pointermove", move);
      window.removeEventListener("pointerup", stop);
      window.removeEventListener("pointercancel", stop);
      setPlacementViewportPanning(false);
      if (event.button === 0 && !insideScreen && !event.altKey && !dragged) {
        placementSelectionAnchorRef.current = null;
        setSelectedPlacementElement(null);
        setSceneSelection({ kind: "scene" });
        clearAssetLibrarySelection();
      }
    };
    window.addEventListener("pointermove", move);
    window.addEventListener("pointerup", stop);
    window.addEventListener("pointercancel", stop);
  };
  const handlePlacementViewportWheel = (event: ReactWheelEvent<HTMLDivElement>) => {
    const target = event.target;
    if (target instanceof Element && target.closest(".placement-tool-palette, .placement-sprite-picker, .placement-viewport-controls")) {
      return;
    }
    event.preventDefault();
    const stageRect = event.currentTarget.getBoundingClientRect();
    const anchorX = event.clientX - stageRect.left - stageRect.width / 2;
    const anchorY = event.clientY - stageRect.top - stageRect.height / 2;
    const factor = event.deltaY < 0 ? PLACEMENT_VIEWPORT_ZOOM_STEP : 1 / PLACEMENT_VIEWPORT_ZOOM_STEP;
    setPlacementViewport((current) => {
      const nextZoom = clampPlacementZoom(current.zoom * factor);
      if (nextZoom === current.zoom) {
        return current;
      }
      const scale = nextZoom / current.zoom;
      return {
        x: anchorX - (anchorX - current.x) * scale,
        y: anchorY - (anchorY - current.y) * scale,
        zoom: nextZoom,
      };
    });
  };
  const renderPlacementViewportControls = () => (
    <div className="placement-viewport-controls" aria-label="Placement view controls">
      <button type="button" onClick={() => zoomPlacementViewport(1 / PLACEMENT_VIEWPORT_ZOOM_STEP)}
        title="Zoom out" aria-label="Zoom out">
        <Minus size={16} aria-hidden="true" />
      </button>
      <span aria-label={`Zoom ${Math.round(placementViewport.zoom * 100)} percent`}>
        {Math.round(placementViewport.zoom * 100)}%
      </span>
      <button type="button" onClick={() => zoomPlacementViewport(PLACEMENT_VIEWPORT_ZOOM_STEP)}
        title="Zoom in" aria-label="Zoom in">
        <Plus size={16} aria-hidden="true" />
      </button>
      <button type="button" onClick={resetPlacementViewport}
        title="Fit screen" aria-label="Fit screen">
        <Maximize2 size={16} aria-hidden="true" />
      </button>
    </div>
  );
  const renderPlacementGridOverlay = () => {
    if (placementGridFrame === null) {
      return null;
    }
    const pixelWidth = placementGridFrame.width / PLACEMENT_WIDTH;
    const pixelHeight = placementGridFrame.height / PLACEMENT_HEIGHT;
    const lineX = (x: number) => snapPlacementGridOffset(x * pixelWidth, placementGridFrame.left, placementGridFrame.width);
    const lineY = (y: number) => snapPlacementGridOffset(y * pixelHeight, placementGridFrame.top, placementGridFrame.height);
    const gridStyle = {
      left: `${placementGridFrame.left}px`,
      top: `${placementGridFrame.top}px`,
      width: `${placementGridFrame.width}px`,
      height: `${placementGridFrame.height}px`,
      "--placement-grid-minor-opacity": placementGridVisible ? placementGridStrength / 100 : 0,
      "--placement-grid-major-opacity": placementGridVisible && placementMajorGridVisible ? (placementGridStrength + 6) / 100 : 0,
    } as CSSProperties;
    return (
      <svg
        className="placement-stage-grid"
        width={placementGridFrame.width}
        height={placementGridFrame.height}
        style={gridStyle}
        aria-hidden="true"
      >
        {PLACEMENT_GRID_MINOR_X.map((x) => (
          <line className="minor" key={`minor-x-${x}`} x1={lineX(x)} y1={0} x2={lineX(x)} y2={placementGridFrame.height} />
        ))}
        {PLACEMENT_GRID_MINOR_Y.map((y) => (
          <line className="minor" key={`minor-y-${y}`} x1={0} y1={lineY(y)} x2={placementGridFrame.width} y2={lineY(y)} />
        ))}
        {PLACEMENT_GRID_MAJOR_X.map((x) => (
          <line className="major" key={`major-x-${x}`} x1={lineX(x)} y1={0} x2={lineX(x)} y2={placementGridFrame.height} />
        ))}
        {PLACEMENT_GRID_MAJOR_Y.map((y) => (
          <line className="major" key={`major-y-${y}`} x1={0} y1={lineY(y)} x2={placementGridFrame.width} y2={lineY(y)} />
        ))}
      </svg>
    );
  };
  const startPlacementDrag = (
    event: ReactPointerEvent<HTMLButtonElement>,
    element: RenderElement,
    renderModelId: string | null,
  ) => {
    if (event.button !== 0) {
      return;
    }
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
    if (renderModelId !== null) {
      setSceneSelection({ kind: "render", id: renderModelId });
    } else if (objectSceneSelected) {
      setSceneSelection(placementStateId === null
        ? { kind: "scene" }
        : { kind: "state", id: placementStateId });
    }

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
    if (event.button !== 0) {
      return;
    }
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
    const kind: PlacementPrimitiveKind | "text" | null = placementTool === "line"
      ? "line"
      : placementTool === "rectangle"
        ? "outline_rect"
        : placementTool === "circle"
          ? "ellipse"
          : placementTool === "text"
            ? "text"
          : null;
    if (
      kind === null ||
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
    const rect = event.currentTarget.getBoundingClientRect();
    const start = placementPointFromClient(rect, event.clientX, event.clientY);
    const boundsKind = kind === "text" ? "outline_rect" : kind;
    let latestBounds = primitiveBoundsFromPoints(boundsKind, start, start);
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
      latestBounds = primitiveBoundsFromPoints(boundsKind, start, end);
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
      if (kind === "text") {
        void addPlacementText(latestBounds);
        return;
      }
      const createdKind = placementTool === "circle" && latestBounds.width === latestBounds.height
        ? "circle"
        : kind;
      void addPlacementPrimitive(createdKind, latestBounds, latestLineDirection);
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
  const togglePlacementAssetPicker = (tool: "sprite" | "animation") => {
    placementDrawCancelRef.current?.();
    const shouldOpen = placementTool !== tool || !spritePickerOpen;
    setPlacementTool(shouldOpen ? tool : "select");
    setSpritePickerOpen(shouldOpen);
    setSelectedPlacementElement(null);
  };
  const closePlacementAssetPicker = () => {
    setSpritePickerOpen(false);
    setPlacementTool("select");
  };
  const legacyPlacementAnimationGroups = visibleCompiledAssetFrameGroups.filter((group) =>
    group.frames.length >= 2
    && group.frames.length <= 4
    && group.frames.every((frame) => frame.width === group.frames[0]?.width && frame.height === group.frames[0]?.height),
  );
  const placementAnimationsAvailable = objectSceneSelected
    ? animationClips.length > 0
    : legacyPlacementAnimationGroups.length > 0;
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
        <SquareMousePointer className="placement-select-icon" aria-hidden="true" />
      </button>
      <button
        type="button"
        disabled={!scopedPlacementAddSupported || busy !== null || selectedSceneDocument === null || (!objectSceneSelected && placementRenderModel === null) || compiledAssetFrames.length === 0}
        className={placementTool === "sprite" && spritePickerOpen ? "active" : ""}
        onClick={() => togglePlacementAssetPicker("sprite")}
        title={compiledAssetFrames.length === 0 ? "No sprite assets available" : "Add sprite"}
        aria-label="Add sprite"
      >
        <StudioIcon name="sprite" />
      </button>
      <button
        type="button"
        disabled={!scopedPlacementAddSupported || busy !== null || selectedSceneDocument === null || (!objectSceneSelected && placementRenderModel === null) || !placementAnimationsAvailable}
        className={placementTool === "animation" && spritePickerOpen ? "active" : ""}
        onClick={() => togglePlacementAssetPicker("animation")}
        title={placementAnimationsAvailable ? "Add animation" : "No compatible animations available"}
        aria-label="Add animation"
      >
        <StudioIcon name="animation" />
      </button>
      <button
        type="button"
        disabled={!runtimeTextSupported || busy !== null || selectedSceneDocument === null}
        className={placementTool === "text" ? "active" : ""}
        onClick={() => {
          placementDrawCancelRef.current?.();
          setPlacementTool("text");
          setSpritePickerOpen(false);
          setSelectedPlacementElement(null);
        }}
        title={runtimeTextSupported ? "Draw system text box" : "Runtime text is unavailable for this scene"}
        aria-label="Add text"
      >
        <StudioIcon name="text" />
      </button>
      {([
        { tool: "rectangle", label: "Rectangle", icon: "rectangle" },
        { tool: "circle", label: "Circle or oval", icon: "circle" },
        { tool: "line", label: "Line", icon: "line" },
      ] as const).map((primitive) => (
        <button
          key={primitive.tool}
          className={`primitive-${primitive.tool} ${placementTool === primitive.tool ? "active" : ""}`}
          type="button"
          disabled={!scopedPlacementAddSupported || busy !== null || selectedSceneDocument === null || (!objectSceneSelected && placementRenderModel === null)}
          onClick={() => {
            placementDrawCancelRef.current?.();
            setPlacementTool(primitive.tool);
            setSpritePickerOpen(false);
            setSelectedPlacementElement(null);
          }}
          title={`Draw ${primitive.label.toLowerCase()}`}
          aria-label={`Draw ${primitive.label.toLowerCase()}`}
        >
          <StudioIcon name={primitive.icon} />
        </button>
      ))}
    </div>
  );
  const renderSpritePicker = () => (
    <div className="placement-sprite-picker" role="dialog" aria-label={placementTool === "animation" ? "Choose animation" : "Choose sprite"}>
      <div className="placement-sprite-picker-heading">
        <strong>{placementTool === "animation" ? "Choose animation" : "Choose sprite"}</strong>
        <button type="button" onClick={closePlacementAssetPicker} title="Close asset picker" aria-label="Close asset picker">
          <X size={14} aria-hidden="true" />
        </button>
      </div>
      <div className="placement-sprite-picker-groups">
        {placementTool === "animation" && objectSceneSelected && animationClips.map(clip => {
          const frames = clip.frame_refs.flatMap(id => compiledAssetFrameById.get(id) ? [compiledAssetFrameById.get(id)!] : []);
          return <section className="placement-sprite-picker-group" key={clip.animation_id}>
            <SpriteAssetCard frames={frames} durations={clip.frame_duration_ms} name={animationLabel(clip)} selected={false}
              disabled={busy !== null || !scopedPlacementAddSupported || !supportsObjectCommand(service, selectedSceneCapability, "object.bind_animation")
                || frames.length !== clip.frame_refs.length}
              playback={preferences.thumbnailPlayback} onSelect={() => {
                if (busy === null && scopedPlacementAddSupported && supportsObjectCommand(service, selectedSceneCapability, "object.bind_animation")
                  && frames.length === clip.frame_refs.length) void addPlacementSprite(frames[0] ?? null, clip.animation_id);
              }} />
          </section>;
        })}
        {placementTool === "animation" && !objectSceneSelected && legacyPlacementAnimationGroups.map((group) => (
          <section className="placement-sprite-picker-group" key={group.assetId}>
            <SpriteAssetCard
              frames={group.frames}
              durations={group.frames.map(() => 400)}
              name={assetDisplayName(group.assetId)}
              selected={false}
              disabled={busy !== null || !scopedPlacementAddSupported}
              playback={preferences.thumbnailPlayback}
              onSelect={() => void addPlacementSprite(group.frames[0] ?? null)}
            />
          </section>
        ))}
        {placementTool === "sprite" && visibleCompiledAssetFrameGroups.map((group) => (
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
  const openEmulatorPopout = async () => {
    if (bridge?.openEmulatorPopout === undefined) {
      return;
    }
    try {
      await bridge.openEmulatorPopout();
      setEmulatorDockVisible(true);
      setEmulatorPoppedOut(true);
      await bridge.syncEmulatorPopout?.(emulatorPopoutState);
    } catch (error) {
      setMessage(errorText(error));
    }
  };
  const focusEmulatorPopout = async () => {
    try {
      const focused = await bridge?.focusEmulatorPopout?.();
      if (focused === false) {
        setEmulatorPoppedOut(false);
      }
    } catch (error) {
      setEmulatorPoppedOut(false);
      setMessage(errorText(error));
    }
  };
  const closeEmulatorPopout = async () => {
    try {
      await bridge?.closeEmulatorPopout?.();
    } catch (error) {
      setMessage(errorText(error));
    } finally {
      setEmulatorPoppedOut(false);
    }
  };
  const hideEmulatorSurface = async () => {
    if (emulatorPoppedOut) {
      await closeEmulatorPopout();
    }
    setEmulatorDockVisible(false);
  };
  const toggleEmulatorSurface = () => {
    if (emulatorDockVisible || emulatorPoppedOut) {
      void hideEmulatorSurface();
      return;
    }
    setEmulatorDockVisible(true);
  };
  const handleEmulatorIconPointerDown = (event: ReactPointerEvent<HTMLButtonElement>) => {
    if (event.button !== 0 || bridge?.openEmulatorPopout === undefined) {
      return;
    }
    emulatorIconDragRef.current = {
      pointerId: event.pointerId,
      startX: event.clientX,
      startY: event.clientY,
    };
    event.currentTarget.setPointerCapture(event.pointerId);
  };
  const handleEmulatorIconPointerMove = (event: ReactPointerEvent<HTMLButtonElement>) => {
    const drag = emulatorIconDragRef.current;
    if (drag === null || drag.pointerId !== event.pointerId) {
      return;
    }
    const distance = Math.hypot(event.clientX - drag.startX, event.clientY - drag.startY);
    if (distance < EMULATOR_POPOUT_DRAG_THRESHOLD) {
      return;
    }
    emulatorIconDragRef.current = null;
    suppressEmulatorToggleClickRef.current = true;
    event.currentTarget.releasePointerCapture(event.pointerId);
    void openEmulatorPopout();
  };
  const handleEmulatorIconPointerEnd = (event: ReactPointerEvent<HTMLButtonElement>) => {
    if (emulatorIconDragRef.current?.pointerId === event.pointerId) {
      emulatorIconDragRef.current = null;
    }
  };
  const handleEmulatorToggleClick = (event: ReactMouseEvent<HTMLButtonElement>) => {
    if (suppressEmulatorToggleClickRef.current) {
      suppressEmulatorToggleClickRef.current = false;
      event.preventDefault();
      event.stopPropagation();
      return;
    }
    toggleEmulatorSurface();
  };
  const renderDockedEmulator = () => (
    !emulatorDockVisible ? null : emulatorPoppedOut ? (
      <section className="emulator-dock-strip" aria-label="Device emulator">
        <div>
          <span className="section-kicker">Emulator</span>
          <strong>Popped out</strong>
        </div>
        <div>
          <button className="button secondary" type="button" onClick={() => void focusEmulatorPopout()}>
            <MonitorDot size={15} aria-hidden="true" />
            Show
          </button>
          <button className="button secondary" type="button" onClick={() => void closeEmulatorPopout()}>
            <X size={15} aria-hidden="true" />
            Dock
          </button>
        </div>
      </section>
    ) : renderPreviewPanel("project")
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
        sceneName={emulatorPopoutState.sceneName}
        playing={playing}
        onReset={resetProjectPreview}
        onTogglePlaying={() => setPlaying((value) => !value)}
        onAdvance={() => void advancePreview(250)}
        onInput={(source) => void sendInput(source)}
      /> : <section className="preview-pane preview-pane-large">
        <div
          className={`display-stage ${variant === "placement" ? `placement-viewport-stage ${placementViewportPanning ? "panning" : ""}` : ""}`}
          ref={variant === "placement" ? placementStageRef : undefined}
          onPointerDown={variant === "placement" ? startPlacementViewportPan : undefined}
          onWheel={variant === "placement" ? handlePlacementViewportWheel : undefined}
        >
          {variant === "placement" && renderPlacementToolPalette()}
          {variant === "placement" && renderPlacementViewportControls()}
          {variant === "placement" && spritePickerOpen && renderSpritePicker()}
          {variant === "placement" && renderPlacementGridOverlay()}
          <div
            className={`panel-bezel ${variant === "placement" ? "placement-viewport-screen" : ""}`}
            style={variant === "placement" ? {
              transform: `translate3d(${placementViewport.x}px, ${placementViewport.y}px, 0) scale(${placementViewport.zoom})`,
            } : undefined}
          >
            <FramebufferCanvas framebuffer={framebuffer} />
          {placementPreviewNotice !== null && (
            <div className="preview-status-card" role="status">
              {placementPreviewNotice}
            </div>
          )}
          {variant === "placement" && (
            <div
              className={`placement-screen-overlay labels-${placementLabelMode} ${placementOverlayVisible ? "" : "boxes-hidden"} ${["line", "rectangle", "circle", "text"].includes(placementTool) ? `drawing-tool drawing-${placementTool}` : ""}`}
              aria-label="Placement selection overlay"
              tabIndex={0}
              ref={placementScreenOverlayRef}
              onKeyDown={handlePlacementKeyDown}
              onPointerDown={startPlacementPrimitiveDraw}
            >
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
                  {(placementPrimitiveDraft.kind === "outline_rect" || placementPrimitiveDraft.kind === "filled_rect" || placementPrimitiveDraft.kind === "text") && (
                    <rect
                      className={placementPrimitiveDraft.kind === "filled_rect" ? "filled" : ""}
                      x={placementPrimitiveDraft.bounds.x + 0.5}
                      y={placementPrimitiveDraft.bounds.y + 0.5}
                      width={Math.max(0, placementPrimitiveDraft.bounds.width - 1)}
                      height={Math.max(0, placementPrimitiveDraft.bounds.height - 1)}
                    />
                  )}
                  {(placementPrimitiveDraft.kind === "circle" || placementPrimitiveDraft.kind === "ellipse"
                    || placementPrimitiveDraft.kind === "filled_circle" || placementPrimitiveDraft.kind === "filled_ellipse") && (
                    <ellipse
                      className={placementPrimitiveDraft.kind === "filled_circle" || placementPrimitiveDraft.kind === "filled_ellipse" ? "filled" : ""}
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
                        title={`${placementObjectDisplayName(element)}: ${x},${y} ${width}x${height}`}
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
                        <span>{placementObjectDisplayName(element)}</span>
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
    <div className="mode-tabs topbar-mode-tabs" aria-label="Workspace mode">
      {WORKSPACE_MODES.map(mode => (
        <button
          key={mode.mode}
          className={`topbar-button ${workspaceMode === mode.mode ? "active" : ""}`}
          type="button"
          onClick={() => setWorkspaceMode(mode.mode)}
          title={mode.label}
        >
          <img className="topbar-icon" src={mode.icon} alt="" aria-hidden="true" />
          <span className="sr-only">{mode.label}</span>
        </button>
      ))}
    </div>
  );
  const renderSpriteImportPanel = (canEditAssets: boolean) => {
    const importCheck = pendingSpriteImport === null
      ? null
      : parseSpriteImportGrid(pendingSpriteImport);
    const frameCount = pendingSpriteImport === null || importCheck === null || importCheck.error !== undefined
      ? 0
      : (pendingSpriteImport.width / importCheck.frameWidth) * (pendingSpriteImport.height / importCheck.frameHeight);
    const conversionCheck = pendingSpriteImport === null
      ? null
      : parseSpriteImportConversion(pendingSpriteImport);
    const previewError = spriteImportPreview?.error ?? conversionCheck?.error ?? null;
    const selectedPresetId = pendingSpriteImport === null ? "custom" : spriteImportPresetId(pendingSpriteImport);
    const guideColumnsValue = pendingSpriteImport === null ? 1 : Number(pendingSpriteImport.columns);
    const guideRowsValue = pendingSpriteImport === null ? 1 : Number(pendingSpriteImport.rows);
    const guideColumns = Number.isInteger(guideColumnsValue) && guideColumnsValue >= 1 && guideColumnsValue <= 256 ? guideColumnsValue : 1;
    const guideRows = Number.isInteger(guideRowsValue) && guideRowsValue >= 1 && guideRowsValue <= 256 ? guideRowsValue : 1;
    const spriteSheetGuides = <span className="sprite-import-grid-guides" aria-hidden="true">
      {Array.from({ length: Math.max(0, guideColumns - 1) }, (_, index) => (
        <i className="vertical" style={{ left: `${((index + 1) / guideColumns) * 100}%` }} key={`column-${index}`} />
      ))}
      {Array.from({ length: Math.max(0, guideRows - 1) }, (_, index) => (
        <i className="horizontal" style={{ top: `${((index + 1) / guideRows) * 100}%` }} key={`row-${index}`} />
      ))}
    </span>;
    const importPreviewScale = pendingSpriteImport === null
      ? 1
      : Math.max(2, Math.round(preferences.assetLibraryZoom * 3));
    const spriteImportPanelClass = [
      "asset-import-panel",
      "sprite-import-panel",
      pendingSpriteImport === null ? "" : "sprite-import-workspace",
    ].filter(Boolean).join(" ");
    const spriteImportPanelStyle = pendingSpriteImport === null ? undefined : {
      "--sprite-import-source-preview-width": `${pendingSpriteImport.width * importPreviewScale}px`,
      "--sprite-import-converted-preview-width": `${pendingSpriteImport.width * spriteImportOutputScale(pendingSpriteImport.conversionMode) * importPreviewScale}px`,
    } as CSSProperties;
    return (
      <div className={spriteImportPanelClass} style={spriteImportPanelStyle}>
        {pendingSpriteImport === null ? (
          <div>
            <strong>PNG import</strong>
          </div>
        ) : (
          <>
            <div className="asset-import-heading">
              <div>
                <strong>{pendingSpriteImport.displayName}</strong>
                <span>{pendingSpriteImport.sourceName} - {pendingSpriteImport.width}x{pendingSpriteImport.height}</span>
              </div>
              <button className="icon-button" type="button" onClick={() => setPendingSpriteImport(null)} title="Cancel import" aria-label="Cancel import">
                <X size={14} aria-hidden="true" />
              </button>
            </div>
            <div className="sprite-import-preview-grid">
              <div className="sprite-import-preview-panel">
                <strong>Source</strong>
                <div className="sprite-import-preview-canvas source-preview">
                  <span className="sprite-import-preview-image">
                    <img src={pendingSpriteImport.sourceDataUrl} alt="" />
                    {spriteSheetGuides}
                  </span>
                </div>
              </div>
              <div className="sprite-import-preview-panel">
                <strong>Converted</strong>
                <div className={`sprite-import-preview-canvas converted-preview ${previewError !== null ? "error" : ""}`}>
                  {spriteImportPreview !== null && spriteImportPreview.error === null ? (
                    <span className="sprite-import-preview-image">
                      <img src={spriteImportPreview.dataUrl} alt="" />
                      {spriteSheetGuides}
                    </span>
                  ) : (
                    <span>{previewError ?? "Rendering preview..."}</span>
                  )}
                </div>
              </div>
            </div>
            <div className="asset-import-controls">
              <label className="asset-import-preset-control">
                Preset
                <select
                  aria-label="Sprite import preset"
                  value={selectedPresetId}
                  disabled={!canEditAssets}
                  onChange={(event) => {
                    const preset = SPRITE_IMPORT_PRESETS.find(item => item.id === event.target.value);
                    if (preset === undefined) {
                      return;
                    }
                    setPendingSpriteImport((current) => current === null ? null : {
                      ...current,
                      conversionMode: preset.conversionMode,
                      threshold: preset.threshold,
                      ditherStrength: preset.ditherStrength,
                      alphaCutoff: preset.alphaCutoff,
                      alphaMode: preset.alphaMode,
                      invert: preset.invert,
                    });
                  }}
                >
                  <option value="custom">Custom</option>
                  {SPRITE_IMPORT_PRESETS.map((preset) => (
                    <option key={preset.id} value={preset.id}>{preset.label}</option>
                  ))}
                </select>
              </label>
              <label>
                Conversion
                <select
                  aria-label="Sprite conversion mode"
                  value={pendingSpriteImport.conversionMode}
                  disabled={!canEditAssets}
                  onChange={(event) => {
                    const value = event.target.value as SpriteImportConversionMode;
                    setPendingSpriteImport((current) => current === null ? null : { ...current, conversionMode: value });
                  }}
                >
                  <option value="threshold_1bpp">B/W mask</option>
                  <option value="ordered_2x2">Pattern dither 2x2 (2x size)</option>
                  <option value="ordered_4x4">Pattern dither 4x4 (4x size)</option>
                  <option value="floyd_steinberg">Error diffusion</option>
                </select>
              </label>
              <label>
                Transparency
                <select
                  aria-label="Sprite import transparency"
                  value={pendingSpriteImport.alphaMode}
                  disabled={!canEditAssets}
                  onChange={(event) => {
                    const value = event.target.value as SpriteImportAlphaMode;
                    setPendingSpriteImport((current) => current === null ? null : { ...current, alphaMode: value });
                  }}
                >
                  <option value="respect_alpha">Respect alpha</option>
                  <option value="ignore_alpha">Ignore alpha</option>
                  <option value="transparent_as_white">Transparent as white</option>
                </select>
              </label>
              <label>
                Threshold
                <input
                  type="number"
                  min={0}
                  max={255}
                  step={1}
                  aria-label="Sprite import threshold"
                  value={pendingSpriteImport.threshold}
                  disabled={!canEditAssets}
                  onChange={(event) => {
                    const value = event.target.value;
                    setPendingSpriteImport((current) => current === null ? null : { ...current, threshold: value });
                  }}
                />
              </label>
              <label>
                Dither strength <span>{pendingSpriteImport.ditherStrength}%</span>
                <input
                  type="range"
                  min={0}
                  max={100}
                  step={5}
                  aria-label="Sprite import dither strength"
                  value={pendingSpriteImport.ditherStrength}
                  disabled={!canEditAssets || pendingSpriteImport.conversionMode === "threshold_1bpp"}
                  onChange={(event) => {
                    const value = event.target.value;
                    setPendingSpriteImport((current) => current === null ? null : { ...current, ditherStrength: value });
                  }}
                />
              </label>
              <label>
                Alpha cutoff
                <input
                  type="number"
                  min={1}
                  max={255}
                  step={1}
                  aria-label="Sprite import alpha cutoff"
                  value={pendingSpriteImport.alphaCutoff}
                  disabled={!canEditAssets || pendingSpriteImport.alphaMode === "ignore_alpha"}
                  onChange={(event) => {
                    const value = event.target.value;
                    setPendingSpriteImport((current) => current === null ? null : { ...current, alphaCutoff: value });
                  }}
                />
              </label>
              <label className="asset-import-inline-toggle">
                Invert
                <input
                  type="checkbox"
                  aria-label="Invert sprite import"
                  checked={pendingSpriteImport.invert}
                  disabled={!canEditAssets}
                  onChange={(event) => {
                    const checked = event.target.checked;
                    setPendingSpriteImport((current) => current === null ? null : { ...current, invert: checked });
                  }}
                />
              </label>
              <label>
                Columns
                <input
                  type="number"
                  min={1}
                  max={256}
                  step={1}
                  aria-label="Sprite sheet columns"
                  value={pendingSpriteImport.columns}
                  disabled={!canEditAssets}
                  onChange={(event) => {
                    const value = event.target.value;
                    setPendingSpriteImport((current) => current === null ? null : { ...current, columns: value });
                  }}
                />
              </label>
              <label>
                Rows
                <input
                  type="number"
                  min={1}
                  max={256}
                  step={1}
                  aria-label="Sprite sheet rows"
                  value={pendingSpriteImport.rows}
                  disabled={!canEditAssets}
                  onChange={(event) => {
                    const value = event.target.value;
                    setPendingSpriteImport((current) => current === null ? null : { ...current, rows: value });
                  }}
                />
              </label>
              <div className={`asset-import-result ${importCheck?.error !== undefined ? "error" : ""}`}>
                {importCheck?.error ?? `${frameCount} frame${frameCount === 1 ? "" : "s"} / ${importCheck?.frameWidth! * importCheck?.scale!}x${importCheck?.frameHeight! * importCheck?.scale!} px each / ${importCheck?.outputWidth}x${importCheck?.outputHeight} sheet`}
              </div>
              <div className={`asset-import-result ${previewError !== null ? "error" : ""}`}>
                {previewError ?? (spriteImportPreview === null
                  ? "Rendering preview..."
                  : `${spriteImportPreview.sourceColorsLimited ? `${spriteImportPreview.sourceColors}+` : spriteImportPreview.sourceColors} source colors / ${spriteImportPreview.blackPixels} black / ${spriteImportPreview.whitePixels} white / ${spriteImportPreview.transparentPixels} transparent`)}
              </div>
              <button
                className="button primary"
                type="button"
                disabled={!canEditAssets || importCheck?.error !== undefined || previewError !== null || spriteImportPreview === null}
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
  const renderBakedTextPanel = (canEditAssets: boolean) => {
    if (bakedTextDraft === null) {
      return null;
    }
    const canCreateTextSprite = canEditAssets
      && bakedTextDraft.previewDataUrl !== null
      && bakedTextDraft.error === null
      && fontAssets.some(item => item.font_id === bakedTextDraft.fontId);
    return (
      <div className="asset-import-panel baked-text-panel">
        <div className="asset-import-heading">
          <div>
            <strong>Baked text sprite</strong>
            <span>Rasterize custom font text into an ordinary PNG sprite asset.</span>
          </div>
          <button className="icon-button" type="button" onClick={() => setBakedTextDraft(null)} title="Cancel text sprite" aria-label="Cancel text sprite">
            <X size={14} aria-hidden="true" />
          </button>
        </div>
        <div className="baked-text-grid">
          <label>
            Asset name
            <input
              type="text"
              maxLength={64}
              value={bakedTextDraft.assetName}
              disabled={!canEditAssets}
              onChange={(event) => setBakedTextDraft(current => current === null ? null : { ...current, assetName: event.target.value })}
            />
          </label>
          <label>
            Size px
            <input
              type="number"
              min={BAKED_TEXT_MIN_FONT_SIZE}
              max={BAKED_TEXT_MAX_FONT_SIZE}
              step={1}
              value={bakedTextDraft.fontSize}
              disabled={!canEditAssets}
              onChange={(event) => setBakedTextDraft(current => current === null ? null : { ...current, fontSize: event.target.value })}
            />
          </label>
          <label>
            Font asset
            <select
              value={bakedTextDraft.fontId}
              disabled={!canEditAssets || fontAssets.length === 0}
              onChange={(event) => setBakedTextDraft(current => current === null ? null : { ...current, fontId: event.target.value })}
            >
              {fontAssets.length === 0 ? (
                <option value="">No fonts imported</option>
              ) : fontAssets.map(font => (
                <option key={font.font_id} value={font.font_id}>{font.display_name}</option>
              ))}
            </select>
          </label>
          <label className="baked-text-content-field">
            Text
            <textarea
              rows={3}
              maxLength={512}
              value={bakedTextDraft.text}
              disabled={!canEditAssets}
              onChange={(event) => setBakedTextDraft(current => current === null ? null : { ...current, text: event.target.value })}
            />
          </label>
          <div className="baked-text-font-field">
            <span>Font</span>
            <button
              className="button secondary"
              type="button"
              disabled={!canEditAssets}
              onClick={() => {
                setAssetTab("font");
                setBakedTextDraft(null);
              }}
            >
              <StudioIcon name="text" />
              Manage fonts
            </button>
            <small>{fontAssets.find(item => item.font_id === bakedTextDraft.fontId)?.source_path ?? "Import a font once, then reuse it."}</small>
          </div>
          <div className={`baked-text-preview ${bakedTextDraft.error !== null ? "error" : ""}`}>
            {bakedTextDraft.previewDataUrl === null ? (
              <span>{bakedTextDraft.error ?? bakedTextDraft.status}</span>
            ) : (
              <>
                <img src={bakedTextDraft.previewDataUrl} alt="Baked text sprite preview" />
                <span>{bakedTextDraft.previewWidth}x{bakedTextDraft.previewHeight} px</span>
              </>
            )}
          </div>
          <div className="baked-text-actions">
            <button
              className="button primary"
              type="button"
              disabled={!canCreateTextSprite}
              onClick={() => void confirmBakedTextSprite()}
            >
              Create sprite
            </button>
          </div>
        </div>
        <div className={`asset-import-debug ${bakedTextDraft.error !== null ? "error" : ""}`}>
          {bakedTextDraft.error ?? bakedTextDraft.status}
        </div>
      </div>
    );
  };
  const renderAnimationNormalizePanel = (canEditAssets: boolean) => {
    if (animationNormalizeDraft === null) {
      return null;
    }
    const selectedFrames = animationNormalizeDraft.frameIds.flatMap(id => compiledAssetFrameById.get(id) ?? []);
    const frameWidth = selectedFrames.length === 0 ? 0 : Math.max(...selectedFrames.map(frame => frame.width));
    const frameHeight = selectedFrames.length === 0 ? 0 : Math.max(...selectedFrames.map(frame => frame.height));
    const sizes = [...new Set(selectedFrames.map(frame => `${frame.width}x${frame.height}`))];
    const cadenceMs = Number(animationNormalizeDraft.cadence);
    const cadenceValid = animationNormalizeDraft.cadence.trim() !== ""
      && Number.isInteger(cadenceMs)
      && cadenceMs >= 1
      && cadenceMs <= 60000;
    const playbackValid = animationPolicies.includes(animationNormalizeDraft.loopPolicy);
    return (
      <div className="asset-import-panel animation-normalize-panel">
        <div className="asset-import-heading">
          <div>
            <strong>Normalize animation frames</strong>
            <span>{selectedFrames.length} selected frame{selectedFrames.length === 1 ? "" : "s"} / {sizes.join(", ")} to {frameWidth}x{frameHeight}</span>
          </div>
          <button className="icon-button" type="button" onClick={() => setAnimationNormalizeDraft(null)} title="Cancel normalization" aria-label="Cancel normalization">
            <X size={14} aria-hidden="true" />
          </button>
        </div>
        <div className="animation-normalize-grid">
          <label>
            Horizontal anchor
            <select
              value={animationNormalizeDraft.horizontal}
              disabled={!canEditAssets}
              onChange={(event) => setAnimationNormalizeDraft(current => current === null ? null : { ...current, horizontal: event.target.value as AnimationAnchorX, error: null })}
            >
              <option value="left">Left</option>
              <option value="center">Center</option>
              <option value="right">Right</option>
            </select>
          </label>
          <label>
            Vertical anchor
            <select
              value={animationNormalizeDraft.vertical}
              disabled={!canEditAssets}
              onChange={(event) => setAnimationNormalizeDraft(current => current === null ? null : { ...current, vertical: event.target.value as AnimationAnchorY, error: null })}
            >
              <option value="top">Top</option>
              <option value="middle">Middle</option>
              <option value="bottom">Bottom</option>
            </select>
          </label>
          <label>
            Cadence (ms)
            <input
              type="number"
              min={1}
              max={60000}
              step={1}
              aria-label="Animation cadence"
              value={animationNormalizeDraft.cadence}
              disabled={!canEditAssets}
              onChange={(event) => setAnimationNormalizeDraft(current => current === null ? null : { ...current, cadence: event.target.value, error: null })}
            />
          </label>
          <label>
            Playback
            <select
              aria-label="Animation playback"
              value={animationNormalizeDraft.loopPolicy}
              disabled={!canEditAssets}
              onChange={(event) => setAnimationNormalizeDraft(current => current === null ? null : { ...current, loopPolicy: event.target.value, error: null })}
            >
              {animationPolicies.map(policy => <option key={policy} value={policy}>{policy === "loop" ? "Loop" : policy === "once" ? "Play once" : policy}</option>)}
            </select>
          </label>
          <div className="animation-normalize-strip" aria-label="Selected animation frames">
            {selectedFrames.map((frame, index) => (
              <span key={`${frame.frame_id}-${index}`}>
                <FramePreviewCanvas frame={frame} />
                <small>{index + 1}</small>
              </span>
            ))}
          </div>
          <button
            className="button primary"
            type="button"
            disabled={!canEditAssets || selectedFrames.length === 0 || selectedFrames.length !== animationNormalizeDraft.frameIds.length || !cadenceValid || !playbackValid}
            onClick={() => void createNormalizedAnimation()}
          >
            Create animation
          </button>
        </div>
        <div className={`asset-import-debug ${animationNormalizeDraft.error !== null ? "error" : ""}`}>
          {animationNormalizeDraft.error ?? "A generated sprite asset will back the animation; source sprites stay unchanged."}
        </div>
      </div>
    );
  };
  const renderAudioImportPanel = (canEditAssets: boolean) => {
    if (pendingAudioImport === null) return null;
    const updateTrim = (startMs: number, endMs: number) => {
      const start = Math.max(0, Math.min(pendingAudioImport.durationMs - 1, startMs));
      const end = Math.max(start + 1, Math.min(pendingAudioImport.durationMs, endMs));
      setPendingAudioImport((current) => current === null ? null : { ...current, trimStartMs: start, trimEndMs: end });
    };
    const retainedMs = pendingAudioImport.trimEndMs - pendingAudioImport.trimStartMs;
    return <section className="audio-import-panel" onClick={(event) => event.stopPropagation()}>
      <div className="audio-import-heading">
        <span><strong>{pendingAudioImport.sourceName}</strong><small>PCM WAV import</small></span>
        <button className="icon-button" type="button" title="Cancel audio import" aria-label="Cancel audio import"
          onClick={() => setPendingAudioImport(null)}><X size={16} aria-hidden="true" /></button>
      </div>
      <AudioImportWaveform
        peaks={pendingAudioImport.waveformPeaks}
        durationMs={pendingAudioImport.durationMs}
        startMs={pendingAudioImport.trimStartMs}
        endMs={pendingAudioImport.trimEndMs}
        onChange={updateTrim}
      />
      <div className="audio-trim-controls">
        <label>Start
          <span><input type="number" min={0} max={Math.max(0, pendingAudioImport.trimEndMs - 1)} step={1}
            value={Math.round(pendingAudioImport.trimStartMs)} disabled={!canEditAssets}
            onChange={(event) => updateTrim(Number(event.target.value), pendingAudioImport.trimEndMs)} /> ms</span>
        </label>
        <label>End
          <span><input type="number" min={pendingAudioImport.trimStartMs + 1} max={pendingAudioImport.durationMs} step={1}
            value={Math.round(pendingAudioImport.trimEndMs)} disabled={!canEditAssets}
            onChange={(event) => updateTrim(pendingAudioImport.trimStartMs, Number(event.target.value))} /> ms</span>
        </label>
        <button className="button secondary" type="button" disabled={!canEditAssets}
          onClick={() => updateTrim(pendingAudioImport.suggestedTrimStartMs, pendingAudioImport.suggestedTrimEndMs)}>
          Trim detected silence
        </button>
        <button className="button secondary" type="button" disabled={!canEditAssets || (pendingAudioImport.trimStartMs === 0 && pendingAudioImport.trimEndMs === pendingAudioImport.durationMs)}
          onClick={() => updateTrim(0, pendingAudioImport.durationMs)}>
          Reset trim
        </button>
      </div>
      <dl className="audio-import-summary">
        <div><dt>Source</dt><dd>{pendingAudioImport.channels === 1 ? "Mono" : "Stereo"}, {pendingAudioImport.bitsPerSample}-bit, {pendingAudioImport.sampleRateHz} Hz</dd></div>
        <div><dt>Source peak</dt><dd>{pendingAudioImport.peakDbfs === null ? "Silent" : `${pendingAudioImport.peakDbfs.toFixed(1)} dBFS`}</dd></div>
        <div><dt>Retained</dt><dd>{Math.round(retainedMs)} ms of {Math.round(pendingAudioImport.durationMs)} ms</dd></div>
        <div><dt>Import peak</dt><dd>{normalizeAudioImports ? `${audioImportPeakDbfs} dBFS` : "Unchanged"}</dd></div>
      </dl>
      <div className="audio-import-actions">
        <button className="button secondary" type="button" onClick={() => setPendingAudioImport(null)}>Cancel</button>
        <button className="button primary" type="button" disabled={!canEditAssets || retainedMs < 1}
          onClick={() => void importPendingAudioWav()}>Import SFX</button>
      </div>
    </section>;
  };
  const renderAssetsWorkspace = () => {
    const canEditAssets = bridge !== undefined && project !== null && busy === null && service?.operations.includes("project.apply_commands") === true;
    const audioSupported = service?.state_scene_audio.host_package_support === true;
    const auditionSupported = service?.operations.includes("project.audio_audition") === true;
    const hasTabAssets = assetTab === "sprite"
      ? animationClips.length > 0 || visibleCompiledAssetFrameGroups.length > 0
      : assetTab === "audio"
        ? audioCues.length > 0
        : fontAssets.length > 0;
    const spriteImportActive = assetTab === "sprite" && pendingSpriteImport !== null;
    const assetTabs: AssetTab[] = ["sprite", "audio", "font"];
    const fontPreviewText = preferences.fontPreviewText.trim() || DEFAULT_FONT_PREVIEW_TEXT;
    const normalizedAssetSearch = assetSearchQuery.trim().toLocaleLowerCase();
    const availableAssetTags = [...new Set((assetTab === "sprite" ? assets : assetTab === "audio" ? audioAssets : [])
      .flatMap(asset => asset.tags ?? []))].sort((left, right) => left.localeCompare(right));
    const matchesAssetSearch = (displayName: string, tags: string[] = []) => normalizedAssetSearch === ""
      || displayName.toLocaleLowerCase().includes(normalizedAssetSearch)
      || tags.some(tag => tag.toLocaleLowerCase().includes(normalizedAssetSearch));
    const matchesTagFilter = (tags: string[] = []) => assetTagFilter === "" || tags.includes(assetTagFilter);
    const filteredAnimationClips = spriteAssetFilter === "all" || spriteAssetFilter === "animation"
      ? animationClips.filter((clip) => assetTagFilter === "" && matchesAssetSearch(animationLabel(clip)))
      : [];
    const filteredSourceSpriteGroups = sourceSpriteGroups.flatMap((sourceGroup) => {
      const textGroup = sourceGroup.label === "Text sprite" || sourceGroup.label === "Text";
      const typeVisible = spriteAssetFilter === "all"
        || (spriteAssetFilter === "text" && textGroup)
        || (spriteAssetFilter === "static" && !textGroup);
      if (!typeVisible) return [];
      const items = sourceGroup.items.filter((group) => {
        const asset = assetById.get(group.assetId);
        return matchesTagFilter(asset?.tags) && matchesAssetSearch(asset?.display_name ?? group.assetId, asset?.tags);
      });
      if (items.length === 0) return [];
      return [{
        ...sourceGroup,
        detail: `${items.length} asset${items.length === 1 ? "" : "s"}`,
        items,
      }];
    });
    const filteredAudioCueGroups = audioCueGroups.flatMap((cueGroup) => {
      const items = cueGroup.items.filter((cue) => {
        const asset = audioAssetById.get(cue.asset_ref);
        return matchesTagFilter(asset?.tags) && matchesAssetSearch(audioCueDisplayName(cue), asset?.tags);
      });
      if (items.length === 0) return [];
      return [{
        ...cueGroup,
        detail: `${items.length} cue${items.length === 1 ? "" : "s"}`,
        items,
      }];
    });
    const filteredFontAssets = fontAssets.filter((font) => matchesAssetSearch(font.display_name));
    const sourceSpriteCount = sourceSpriteGroups.reduce((total, group) => total + group.items.length, 0);
    const filteredSourceSpriteCount = filteredSourceSpriteGroups.reduce((total, group) => total + group.items.length, 0);
    const totalAssetCount = assetTab === "sprite"
      ? animationClips.length + sourceSpriteCount
      : assetTab === "audio"
        ? audioCues.length
        : fontAssets.length;
    const matchingAssetCount = assetTab === "sprite"
      ? filteredAnimationClips.length + filteredSourceSpriteCount
      : assetTab === "audio"
        ? filteredAudioCueGroups.reduce((total, group) => total + group.items.length, 0)
        : filteredFontAssets.length;
    const hasMatchingAssets = matchingAssetCount > 0;
    const assetLibraryZoom = preferences.assetLibraryZoom;
    const assetLibraryZoomPercent = Math.round(assetLibraryZoom * 100);
    const assetLibraryZoomStyle = {
      "--asset-library-card-min": `${Math.round(178 * assetLibraryZoom)}px`,
      "--asset-library-card-min-height": `${Math.round(174 * assetLibraryZoom)}px`,
      "--asset-library-card-padding": `${Math.round(10 * assetLibraryZoom)}px`,
      "--asset-library-gallery-gap": `${Math.round(12 * assetLibraryZoom)}px`,
      "--asset-library-preview-height": `${Math.round(120 * assetLibraryZoom)}px`,
      "--asset-library-frame-width": `${Math.round(136 * assetLibraryZoom)}px`,
      "--asset-library-frame-height": `${Math.round(104 * assetLibraryZoom)}px`,
      "--asset-library-sheet-padding": `${Math.round(8 * assetLibraryZoom)}px`,
      "--asset-library-sheet-gap": `${Math.max(3, Math.round(5 * assetLibraryZoom))}px`,
      "--asset-library-sheet-cell-size": `${Math.round(32 * assetLibraryZoom)}px`,
      "--asset-library-text-preview-width": `${Math.round(220 * assetLibraryZoom)}px`,
      "--asset-library-text-preview-min": `${Math.round(96 * assetLibraryZoom)}px`,
      "--asset-library-audio-card-min": `${Math.round(236 * assetLibraryZoom)}px`,
      "--asset-library-font-preview-size": `${Math.round(24 * assetLibraryZoom)}px`,
    } as CSSProperties;
    return (
      <section className="asset-workspace-pane">
        <div className="asset-workspace" style={assetLibraryZoomStyle} onClick={handleAssetWorkspaceBackgroundClick} onWheel={handleAssetWorkspaceWheel}>
          <div className="asset-workspace-summary">
            <div className="asset-library-tabs" role="tablist" aria-label="Asset types">
              {assetTabs.map(tab => (
                <button key={tab} type="button" role="tab" id={`asset-tab-${tab}`}
                  aria-selected={assetTab === tab} aria-controls="asset-library-panel"
                  tabIndex={assetTab === tab ? 0 : -1}
                  onClick={() => selectAssetTab(tab)}
                  onKeyDown={event => {
                    if (!["ArrowLeft", "ArrowRight", "Home", "End"].includes(event.key)) return;
                    event.preventDefault();
                    const currentIndex = assetTabs.indexOf(tab);
                    const next = event.key === "Home"
                      ? assetTabs[0]
                      : event.key === "End"
                        ? assetTabs[assetTabs.length - 1]
                        : assetTabs[(currentIndex + (event.key === "ArrowRight" ? 1 : assetTabs.length - 1)) % assetTabs.length];
                    selectAssetTab(next);
                    document.getElementById(`asset-tab-${next}`)?.focus();
                  }}>
                  {tab === "sprite" ? <StudioIcon name="sprite" /> : tab === "audio" ? <StudioIcon name="sfx" /> : <StudioIcon name="text" />}
                  {tab === "sprite" ? "Sprites" : tab === "audio" ? "Audio" : "Fonts"}
                </button>
              ))}
            </div>
            <div className="asset-library-zoom-controls" aria-label="Asset library zoom controls" onClick={(event) => event.stopPropagation()}>
              <button
                className="icon-button"
                type="button"
                title="Zoom asset library out"
                aria-label="Zoom asset library out"
                disabled={assetLibraryZoom <= ASSET_LIBRARY_MIN_ZOOM}
                onClick={() => setAssetLibraryZoom(assetLibraryZoom - ASSET_LIBRARY_ZOOM_STEP)}
              >
                <ZoomOut size={15} aria-hidden="true" />
              </button>
              <span className="asset-library-zoom-value">{assetLibraryZoomPercent}%</span>
              <button
                className="icon-button"
                type="button"
                title="Zoom asset library in"
                aria-label="Zoom asset library in"
                disabled={assetLibraryZoom >= ASSET_LIBRARY_MAX_ZOOM}
                onClick={() => setAssetLibraryZoom(assetLibraryZoom + ASSET_LIBRARY_ZOOM_STEP)}
              >
                <ZoomIn size={15} aria-hidden="true" />
              </button>
              <button
                className="icon-button"
                type="button"
                title="Reset asset library zoom"
                aria-label="Reset asset library zoom"
                disabled={assetLibraryZoom === 1}
                onClick={() => setAssetLibraryZoom(1)}
              >
                <RotateCcw size={14} aria-hidden="true" />
              </button>
            </div>
            <div className="asset-workspace-actions">
              {assetTab === "sprite" ? <>
              <button
                className="button secondary"
                type="button"
                disabled={!canEditAssets || projectPath === null}
                onClick={() => void chooseSpritePng()}
              >
                <StudioIcon name="sprite" />
                Choose PNG
              </button>
              <button
                className="button secondary"
                type="button"
                disabled={!canEditAssets || projectPath === null || fontAssets.length === 0}
                title={fontAssets.length === 0 ? "Import a font asset first" : "Create baked text sprite"}
                onClick={startBakedTextSprite}
              >
                <StudioIcon name="textSprite" />
                New text sprite
              </button>
              <button className="button secondary" type="button" disabled={!canAuthorAnimations || busy !== null || !combineFrameIds.length}
                onClick={startAssetAnimation}><Plus size={15} />Create animation</button>
              </> : assetTab === "audio" ? <>
              <button
                className="button secondary"
                type="button"
                disabled={!canEditAssets || projectPath === null || !audioSupported}
                onClick={() => void chooseAudioWav()}
              >
                <StudioIcon name="sfx" />
                WAV SFX
              </button>
              </> : <>
              <button
                className="button secondary"
                type="button"
                disabled={bridge === undefined || projectPath === null || busy !== null || bridge.importFontAsset === undefined}
                onClick={() => void importFontAsset()}
                title={bridge?.importFontAsset === undefined ? "Restart Peep Studio to enable font import" : "Import TTF or OTF font"}
              >
                <StudioIcon name="text" />
                Import font
              </button>
              </>}
            </div>
          </div>
          <div className="asset-library-tools" onClick={(event) => event.stopPropagation()}>
            <label className="asset-library-search">
              <Search size={16} aria-hidden="true" />
              <input
                type="search"
                value={assetSearchQuery}
                onChange={(event) => setAssetSearchQuery(event.target.value)}
                placeholder={`Search ${assetTab === "sprite" ? "sprites" : assetTab === "audio" ? "SFX" : "fonts"}`}
                aria-label={`Search ${assetTab === "sprite" ? "sprites" : assetTab === "audio" ? "SFX" : "fonts"}`}
              />
              {assetSearchQuery !== "" && <button
                className="asset-library-search-clear"
                type="button"
                title="Clear asset search"
                aria-label="Clear asset search"
                onClick={() => setAssetSearchQuery("")}
              >
                <X size={15} aria-hidden="true" />
              </button>}
            </label>
            {assetTab === "sprite" && <div className="asset-library-filters" role="group" aria-label="Filter sprite assets">
              {(["all", "static", "animation", "text"] as const).map((filter) => (
                <button
                  key={filter}
                  type="button"
                  className={spriteAssetFilter === filter ? "active" : ""}
                  aria-pressed={spriteAssetFilter === filter}
                  onClick={() => setSpriteAssetFilter(filter)}
                >
                  {filter === "all" ? "All" : filter === "animation" ? "Animated" : filter[0].toUpperCase() + filter.slice(1)}
                </button>
              ))}
            </div>}
            {assetTab !== "font" && availableAssetTags.length > 0 && <label className="asset-tag-filter">
              <span>Tag</span>
              <select value={assetTagFilter} onChange={event => setAssetTagFilter(event.target.value)}>
                <option value="">All tags</option>
                {availableAssetTags.map(tag => <option key={tag} value={tag}>{tag}</option>)}
              </select>
            </label>}
            <span className="asset-library-match-count">
              {matchingAssetCount === totalAssetCount ? totalAssetCount : `${matchingAssetCount} of ${totalAssetCount}`}
            </span>
          </div>
          <div id="asset-library-panel" role="tabpanel" aria-labelledby={`asset-tab-${assetTab}`}>
          {assetTab === "audio" && pendingAudioImport !== null ? (
            renderAudioImportPanel(canEditAssets)
          ) : spriteImportActive ? (
            renderSpriteImportPanel(canEditAssets)
          ) : (
            <>
          {assetTab === "sprite" && renderBakedTextPanel(canEditAssets)}
          {assetTab === "sprite" && renderSpriteImportPanel(canEditAssets)}
          {assetTab === "sprite" && renderAnimationNormalizePanel(canEditAssets)}
          <div className="asset-group-stack">
            {assetTab === "sprite" && filteredAnimationClips.length > 0 && <section className="asset-group-panel">
              <div className="asset-group-heading">
                <strong>Animated sprites</strong>
                <span>{filteredAnimationClips.length} animation{filteredAnimationClips.length === 1 ? "" : "s"}</span>
              </div>
              <div className="asset-frame-gallery animation-asset-gallery">{filteredAnimationClips.map(clip => <SpriteAssetCard key={clip.animation_id}
                frames={clip.frame_refs.flatMap(id => compiledAssetFrameById.get(id) ? [compiledAssetFrameById.get(id)!] : [])}
                durations={clip.frame_duration_ms} name={animationLabel(clip)}
                selected={assetSelection?.kind === "animation" && assetSelection.clipId === clip.animation_id}
                playback={preferences.thumbnailPlayback} onSelect={() => selectAssetRecord({kind:"animation",clipId:clip.animation_id})} />)}</div>
            </section>}
            {assetTab === "sprite" && filteredSourceSpriteCount > 0 && (
              <section className="asset-group-panel">
                <div className="asset-group-heading">
                  <strong>{spriteAssetFilter === "text" ? "Text sprites" : "Static sprites"}</strong>
                  <span>{filteredSourceSpriteCount} sprite{filteredSourceSpriteCount === 1 ? "" : "s"}</span>
                </div>
                <div className="asset-subgroup-stack">
                  {filteredSourceSpriteGroups.map((sourceGroup) => (
                    <section className="asset-subgroup" key={sourceGroup.key}>
                      <div className="asset-subgroup-heading">
                        <strong>{sourceGroup.label}</strong>
                        <span>{sourceGroup.detail}</span>
                      </div>
                      <div className="asset-frame-gallery">
                        {sourceGroup.items.map(group => {
                          const authoredOrder = new Map((assetById.get(group.assetId)?.frames ?? []).map((frame, index) => [frame.frame_id, index]));
                          const frames = [...group.frames].sort((a, b) => (authoredOrder.get(a.frame_id) ?? Infinity) - (authoredOrder.get(b.frame_id) ?? Infinity));
                          const asset = assetById.get(group.assetId);
                          const kind = spriteAssetKind(asset, frames.length);
                          const frameIds = frames.map(frame => frame.frame_id);
                          const selectedFrameCount = frameIds.filter(frameId => combineFrameIds.includes(frameId)).length;
                          const columns = spriteSheetColumns(asset, frames.length);
                          const textPreview = isTextSpriteAsset(asset);
                          const sheetMetrics = !textPreview && frames.length > 1
                            ? spriteSheetPreviewMetrics(columns, frames, assetLibraryZoom)
                            : null;
                          const sourceItemStyle = sheetMetrics === null ? undefined : {
                            "--asset-sheet-preview-height": `${sheetMetrics.height}px`,
                            "--asset-sheet-cell-width": `${sheetMetrics.cellWidth}px`,
                            "--asset-sheet-cell-height": `${sheetMetrics.cellHeight}px`,
                            "--asset-sheet-card-width": `${sheetMetrics.width}px`,
                            ...(sheetMetrics.expanded ? { "--asset-sheet-card-min-width": `${sheetMetrics.width}px` } : {}),
                          } as CSSProperties;
                          const sourceItemClassName = [
                            "sprite-source-item",
                            sheetMetrics && !sheetMetrics.expanded ? "sprite-source-item-compact" : "",
                            sheetMetrics?.expanded ? "sprite-source-item-sheet" : "",
                          ].filter(Boolean).join(" ");
                          const cardFrameSelection = canAuthorAnimations && !textPreview && frameIds.length > 1;
                          return <div className={sourceItemClassName} style={sourceItemStyle} key={group.assetId}>
                            <span className="asset-kind-badge">{kind}</span>
                            {canAuthorAnimations && <>
                              <input type="checkbox" aria-label={frameIds.length === 1
                                ? `Include ${assetDisplayName(group.assetId)} in animation`
                                : `Include ${assetDisplayName(group.assetId)} frames in animation`}
                                checked={selectedFrameCount === frameIds.length && frameIds.length > 0}
                                onChange={event => setAnimationFrameSelectionGroup(frameIds, event.target.checked)} />
                              {selectedFrameCount > 0 && <span className="animation-selection-badge">{selectedFrameCount}</span>}
                            </>}
                            <SpriteSheetAssetCard frames={frames}
                              name={assetDisplayName(group.assetId)}
                              selected={selectedAssetFrame?.asset_id === group.assetId}
                              columns={columns}
                              frameScale={sheetMetrics?.frameScale}
                              textPreview={textPreview}
                              animationSelection={cardFrameSelection ? {
                                selectedFrameIds: combineFrameIds,
                                onToggleFrame: toggleAnimationFrameSelection,
                              } : undefined}
                              onSelect={() => {
                                setCombineFrameIds([]);
                                selectAssetRecord({ kind: "sprite", frameId: selectedAssetFrame?.asset_id === group.assetId ? selectedAssetFrame.frame_id : frames[0].frame_id });
                              }} />
                          </div>;
                        })}
                      </div>
                    </section>
                  ))}
                </div>
              </section>
            )}
            {assetTab === "audio" && matchingAssetCount > 0 && (
              <section className="asset-group-panel">
                <div className="asset-group-heading">
                  <strong>Sampled SFX</strong>
                  <span>{matchingAssetCount} cue{matchingAssetCount === 1 ? "" : "s"}</span>
                </div>
                <div className="asset-subgroup-stack">
                  {filteredAudioCueGroups.map((cueGroup) => (
                    <section className="asset-subgroup" key={cueGroup.key}>
                      <div className="asset-subgroup-heading">
                        <strong>{cueGroup.label}</strong>
                        <span>{cueGroup.detail}</span>
                      </div>
                      <div className="audio-cue-gallery">
                        {cueGroup.items.map((cue) => {
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
                                <AudioWaveform
                                  projectPath={projectPath}
                                  sourcePath={asset?.source_path}
                                  revision={project?.project_revision}
                                  progress={audioAuditionProgress?.cueId === cue.cue_id ? audioAuditionProgress.progress : null}
                                />
                                <span>
                                  <span className="asset-kind-badge audio-cue-badge">SFX</span>
                                  <strong>{audioCueDisplayName(cue)}</strong>
                                  <small>
                                    {asset === undefined ? "Source unavailable" : `${asset.duration_ms} ms / ${asset.adpcm_bytes} ADPCM bytes`}
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
                  ))}
                </div>
              </section>
            )}
            {assetTab === "font" && filteredFontAssets.length > 0 && (
              <section className="asset-group-panel">
                <div className="asset-group-heading">
                  <strong>Imported fonts</strong>
                  <span>{filteredFontAssets.length} font{filteredFontAssets.length === 1 ? "" : "s"}</span>
                </div>
                <div className="font-asset-list">
                  {filteredFontAssets.map(font => {
                    const previewFamily = fontPreviewFamilies[font.font_id];
                    const previewStyle: CSSProperties | undefined = previewFamily !== undefined && previewFamily !== ""
                      ? { fontFamily: `"${previewFamily}"` }
                      : undefined;
                    return (
                    <button
                      key={font.font_id}
                      className={assetSelection?.kind === "font" && assetSelection.fontId === font.font_id ? "selected" : ""}
                      type="button"
                      onClick={() => selectAssetRecord({ kind: "font", fontId: font.font_id })}
                    >
                      <span className="font-asset-row-meta">
                        <strong>{font.display_name}</strong>
                        <small>{font.source_format.toUpperCase()} / {font.source_path}</small>
                      </span>
                      <span className="font-asset-preview-line" style={previewStyle}>
                        {previewFamily === undefined ? "Loading preview..." : fontPreviewText}
                      </span>
                    </button>
                  );})}
                </div>
              </section>
            )}
            {!hasTabAssets && (
              <div className="asset-workspace-empty">
                <StudioIcon name={assetTab === "sprite" ? "sprite" : assetTab === "audio" ? "sfx" : "text"} className="studio-ui-icon-empty" />
                <strong>{assetTab === "sprite" ? "No sprites" : assetTab === "audio" ? "No audio assets" : "No fonts"}</strong>
                {assetTab === "font" && <span>Import a custom font once, then generate text sprites from it.</span>}
              </div>
            )}
            {hasTabAssets && !hasMatchingAssets && (
              <div className="asset-workspace-empty">
                <Search size={30} aria-hidden="true" />
                <strong>No matching assets</strong>
                <span>Try another name or clear the current filter.</span>
                <button className="button secondary" type="button" onClick={(event) => {
                  event.stopPropagation();
                  setAssetSearchQuery("");
                  setSpriteAssetFilter("all");
                  setAssetTagFilter("");
                }}>Clear filters</button>
              </div>
            )}
          </div>
            </>
          )}
          </div>
        </div>
      </section>
    );
  };
  const renderSpriteInspector = () => (
    <section className="inspector-section asset-inspector">
      <h3><StudioIcon name="sprite" /> Sprite</h3>
      {selectedAssetFrame === null ? (
        <p className="muted">Select a sprite to inspect its frames.</p>
      ) : (
        (() => {
          const source = sourceFrameById.get(selectedAssetFrame.frame_id);
          const sourceAsset = source?.asset ?? assetById.get(selectedAssetFrame.asset_id) ?? null;
          const textPreviewAsset = isTextSpriteAsset(sourceAsset ?? undefined);
          const editableSystemTextAsset = sourceAsset?.source_format === "system_font_text";
          const bakedTextSource = sourceAsset === null ? null : bakedTextSourceByAssetId.get(sourceAsset.asset_id) ?? null;
          const bakedTextFont = bakedTextSource === null ? null : fontAssets.find(font => font.font_id === bakedTextSource.font_id) ?? null;
          const bakedTextEdit = bakedTextSource === null
            ? null
            : bakedTextEditDraft?.assetId === bakedTextSource.asset_id
              ? bakedTextEditDraft
              : {
                assetId: bakedTextSource.asset_id,
                displayName: sourceAsset?.display_name ?? bakedTextSource.display_name,
                text: bakedTextSource.text,
                fontSize: String(bakedTextSource.font_size_px),
                fontId: bakedTextSource.font_id,
          };
          const selectedAssetFrameIds = selectedAssetFrames.map(frame => frame.frame_id);
          const selectedAnimationFrameCount = selectedAnimationFrames.length;
          const allAssetFramesSelected = selectedAssetFrameIds.length > 0 && selectedAssetFrameIds.every(frameId => combineFrameIds.includes(frameId));
          const selectedAnimationSizes = new Set(selectedAnimationFrames.map(frame => `${frame.width}x${frame.height}`));
          const selectedAnimationSizeLabel = selectedAnimationSizes.size === 0
            ? "None"
            : selectedAnimationSizes.size === 1
              ? [...selectedAnimationSizes][0]
              : `${selectedAnimationSizes.size} sizes`;
          const selectedAnimationAssetCount = new Set(selectedAnimationFrames.map(frame => frame.asset_id)).size;
          return (
            <>
              <div className={`asset-inspector-preview ${textPreviewAsset && selectedAnimationFrames.length === 0 ? "text-asset-preview" : ""}`}>
                <FramePreviewCanvas frame={animatedAssetPreviewFrame ?? selectedAssetFrame} />
                {assetInspectorPreviewFrames.length > 1 && (
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
                      Frame {assetInspectorPreviewIndex + 1}
                      /{assetInspectorPreviewFrames.length}
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
              {sourceAsset !== null && assetTagCapability?.supported === true
                && assetTagCapability.commands.includes("asset.set_tags") && (
                <AssetTagEditor tags={sourceAsset.tags ?? []} disabled={busy !== null}
                  maximumCount={assetTagCapability.maximum_count} maximumLength={assetTagCapability.maximum_length}
                  onApply={tags => updateAssetTags("sprite", sourceAsset.asset_id, tags)} />
              )}
              {canAuthorAnimations && selectedAnimationFrames.length > 0 && (
                <div className="asset-animation-frame-panel">
                  <div className="asset-frame-strip-heading">
                    <strong>Selected frames</strong>
                    <span>{selectedAnimationFrameCount} selected</span>
                  </div>
                  <div className="asset-animation-frame-actions">
                    <button
                      className="button secondary"
                      type="button"
                      disabled={busy !== null || allAssetFramesSelected}
                      onClick={() => setAnimationFrameSelectionGroup(selectedAssetFrameIds, true)}
                    >
                      Select sheet
                    </button>
                    <button
                      className="button secondary"
                      type="button"
                      disabled={busy !== null || selectedAnimationFrameCount === 0}
                      onClick={() => setCombineFrameIds([])}
                    >
                      Clear selection
                    </button>
                    <button
                      className="button primary"
                      type="button"
                      disabled={!combineFrameIds.length || busy !== null}
                      onClick={startAssetAnimation}
                    >
                      Create animation
                    </button>
                  </div>
                  <dl className="asset-animation-selection-summary">
                    <div><dt>Selected</dt><dd>{selectedAnimationFrameCount} frame{selectedAnimationFrameCount === 1 ? "" : "s"}</dd></div>
                    <div><dt>Size</dt><dd>{selectedAnimationSizeLabel}</dd></div>
                    <div><dt>Sources</dt><dd>{selectedAnimationAssetCount}</dd></div>
                  </dl>
                  <div className="asset-animation-selection-strip" aria-label="Selected animation frame order">
                    {selectedAnimationFrames.slice(0, 24).map((frame, index) => (
                      <span key={frame.frame_id}>
                        <FramePreviewCanvas frame={frame} />
                        <small>{index + 1}</small>
                      </span>
                    ))}
                    {selectedAnimationFrames.length > 24 && (
                      <span className="asset-animation-selection-overflow">+{selectedAnimationFrames.length - 24}</span>
                    )}
                  </div>
                </div>
              )}
              {source !== undefined && !textPreviewAsset && (
                <div className="asset-frame-label-panel">
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
              {editableSystemTextAsset && sourceAsset !== null && (
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
              {bakedTextSource !== null && sourceAsset !== null && bakedTextEdit !== null && (
                <div className="baked-text-source-summary baked-text-source-editor">
                  <div>
                    <strong>Baked text source</strong>
                    <span>{bakedTextFont?.display_name ?? bakedTextSource.font_id} / {bakedTextSource.font_size_px}px</span>
                  </div>
                  <label>
                    Text
                    <textarea
                      rows={3}
                      maxLength={512}
                      value={bakedTextEdit.text}
                      disabled={busy !== null}
                      onChange={(event) => setBakedTextEditDraft(current => ({
                        ...(current?.assetId === bakedTextSource.asset_id ? current : bakedTextEdit),
                        text: event.target.value,
                      }))}
                    />
                  </label>
                  <div className="baked-text-source-fields">
                    <label>
                      Font
                      <select
                        value={bakedTextEdit.fontId}
                        disabled={busy !== null || fontAssets.length === 0}
                        onChange={(event) => setBakedTextEditDraft(current => ({
                          ...(current?.assetId === bakedTextSource.asset_id ? current : bakedTextEdit),
                          fontId: event.target.value,
                        }))}
                      >
                        {fontAssets.every(font => font.font_id !== bakedTextEdit.fontId) && (
                          <option value={bakedTextEdit.fontId}>Missing font: {bakedTextEdit.fontId}</option>
                        )}
                        {fontAssets.map(font => (
                          <option key={font.font_id} value={font.font_id}>{font.display_name}</option>
                        ))}
                      </select>
                    </label>
                    <label>
                      Size px
                      <input
                        type="number"
                        min={BAKED_TEXT_MIN_FONT_SIZE}
                        max={BAKED_TEXT_MAX_FONT_SIZE}
                        step={1}
                        value={bakedTextEdit.fontSize}
                        disabled={busy !== null}
                        onChange={(event) => setBakedTextEditDraft(current => ({
                          ...(current?.assetId === bakedTextSource.asset_id ? current : bakedTextEdit),
                          fontSize: event.target.value,
                        }))}
                      />
                    </label>
                    <button
                      className="button primary"
                      type="button"
                      disabled={busy !== null || fontAssets.every(font => font.font_id !== bakedTextEdit.fontId)}
                      title={fontAssets.every(font => font.font_id !== bakedTextEdit.fontId) ? "Import or choose an available font before regenerating" : "Regenerate text sprite"}
                      onClick={() => void regenerateBakedTextSprite(bakedTextSource, sourceAsset, bakedTextEdit)}
                    >
                      Regenerate
                    </button>
                  </div>
                </div>
              )}
              <dl className="inspector-list">
                <div><dt>Asset</dt><dd>{assetDisplayName(selectedAssetFrame.asset_id)}</dd></div>
                {!textPreviewAsset && <div><dt>Frames</dt><dd>{selectedAssetFrames.length}</dd></div>}
                {!textPreviewAsset && <div><dt>Selected</dt><dd>{placementFrameLabel(selectedAssetFrame)}</dd></div>}
                <div><dt>Size</dt><dd>{selectedAssetFrame.width}x{selectedAssetFrame.height}</dd></div>
                <div><dt>Mask</dt><dd>{selectedAssetFrame.opaque ? "Opaque" : "Transparent"}</dd></div>
                <div><dt>Source</dt><dd>{textPreviewAsset ? "Text sprite" : "PNG sprite"}</dd></div>
                {editableSystemTextAsset && <div><dt>Font</dt><dd>{sourceAsset.font_id ?? SYSTEM_FONT_8X8_BASIC_ID}</dd></div>}
                {bakedTextSource !== null && <div><dt>Font</dt><dd>{bakedTextFont?.display_name ?? bakedTextSource.font_id}</dd></div>}
                {bakedTextSource !== null && <div><dt>Generated</dt><dd title={bakedTextSource.source_path}>{bakedTextSource.source_path}</dd></div>}
                <div><dt>Asset ID</dt><dd>{selectedAssetFrame.asset_id}</dd></div>
                {!textPreviewAsset && <div><dt>Frame ID</dt><dd>{selectedAssetFrame.frame_id}</dd></div>}
              </dl>
            </>
          );
        })()
      )}
    </section>
  );
  const renderAudioInspector = () => (
    <section className="inspector-section asset-inspector">
      <h3><StudioIcon name="sfx" /> Sampled SFX</h3>
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
          <AudioCueSettings
            cue={selectedAudioCue}
            disabled={busy !== null || service?.state_scene_audio.cue_commands.includes("audio_cue.upsert") !== true}
            onApply={(nextCue) => void updateAudioCueSettings(nextCue)}
          />
          {selectedAudioAsset !== null && assetTagCapability?.supported === true
            && assetTagCapability.commands.includes("audio_asset.set_tags") && (
            <AssetTagEditor tags={selectedAudioAsset.tags ?? []} disabled={busy !== null}
              maximumCount={assetTagCapability.maximum_count} maximumLength={assetTagCapability.maximum_length}
              onApply={tags => updateAssetTags("audio", selectedAudioAsset.asset_id, tags)} />
          )}
          <div className="audio-inspector-controls">
            {selectedAudioTrim === null ? (
              <AudioWaveform
                projectPath={projectPath}
                sourcePath={selectedAudioAsset?.source_path}
                revision={project?.project_revision}
                progress={audioAuditionProgress?.cueId === selectedAudioCue.cue_id ? audioAuditionProgress.progress : null}
              />
            ) : (
              <>
                <AudioImportWaveform
                  compact
                  peaks={selectedAudioTrim.waveformPeaks}
                  durationMs={selectedAudioTrim.durationMs}
                  startMs={selectedAudioTrim.trimStartMs}
                  endMs={selectedAudioTrim.trimEndMs}
                  progress={audioAuditionProgress?.cueId === selectedAudioCue.cue_id ? audioAuditionProgress.progress : null}
                  onChange={(startMs, endMs) => setSelectedAudioTrim((current) => current === null ? null : {
                    ...current,
                    trimStartMs: Math.max(0, Math.min(current.durationMs - 1, startMs)),
                    trimEndMs: Math.max(startMs + 1, Math.min(current.durationMs, endMs)),
                  })}
                />
                <div className="audio-inspector-trim-controls">
                  <label>Start
                    <span><input type="number" min={0} max={Math.max(0, selectedAudioTrim.trimEndMs - 1)} step={1}
                      value={Math.round(selectedAudioTrim.trimStartMs)} disabled={busy !== null}
                      onChange={(event) => setSelectedAudioTrim((current) => current === null ? null : {
                        ...current,
                        trimStartMs: Math.max(0, Math.min(current.trimEndMs - 1, Number(event.target.value))),
                      })} /> ms</span>
                  </label>
                  <label>End
                    <span><input type="number" min={selectedAudioTrim.trimStartMs + 1} max={selectedAudioTrim.durationMs} step={1}
                      value={Math.round(selectedAudioTrim.trimEndMs)} disabled={busy !== null}
                      onChange={(event) => setSelectedAudioTrim((current) => current === null ? null : {
                        ...current,
                        trimEndMs: Math.max(current.trimStartMs + 1, Math.min(current.durationMs, Number(event.target.value))),
                      })} /> ms</span>
                  </label>
                </div>
                <div className="audio-inspector-trim-actions">
                  <button className="button secondary" type="button" disabled={busy !== null}
                    onClick={() => setSelectedAudioTrim((current) => current === null ? null : {
                      ...current,
                      trimStartMs: current.suggestedTrimStartMs,
                      trimEndMs: current.suggestedTrimEndMs,
                    })}>Trim detected silence</button>
                  <button className="button secondary" type="button"
                    disabled={busy !== null || (selectedAudioTrim.trimStartMs === 0 && selectedAudioTrim.trimEndMs === selectedAudioTrim.durationMs)}
                    onClick={() => setSelectedAudioTrim((current) => current === null ? null : {
                      ...current,
                      trimStartMs: 0,
                      trimEndMs: current.durationMs,
                    })}>Reset</button>
                  <button className="button primary" type="button"
                    disabled={busy !== null
                      || service?.state_scene_audio.asset_commands.includes("audio_asset.upsert") !== true
                      || selectedAudioTrim.trimEndMs - selectedAudioTrim.trimStartMs < 1
                      || (selectedAudioTrim.trimStartMs === selectedAudioTrim.appliedTrimStartMs
                        && selectedAudioTrim.trimEndMs === selectedAudioTrim.appliedTrimEndMs)}
                    onClick={() => void applySelectedAudioTrim()}>Apply trim</button>
                </div>
                <span>{Math.round(selectedAudioTrim.trimEndMs - selectedAudioTrim.trimStartMs)} ms retained of {Math.round(selectedAudioTrim.durationMs)} ms</span>
              </>
            )}
            {selectedAudioTrimError !== null && <span className="error-text">Trim unavailable: {selectedAudioTrimError}</span>}
            <button
              className="button primary"
              type="button"
              disabled={busy !== null || selectedAudioTrim === null}
              onClick={() => void auditionSelectedAudioTrim()}
            >
              <Play size={15} aria-hidden="true" />
              Audition selection
            </button>
            <span>{audioAuditionStatus}</span>
          </div>
          <dl className="inspector-list">
            <div><dt>Cue ID</dt><dd>{selectedAudioCue.cue_id}</dd></div>
            <div><dt>Asset</dt><dd>{selectedAudioCue.asset_ref}</dd></div>
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
  const renderFontInspector = () => (
    <section className="inspector-section asset-inspector">
      <h3><StudioIcon name="text" /> Font</h3>
      {selectedFontAsset === null ? (
        <p className="muted">Import a TTF or OTF font to generate baked text sprites.</p>
      ) : (
        (() => {
          const previewFamily = fontPreviewFamilies[selectedFontAsset.font_id];
          const previewStyle: CSSProperties | undefined = previewFamily !== undefined && previewFamily !== ""
            ? { fontFamily: `"${previewFamily}"` }
            : undefined;
          const bakedTextReferences = bakedTextSources.filter(source => source.font_id === selectedFontAsset.font_id);
          return <>
          <div className="font-inspector-preview">
            <span style={previewStyle}>{previewFamily === undefined ? "Loading preview..." : (preferences.fontPreviewText.trim() || DEFAULT_FONT_PREVIEW_TEXT)}</span>
          </div>
          <div className="asset-name-editor">
            <label>
              Font name
              <input
                key={`${selectedFontAsset.font_id}-${selectedFontAsset.display_name}`}
                type="text"
                maxLength={64}
                defaultValue={selectedFontAsset.display_name}
                disabled={busy !== null}
                onBlur={(event) => {
                  const value = event.currentTarget.value.trim();
                  if (value.length === 0) {
                    event.currentTarget.value = selectedFontAsset.display_name;
                    return;
                  }
                  void renameFontAsset(selectedFontAsset, value);
                }}
                onKeyDown={(event) => {
                  if (event.key === "Enter") {
                    event.preventDefault();
                    event.currentTarget.blur();
                  } else if (event.key === "Escape") {
                    event.preventDefault();
                    event.stopPropagation();
                    event.currentTarget.value = selectedFontAsset.display_name;
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
              disabled={busy !== null || projectPath === null}
              onClick={startBakedTextSprite}
            >
              <StudioIcon name="textSprite" />
              Create text sprite
            </button>
            <button
              className="button secondary"
              type="button"
              disabled={busy !== null || bakedTextReferences.length > 0}
              title={bakedTextReferences.length > 0 ? "Baked text sprites still use this font" : "Remove this font from the project catalog"}
              onClick={() => void deleteFontAsset(selectedFontAsset)}
            >
              <Trash2 size={15} aria-hidden="true" />
              Delete font
            </button>
          </div>
          <dl className="inspector-list">
            <div><dt>Name</dt><dd>{selectedFontAsset.display_name}</dd></div>
            <div><dt>Format</dt><dd>{selectedFontAsset.source_format.toUpperCase()}</dd></div>
            <div><dt>Source</dt><dd title={selectedFontAsset.source_path}>{selectedFontAsset.source_path}</dd></div>
            <div><dt>Used by</dt><dd>{bakedTextReferences.length} text sprite{bakedTextReferences.length === 1 ? "" : "s"}</dd></div>
            <div><dt>Font ID</dt><dd>{selectedFontAsset.font_id}</dd></div>
          </dl>
        </>;
        })()
      )}
    </section>
  );
  const renderAssetInspector = () => {
    if (assetSelection?.kind === "animation" || assetSelection?.kind === "animation-draft") {
      const creating = assetSelection.kind === "animation-draft";
      const clip = assetSelection.kind === "animation-draft" ? assetSelection.clip : animationClips.find(item => item.animation_id === assetSelection.clipId);
      return <section className="inspector-section asset-inspector"><h3>Animation</h3>{clip && <AnimationClipEditor
        key={`${creating}:${JSON.stringify(clip)}`} clip={clip} frames={compiledAssetFrames} assets={animationEditorAssets} scenes={scenes}
        displayName={creating ? clip.display_name ?? "New animation" : animationLabel(clip)} creating={creating} initiallyOpen loopPolicies={animationPolicies}
        disabled={!canAuthorAnimations || busy !== null} onCancel={() => setAssetSelection(null)}
        onDuplicate={!creating && canUpsertAnimations ? () => duplicateAnimationClip(clip) : undefined}
        onDelete={!creating && canDeleteAnimations ? () => deleteAnimationClip(clip) : undefined}
        onApply={async commands => {
          const applied = await applySceneObjectCommands(commands);
          if (applied) { setCombineFrameIds([]); selectAssetRecord({kind:"animation",clipId:clip.animation_id}); }
          return applied;
        }} />}</section>;
    }
    if (assetSelection?.kind === "sprite") {
      return renderSpriteInspector();
    }
    if (assetSelection?.kind === "audio") {
      return renderAudioInspector();
    }
    if (assetSelection?.kind === "font") {
      return renderFontInspector();
    }
    return (
      <section className="inspector-section asset-inspector">
        <h3>{assetTab === "sprite" ? <StudioIcon name="sprite" /> : assetTab === "audio" ? <StudioIcon name="sfx" /> : <StudioIcon name="text" />} {assetTab === "sprite" ? "Sprite" : assetTab === "audio" ? "Audio" : "Font"}</h3>
      </section>
    );
  };
  const renderPlacementViewSettings = () => (
    <section className="inspector-section placement-view-settings-section">
      <h3>Placement</h3>
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
      case "text":
        return "Text";
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
      case "filled_circle":
        return "Filled circle";
      case "filled_ellipse":
        return "Filled ellipse";
      default:
        return kind.replaceAll("_", " ");
    }
  };
  const placementKindIcon = (kind: string) => {
    return <StudioIcon name={placementKindIconName(kind)} />;
  };
  const placementKindIconName = (kind: string): StudioIconName => {
    switch (kind) {
      case "sprite":
        return "sprite";
      case "text":
        return "text";
      case "line":
        return "line";
      case "outline_rect":
      case "filled_rect":
        return "rectangle";
      case "circle":
      case "ellipse":
      case "filled_circle":
      case "filled_ellipse":
        return "circle";
      default:
        return "rectangle";
    }
  };
  const placementElementIconName = (element: RenderElement): StudioIconName => {
    if (element.kind === "sprite") {
      const frame = element.visual_ref === undefined
        ? null
        : compiledAssetFrameById.get(element.visual_ref) ?? null;
      const asset = frame === null ? undefined : assetById.get(frame.asset_id);
      return isTextSpriteAsset(asset) ? "textSprite" : "sprite";
    }
    return placementKindIconName(element.kind);
  };
  const placementElementIcon = (element: RenderElement) => {
    return <StudioIcon name={placementElementIconName(element)} />;
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
  const placementObjectDisplayName = (element: RenderElement) => {
    const displayName = placementOwnershipScene?.objects
      ?.find((object) => object.object_id === element.element_id)?.display_name?.trim();
    return displayName || placementObjectLabelBase(element);
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
  const addSceneObject = async (element: RenderElement, animationId?: string) => {
    if (selectedSceneDocument === null) return false;
    const { element_id, x, y, visible, visual_ref, ...geometry } = element;
    const stateIds = placementEditStateTargets();
    const applied = await applySceneObjectCommands([{ kind: "object.add", scene_id: selectedSceneDocument.scene_id,
      object: { ...geometry, object_id: element_id,
        defaults: { x, y, visible: visible !== false, ...(visual_ref === undefined ? {} : { visual_ref }) } },
      ...(stateIds.length === 0 ? {} : { visible_in_states: stateIds }) },
      ...(animationId ? [{kind:"object.bind_animation",scene_id:selectedSceneDocument.scene_id,object_id:element_id,animation_ref:animationId}] : [])]);
    if (applied) {
      setSelectedPlacementElement(element_id);
    }
    return applied;
  };
  const addPlacementText = async (requestedBounds: PlacementBounds) => {
    if (!runtimeTextSupported || runtimeTextProfile === undefined || selectedSceneDocument === null) return;
    const minimumWidth = runtimeTextProfile.glyph_cell.width * runtimeTextProfile.scale.minimum * 4;
    const minimumHeight = runtimeTextProfile.glyph_cell.height * runtimeTextProfile.scale.minimum;
    const width = Math.min(PLACEMENT_WIDTH, Math.max(minimumWidth, requestedBounds.width));
    const height = Math.min(PLACEMENT_HEIGHT, Math.max(minimumHeight, requestedBounds.height));
    const x = Math.min(requestedBounds.x, PLACEMENT_WIDTH - width);
    const y = Math.min(requestedBounds.y, PLACEMENT_HEIGHT - height);
    const elements = baseObjectRows(selectedSceneDocument, placementOwnershipScene);
    await addSceneObject({
      element_id: nextPlacementElementId("text", elements), kind: "text",
      text: "Text", font_id: runtimeTextProfile.font_ids[0], scale: runtimeTextProfile.scale.minimum,
      alignment: "left", x, y, width, height,
      z_order: Math.min(255, Math.max(0, ...elements.map(item => item.z_order)) + 1),
      layer: "UI", visible: true,
    });
    setPlacementTool("select");
  };
  const addPlacementSprite = async (frame: CompiledAssetFrame | null, animationId?: string) => {
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
        layer: "SCENE", visible: true }, animationId);
      if (added) closePlacementAssetPicker();
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
      closePlacementAssetPicker();
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
  };
  const selectPlacementElement = (elementId: string) => {
    setSelectedPlacementElement(elementId);
    if (placementRenderModel !== null) {
      setSceneSelection({ kind: "render", id: placementRenderModel.visual_id });
    } else if (objectSceneSelected) {
      setSceneSelection(placementStateId === null
        ? { kind: "scene" }
        : { kind: "state", id: placementStateId });
    }
  };
  const selectPlacementBase = (elementId?: string) => {
    placementSelectionAnchorRef.current = null;
    setPlacementEditStateIds([]);
    setPlacementStateId(null);
    if (elementId !== undefined) {
      setSelectedPlacementElement(elementId);
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
    setSceneSelection({ kind: "state", id: stateId });
  };
  const handlePlacementHierarchyKeyDown = (event: ReactKeyboardEvent<HTMLDivElement>) => {
    if (objectSceneSelected) return;
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
        <h3><SquareMousePointer size={14} aria-hidden="true" /> Editing</h3>
        {selectedSceneDocument === null ? (
          <p className="muted">Select a scene to choose where placement edits apply.</p>
        ) : (
          <div className="placement-edit-scope">
            {!objectSceneSelected && <>
            <div className="placement-scope-header">
              <span>{targetStates.length === 0 ? <StudioIcon name="scene" /> : <StudioIcon name="state" />}</span>
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
            </>}
            {objectSceneSelected && <select aria-label="Editing" value={placementStateId ?? ""}
                onChange={event => void openHierarchyPlacementTarget(selectedSceneDocument, event.target.value || null, selectedPlacementElement ?? undefined)}>
                <option value="">Scene defaults</option>
                {(selectedSceneDocument.states ?? []).map(state => <option key={state.state_id} value={state.state_id}>{state.display_name}</option>)}
              </select>}
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
    if (usesSceneObjects(scene)) {
      setSelectedScene(scene.scene_id);
    } else if (selectedScene !== scene.scene_id) {
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
  const openHierarchyStateLogic = (scene: SceneDocument, state: StateRecord) => {
    setWorkspaceMode("logic");
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
  const openHierarchyTimer = (scene: SceneDocument, bindingId: string) => {
    setWorkspaceMode("logic");
    setSelectedScene(scene.scene_id);
    setSelectedPlacementElement(null);
    const binding = scene.event_bindings?.find((item) => item.binding_id === bindingId);
    const route = binding?.event_type === STATE_TIMER
      ? scene.routes?.find((item) => item.event_ref === bindingId)
      : undefined;
    setSceneSelection(route === undefined
      ? { kind: "timer", id: bindingId }
      : { kind: "route", id: route.route_id, sourceState: route.from_states[0] });
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
        const authoredObjectNames = new Map((ownership?.objects ?? []).map(object => [object.object_id, object.display_name]));
        for (const element of elements) {
          const authoredName = authoredObjectNames.get(element.element_id);
          if (authoredName) {
            objectLabelById.set(element.element_id, authoredName);
            continue;
          }
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
          onDoubleClick?: () => void,
        ) => (
          <button
            key={key}
            className={`placement-tree-object hierarchy-object-row ${selected ? "selected" : ""}`}
            type="button"
            title={`${objectLabelById.get(element.element_id) ?? placementObjectLabelBase(element)} · ${placementKindLabel(element.kind)}${badges.length ? ` · ${badges.join(", ")}` : ""}`}
            onClick={onClick}
            onDoubleClick={onDoubleClick}
          >
            <span className="placement-object-kind">
              {placementElementIcon(element)}
              <span>
                <strong>{objectLabelById.get(element.element_id) ?? placementObjectLabelBase(element)}</strong>
              </span>
            </span>
          </button>
        );
        const sceneSelected = sceneSelection.kind !== "project" && selectedScene === scene.scene_id;
        const expanded = expandedSceneIds.includes(scene.scene_id);
        const baseGroupId = `${scene.scene_id}:base`;
        const statesGroupId = `${scene.scene_id}:states`;
        const timersGroupId = `${scene.scene_id}:timers`;
        const variablesGroupId = `${scene.scene_id}:variables`;
        const baseExpanded = !collapsedHierarchyIds.includes(baseGroupId);
        const statesExpanded = !collapsedHierarchyIds.includes(statesGroupId);
        const timersExpanded = !collapsedHierarchyIds.includes(timersGroupId);
        const variablesExpanded = !collapsedHierarchyIds.includes(variablesGroupId);
        const timers = (scene.event_bindings ?? []).filter((binding) => (
          binding.event_type === SCENE_TIMER || binding.event_type === STATE_TIMER
        ));
        const timerSourceStateIds = (bindingId: string) => Array.from(new Set((scene.routes ?? [])
          .filter((route) => route.event_ref === bindingId)
          .flatMap((route) => route.from_states)));
        const sceneTimers = timers.filter((binding) => binding.event_type === SCENE_TIMER);
        const unassignedStateTimers = timers.filter((binding) => (
          binding.event_type === STATE_TIMER && timerSourceStateIds(binding.binding_id).length === 0
        ));
        const sceneLevelTimers = [...sceneTimers, ...unassignedStateTimers];
        const selectedTimerBindingId = sceneSelection.kind === "timer"
          ? sceneSelection.id
          : sceneSelection.kind === "route"
            ? scene.routes?.find((route) => route.route_id === sceneSelection.id)?.event_ref ?? null
            : null;
        const timerDelayLabel = (binding: (typeof timers)[number]) => (
          typeof binding.configuration.delay_ms === "number"
            ? `${binding.configuration.delay_ms} ms`
            : "Delay unset"
        );
        const renderHierarchyTimerRow = (binding: (typeof timers)[number], label: string) => {
          const selected = sceneSelected
            && workspaceMode === "logic"
            && selectedTimerBindingId === binding.binding_id;
          return (
            <button
              className={`scene-reference-row hierarchy-timer-row ${selected ? "selected" : ""}`}
              key={binding.binding_id}
              type="button"
              title={`${label} - ${timerDelayLabel(binding)}`}
              onClick={() => openHierarchyTimer(scene, binding.binding_id)}
            >
              <img
                className="studio-ui-icon"
                src={binding.event_type === SCENE_TIMER ? "/ui-icons/scene_timer.png" : "/ui-icons/state_timer.png"}
                alt=""
                aria-hidden="true"
              />
              <span><strong>{label} - {timerDelayLabel(binding)}</strong></span>
            </button>
          );
        };
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
                <StudioIcon name="scene" />
                <span>
                  <strong>{scene.display_name}</strong>
                </span>
              </button>
            </div>
            {expanded && <div className="scene-hierarchy-children">
              {usesSceneObjects(scene) ? elements.map(element => {
                const overrides = (scene.states ?? []).flatMap(state => {
                  const projection = ownership?.states[state.state_id];
                  const properties = projection?.changes[element.element_id]?.local_properties ?? [];
                  if (!properties.length) return [];
                  const resolved = projection?.resolved_elements.find(item => item.element_id === element.element_id);
                  const description = properties.map(property => {
                    if (property === "x" || property === "y") return `${property.toUpperCase()}: ${resolved?.[property] ?? "?"}`;
                    if (property === "visible") return resolved?.visible ? "Visible" : "Hidden";
                    if (property === "visual_ref") return "Frame override";
                    return property;
                  }).join(" / ");
                  return [{ state, description }];
                });
                const groupId = JSON.stringify([scene.scene_id, "object", element.element_id]);
                const objectExpanded = !collapsedHierarchyIds.includes(groupId);
                const label = objectLabelById.get(element.element_id) ?? placementObjectLabelBase(element);
                return <section className="native-object-branch" key={element.element_id} data-object-id={element.element_id}>
                  <div className="native-object-row">
                    {overrides.length > 0 ? <button className="hierarchy-disclosure-control" type="button"
                      aria-expanded={objectExpanded} aria-label={`${objectExpanded ? "Collapse" : "Expand"} ${label} overrides`}
                      title={`${objectExpanded ? "Collapse" : "Expand"} ${label} overrides`}
                      onClick={() => toggleHierarchyGroup(groupId)}>
                      <ChevronRight className={objectExpanded ? "expanded" : ""} size={14} aria-hidden="true" />
                    </button> : <span className="native-object-disclosure-spacer" />}
                    {renderObjectRow(element, element.element_id,
                      () => void openHierarchyPlacementTarget(scene, null, element.element_id),
                      [...(element.visible === false ? ["Hidden"] : []), `z${element.z_order}`],
                      sceneSelected && workspaceMode === "placement" && placementEditStateIds.length === 0 && selectedPlacementElement === element.element_id,
                      overrides.length ? () => toggleHierarchyGroup(groupId) : undefined)}
                  </div>
                  {objectExpanded && overrides.length > 0 && <div className="native-object-overrides">
                    {overrides.map(({ state, description }) => {
                      const active = preview?.scene.scene_id === scene.scene_id && preview.scene.state_id === state.state_id;
                      const selected = sceneSelected && workspaceMode === "placement" && placementStateId === state.state_id && selectedPlacementElement === element.element_id;
                      return (
                        <div className="native-object-override-row" key={state.state_id} data-state-id={state.state_id}>
                          <button
                            className="hierarchy-state-logic-button"
                            type="button"
                            title={`Open ${state.display_name} logic`}
                            aria-label={`Open ${state.display_name} logic`}
                            onClick={() => openHierarchyStateLogic(scene, state)}
                          >
                            <StudioIcon name="state" />
                          </button>
                          <button
                            type="button"
                            className={`hierarchy-branch-select native-object-override-select ${selected ? "selected" : ""}`}
                            title={`${state.display_name} · ${description}`}
                            onClick={() => void openHierarchyPlacementTarget(scene, state.state_id, element.element_id)}
                          >
                            <span><strong>{state.display_name}</strong></span>
                          </button>
                          <button
                            className={`hierarchy-emulator-load ${active ? "active" : ""}`}
                            type="button"
                            disabled={!projectValid || busy !== null}
                            aria-label={active ? `${state.display_name} is active in emulator` : `Load ${state.display_name} in emulator`}
                            title={active ? `${state.display_name} is active in emulator` : `Load ${state.display_name} in emulator`}
                            onClick={() => void startPreview(scene.scene_id, {
                              stateId: state.state_id,
                              updateSelection: false,
                            })}
                          >
                            <img src={TOPBAR_ICONS.emulator} alt="" aria-hidden="true" />
                          </button>
                        </div>
                      );
                    })}
                  </div>}
                </section>;
              }) : <>
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
                    <StudioIcon name="scene" />
                    <span>
                      <strong>Base objects</strong>
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
                    <StudioIcon name="state" />
                    <span>
                      <strong>States</strong>
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
                    const stateTimers = timers.filter((binding) => (
                      binding.event_type === STATE_TIMER
                      && timerSourceStateIds(binding.binding_id).includes(state.state_id)
                    ));
                    const stateChildCount = changedElements.length + stateTimers.length;
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
                            title={[
                              state.display_name,
                              active ? "Emulator active" : null,
                              primary ? "Placement preview" : null,
                              state.state_id,
                            ].filter(Boolean).join(" · ")}
                            onClick={(event) => { if (event.detail < 2) openHierarchyState(scene, state, event); }}
                            onDoubleClick={() => toggleHierarchyGroup(stateGroupId)}
                          >
                            <span className="hierarchy-state-icon">
                              <StudioIcon name="state" />
                              {active && (
                                <img className="hierarchy-runtime-indicator" src={TOPBAR_ICONS.emulator} title="Emulator active" alt="Emulator active" />
                              )}
                            </span>
                            <span>
                              <strong>{state.display_name}</strong>
                            </span>
                            <span className="hierarchy-state-meta">
                              {primary && (
                                <span className="hierarchy-placement-indicator" title="Placement preview" aria-label="Placement preview">
                                  <Eye size={13} aria-hidden="true" />
                                </span>
                              )}
                              <code>{stateChildCount}</code>
                            </span>
                          </button>
                          <button
                            className={`hierarchy-emulator-load ${active ? "active" : ""}`}
                            type="button"
                            disabled={!projectValid || busy !== null}
                            aria-label={active ? `${state.display_name} is active in emulator` : `Load ${state.display_name} in emulator`}
                            title={active ? `${state.display_name} is active in emulator` : `Load ${state.display_name} in emulator`}
                            onClick={() => void startPreview(scene.scene_id, {
                              stateId: state.state_id,
                              updateSelection: false,
                            })}
                          >
                            <img src={TOPBAR_ICONS.emulator} alt="" aria-hidden="true" />
                          </button>
                        </div>
                        {stateExpanded && stateChildCount > 0 && (
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
                            {stateTimers.map((binding) => renderHierarchyTimerRow(binding, "State timer"))}
                          </div>
                        )}
                      </section>
                    );
                  })}
                </div>}
              </section>
              </>}
              {sceneLevelTimers.length > 0 && (
                <section className="hierarchy-branch timers-branch">
                  <div className="hierarchy-branch-row">
                    <button
                      className="hierarchy-disclosure-control"
                      type="button"
                      aria-expanded={timersExpanded}
                      aria-label={`${timersExpanded ? "Collapse" : "Expand"} Timers`}
                      title={`${timersExpanded ? "Collapse" : "Expand"} Timers`}
                      onClick={() => toggleHierarchyGroup(timersGroupId)}
                    >
                      <ChevronRight className={timersExpanded ? "expanded" : ""} size={14} aria-hidden="true" />
                    </button>
                    <button
                      className="hierarchy-branch-select"
                      type="button"
                      onClick={() => selectHierarchyScene(scene.scene_id)}
                      onDoubleClick={() => toggleHierarchyGroup(timersGroupId)}
                    >
                      <img className="studio-ui-icon" src="/ui-icons/scene_timer.png" alt="" aria-hidden="true" />
                      <span><strong>Scene timers</strong></span>
                      <code>{sceneLevelTimers.length}</code>
                    </button>
                  </div>
                  {timersExpanded && <div className="hierarchy-reference-rows">
                    {sceneLevelTimers.map((binding) => renderHierarchyTimerRow(
                      binding,
                      binding.event_type === SCENE_TIMER ? "Scene timer" : "Unassigned state timer",
                    ))}
                  </div>}
                </section>
              )}
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
                      <StudioIcon name="text" />
                      <span>
                        <strong>Variables</strong>
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
                        <StudioIcon name="text" />
                        <span>
                          <strong>{variable.variable_id}</strong>
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
        label={object?.display_name ?? (selectedElement === null ? "" : placementObjectLabelBase(selectedElement))}
        stateIds={placementEditStateTargets()} ownership={placementOwnershipScene}
        frames={compiledAssetFrames} clips={project?.document?.animations ?? []} busy={busy !== null}
        assets={assets}
        runtimeTextProfile={selectedSceneCapability?.runtime_text_profile ?? null}
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
    const setElementFilled = (filled: boolean) => {
      if (selectedSceneDocument === null || selectedElement === null || placementRenderModel === null || targetStateIds.length > 0) {
        return;
      }
      const elementKind = shapeKindWithFill(selectedElement.kind, filled);
      if (elementKind === null || service?.state_scene_presentation.element_commands.includes("render_element.set_kind") !== true) {
        return;
      }
      void applyRenderElementCommand(
        "Changing fill",
        selectedSceneDocument.scene_id,
        placementRenderModel.visual_id,
        selectedElement.element_id,
        { kind: "render_element.set_kind", element_kind: elementKind },
        "Fill updated. Save to write it to the project.",
      );
    };
    return (
      <>
        {renderPlacementEditScope()}
        <section className="inspector-section placement-inspector">
        <h3>{selectedElement === null ? <StudioIcon name="rectangle" /> : placementElementIcon(selectedElement)} Object</h3>
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
              {isFillableShapeKind(selectedElement.kind) && (
                <label className="placement-toggle-row" title={service?.state_scene_presentation.element_commands.includes("render_element.set_kind") === true
                  ? "Fill this shape"
                  : "Fill editing is not available in this project version"}>
                  <input
                    type="checkbox"
                    checked={selectedElement.kind === "filled_rect" || selectedElement.kind === "filled_circle" || selectedElement.kind === "filled_ellipse"}
                    disabled={busy !== null || service?.state_scene_presentation.element_commands.includes("render_element.set_kind") !== true}
                    onChange={(event) => setElementFilled(event.target.checked)}
                  />
                  Filled
                </label>
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
      {project !== null && (
        <section className="inspector-section project-settings-inspector">
          <h3>Current SFX import</h3>
          <div className="project-settings-group">
            <div className="project-settings-group-heading">
              <strong>Studio processing</strong>
              <span>Applied to WAV imports during this Studio session. This is separate from the saved project preference.</span>
            </div>
            <label className="toggle-field">
              <input
                type="checkbox"
                checked={normalizeAudioImports}
                disabled={busy !== null}
                onChange={(event) => setNormalizeAudioImports(event.target.checked)}
              />
              <span>Normalize imported SFX</span>
            </label>
            <label className="select-field">
              <span>SFX peak level</span>
              <select
                value={audioImportPeakDbfs}
                disabled={!normalizeAudioImports || busy !== null}
                onChange={(event) => setAudioImportPeakDbfs(Number(event.target.value))}
              >
                <option value={-6}>-6 dBFS (HW6 default)</option>
                <option value={-9}>-9 dBFS</option>
                <option value={-12}>-12 dBFS</option>
              </select>
            </label>
            {!normalizeAudioImports && <p className="audio-import-warning">Unnormalized files may play unexpectedly loud.</p>}
          </div>
        </section>
      )}
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
            {build.package.container_version !== undefined && <div><dt>Container</dt><dd>V{build.package.container_version}</dd></div>}
            {build.package.export_profile_id && <div><dt>Profile</dt><dd>{build.package.export_profile_id}</dd></div>}
          </dl>
          <code>{build.package.sha256.slice(0, 16)}...</code>
        </section>
      )}
    </>
  );
  const workspaceDetail = workspaceMode === "placement"
    ? selectedSceneDocument === null
      ? "Placement / no scene selected"
      : `Placement / ${selectedSceneDocument.display_name} / ${placementState?.display_name ?? "Scene default"}`
    : workspaceMode === "logic"
      ? `Logic / ${selectedSceneDocument?.display_name ?? "No scene selected"}`
      : workspaceMode === "assets"
        ? `Assets / ${assetTab === "sprite" ? "Sprites" : assetTab === "audio" ? "Audio" : "Fonts"}`
        : `Scene flow / ${project?.summary.project_name ?? "No project open"}`;
  const openMenuDisabled = bridge === undefined || busy !== null;

  return (
    <main
      className="studio-shell"
      data-theme={resolvedTheme}
      style={{
        "--sprite-preview-background": preferences.spritePreviewBackground,
        "--studio-chrome": preferences.chromeColor,
        "--studio-chrome-text": chromeTextColor,
        "--project-width": `${projectWidth}px`,
        "--inspector-width": `${inspectorWidth}px`,
      } as CSSProperties}
    >
      <header className="app-toolbar">
        <div className="topbar-project-panel">
          <button
            className={`topbar-button topbar-emulator-button ${emulatorDockVisible || emulatorPoppedOut ? "active" : ""}`}
            type="button"
            title={emulatorDockVisible || emulatorPoppedOut ? "Hide emulator" : "Show emulator"}
            aria-label={emulatorDockVisible || emulatorPoppedOut ? "Hide emulator" : "Show emulator"}
            aria-pressed={emulatorDockVisible || emulatorPoppedOut}
            onPointerDown={handleEmulatorIconPointerDown}
            onPointerMove={handleEmulatorIconPointerMove}
            onPointerUp={handleEmulatorIconPointerEnd}
            onPointerCancel={handleEmulatorIconPointerEnd}
            onClick={handleEmulatorToggleClick}
          >
            <img
              className="topbar-icon"
              src={emulatorDockVisible || emulatorPoppedOut ? TOPBAR_ICONS.emulatorOpen : TOPBAR_ICONS.emulatorSleep}
              alt=""
              aria-hidden="true"
              draggable={false}
            />
            <span className="sr-only">Emulator</span>
          </button>
          <div className="topbar-project-title">
            <h1>{project?.summary.project_name ?? "Peep Studio"}</h1>
            <p>{workspaceDetail}</p>
          </div>
          <span className={`service-state topbar-service ${connected ? "connected" : "disconnected"}`}>
            <MonitorDot size={15} aria-hidden="true" />
            {connected ? `API ${service.service_api_version}` : "Offline"}
          </span>
        </div>

        <div className="toolbar-actions topbar-file-actions" aria-label="Project actions">
          <button className="topbar-button" onClick={newProject} disabled={bridge === undefined || busy !== null || service?.operations.includes("project.create") !== true} title="New project" aria-label="New project">
            <img className="topbar-icon" src={TOPBAR_ICONS.newProject} alt="" aria-hidden="true" />
            <span className="sr-only">New project</span>
          </button>
          <details className={`topbar-dropdown ${openMenuDisabled ? "disabled" : ""}`}>
            <summary
              className="topbar-button"
              aria-label="Open"
              onClick={(event) => {
                if (openMenuDisabled) event.preventDefault();
              }}
              onKeyDown={(event) => {
                if (openMenuDisabled && (event.key === "Enter" || event.key === " ")) event.preventDefault();
              }}
              title="Open"
            >
              <img className="topbar-icon" src={TOPBAR_ICONS.open} alt="" aria-hidden="true" />
              <span className="sr-only">Open</span>
            </summary>
            <div className="topbar-dropdown-menu">
              <button type="button" onClick={(event) => { (event.currentTarget.closest("details") as HTMLDetailsElement | null)?.removeAttribute("open"); void openExample(); }}>
                <FileCode2 size={14} aria-hidden="true" />
                Example
              </button>
              <button type="button" onClick={(event) => { (event.currentTarget.closest("details") as HTMLDetailsElement | null)?.removeAttribute("open"); void openProject(); }}>
                <FolderOpen size={14} aria-hidden="true" />
                Project
              </button>
            </div>
          </details>
          <button className="topbar-button" onClick={saveProject} disabled={!dirty || project === null || busy !== null || service?.operations.includes("project.save") !== true} title="Save" aria-label="Save">
            <img className="topbar-icon" src={TOPBAR_ICONS.save} alt="" aria-hidden="true" />
            <span className="sr-only">Save</span>
          </button>
          <button className="topbar-button" onClick={saveProjectAs} disabled={project === null || projectPath === null || busy !== null || service?.operations.includes("project.save") !== true} title="Save as" aria-label="Save as">
            <img className="topbar-icon" src={TOPBAR_ICONS.saveAs} alt="" aria-hidden="true" />
            <span className="sr-only">Save as</span>
          </button>
          <button className="topbar-button primary" onClick={buildPackage} disabled={!buildReady || busy !== null} title="Build" aria-label="Build">
            <img className="topbar-icon" src={TOPBAR_ICONS.build} alt="" aria-hidden="true" />
            <span className="sr-only">Build</span>
          </button>
          <button className="topbar-button" onClick={exportPackage} disabled={build === null || busy !== null || !buildReady || build.project_revision !== project?.project_revision} title="Export .egg" aria-label="Export .egg">
            <img className="topbar-icon" src={TOPBAR_ICONS.egg} alt="" aria-hidden="true" />
            <span className="sr-only">Export .egg</span>
          </button>
        </div>

        <div className="topbar-studio-brand" aria-label="Peep Studio">
          <div className="brand-logo-lockup">
            <img className="brand-icon-image" src={TOPBAR_ICONS.studioIcon} alt="" aria-hidden="true" />
            <img className="brand-name-logo" src={TOPBAR_ICONS.studioName} alt="Peep Studio" />
          </div>
        </div>

        <div className="toolbar-actions topbar-workspace-actions" aria-label="Workspace actions">
          {renderModeTabs()}
        </div>
        <div className="topbar-system-actions">
          <div className="topbar-history" aria-label="Edit history">
            <button className="icon-button" onClick={() => void stepHistory("project.undo")} disabled={!canUndo || project === null || busy !== null || service?.operations.includes("project.undo") !== true} title="Undo" aria-label="Undo">
              <img className="topbar-history-icon" src={TOPBAR_ICONS.undo} alt="" aria-hidden="true" />
            </button>
            <button className="icon-button" onClick={() => void stepHistory("project.redo")} disabled={!canRedo || project === null || busy !== null || service?.operations.includes("project.redo") !== true} title="Redo" aria-label="Redo">
              <img className="topbar-history-icon" src={TOPBAR_ICONS.redo} alt="" aria-hidden="true" />
            </button>
          </div>
          <button className={`topbar-button ${settingsOpen ? "active" : ""}`} type="button" title="Settings" aria-label="Settings"
            aria-pressed={settingsOpen} aria-controls="studio-inspector"
            onClick={() => setSettingsOpen(current => !current)}>
            <img className="topbar-icon" src={TOPBAR_ICONS.settings} alt="" aria-hidden="true" />
            <span className="sr-only">Settings</span>
          </button>
          {bridge?.windowControl !== undefined && (
            <div className="window-controls" aria-label="Window controls">
              <button type="button" className="window-control-button" title="Minimize" aria-label="Minimize"
                onClick={() => void bridge.windowControl?.("minimize")}>
                <Minus size={16} aria-hidden="true" />
              </button>
              <button type="button" className="window-control-button" title="Maximize or restore" aria-label="Maximize or restore"
                onClick={() => void bridge.windowControl?.("maximize")}>
                <Maximize2 size={15} aria-hidden="true" />
              </button>
              <button type="button" className="window-control-button close" title="Close" aria-label="Close"
                onClick={() => void bridge.windowControl?.("close")}>
                <X size={17} aria-hidden="true" />
              </button>
            </div>
          )}
        </div>
      </header>

      {hostOnlyProject && <div className="host-preview-notice" role="status">
        <span>V2 export is blocked for this project. Draft editing and supported preview remain available.</span>
        {(project?.build_issues ?? []).map((issue, index) => <div key={`${issue.code}-${index}`}><strong>{issue.code}</strong>: {issue.message}</div>)}
        <button type="button" className="button secondary" onClick={() => { setSettingsOpen(false); selectProjectRoot(); }}>Project validation</button>
      </div>}

      <section
        className={`workspace-grid ${workspaceMode === "placement" ? "placement-mode" : workspaceMode === "scene-flow" ? "scene-flow-mode" : workspaceMode === "assets" ? "assets-mode" : "logic-mode"}`}
        style={{
          "--project-width": `${projectWidth}px`,
          "--inspector-width": `${inspectorWidth}px`,
        } as CSSProperties}
      >
        <aside className="project-pane">
          {renderDockedEmulator()}

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
            {renderPreviewPanel("placement")}
          </section>
        )}

        {workspaceMode === "scene-flow" && (
        <section className="scene-flow-pane">
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
              onSelectBackground={clearSceneFlowSelection}
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
              canEdit={service?.operations.includes("project.apply_commands") === true && busy === null && scenes.every(sceneConnectionsEditable)}
              canAddScene={busy === null && (project?.document?.scenes?.find(scene => scene.scene_id === project.summary.entry_scene)?.schema_version === 2
                ? supportsNativeCreation(service) : service?.state_scene_graph.scene_commands?.includes("scene.add") === true)}
              readOnlySceneIds={readOnlySceneIds}
            />
          </div>
        </section>
        )}

        {workspaceMode === "logic" && (
        <section className="state-graph-pane">
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
                setSceneSelection({ kind: "timerDraft", eventType, stateId: eventType === STATE_TIMER ? stateId : undefined });
              }}
              onSelect={setSceneSelection}
              onSelectBackground={clearLogicSelection}
              onCreateState={(sceneId, x, y) => {
                void createState(sceneId, x, y);
              }}
              onDeleteState={(sceneId, stateId) => {
                void deleteState(sceneId, stateId);
              }}
              onMoveStateNode={(sceneId, stateId, x, y) => {
                void moveStateNode(sceneId, stateId, x, y);
              }}
              onSetHandlerLayout={(sceneId, handlerId, layout) => {
                void setTimerHandlerLayout(sceneId, handlerId, layout);
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
              canEditTimerHandlers={service?.state_scene_graph.scene_timers?.editor_layout?.supported === true
                && service.state_scene_graph.scene_timers.editor_layout.commands.includes("editor.state_graph.set_handler_layout")
                && selectedSceneCapability?.timer_handler_layout?.supported === true
                && selectedSceneCapability.timer_handler_layout.commands.includes("editor.state_graph.set_handler_layout")
                && localCommandAllowed("editor.state_graph.set_handler_layout")}
              canDeleteStates={stateCommandAllowed("state.delete")}
              canEditEntry={stateCommandAllowed("state.set_entry") && stateCommandAllowed("editor.state_graph.set_entry_layout")}
              canEdit={canEditLocalGraph}
              canConnectScenes={canConnectSelectedScene}
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

        <ObjectActionContext.Provider value={{ scene: selectedSceneDocument,
          preview: preview?.project_revision === projectRevision ? preview : null,
          label: placementObjectLabelBase }}>
        <aside className="inspector-pane" id="studio-inspector">
          <div className="pane-heading inspector-heading">
            <span>{settingsOpen ? "Settings" : "Inspector"}</span>
            {settingsOpen && (
              <button className="icon-button" type="button" title="Close settings" aria-label="Close settings"
                onClick={() => setSettingsOpen(false)}><X size={16} aria-hidden="true" /></button>
            )}
          </div>
          {settingsOpen ? <>
            <section className="inspector-section">
              <h3>Appearance</h3>
              <div className="placement-view-settings">
                <label>Theme
                  <select aria-label="Theme" value={preferences.theme}
                    onChange={event => updatePreference("theme", event.target.value as "light" | "dark" | "system")}>
                    <option value="light">Light</option>
                    <option value="dark">Dark</option>
                    <option value="system">System</option>
                  </select>
                </label>
                <label>Chrome colour
                  <input
                    type="color"
                    aria-label="Chrome colour"
                    value={preferences.chromeColor}
                    onChange={event => updatePreference("chromeColor", event.target.value)}
                  />
                </label>
              </div>
            </section>
            <section className="inspector-section">
              <h3>Assets</h3>
              <div className="placement-view-settings">
                <label>Animated thumbnails
                  <select aria-label="Animated thumbnails" value={preferences.thumbnailPlayback}
                    onChange={event => updatePreference("thumbnailPlayback", event.target.value as "hover" | "always" | "off")}>
                    <option value="always">Always</option>
                    <option value="off">Off</option>
                  </select>
                </label>
                <label>Sprite preview background
                  <input
                    type="color"
                    aria-label="Sprite preview background"
                    value={preferences.spritePreviewBackground}
                    onChange={event => updatePreference("spritePreviewBackground", event.target.value)}
                  />
                </label>
                <label>Library zoom
                  <input
                    type="range"
                    min={ASSET_LIBRARY_MIN_ZOOM}
                    max={ASSET_LIBRARY_MAX_ZOOM}
                    step={ASSET_LIBRARY_ZOOM_STEP}
                    aria-label="Asset library zoom"
                    value={preferences.assetLibraryZoom}
                    onChange={event => setAssetLibraryZoom(Number(event.target.value))}
                  />
                </label>
                <label>Font preview text
                  <input
                    type="text"
                    maxLength={120}
                    value={preferences.fontPreviewText}
                    onChange={event => updatePreference("fontPreviewText", event.target.value)}
                  />
                </label>
              </div>
            </section>
            {project !== null && service?.project_settings?.persisted === true
              && service.operations.includes(service.project_settings.read_operation) && (() => {
              const capability = service.project_settings;
              const projectSettings = project.document?.project?.settings ?? {};
              const sfx = projectSettings.sfx_import;
              const inactivity = projectSettings.runtime_preferences?.inactivity_timeout_ms;
              const inactivityMode = projectSettings.runtime_preferences === undefined
                ? "unset" : inactivity === null ? "system" : "custom";
              const replaceGroup = (group: keyof ProjectSettings, value: ProjectSettings[typeof group] | undefined) => {
                const next = { ...projectSettings };
                if (value === undefined) delete next[group];
                else Object.assign(next, { [group]: value });
                void updateProjectSettings(next);
              };
              return <section className="inspector-section project-settings-inspector">
                <h3>Project preferences</h3>
                <p className="settings-status-note">Saved with this project. These preferences are not currently applied to imports, firmware, or exported packages.</p>
                <div className="project-settings-group">
                  <div className="project-settings-group-heading">
                    <strong>SFX import preference</strong>
                    <span>Stored for future import integration.</span>
                  </div>
                  <label className="select-field">Normalization
                    <select value={sfx?.normalization ?? "unset"} disabled={busy !== null}
                      onChange={event => {
                        const normalization = event.target.value;
                        replaceGroup("sfx_import", normalization === "unset" ? undefined : {
                          normalization: normalization as "none" | "peak",
                          target_peak_dbfs: sfx?.target_peak_dbfs ?? -6,
                        });
                      }}>
                      <option value="unset">Not set</option>
                      {capability.sfx_import.normalization.includes("none") && <option value="none">None</option>}
                      {capability.sfx_import.normalization.includes("peak") && <option value="peak">Peak</option>}
                    </select>
                  </label>
                  {sfx !== undefined && <label className="select-field">Target peak (dBFS)
                    <input type="number" step={1}
                      min={capability.sfx_import.target_peak_dbfs.minimum}
                      max={capability.sfx_import.target_peak_dbfs.maximum}
                      defaultValue={sfx.target_peak_dbfs} key={`sfx-peak-${sfx.target_peak_dbfs}`}
                      disabled={busy !== null} onBlur={event => {
                        const value = Number(event.currentTarget.value);
                        if (!Number.isInteger(value) || value < capability.sfx_import.target_peak_dbfs.minimum
                          || value > capability.sfx_import.target_peak_dbfs.maximum) {
                          event.currentTarget.value = String(sfx.target_peak_dbfs);
                          return;
                        }
                        if (value !== sfx.target_peak_dbfs) replaceGroup("sfx_import", { ...sfx, target_peak_dbfs: value });
                      }} />
                  </label>}
                </div>
                <div className="project-settings-group">
                  <div className="project-settings-group-heading">
                    <strong>Inactivity preference</strong>
                    <span>Stored for future device-policy support.</span>
                  </div>
                  <label className="select-field">Timeout source
                    <select value={inactivityMode} disabled={busy !== null} onChange={event => {
                      const mode = event.target.value;
                      replaceGroup("runtime_preferences", mode === "unset" ? undefined : {
                        inactivity_timeout_ms: mode === "system" ? null : (typeof inactivity === "number" ? inactivity : 30000),
                      });
                    }}>
                      <option value="unset">Not set</option>
                      <option value="system">System selection</option>
                      <option value="custom">Project preference</option>
                    </select>
                  </label>
                  {inactivityMode === "custom" && typeof inactivity === "number" && <label className="select-field">Timeout (seconds)
                    <input type="number" step={1}
                      min={capability.runtime_preferences.inactivity_timeout_ms.minimum / 1000}
                      max={capability.runtime_preferences.inactivity_timeout_ms.maximum / 1000}
                      defaultValue={inactivity / 1000} key={`inactivity-${inactivity}`}
                      disabled={busy !== null} onBlur={event => {
                        const seconds = Number(event.currentTarget.value);
                        const value = seconds * 1000;
                        if (!Number.isInteger(seconds) || value < capability.runtime_preferences.inactivity_timeout_ms.minimum
                          || value > capability.runtime_preferences.inactivity_timeout_ms.maximum) {
                          event.currentTarget.value = String(inactivity / 1000);
                          return;
                        }
                        if (value !== inactivity) replaceGroup("runtime_preferences", { inactivity_timeout_ms: value });
                      }} />
                  </label>}
                </div>
              </section>;
            })()}
            {renderPlacementViewSettings()}
          </> : <>
          {projectRootSelected && renderProjectInspector()}
          {!projectRootSelected && workspaceMode === "placement" && renderPlacementInspector()}
          {!projectRootSelected && workspaceMode === "assets" && renderAssetInspector()}

          {!projectRootSelected && workspaceMode === "logic" && (
            <section className="inspector-section">
              <h3>Runtime</h3>
              {preview === null ? (
                <p className="muted">Start a scene preview to inspect its runtime state.</p>
              ) : (
                <dl className="inspector-list">
                  <div><dt>Scene</dt><dd>{scenes.find(scene => scene.scene_id === preview.scene.scene_id)?.display_name ?? "Unknown scene"}</dd></div>
                  <div><dt>State</dt><dd>{scenes.find(scene => scene.scene_id === preview.scene.scene_id)?.states
                    ?.find(state => state.state_id === preview.scene.state_id)?.display_name ?? "Unknown state"}</dd></div>
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
              canEdit={canConnectSelectedScene}
            />
          )}

          {!projectRootSelected && workspaceMode === "logic" && (
            <SceneAuthoringInspector
              canConnectScenes={canConnectSelectedScene}
              sceneExitActionKinds={objectSceneSelected ? selectedSceneCapability?.scene_exit_action_kinds ?? [] : targetSceneActionKinds}
              routeActionKinds={audioRouteActionKinds}
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
            <TimerInspector key={selectedSceneDocument.scene_id} scene={selectedSceneDocument} scenes={scenes} service={service}
              canConnectScenes={canConnectSelectedScene} sceneExitActionKinds={selectedSceneCapability?.scene_exit_action_kinds ?? []}
              routeActionKinds={audioRouteActionKinds}
              profileId={project?.summary.target_profile ?? ""} selection={sceneSelection} onSelect={setSceneSelection}
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

          </>}
        </aside>
        </ObjectActionContext.Provider>
      </section>

      <footer className="status-bar">
        <span>{busy !== null ? <><LoaderCircle className="spin" size={14} aria-hidden="true" /> {busy}</> : message ?? "Ready"}</span>
        <span>{project?.source_name ?? "No project"}</span>
      </footer>
    </main>
  );
}
