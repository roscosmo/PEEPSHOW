import { contextBridge, ipcRenderer } from "electron";

contextBridge.exposeInMainWorld("peepStudio", {
  serviceRequest: (operation: string, params: Record<string, unknown>) =>
    ipcRenderer.invoke("peep:service-request", operation, params),
  chooseNewProjectPath: () => ipcRenderer.invoke("peep:choose-new-project-path"),
  openProject: () => ipcRenderer.invoke("peep:open-project"),
  openExampleProject: () => ipcRenderer.invoke("peep:open-example"),
  importSpritePng: (projectPath: string) => ipcRenderer.invoke("peep:import-sprite-png", projectPath),
  importAudioWav: (projectPath: string) => ipcRenderer.invoke("peep:import-audio-wav", projectPath),
  saveProjectAs: (sourcePath: string, defaultName: string) =>
    ipcRenderer.invoke("peep:save-project-as", sourcePath, defaultName),
  exportEgg: (defaultName: string, blobBase64: string) =>
    ipcRenderer.invoke("peep:export-egg", defaultName, blobBase64),
});
