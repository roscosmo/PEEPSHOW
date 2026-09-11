const assert = require('node:assert/strict'), fs = require('node:fs'), os = require('node:os'), path = require('node:path');
const readline = require('node:readline'), { spawn } = require('node:child_process');
const { app, BrowserWindow, ipcMain, nativeImage } = require('electron');
const root = path.resolve(__dirname, '../../..');
const frameCount = process.argv.includes('--ten') ? 10 : 4;
const grid = process.argv.includes('--grid');
const sheetColumns = grid ? 2 : frameCount;
const sheetRows = frameCount / sheetColumns;
const workflow = process.argv.includes('--workflow');
const reloadRace = process.argv.includes('--reload-race');
const projectPath = path.join(fs.mkdtempSync(path.join(os.tmpdir(), 'peep-workflow-audit-')), 'fresh.peepproj');
const output = path.resolve('dist/native-sprite-loop-check');
fs.mkdirSync(output, { recursive: true });
app.setPath('userData', path.join(output, 'profile'));
app.disableHardwareAcceleration(); app.on('window-all-closed', () => {});
let child, window, latest, hello, id = 0;
let loadPending = false, holdNextPreview = false, releaseHeldPreview;
let failNextLoad = false;
const previewsDuringLoad = [];
const pending = new Map(), errors = [], commands = [], batches = [];
const wait = ms => new Promise(resolve => setTimeout(resolve, ms));
const watchdog = setTimeout(() => { child?.kill(); app.exit(1); }, workflow ? 90000 : 60000);
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
    if (operation === 'project.load' && failNextLoad) {
      failNextLoad = false;
      throw new Error('Load failure diagnostic');
    }
    const placementRequest = ['project.preview_state','project.preview_scene_base'].includes(operation);
    if (placementRequest && loadPending) previewsDuringLoad.push({operation,params});
    const held = placementRequest && holdNextPreview;
    if (held) holdNextPreview = false;
    if (operation === 'project.load') loadPending = true;
    if (operation === 'project.apply_commands') { commands.push(...params.commands); batches.push(params.commands); }
    const requestId = String(++id);
    const result = await new Promise((resolve, reject) => {
      pending.set(requestId, { resolve, reject, operation });
      child.stdin.write(JSON.stringify({ protocol_version: 1, id: requestId, operation, params }) + '\n');
    });
    if (result.document) latest = result;
    if (operation === 'service.hello') hello = result;
    if (operation === 'project.load') {
      if (reloadRace) await wait(400);
      loadPending = false;
    }
    if (held) {
      const outcome = await new Promise(resolve => { releaseHeldPreview = resolve; });
      if (outcome === 'reject') throw new Error('Obsolete preview diagnostic');
      return {...result, framebuffer:{...result.framebuffer,
        data_base64:Buffer.alloc(result.framebuffer.size_bytes,255).toString('base64')}};
    }
    return result;
  });
  ipcMain.handle('audit:png', () => {
    const width = sheetColumns * 16, height = sheetRows * 16;
    const pixels = Buffer.alloc(width * height * 4, 255);
    for (let y = 3; y < 13; y++) for (let frame = 0; frame < frameCount; frame++) for (let x = 2; x < 4 + frame; x++) {
      const i = ((y + Math.floor(frame / sheetColumns) * 16) * width + (frame % sheetColumns) * 16 + x) * 4; pixels[i] = pixels[i + 1] = pixels[i + 2] = 0;
    }
    const destination = path.join(projectPath, 'assets', 'audit.png');
    fs.mkdirSync(path.dirname(destination), { recursive: true });
    fs.writeFileSync(destination, nativeImage.createFromBitmap(pixels, { width, height }).toPNG());
    return { assetId: 'audit', displayName: 'Sprite loop test', sourcePath: 'assets/audit.png', width, height };
  });
  window = new BrowserWindow({ width: 1440, height: 1000, show: false, webPreferences: {
    preload: path.join(__dirname, 'native-sprite-loop-preload.cjs'), sandbox: true, contextIsolation: true, offscreen: true, backgroundThrottling: false,
  } });
  const evaluate = code => window.webContents.executeJavaScript(code);
  const button = async label => { await evaluate(`(() => { const e=[...document.querySelectorAll('button')].find(e=>e.textContent.trim()===${JSON.stringify(label)}); if(!e) throw Error('Missing button: '+${JSON.stringify(label)}); if(e.disabled) throw Error('Disabled button'); e.click(); })()`); await wait(450); };
  const click = async selector => { await evaluate(`document.querySelector(${JSON.stringify(selector)}).click()`); await wait(450); };
  await window.loadURL('http://127.0.0.1:5174'); await wait(800);
  if (process.argv.includes('--settings')) {
    await evaluate(`localStorage.setItem('peep-studio.editor-preferences.v1','invalid json')`);
    await window.loadURL('http://127.0.0.1:5174'); await wait(800);
    await click('[aria-label="Settings"]');
    assert(await evaluate("document.querySelector('.placement-view-settings input').checked"));
    await click('.placement-view-settings input');
    await click('[aria-label="Close settings"]');
    assert.equal(commands.length, 0);
    await window.loadURL('http://127.0.0.1:5174'); await wait(800);
    await click('[aria-label="Settings"]');
    assert.equal(await evaluate("document.querySelector('.placement-view-settings input').checked"), false);
    await evaluate("window.dispatchEvent(new KeyboardEvent('keydown',{key:'Escape',bubbles:true}))"); await wait(100);
    assert.equal(await evaluate("document.querySelector('[aria-label=Settings]').getAttribute('aria-pressed')"), 'false');
  }
  await button('New project');
  assert.equal(latest.document.scenes[0].schema_version, 2);
  if (process.argv.includes('--settings')) {
    const revision = latest.project_revision;
    const selection = await evaluate("[...document.querySelectorAll('.scene-hierarchy-node.selected')].map(e=>e.textContent)");
    await click('[aria-label="Settings"]');
    await button('Placement');
    assert.equal(await evaluate("document.querySelector('[aria-label=Settings]').getAttribute('aria-pressed')"), 'true');
    assert.equal(await evaluate("document.querySelector('.placement-view-settings input').checked"), false);
    window.webContents.invalidate(); await wait(200);
    fs.writeFileSync(path.join(output,'settings.png'),(await window.webContents.capturePage()).toPNG());
    await click('[aria-label="Close settings"]');
    assert.deepEqual(await evaluate("[...document.querySelectorAll('.scene-hierarchy-node.selected')].map(e=>e.textContent)"), selection);
    assert.equal(latest.project_revision, revision);
    console.log('Settings: no-project access, corrupt storage fallback, reload persistence, workspace switching and selection preservation passed');
  }
  await button('Assets'); await button('Choose PNG');
  const setGrid = async (axis, value) => {
    await evaluate(`(() => {const e=document.querySelector('[aria-label="Sprite sheet ${axis}"]'); Object.getOwnPropertyDescriptor(HTMLInputElement.prototype,'value').set.call(e,${JSON.stringify(String(value))}); e.dispatchEvent(new Event('input',{bubbles:true}));})()`); await wait(100);
  };
  for (const invalid of ['', '0', '1.5', '3', '257']) {
    await setGrid('columns', invalid);
    assert(await evaluate("[...document.querySelectorAll('button')].find(e=>e.textContent.trim()==='Import').disabled"));
  }
  await setGrid('columns', sheetColumns);
  await setGrid('rows', sheetRows);
  assert.match(await evaluate("document.querySelector('.asset-import-result').textContent"), /16x16 px each/);
  window.webContents.invalidate(); await wait(200);
  fs.writeFileSync(path.join(output,'sheet-import.png'),(await window.webContents.capturePage()).toPNG());
  await button('Import');
  assert.equal(commands.find(c=>c.kind==='asset.upsert').asset.frames.length, frameCount);
  assert.deepEqual(commands.find(c=>c.kind==='asset.upsert').asset.frames.map(f=>f.source_rect),
    Array.from({length:frameCount},(_,i)=>({x:(i%sheetColumns)*16,y:Math.floor(i/sheetColumns)*16,width:16,height:16})));
  await click('.asset-frame-gallery button');
  assert(await evaluate("!!document.querySelector('.asset-inspector-preview')"));
  const tabRevision = latest.project_revision;
  await click('#asset-tab-audio');
  assert.equal(await evaluate("document.querySelector('.asset-frame-gallery')"), null);
  assert.equal(await evaluate("document.querySelector('.asset-inspector-preview')"), null);
  assert.match(await evaluate("document.querySelector('.asset-workspace-empty').textContent"), /No audio assets/);
  assert.equal(await evaluate("[...document.querySelectorAll('button')].some(e=>e.textContent.trim()==='Choose PNG')"), false);
  await evaluate("document.querySelector('#asset-tab-audio').dispatchEvent(new KeyboardEvent('keydown',{key:'ArrowLeft',bubbles:true}))"); await wait(100);
  assert.equal(await evaluate("document.activeElement.id"), 'asset-tab-sprite');
  assert.equal(await evaluate("document.querySelector('.asset-frame-gallery button.selected')"), null);
  assert.equal(latest.project_revision, tabRevision);
  window.webContents.invalidate(); await wait(200);
  fs.writeFileSync(path.join(output,'asset-tabs.png'),(await window.webContents.capturePage()).toPNG());
  console.log('Asset tabs: filtered controls, empty state, cleared selection and keyboard navigation passed');
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
  let rightId, markerId, timerObjectId;
  const currentScene = () => latest.document.scenes[0];
  if (workflow) {
    const field = async (label, value, tag = 'input') => {
      const selector = `[aria-label="${label}"]`;
      await evaluate(`(() => {const e=document.querySelector(${JSON.stringify(selector)});
        Object.getOwnPropertyDescriptor(${tag === 'select' ? 'HTMLSelectElement' : 'HTMLInputElement'}.prototype,'value').set.call(e,${JSON.stringify(String(value))});
        e.dispatchEvent(new Event('${tag === 'select' ? 'change' : 'input'}',{bubbles:true}));})()`); await wait(150);
      if (tag === 'input') await evaluate(`document.querySelector(${JSON.stringify(selector)}).dispatchEvent(new FocusEvent('focusout',{bubbles:true}))`);
      await wait(300);
    };
    await button('Local logic'); await click('button[title="Add state"]');
    rightId = currentScene().states.find(state => state.state_id !== 'start').state_id;
    await field('State name', 'Right');
    for (const [source, target, input] of [['start', rightId, 'BUTTON_A'], [rightId, 'start', 'BUTTON_B']]) {
      await click(`.react-flow__node[data-id="${source}"] [data-handleid="new-physical-trigger:${input}"]`);
      await click(`.react-flow__node[data-id="${target}"] [data-handleid="entry-top-left:top"]`);
      await button('Create transition');
    }
    assert.equal(currentScene().routes.length, 2);
    await button('Placement');
    await click('.scene-hierarchy-node.selected .base-branch .hierarchy-branch-select');
    const pointer = async (selector, type, x, y) => {
      await evaluate(`(() => {const r=document.querySelector('.placement-screen-overlay').getBoundingClientRect();
        const target=${selector === 'window' ? 'window' : `document.querySelector(${JSON.stringify(selector)})`};
        target.dispatchEvent(new PointerEvent(${JSON.stringify(type)}, {bubbles:true,button:0,pointerId:1,
          clientX:r.left+${x + 0.5}/168*r.width,clientY:r.top+${y + 0.5}/144*r.height}));})()`); await wait(100);
    };
    const draw = async (x, y) => {
      const ids = new Set(currentScene().objects.map(object => object.object_id));
      await click('.placement-tool-palette .primitive-filled_rect');
      await pointer('.placement-screen-overlay', 'pointerdown', x, y);
      await pointer('window', 'pointermove', x + 15, y + 15);
      await pointer('window', 'pointerup', x + 15, y + 15); await wait(400);
      const object = currentScene().objects.find(object => !ids.has(object.object_id));
      assert(object, 'Drawing must create a scene-owned object');
      return object.object_id;
    };
    markerId = await draw(32, 100);
    await evaluate(`[...document.querySelectorAll('.scene-hierarchy-node.selected .state-branch .hierarchy-branch-select')].find(e=>e.textContent.includes(${JSON.stringify(rightId)})).click()`); await wait(400);
    await field('Object X', 120);
    assert.deepEqual(currentScene().states.find(state => state.state_id === rightId).object_overrides,
      [{object_ref:markerId,x:120}]);
    assert.deepEqual(currentScene().states.find(state => state.state_id === 'start').object_overrides, []);
    await click('.scene-hierarchy-node.selected .base-branch .hierarchy-branch-select');
    timerObjectId = await draw(76, 76); await click('[aria-label="Object visible"]');
    assert.equal(currentScene().objects.find(object => object.object_id === timerObjectId).defaults.visible, false);
    await button('Local logic'); await click('.react-flow__pane');
    await button('Scene timer'); await field('Timer delay', 2000); await button('Create timer');
    const bindingId = currentScene().event_bindings[0].binding_id;
    assert(batches.some(batch => batch.some(c=>c.kind==='event_binding.add') && batch.some(c=>c.kind==='event_handler.add')));
    await click('button[title="Undo"]');
    assert.equal(currentScene().event_bindings?.length ?? 0, 0); assert.equal(currentScene().event_handlers?.length ?? 0, 0);
    await click('button[title="Redo"]'); await field('Selected timer', bindingId, 'select');
    await field('Add effect', 'object.set_visibility', 'select');
    await field('Effect 1 object', timerObjectId, 'select');
    assert.deepEqual(currentScene().event_handlers[0].actions,[{kind:'object.set_visibility',object_ref:timerObjectId,visible:true}]);
    assert.equal(currentScene().event_handlers[0].target_state, undefined);
    assert.equal(currentScene().objects.length, 3);
    assert(currentScene().states.every(state=>state.object_overrides.every(item=>item.object_ref===markerId)));
    window.webContents.invalidate(); await wait(200);
    fs.writeFileSync(path.join(output,'workflow-timer.png'),(await window.webContents.capturePage()).toPNG());
  }
  await button('Save'); const saved = JSON.stringify(latest.document.scenes);
  if (reloadRace) {
    for (const outcome of ['resolve','reject']) {
      holdNextPreview = true; releaseHeldPreview = undefined;
      await click('.scene-hierarchy-node.selected .state-branch .hierarchy-branch-select');
      assert.equal(typeof releaseHeldPreview, 'function', 'Must hold an actual state preview response');
      await button('Open project'); await wait(400);
      assert.equal(JSON.stringify(latest.document.scenes), saved);
      const before = await evaluate("document.querySelector('.panel-bezel canvas').toDataURL()");
      releaseHeldPreview(outcome); await wait(250);
      assert.equal(await evaluate("document.querySelector('.panel-bezel canvas').toDataURL()"),before,'Obsolete response must not replace current pixels');
      assert(!(await evaluate("document.querySelector('.status-bar').textContent")).includes('Obsolete preview diagnostic'));
    }
    assert.deepEqual(previewsDuringLoad, [], 'No placement requests may be dispatched during project replacement');
    failNextLoad = true;
    await button('Open project'); await wait(400);
    assert.equal(failNextLoad, false, 'The failed load must have been attempted');
  }
  const revisionBeforeReload = latest.project_revision;
  await button('Open project'); assert.equal(JSON.stringify(latest.document.scenes), saved);
  assert(latest.project_revision > revisionBeforeReload, 'Reopen must reach the service even after a failed load');
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
  if (workflow) {
    snapshot = await preview('project.preview_reset', {scene_id:'main',state_id:'start'});
    const advance = elapsed_ms => preview('project.preview_advance', {preview_revision:snapshot.preview_revision,elapsed_ms});
    const input = logical_source => preview('project.preview_input', {preview_revision:snapshot.preview_revision,logical_source});
    const object = id => snapshot.objects.find(item=>item.object_id===id);
    const spriteId = currentScene().objects.find(item=>item.animation_ref===clip.animation_id).object_id;
    snapshot = await advance(650);
    const beforeA = object(spriteId).playback;
    snapshot = await input('BUTTON_A');
    assert.equal(snapshot.scene.state_id,rightId); assert.deepEqual(object(spriteId).playback,beforeA);
    assert.equal(object(markerId).effective.x,120); assert.equal(object(markerId).underlying.x,32);
    snapshot = await advance(300); const beforeB = object(spriteId).playback;
    snapshot = await input('BUTTON_B');
    assert.equal(snapshot.scene.state_id,'start'); assert.deepEqual(object(spriteId).playback,beforeB);
    assert.equal(object(markerId).effective.x,32);
    snapshot = await advance(1049);
    assert.equal(snapshot.timeline.elapsed_ms,1999); assert.equal(object(timerObjectId).effective.visible,false);
    snapshot = await advance(1);
    assert.equal(snapshot.timer_events.length,1); assert.equal(object(timerObjectId).effective.visible,true);
    assert.equal(object(spriteId).playback.phase_index,1);
    assert.equal(snapshot.scene.state_id,'start');
    snapshot = await input('BUTTON_A'); snapshot = await advance(6000);
    assert.equal(snapshot.timer_events.length,0); assert.equal(object(timerObjectId).effective.visible,true);
    assert.equal(object(timerObjectId).effective.x,76); assert.equal(object(timerObjectId).effective.y,76);
    console.log('Fresh GUI workflow: two states, A/B routes, independent X override, atomic timer undo/redo, exact one-shot expiry and animation continuity after save/reopen passed');
  }
  assert.deepEqual(errors, [], 'The workflow must not emit stale or other service errors');
  assert(!(await evaluate("document.querySelector('.status-bar').textContent")).includes('PROJECT_REVISION_STALE'));
  window.webContents.invalidate(); await wait(250);
  fs.writeFileSync(path.join(output, 'fresh-sprite.png'), (await window.webContents.capturePage()).toPNG());
  const result = {projectPath,workflow,animation,objects:currentScene().objects,clips:latest.document.animations,animationCommands:commands.filter(c=>c.kind.includes('animation')),capabilities:hello?.scene_object_authoring,errors};
  fs.writeFileSync(path.join(output,'result.json'),JSON.stringify(result,null,2));
  console.log(`Native sprite loop: fresh GUI creation, ${frameCount} ordered frames, one-batch binding, undo/redo, save/reopen and distinct looping host frames passed`);
  if (reloadRace) console.log('Delayed load: no previews during replacement; old replies ignored; reopening recovers after a failed load');
}).catch(error => {console.error(error);process.exitCode=1;}).finally(()=>{clearTimeout(watchdog);child?.kill();window?.destroy();app.exit(process.exitCode||0);});
