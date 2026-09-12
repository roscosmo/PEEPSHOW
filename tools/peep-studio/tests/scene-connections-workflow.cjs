const assert=require('node:assert/strict'),fs=require('node:fs'),os=require('node:os'),path=require('node:path'),readline=require('node:readline');
const {spawn}=require('node:child_process');
const {app,BrowserWindow,ipcMain}=require('electron');
const root=path.resolve(__dirname,'../../..');
const temporary=fs.mkdtempSync(path.join(os.tmpdir(),'peep-scene-connections-'));
const source=path.join(temporary,'menu.peepproj');
fs.cpSync(path.join(root,'examples/authoring/native_v2_scene_connections.peepproj'),source,{recursive:true});
const output=path.resolve('dist/scene-connections-workflow');fs.mkdirSync(output,{recursive:true});
app.setPath('userData',path.join(temporary,'profile'));app.disableHardwareAcceleration();
let child,id=0,latest;const pending=new Map(),errors=[],batches=[];
const wait=ms=>new Promise(resolve=>setTimeout(resolve,ms));
const watchdog=setTimeout(()=>{child?.kill();app.exit(1)},60000);
app.whenReady().then(async()=>{
  child=spawn(process.env.PEEPSHOW_PYTHON,['-u','tools/authoring/egg_tool.py','service'],{cwd:root,windowsHide:true});
  readline.createInterface({input:child.stdout}).on('line',line=>{
    const r=JSON.parse(line),p=pending.get(r.id);pending.delete(r.id);
    if(r.ok)p.resolve(r.result);else {errors.push(r.error);p.reject(new Error(JSON.stringify(r.error)))}
  });
  child.stderr.on('data',data=>process.stderr.write(data));
  ipcMain.handle('export:source',()=>source);
  ipcMain.handle('export:service',async(_,operation,params)=>{
    assert(!['project.create','project.build_package'].includes(operation));
    if(operation==='project.apply_commands') batches.push(params.commands);
    const result=await new Promise((resolve,reject)=>{const requestId=String(++id);pending.set(requestId,{resolve,reject});
      child.stdin.write(JSON.stringify({protocol_version:1,id:requestId,operation,params})+'\n')});
    if(result.document) latest=result;
    return result;
  });
  const window=new BrowserWindow({width:1440,height:1000,show:false,webPreferences:{
    preload:path.join(__dirname,'restricted-export-preload.cjs'),sandbox:true,contextIsolation:true,offscreen:true}});
  const evaluate=code=>window.webContents.executeJavaScript(code).catch(error=>{throw new Error(`${code}: ${error.message}`)});
  const click=async selector=>{await evaluate(`document.querySelector(${JSON.stringify(selector)}).click()`);await wait(250)};
  const button=async name=>{await evaluate(`([...document.querySelectorAll('button')].find(e=>e.textContent.trim()===${JSON.stringify(name)})).click()`);await wait(350)};
  const field=async(label,value)=>{await evaluate(`(()=>{const e=document.querySelector('[aria-label="${label}"]');Object.getOwnPropertyDescriptor(HTMLInputElement.prototype,'value').set.call(e,${JSON.stringify(String(value))});e.dispatchEvent(new Event('input',{bubbles:true}));})()`);await wait(100)};
  const capture=async name=>{window.webContents.invalidate();await wait(100);fs.writeFileSync(path.join(output,name),(await window.webContents.capturePage()).toPNG())};
  await window.loadURL('http://127.0.0.1:5174');await wait(500);await button('Open project');await button('Scene flow');
  assert.equal(await evaluate("document.querySelector('.react-flow__node[data-id=main] .scene-new-exit-slot').classList.contains('disabled')"),false);
  assert(await evaluate("Array.from(document.querySelectorAll('button')).find(e=>e.textContent.trim()==='Build').disabled"));
  const drag=async(sourceSelector,targetSelector)=>{
    const points=await evaluate(`[...[${JSON.stringify(sourceSelector)},${JSON.stringify(targetSelector)}]].map(s=>{const r=document.querySelector(s).getBoundingClientRect();return {x:Math.round(r.x+r.width/2),y:Math.round(r.y+r.height/2)}})`);
    window.webContents.sendInputEvent({type:'mouseMove',...points[0]});
    window.webContents.sendInputEvent({type:'mouseDown',button:'left',clickCount:1,...points[0]});
    for(let i=1;i<=10;i++){window.webContents.sendInputEvent({type:'mouseMove',x:Math.round(points[0].x+(points[1].x-points[0].x)*i/10),y:Math.round(points[0].y+(points[1].y-points[0].y)*i/10)});await wait(30)}
    window.webContents.sendInputEvent({type:'mouseUp',button:'left',clickCount:1,...points[1]});await wait(600);
  };
  const main=()=>latest.document.scenes.find(scene=>scene.scene_id==='main');
  await drag('.react-flow__node[data-id="main"] .scene-new-exit-handle','.react-flow__node[data-id="garden"] .react-flow__handle.target');
  assert.equal(main().scene_exits.length,2);assert.equal(main().routes.length,1);assert.equal(main().input_actions.length,1);
  const exit=main().scene_exits.at(-1);
  await capture('scene-flow.png');
  await button('Local logic');
  await click('.react-flow__node[data-id="start"] [data-handleid="new-physical-trigger:BUTTON_B"]');
  await click(`.react-flow__node[data-id="scene-exit-${exit.scene_exit_id}"] .react-flow__handle.target`);
  await button('Create transition');
  assert.equal(main().routes.length,2);assert.equal(main().routes.at(-1).scene_exit_ref,exit.scene_exit_id);
  await click('.state-physical-trigger[aria-label="B trigger, configured"]');
  assert.equal(await evaluate("document.querySelector('.selected-record [aria-label=\"Add effect\"]')"),null);
  await click('.react-flow__pane');await button('Scene timer');await button('Create timer');
  await evaluate(`(()=>{const e=document.querySelector('[aria-label="Timer destination"]');Object.getOwnPropertyDescriptor(HTMLSelectElement.prototype,'value').set.call(e,'exit:${exit.scene_exit_id}');e.dispatchEvent(new Event('change',{bubbles:true}));})()`);await wait(500);
  assert.equal(main().event_handlers[0].scene_exit_ref,exit.scene_exit_id);assert.deepEqual(main().event_handlers[0].actions,[]);
  assert.equal(await evaluate("document.querySelector('.timer-inspector [aria-label=\"Add effect\"]')"),null);
  await capture('timer-exit.png');
  await evaluate(`(()=>{const e=document.querySelector('[aria-label="Timer destination"]');Object.getOwnPropertyDescriptor(HTMLSelectElement.prototype,'value').set.call(e,'');e.dispatchEvent(new Event('change',{bubbles:true}));})()`);await wait(400);
  assert.equal(main().event_handlers[0].target_scene,undefined);assert.equal(main().event_handlers[0].scene_exit_ref,undefined);
  assert(await evaluate("!!document.querySelector('.timer-inspector [aria-label=\"Add effect\"]')"));
  await click('button[title="Undo"]');assert.equal(main().event_handlers[0].scene_exit_ref,exit.scene_exit_id);
  await click('button[title="Redo"]');assert.equal(main().event_handlers[0].scene_exit_ref,undefined);
  await button('Save');const saved=JSON.stringify(latest.document.scenes);await button('Open project');assert.equal(JSON.stringify(latest.document.scenes),saved);
  assert(latest.build_issues.length>0);assert.deepEqual(errors,[]);
  console.log('API 43 GUI passed: drag-created shared exit, no invented input, button connection, empty-action timer exit, explicit detach, undo/redo, save/reopen and export blocked.');
  child.kill();clearTimeout(watchdog);app.exit(0);
}).catch(error=>{console.error(error);child?.kill();clearTimeout(watchdog);app.exit(1)});
