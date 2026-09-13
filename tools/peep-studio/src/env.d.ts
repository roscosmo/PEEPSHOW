/// <reference types="vite/client" />

type StudioFontAssetRecord = {
  font_id: string;
  display_name: string;
  source_path: string;
  source_format: "ttf" | "otf";
};

type StudioBakedTextSourceRecord = {
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

interface PeepStudioBridge {
  serviceRequest<T>(operation: string, params: Record<string, unknown>): Promise<T>;
  chooseNewProjectPath(): Promise<string | null>;
  openProject(): Promise<string | null>;
  openExampleProject(): Promise<string>;
  importSpritePng(projectPath: string): Promise<{ assetId: string; displayName: string; sourcePath: string; width: number; height: number } | null>;
  readFontAssets?(projectPath: string): Promise<StudioFontAssetRecord[]>;
  importFontAsset?(projectPath: string): Promise<StudioFontAssetRecord | null>;
  renameFontAsset?(projectPath: string, fontId: string, displayName: string): Promise<StudioFontAssetRecord>;
  deleteFontAsset?(projectPath: string, fontId: string): Promise<{ font_id: string; deleted: boolean }>;
  fontAssetSource?(projectPath: string, sourcePath: string): Promise<{ key: string; data: string }>;
  readBakedTextSources?(projectPath: string): Promise<StudioBakedTextSourceRecord[]>;
  upsertBakedTextSource?(projectPath: string, record: StudioBakedTextSourceRecord): Promise<StudioBakedTextSourceRecord>;
  writeGeneratedSpritePng?(projectPath: string, requestedAssetId: string, pngDataUrl: string): Promise<{ assetId: string; sourcePath: string; width: number; height: number }>;
  overwriteGeneratedSpritePng?(projectPath: string, sourcePath: string, pngDataUrl: string): Promise<{ sourcePath: string; width: number; height: number }>;
  importAudioWav(projectPath: string): Promise<{ assetId: string; sourcePath: string } | null>;
  audioThumbnailSource?(projectPath: string, sourcePath: string): Promise<{ key: string; data: string }>;
  saveProjectAs(sourcePath: string, defaultName: string): Promise<string | null>;
  exportEgg(defaultName: string, blobBase64: string): Promise<string | null>;
}

interface Window {
  peepStudio?: PeepStudioBridge;
}
