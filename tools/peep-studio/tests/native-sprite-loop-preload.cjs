const { contextBridge, ipcRenderer } = require('electron');
contextBridge.exposeInMainWorld('peepStudio', {
  serviceRequest: (operation, params) => ipcRenderer.invoke('audit:service', operation, params),
  chooseNewProjectPath: () => ipcRenderer.invoke('audit:path'),
  openProject: () => ipcRenderer.invoke('audit:path'),
  importSpritePng: () => ipcRenderer.invoke('audit:png'),
  writeGeneratedSpritePng: (projectPath, requestedAssetId, pngDataUrl) =>
    ipcRenderer.invoke('audit:generated-png', projectPath, requestedAssetId, pngDataUrl),
});
