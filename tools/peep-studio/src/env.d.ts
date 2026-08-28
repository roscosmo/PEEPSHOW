/// <reference types="vite/client" />

interface PeepStudioBridge {
  serviceRequest<T>(operation: string, params: Record<string, unknown>): Promise<T>;
  openProject(): Promise<string | null>;
  openExampleProject(): Promise<string>;
  importSpritePng(projectPath: string): Promise<{ assetId: string; displayName: string; sourcePath: string; width: number; height: number } | null>;
  saveProjectAs(sourcePath: string, defaultName: string): Promise<string | null>;
  exportEgg(defaultName: string, blobBase64: string): Promise<string | null>;
}

interface Window {
  peepStudio?: PeepStudioBridge;
}
