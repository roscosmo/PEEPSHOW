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
  chooseAudioWav: (projectPath: string) => ipcRenderer.invoke("peep:choose-audio-wav", projectPath),
  inspectProjectAudioWav: (projectPath: string, sourcePath: string) =>
    ipcRenderer.invoke("peep:inspect-project-audio-wav", projectPath, sourcePath),
  previewAudioWav: (projectPath: string, sourcePath: string, options: { normalize: boolean; targetPeakDbfs: number; trimStartMs: number; trimEndMs: number }) =>
    ipcRenderer.invoke("peep:preview-audio-wav", projectPath, sourcePath, options),
  importAudioWav: (projectPath: string, sourcePath: string, options: { normalize: boolean; targetPeakDbfs: number; trimStartMs: number; trimEndMs: number }) =>
    ipcRenderer.invoke("peep:import-audio-wav", projectPath, sourcePath, options),
  audioThumbnailSource: (projectPath: string, sourcePath: string) => ipcRenderer.invoke("peep:audio-thumbnail-source", projectPath, sourcePath),
  getEmulatorPopoutStatus: () => ipcRenderer.invoke("peep:emulator-popout-status"),
  openEmulatorPopout: () => ipcRenderer.invoke("peep:emulator-popout-open"),
  focusEmulatorPopout: () => ipcRenderer.invoke("peep:emulator-popout-focus"),
  closeEmulatorPopout: () => ipcRenderer.invoke("peep:emulator-popout-close"),
  syncEmulatorPopout: (state: unknown) => ipcRenderer.invoke("peep:emulator-popout-sync", state),
  setEmulatorPopoutCollapsed: (collapsed: boolean) => ipcRenderer.invoke("peep:emulator-popout-layout", collapsed),
  sendEmulatorPopoutCommand: (command: unknown) => ipcRenderer.invoke("peep:emulator-popout-command", command),
  windowControl: (action: "minimize" | "maximize" | "close") => ipcRenderer.invoke("peep:window-control", action),
  onEmulatorPopoutState: (callback: (state: unknown) => void) => {
    const listener = (_event: Electron.IpcRendererEvent, state: unknown) => callback(state);
    ipcRenderer.on("peep:emulator-popout-state", listener);
    return () => ipcRenderer.removeListener("peep:emulator-popout-state", listener);
  },
  onEmulatorPopoutCommand: (callback: (command: unknown) => void) => {
    const listener = (_event: Electron.IpcRendererEvent, command: unknown) => callback(command);
    ipcRenderer.on("peep:emulator-popout-command", listener);
    return () => ipcRenderer.removeListener("peep:emulator-popout-command", listener);
  },
  onEmulatorPopoutClosed: (callback: () => void) => {
    const listener = () => callback();
    ipcRenderer.on("peep:emulator-popout-closed", listener);
    return () => ipcRenderer.removeListener("peep:emulator-popout-closed", listener);
  },
  onNativeWindowInteraction: (callback: (active: boolean) => void) => {
    const listener = (_event: Electron.IpcRendererEvent, state: unknown) => {
      callback(state !== null && typeof state === "object" && (state as { active?: unknown }).active === true);
    };
    ipcRenderer.on("peep:native-window-interaction", listener);
    return () => ipcRenderer.removeListener("peep:native-window-interaction", listener);
  },
  saveProjectAs: (sourcePath: string, defaultName: string) =>
    ipcRenderer.invoke("peep:save-project-as", sourcePath, defaultName),
  exportEgg: (defaultName: string, blobBase64: string) =>
    ipcRenderer.invoke("peep:export-egg", defaultName, blobBase64),
});
