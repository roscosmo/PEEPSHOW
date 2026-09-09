const { contextBridge, ipcRenderer } = require('electron');
contextBridge.exposeInMainWorld('peepStudio', {
  serviceRequest: (operation, params) => ipcRenderer.invoke('native:service', operation, params),
  chooseNewProjectPath: () => ipcRenderer.invoke('native:path'),
  openProject: () => ipcRenderer.invoke('native:path'),
});
