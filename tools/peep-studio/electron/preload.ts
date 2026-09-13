import { contextBridge, ipcRenderer } from "electron";

contextBridge.exposeInMainWorld("peepStudio", {
  serviceRequest: (operation: string, params: Record<string, unknown>) =>
    ipcRenderer.invoke("peep:service-request", operation, params),
  chooseNewProjectPath: () => ipcRenderer.invoke("peep:choose-new-project-path"),
  openProject: () => ipcRenderer.invoke("peep:open-project"),
  openExampleProject: () => ipcRenderer.invoke("peep:open-example"),
  importSpritePng: (projectPath: string) => ipcRenderer.invoke("peep:import-sprite-png", projectPath),
  readFontAssets: (projectPath: string) => ipcRenderer.invoke("peep:read-font-assets", projectPath),
  importFontAsset: (projectPath: string) => ipcRenderer.invoke("peep:import-font-asset", projectPath),
  renameFontAsset: (projectPath: string, fontId: string, displayName: string) =>
    ipcRenderer.invoke("peep:rename-font-asset", projectPath, fontId, displayName),
  deleteFontAsset: (projectPath: string, fontId: string) =>
    ipcRenderer.invoke("peep:delete-font-asset", projectPath, fontId),
  fontAssetSource: (projectPath: string, sourcePath: string) => ipcRenderer.invoke("peep:font-asset-source", projectPath, sourcePath),
  readBakedTextSources: (projectPath: string) => ipcRenderer.invoke("peep:read-baked-text-sources", projectPath),
  upsertBakedTextSource: (projectPath: string, record: Record<string, unknown>) =>
    ipcRenderer.invoke("peep:upsert-baked-text-source", projectPath, record),
  writeGeneratedSpritePng: (projectPath: string, requestedAssetId: string, pngDataUrl: string) =>
    ipcRenderer.invoke("peep:write-generated-sprite-png", projectPath, requestedAssetId, pngDataUrl),
  overwriteGeneratedSpritePng: (projectPath: string, sourcePath: string, pngDataUrl: string) =>
    ipcRenderer.invoke("peep:overwrite-generated-sprite-png", projectPath, sourcePath, pngDataUrl),
  importAudioWav: (projectPath: string) => ipcRenderer.invoke("peep:import-audio-wav", projectPath),
  audioThumbnailSource: (projectPath: string, sourcePath: string) => ipcRenderer.invoke("peep:audio-thumbnail-source", projectPath, sourcePath),
  saveProjectAs: (sourcePath: string, defaultName: string) =>
    ipcRenderer.invoke("peep:save-project-as", sourcePath, defaultName),
  exportEgg: (defaultName: string, blobBase64: string) =>
    ipcRenderer.invoke("peep:export-egg", defaultName, blobBase64),
});
