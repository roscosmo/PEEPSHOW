const {contextBridge,ipcRenderer}=require('electron');
contextBridge.exposeInMainWorld('peepStudio',{
  audioThumbnailSource:(project,source)=>ipcRenderer.invoke('wave:source',project,source),
});
