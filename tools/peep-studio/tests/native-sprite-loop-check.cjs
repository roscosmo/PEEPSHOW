const assert = require('node:assert/strict'), fs = require('node:fs'), os = require('node:os'), path = require('node:path');
const readline = require('node:readline'), { spawn } = require('node:child_process');
const { app, BrowserWindow, ipcMain, nativeImage } = require('electron');
const root = path.resolve(__dirname, '../../..');
const frameCount = process.argv.includes('--ten') ? 10 : 4;
const projectPath = path.join(fs.mkdtempSync(path.join(os.tmpdir(), 'peep-workflow-audit-')), 'fresh.peepproj');
const output = path.resolve('dist/native-sprite-loop-check');
fs.mkdirSync(output, { recursive: true });
app.setPath('userData', path.join(output, 'profile'));
app.disableHardwareAcceleration(); app.on('window-all-closed', () => {});
let child, window, latest, hello, id = 0;
const pending = new Map(), errors = [], commands = [], batches = [];
const wait = ms => new Promise(resolve => setTimeout(resolve, ms));
const watchdog = setTimeout(() => { child?.kill(); app.exit(1); }, 60000);
app.whenReady().then(async () => {
  child = spawn(process.env.PEEPSHOW_PYTHON, ['-u', 'tools/authoring/egg_tool.py', 'service'], { cwd: root, windowsHide: true });
  readline.createInterface({ input: child.stdout }).on('line', line => {
    const r = JSON.parse(line), p = pending.get(r.id); pending.delete(r.id);
    if (r.ok) p?.resolve(r.result); else { errors.push({ ...r.error, operation: p?.operation }); p?.reject(new Error(JSON.stringify(r.error))); }
  });
  child.stderr.on('data', data => process.stderr.write(data));
  ipcMain.handle('audit:path', () => projectPath);
  ipcMain.handle('audit:service', async (_, operation, params) => {
    assert(!operation.includes('migration'));
    if (operation === 'project.apply_commands') { commands.push(...params.commands); batches.push(params.commands); }
    const requestId = String(++id);
    const result = await new Promise((resolve, reject) => {
      pending.set(requestId, { resolve, reject, operation });
      child.stdin.write(JSON.stringify({ protocol_version: 1, id: requestId, operation, params }) + '\n');
    });
    if (result.document) latest = result;
    if (operation === 'service.hello') hello = result;
    return result;
  });
  ipcMain.handle('audit:png', () => {
    const width = frameCount * 16;
    const pixels = Buffer.alloc(width * 16 * 4, 255);
    for (let y = 3; y < 13; y++) for (let frame = 0; frame < frameCount; frame++) for (let x = 2; x < 4 + frame; x++) {
      const i = (y * width + frame * 16 + x) * 4; pixels[i] = pixels[i + 1] = pixels[i + 2] = 0;
    }
    const destination = path.join(projectPath, 'assets', 'audit.png');
    fs.mkdirSync(path.dirname(destination), { recursive: true });
    fs.writeFileSync(destination, nativeImage.createFromBitmap(pixels, { width, height: 16 }).toPNG());
    return { assetId: 'audit', displayName: 'Sprite loop test', sourcePath: 'assets/audit.png', width, height: 16 };
  });
  window = new BrowserWindow({ width: 1440, height: 1000, show: false, webPreferences: {
    preload: path.join(__dirname, 'native-sprite-loop-preload.cjs'), sandbox: true, contextIsolation: true, offscreen: true, backgroundThrottling: false,
  } });
  const evaluate = code => window.webContents.executeJavaScript(code);
  const button = async label => { await evaluate(`(() => { const e=[...document.querySelectorAll('button')].find(e=>e.textContent.trim()===${JSON.stringify(label)}); if(!e) throw Error('Missing button: '+${JSON.stringify(label)}); if(e.disabled) throw Error('Disabled button'); e.click(); })()`); await wait(450); };
  const click = async selector => { await evaluate(`document.querySelector(${JSON.stringify(selector)}).click()`); await wait(450); };
  await window.loadURL('http://127.0.0.1:5174'); await wait(800);
  await button('New project');
  assert.equal(latest.document.scenes[0].schema_version, 2);
  await button('Assets'); await button('Choose PNG');
  await evaluate(`(() => {const e=document.querySelector('.asset-import-controls input'); Object.getOwnPropertyDescriptor(HTMLInputElement.prototype,'value').set.call(e,'16'); e.dispatchEvent(new Event('input',{bubbles:true}));})()`); await wait(250);
  await button('Import');
  assert.equal(commands.find(c=>c.kind==='asset.upsert').asset.frames.length, frameCount);
  await button('Placement');
  await click('.scene-hierarchy-node.selected .base-branch .hierarchy-branch-select');
  await click('[aria-label="Add sprite"]'); await click('.placement-sprite-picker-group button');
  const scene = latest.document.scenes[0]; assert.equal(scene.objects.length, 1);
  const animation = await evaluate(`(() => {const e=document.querySelector('[aria-label="Object animation"]');return {disabled:e.disabled,options:[...e.options].map(o=>({value:o.value,text:o.text}))};})()`);
  assert.deepEqual(animation.options, [{ value: '', text: 'Static' }]);
  await button('New loop');
  const durationField = '[aria-label="New loop frame duration"]';
  const duration = async value => {
    await evaluate(`(() => {const e=document.querySelector(${JSON.stringify(durationField)});Object.getOwnPropertyDescriptor(HTMLInputElement.prototype,'value').set.call(e,${JSON.stringify(value)});e.dispatchEvent(new Event('input',{bubbles:true}));})()`); await wait(100);
  };
  await duration('0');
  assert(await evaluate("[...document.querySelectorAll('button')].find(e=>e.textContent.trim()==='Create loop').disabled"));
  await duration('400');
  window.webContents.invalidate(); await wait(200);
  fs.writeFileSync(path.join(output,'create-loop.png'),(await window.webContents.capturePage()).toPNG());
  await button('Create loop');
  const clip = latest.document.animations[0];
  assert.equal(clip.loop_policy, 'loop');
  assert.deepEqual(clip.frame_refs, Array.from({length:frameCount},(_,i)=>`audit.frame_${i+1}`));
  assert.deepEqual(clip.frame_duration_ms, Array(frameCount).fill(400));
  assert.equal(latest.document.scenes[0].objects[0].animation_ref, clip.animation_id);
  assert(batches.some(batch => batch.length === 2 && batch[0].kind === 'animation.upsert' && batch[1].kind === 'object.bind_animation'));
  assert(!commands.some(command => /waiting/.test(command.kind)));
  await click('button[title="Undo"]');
  assert.equal(latest.document.animations.length, 0);
  assert.equal(latest.document.scenes[0].objects[0].animation_ref, undefined);
  await click('button[title="Redo"]');
  assert.deepEqual(latest.document.animations, [clip]);
  assert.equal(latest.document.scenes[0].objects[0].animation_ref, clip.animation_id);
  assert(await evaluate("[...document.querySelectorAll('button')].find(e=>e.textContent.trim()==='Build').disabled"));
  await button('Save'); const saved = JSON.stringify(latest.document.scenes);
  await button('Open project'); assert.equal(JSON.stringify(latest.document.scenes), saved);
  assert.deepEqual(latest.document.animations, [clip]);
  const preview = (operation, params) => new Promise((resolve, reject) => {
    const requestId = String(++id); pending.set(requestId, {resolve, reject, operation});
    child.stdin.write(JSON.stringify({protocol_version:1,id:requestId,operation,params:{project_revision:latest.project_revision,...params}})+'\n');
  });
  let snapshot = await preview('project.preview_reset', {scene_id:'main'});
  const hashes = new Set([snapshot.framebuffer.sha256]);
  for (let phase = 1; phase <= frameCount; phase++) {
    snapshot = await preview('project.preview_advance', {preview_revision:snapshot.preview_revision,elapsed_ms:400});
    assert.equal(snapshot.objects[0].playback.phase_index, phase % frameCount);
    hashes.add(snapshot.framebuffer.sha256);
  }
  assert.equal(hashes.size, frameCount, 'Distinct frames must actually render');
  assert(errors.every(error => error.code === 'PROJECT_REVISION_STALE' && ['project.preview_state','project.preview_scene_base'].includes(error.operation)), JSON.stringify(errors));
  assert(!(await evaluate("document.querySelector('.status-bar').textContent")).includes('PROJECT_REVISION_STALE'));
  window.webContents.invalidate(); await wait(250);
  fs.writeFileSync(path.join(output, 'fresh-sprite.png'), (await window.webContents.capturePage()).toPNG());
  const result = {projectPath,animation,objects:scene.objects,clips:latest.document.animations,animationCommands:commands.filter(c=>c.kind.includes('animation')),capabilities:hello?.scene_object_authoring,errors};
  fs.writeFileSync(path.join(output,'result.json'),JSON.stringify(result,null,2));
  console.log(`Native sprite loop: fresh GUI creation, ${frameCount} ordered frames, one-batch binding, undo/redo, save/reopen and distinct looping host frames passed`);
  if (errors.length) console.log('Cancelled stale placement-preview requests:', errors.length);
}).catch(error => {console.error(error);process.exitCode=1;}).finally(()=>{clearTimeout(watchdog);child?.kill();window?.destroy();app.exit(process.exitCode||0);});
