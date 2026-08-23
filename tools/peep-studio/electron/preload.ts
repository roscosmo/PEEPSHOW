import { contextBridge, ipcRenderer } from "electron";

contextBridge.exposeInMainWorld("peepStudio", {
  serviceRequest: (operation: string, params: Record<string, unknown>) =>
    ipcRenderer.invoke("peep:service-request", operation, params),
  openProject: () => ipcRenderer.invoke("peep:open-project"),
  openExampleProject: () => ipcRenderer.invoke("peep:open-example"),
  exportEgg: (defaultName: string, blobBase64: string) =>
    ipcRenderer.invoke("peep:export-egg", defaultName, blobBase64),
});
