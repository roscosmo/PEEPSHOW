const {contextBridge, ipcRenderer} = require('electron');
contextBridge.exposeInMainWorld('peepStudio', {
  serviceRequest: (operation, params) => ipcRenderer.invoke('export:service', operation, params),
  openProject: () => ipcRenderer.invoke('export:source'),
  exportEgg: (name, bytes) => ipcRenderer.invoke('export:write', name, bytes),
});
