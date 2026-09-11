const assert = require('node:assert/strict');
const fs = require('node:fs'), path = require('node:path'), crypto = require('node:crypto');
const {spawn} = require('node:child_process'), readline = require('node:readline');
const {app, BrowserWindow, ipcMain} = require('electron');
const root = path.resolve(__dirname, '../../..');
const source = path.join(root, 'examples/authoring/native_v2_installation.peepproj');
const output = path.resolve('dist/restricted-v2-export');
fs.mkdirSync(output, {recursive:true});
const eggPath = path.join(output, 'native_v2_installation.egg');
app.setPath('userData', path.join(output,'profile'));
app.disableHardwareAcceleration();
const hash = bytes => crypto.createHash('sha256').update(bytes).digest('hex');
function sourceHashes(dir) {
  return fs.readdirSync(dir,{withFileTypes:true}).flatMap(entry => {
    const file = path.join(dir,entry.name);
    return entry.isDirectory() ? sourceHashes(file) : [[path.relative(source,file),hash(fs.readFileSync(file))]];
  });
}
const before = sourceHashes(source), pending = new Map();
let child, window, id=0, latest, built, writes=0, fault=null, blocked=false;
const wait = ms => new Promise(resolve=>setTimeout(resolve,ms));
const watchdog = setTimeout(()=>{child?.kill();app.exit(1)},60000);
app.whenReady().then(async()=>{
  child=spawn(process.env.PEEPSHOW_PYTHON,['-u','tools/authoring/egg_tool.py','service'],{cwd:root,windowsHide:true});
  readline.createInterface({input:child.stdout}).on('line',line=>{
    const r=JSON.parse(line), p=pending.get(r.id); pending.delete(r.id);
    if(r.ok)p.resolve(r.result);else p.reject(new Error(JSON.stringify(r.error)));
  });
  child.stderr.on('data',data=>process.stderr.write(data));
  const call=(operation,params)=>new Promise((resolve,reject)=>{
    const requestId=String(++id);pending.set(requestId,{resolve,reject});
    child.stdin.write(JSON.stringify({protocol_version:1,id:requestId,operation,params})+'\n');
  });
  ipcMain.handle('export:source',()=>source);
  ipcMain.handle('export:service',async(_,operation,params)=>{
    assert(!['project.save','project.create'].includes(operation), 'Frozen fixture must not be changed');
    if(operation==='project.build_package' && fault==='failure') throw Error('Build failure diagnostic');
    let result=await call(operation,params);
    if(operation==='project.load' && blocked) {
      result={...result,...await call('project.apply_commands',{project_revision:result.project_revision,
        commands:[{kind:'scene.add',display_name:'Extra',scene_schema_version:2}]})};
    }
    if(result.document)latest=result;
    if(operation==='project.build_package') {
      built=result;
      if(fault==='stale')return {...result,project_revision:result.project_revision-1};
    }
    return result;
  });
  ipcMain.handle('export:write',(_,name,encoded)=>{
    assert.equal(fault,null); assert.equal(blocked,false);
    const bytes=Buffer.from(encoded,'base64');
    assert.equal(hash(bytes),built.package.sha256);
    assert.equal(bytes.length,built.package.size_bytes);
    fs.writeFileSync(eggPath,bytes);writes++;
    return eggPath;
  });
  window=new BrowserWindow({width:1440,height:1000,show:false,webPreferences:{
    preload:path.join(__dirname,'restricted-export-preload.cjs'),sandbox:true,contextIsolation:true,offscreen:true,backgroundThrottling:false}});
  const evaluate=code=>window.webContents.executeJavaScript(code);
  const button=async label=>{
    await evaluate(`(()=>{const e=[...document.querySelectorAll('button')].find(e=>e.textContent.trim()===${JSON.stringify(label)});if(!e||e.disabled)throw Error('Unavailable: '+${JSON.stringify(label)});e.click()})()`);await wait(650);
  };
  const exportDisabled=()=>evaluate("document.querySelector('[title=\"Export .egg\"]').disabled");
  await window.loadURL('http://127.0.0.1:5174');await wait(800);
  await button('Open project');assert(latest.valid);
  assert.equal(latest.scene_capabilities.main.export_ready,true);
  assert(await exportDisabled());
  await button('Build');assert.equal(await exportDisabled(),false);
  assert.equal(built.package.container_version,2);
  assert.equal(built.package.export_profile_id,'hw6_v2_resident_v1');
  assert.equal(built.package.size_bytes,2196);
  await evaluate("document.querySelector('[title=\"Export .egg\"]').click()");await wait(300);
  assert.equal(writes,1);
  const exported={path:eggPath,...built.package,blob_base64:undefined};
  fault='failure';await button('Build');assert(await exportDisabled());
  fault='stale';await button('Build');assert(await exportDisabled());
  fault=null;await button('Build');assert.equal(await exportDisabled(),false);
  blocked=true;await button('Open project');
  assert.equal(latest.valid,true);assert(latest.build_issues.length>0);
  assert.equal(await evaluate("[...document.querySelectorAll('button')].find(e=>e.textContent.trim()==='Build').disabled"),true);
  assert(await exportDisabled());
  const notice=await evaluate("document.querySelector('.host-preview-notice').textContent");
  for(const issue of latest.build_issues)assert(notice.includes(issue.code)&&notice.includes(issue.message));
  await button('Placement');await button('Local logic');
  window.webContents.invalidate();await wait(200);
  fs.writeFileSync(path.join(output,'blocked-project.png'),(await window.webContents.capturePage()).toPNG());
  assert.deepEqual(sourceHashes(source),before);
  fs.writeFileSync(path.join(output,'export-result.json'),JSON.stringify({exported,sourceHashes:before,checks:{ready:true,failedBuild:true,staleBuild:true,blockedDraft:true,sourceUnchanged:true}},null,2));
  console.log(JSON.stringify(exported,null,2));
  console.log('Studio export, failed/stale build protection, blocked draft checks and unchanged source passed');
  child.kill();clearTimeout(watchdog);app.exit(0);
}).catch(error=>{console.error(error);child?.kill();clearTimeout(watchdog);app.exit(1)});
