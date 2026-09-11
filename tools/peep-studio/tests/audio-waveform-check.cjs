const assert=require('node:assert/strict'),fs=require('node:fs'),os=require('node:os'),path=require('node:path');
const {app,BrowserWindow,ipcMain}=require('electron');
const {readThumbnailAudio}=require('../dist-electron/audioThumbnail.js');
const output=path.resolve('dist/audio-waveform-check');fs.mkdirSync(output,{recursive:true});
const temp=fs.mkdtempSync(path.join(os.tmpdir(),'peep-wave-'));
const project=path.join(temp,'wave.peepproj');fs.mkdirSync(path.join(project,'assets'),{recursive:true});
function wav(name,channels=1,amplitude=1) {
  const samples=8000,bytes=Buffer.alloc(44+samples*channels*2);
  bytes.write('RIFF');bytes.writeUInt32LE(bytes.length-8,4);bytes.write('WAVEfmt ',8);
  bytes.writeUInt32LE(16,16);bytes.writeUInt16LE(1,20);bytes.writeUInt16LE(channels,22);
  bytes.writeUInt32LE(8000,24);bytes.writeUInt32LE(8000*channels*2,28);bytes.writeUInt16LE(channels*2,32);
  bytes.writeUInt16LE(16,34);bytes.write('data',36);bytes.writeUInt32LE(samples*channels*2,40);
  for(let i=0;i<samples;i++) for(let channel=0;channel<channels;channel++) {
    const envelope=name==='silence'?0:name==='ramp'?i/samples:(i%2000<500?1:0);
    bytes.writeInt16LE(Math.round(Math.sin(i*.3)*envelope*amplitude*20000*(channel===1?-1:1)),44+(i*channels+channel)*2);
  }
  fs.writeFileSync(path.join(project,'assets',`${name}.wav`),bytes);
}
wav('pulse');wav('ramp',2);wav('silence');
app.setPath('userData',path.join(output,'profile'));app.disableHardwareAcceleration();
const wait=ms=>new Promise(r=>setTimeout(r,ms));
const watchdog=setTimeout(()=>app.exit(1),30000);
app.whenReady().then(async()=>{
  await assert.rejects(()=>readThumbnailAudio(project,'../../outside.wav'));
  const outside=path.join(temp,'outside.wav');fs.copyFileSync(path.join(project,'assets/pulse.wav'),outside);
  await assert.rejects(()=>readThumbnailAudio(project,'../outside.wav'),/inside the project/);
  await assert.rejects(()=>readThumbnailAudio(project,path.join(project,'assets/pulse.wav')),/Invalid/);
  ipcMain.handle('wave:source',(_,p,s)=>readThumbnailAudio(p,s));
  const window=new BrowserWindow({width:580,height:420,show:false,webPreferences:{
    preload:path.join(__dirname,'audio-waveform-preload.cjs'),sandbox:true,contextIsolation:true,offscreen:true}});
  const url='http://127.0.0.1:5174/tests/audio-waveform.html?project='+encodeURIComponent(project);
  const evaluate=code=>window.webContents.executeJavaScript(code);
  await window.loadURL(url);await wait(600);
  await evaluate("localStorage.removeItem('peep-studio.waveforms.v1')");
  await window.loadURL(url);await wait(800);
  assert.equal(await evaluate("document.querySelectorAll('canvas').length"),3);
  assert.equal(await evaluate("document.body.dataset.decodes"),'3');
  const pictures=await evaluate("[...document.querySelectorAll('canvas')].map(c=>c.toDataURL())");
  assert.equal(new Set(pictures).size,3);
  const ink=await evaluate("[...document.querySelectorAll('canvas')].map(c=>{const p=c.getContext('2d').getImageData(0,0,128,40).data;let n=0;for(let i=3;i<p.length;i+=4)if(p[i])n++;return n})");
  assert(ink.every(n=>n>0));assert(ink[2]<ink[0]);
  assert(await evaluate("!!document.querySelector('[data-audio=missing] svg')"));
  await window.loadURL(url);await wait(800);
  assert.equal(await evaluate("document.body.dataset.decodes"),undefined);
  assert.deepEqual(await evaluate("[...document.querySelectorAll('canvas')].map(c=>c.toDataURL())"),pictures);
  wav('pulse',1,.5);
  await window.loadURL(url);await wait(800);
  assert.equal(await evaluate("document.body.dataset.decodes"),'1');
  window.webContents.invalidate();await wait(200);
  fs.writeFileSync(path.join(output,'waveforms.png'),(await window.webContents.capturePage()).toPNG());
  window.setSize(320,420);await wait(200);window.webContents.invalidate();await wait(100);
  fs.writeFileSync(path.join(output,'waveforms-compact.png'),(await window.webContents.capturePage()).toPNG());
  console.log('Mono/stereo/silence waveforms, canvas pixels, missing-file fallback, reload cache, content invalidation and path confinement passed');
  clearTimeout(watchdog);app.exit(0);
}).catch(error=>{console.error(error);clearTimeout(watchdog);app.exit(1)});
