const { contextBridge, ipcRenderer } = require("electron");
contextBridge.exposeInMainWorld("peepStudio", {
  serviceRequest: (operation, params) => ipcRenderer.invoke("hierarchy:service", operation, params),
  openExampleProject: () => ipcRenderer.invoke("hierarchy:example"),
});
