const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const readline = require('node:readline');
const { spawn } = require('node:child_process');
const { app, BrowserWindow, ipcMain } = require('electron');
const root = path.resolve(__dirname, '../../..');
const temp = fs.mkdtempSync(path.join(os.tmpdir(), 'peep-timers-'));
const projectPath = path.join(temp, 'timer.peepproj');
fs.cpSync(path.join(root, 'examples/authoring/native_v2_continuity.peepproj'), projectPath, { recursive: true });
const output = path.resolve('dist/timer-authoring-check');
fs.mkdirSync(output, { recursive: true });
app.setPath('userData', path.join(output, 'profile'));
app.disableHardwareAcceleration();
app.on('window-all-closed', () => {});
let child, window, latest, hello, id = 0;
const pending = new Map(), batches = [], errors = [];
const wait = ms => new Promise(resolve => setTimeout(resolve, ms));
const watchdog = setTimeout(() => { child?.kill(); app.exit(1); }, 90000);
app.whenReady().then(async () => {
  child = spawn(process.env.PEEPSHOW_PYTHON || 'python', ['-u', 'tools/authoring/egg_tool.py', 'service'], { cwd: root, windowsHide: true });
  readline.createInterface({ input: child.stdout }).on('line', line => {
    const response = JSON.parse(line), p = pending.get(response.id); pending.delete(response.id);
    if (response.ok) p?.resolve(response.result); else { errors.push(response.error); p?.reject(new Error(JSON.stringify(response.error))); }
  });
  child.stderr.on('data', data => process.stderr.write(data));
  const request = (operation, params) => new Promise((resolve, reject) => {
    const requestId = String(++id); pending.set(requestId, { resolve, reject });
    child.stdin.write(JSON.stringify({protocol_version:1,id:requestId,operation,params})+'\n');
  });
  ipcMain.handle('native:path', () => projectPath);
  ipcMain.handle('native:service', async (_, operation, params) => {
    assert.notEqual(operation,'project.build_package','This regression must not produce an egg');
    if (operation === 'project.apply_commands') batches.push(params.commands);
    const result = await request(operation, params);
    if (result.document) latest = result;
    if (operation === 'service.hello') hello = result;
    return result;
  });
  window = new BrowserWindow({ width: 1440, height: 1000, show: false, webPreferences: {
    preload:path.join(__dirname,'native-creation-preload.cjs'), sandbox:true, contextIsolation:true, offscreen:true, backgroundThrottling:false,
  } });
  const evaluate = code => window.webContents.executeJavaScript(code);
  const click = async selector => { await evaluate(`document.querySelector(${JSON.stringify(selector)}).click()`); await wait(350); };
  const button = async label => { await evaluate(`[...document.querySelectorAll('button')].find(e => e.textContent.trim()===${JSON.stringify(label)}).click()`); await wait(400); };
  const field = async (label, value, tag = 'input') => {
    await evaluate(`(() => {const e=document.querySelector('[aria-label="${label}"]');
      Object.getOwnPropertyDescriptor(${tag === 'select' ? 'HTMLSelectElement' : 'HTMLInputElement'}.prototype,'value').set.call(e,${JSON.stringify(String(value))});
      e.dispatchEvent(new Event('${tag === 'select' ? 'change' : 'input'}',{bubbles:true}));})()`); await wait(250);
    if (tag === 'input') { await evaluate(`document.querySelector('[aria-label="${label}"]').dispatchEvent(new FocusEvent('focusout',{bubbles:true}))`); await wait(300); }
  };
  const scene = () => latest.document.scenes[0];
  const run = (op, params={}) => request(op, {project_revision:latest.project_revision,...params});
  await window.loadURL('http://127.0.0.1:5174'); await wait(800);
  await button('Open project'); await wait(500); await button('Local logic');
  await button('Scene timer'); await field('Timer delay',1000); await button('Create timer');
  const bindingId = scene().event_bindings[0].binding_id;
  assert.equal(scene().event_handlers.length,1);
  assert.equal(await evaluate("document.querySelectorAll('.action-editor-list').length"),1);
  assert(await evaluate("![...document.querySelectorAll('h3')].some(e=>e.textContent.trim()==='Selected transition')"));
  assert(batches.some(batch => batch.some(c => c.kind === 'event_binding.add') && batch.some(c => c.kind === 'event_handler.add')));
  await field('Add effect','object.move_by','select'); await field('Effect 1 dx',5);
  assert.equal(scene().event_handlers[0].actions[0].dx,5);
  assert(batches.some(batch => batch.some(c => c.kind === 'object_actions.set' && c.owner_kind === 'handler')));
  let snapshot = await run('project.preview_reset',{scene_id:'main'});
  snapshot = await run('project.preview_advance',{preview_revision:snapshot.preview_revision,elapsed_ms:700});
  snapshot = await run('project.preview_input',{preview_revision:snapshot.preview_revision,logical_source:'BUTTON_A'});
  snapshot = await run('project.preview_advance',{preview_revision:snapshot.preview_revision,elapsed_ms:300});
  assert.equal(snapshot.scene.state_id,'marker_right');
  assert.equal(snapshot.objects.find(o => o.object_id === 'continuity_sprite').underlying.x,85);
  assert.equal(snapshot.objects.find(o => o.object_id === 'position_marker').effective.x,120);
  snapshot = await run('project.preview_advance',{preview_revision:snapshot.preview_revision,elapsed_ms:2000});
  assert.equal(snapshot.objects.find(o => o.object_id === 'continuity_sprite').underlying.x,85);
  await field('Add effect','restart_timer','select');
  assert.equal(scene().event_handlers[0].actions[1].timer_ref,bindingId);
  await click('[aria-label="Delete effect 2"]');
  await field('Timer start policy','action','select');
  await evaluate("document.querySelector('.state-transition-edge').dispatchEvent(new MouseEvent('click',{bubbles:true}))"); await wait(300);
  assert.equal(await evaluate("document.querySelectorAll('[aria-label=\"Timer delay\"]').length"),0);
  assert.equal(await evaluate("document.querySelectorAll('.action-editor-list').length"),1);
  await field('Add effect','start_timer','select');
  assert.equal(scene().routes[0].actions[0].kind,'start_timer');
  snapshot = await run('project.preview_reset',{scene_id:'main'});
  snapshot = await run('project.preview_advance',{preview_revision:snapshot.preview_revision,elapsed_ms:2000});
  assert.equal(snapshot.objects.find(o => o.object_id === 'continuity_sprite').underlying.x,80);
  snapshot = await run('project.preview_input',{preview_revision:snapshot.preview_revision,logical_source:'BUTTON_A'});
  snapshot = await run('project.preview_advance',{preview_revision:snapshot.preview_revision,elapsed_ms:1000});
  assert.equal(snapshot.objects.find(o => o.object_id === 'continuity_sprite').underlying.x,85);
  // Use a scene-entry timer so the button acts on an already running countdown.
  await field('Selected timer',bindingId,'select');
  await field('Timer start policy','scene_entry','select');
  await evaluate("document.querySelector('.state-transition-edge').dispatchEvent(new MouseEvent('click',{bubbles:true}))"); await wait(300);
  await field('Effect 1 kind','restart_timer','select');
  assert.equal(scene().routes[0].actions[0].timer_ref,bindingId);
  snapshot = await run('project.preview_reset',{scene_id:'main'});
  snapshot = await run('project.preview_advance',{preview_revision:snapshot.preview_revision,elapsed_ms:700});
  snapshot = await run('project.preview_input',{preview_revision:snapshot.preview_revision,logical_source:'BUTTON_A'});
  snapshot = await run('project.preview_advance',{preview_revision:snapshot.preview_revision,elapsed_ms:999});
  assert.equal(snapshot.objects.find(o => o.object_id === 'continuity_sprite').underlying.x,80,
    'Restart must discard the old deadline and wait the full duration');
  assert.equal(snapshot.timer_events.length,0);
  snapshot = await run('project.preview_advance',{preview_revision:snapshot.preview_revision,elapsed_ms:1});
  assert.equal(snapshot.objects.find(o => o.object_id === 'continuity_sprite').underlying.x,85);
  assert.equal(snapshot.timer_events.length,1);
  snapshot = await run('project.preview_advance',{preview_revision:snapshot.preview_revision,elapsed_ms:2000});
  assert.equal(snapshot.objects.find(o => o.object_id === 'continuity_sprite').underlying.x,85);
  assert.equal(snapshot.timer_events.length,0,'Restart must not introduce implicit repetition');
  await field('Effect 1 kind','cancel_timer','select');
  assert.equal(scene().routes[0].actions[0].timer_ref,bindingId);
  snapshot = await run('project.preview_reset',{scene_id:'main'});
  snapshot = await run('project.preview_advance',{preview_revision:snapshot.preview_revision,elapsed_ms:700});
  snapshot = await run('project.preview_input',{preview_revision:snapshot.preview_revision,logical_source:'BUTTON_A'});
  snapshot = await run('project.preview_advance',{preview_revision:snapshot.preview_revision,elapsed_ms:2000});
  assert.equal(snapshot.objects.find(o => o.object_id === 'continuity_sprite').underlying.x,80,
    'Cancel must prevent the pending handler from mutating the object');
  assert.equal(snapshot.timer_events.length,0);
  await field('Effect 1 kind','start_timer','select');
  await field('Selected timer',bindingId,'select');
  assert.equal(await evaluate("document.querySelectorAll('.action-editor-list').length"),1);
  assert(await evaluate("![...document.querySelectorAll('h3')].some(e=>e.textContent.trim()==='Selected transition')"));
  assert(await evaluate("[...document.querySelectorAll('button')].find(e=>e.textContent.trim()==='Delete timer').disabled"));
  await evaluate("document.querySelector('.state-transition-edge').dispatchEvent(new MouseEvent('click',{bubbles:true}))"); await wait(300);
  await click('[aria-label="Delete effect 1"]');
  assert.equal(scene().routes[0].actions.length,0);
  await field('Selected timer',bindingId,'select');
  assert.equal(scene().event_handlers[0].actions[0].dx,5,'Editing the route must not edit the timer handler');
  await evaluate("document.querySelector('.react-flow__node[data-id=\"start\"]').dispatchEvent(new MouseEvent('click',{bubbles:true}))"); await wait(250);
  assert.equal(await evaluate("document.querySelectorAll('.action-editor-list').length"),0);
  assert.equal(await evaluate("document.querySelectorAll('[aria-label=\"Timer delay\"]').length"),0);
  await field('Selected timer',bindingId,'select');
  assert.equal(await evaluate("document.querySelectorAll('.action-editor-list').length"),1);
  await field('Timer start policy','scene_entry','select');
  window.webContents.invalidate(); await wait(200);
  await evaluate("document.querySelector('.timer-inspector').scrollIntoView({block:'end'})"); await wait(200);
  fs.writeFileSync(path.join(output,'scene-timer.png'),(await window.webContents.capturePage()).toPNG());
  window.setSize(1280,900); await wait(250);
  assert(await evaluate("[...document.querySelectorAll('.timer-inspector input,.timer-inspector select,.timer-inspector button')].every(e=>{const r=e.getBoundingClientRect(),p=document.querySelector('.timer-inspector').getBoundingClientRect();return r.left>=p.left && r.right<=p.right+1 && r.right<=innerWidth;})"));
  fs.writeFileSync(path.join(output,'scene-timer-compact.png'),(await window.webContents.capturePage()).toPNG());
  window.setSize(1440,1000); await wait(200);
  await button('Save');
  const loaded = await request('project.load',{path:projectPath});
  assert.deepEqual(loaded.document.scenes, latest.document.scenes);
  // Keep the GUI revision synchronized after direct read-back validation.
  await button('Open project'); await wait(400); await button('Local logic');
  await field('Selected timer',bindingId,'select'); await button('Delete timer');
  assert.equal(scene().event_bindings.length,0); assert.equal(scene().event_handlers.length,0);
  assert(batches.some(batch => batch.findIndex(c => c.kind === 'event_handler.delete') >= 0
    && batch.findIndex(c => c.kind === 'event_binding.delete') > batch.findIndex(c => c.kind === 'event_handler.delete')));
  await click('button[title="Undo"]'); assert.equal(scene().event_handlers.length,1);
  await click('button[title="Redo"]'); assert.equal(scene().event_handlers.length,0);
  await evaluate("document.querySelector('.react-flow__node[data-id=\"start\"]').dispatchEvent(new MouseEvent('click',{bubbles:true}))"); await wait(300);
  await button('State timer'); await field('Timer delay',1000); await field('Timer destination','marker_right','select'); await button('Create timer');
  assert.equal(scene().event_bindings[0].event_type,'time.state_entry_elapsed');
  assert.equal(scene().event_handlers.length,0);
  assert.equal(await evaluate("document.querySelectorAll('.action-editor-list').length"),1);
  assert.equal(await evaluate("document.querySelectorAll('[aria-label=\"Timer destination\"]').length"),0);
  assert.equal(await evaluate("document.querySelectorAll('[aria-label=\"Timer delay\"]').length"),1);
  snapshot = await run('project.preview_reset',{scene_id:'main'});
  snapshot = await run('project.preview_advance',{preview_revision:snapshot.preview_revision,elapsed_ms:700});
  snapshot = await run('project.preview_input',{preview_revision:snapshot.preview_revision,logical_source:'BUTTON_A'});
  snapshot = await run('project.preview_input',{preview_revision:snapshot.preview_revision,logical_source:'BUTTON_B'});
  snapshot = await run('project.preview_advance',{preview_revision:snapshot.preview_revision,elapsed_ms:400});
  assert.equal(snapshot.scene.state_id,'start','Re-entry must restart the full state timer');
  snapshot = await run('project.preview_advance',{preview_revision:snapshot.preview_revision,elapsed_ms:600});
  assert.equal(snapshot.scene.state_id,'marker_right');
  await evaluate("document.querySelector('.react-flow__node[data-id=\"start\"]').dispatchEvent(new MouseEvent('click',{bubbles:true}))"); await wait(250);
  assert.equal(await evaluate("document.querySelectorAll('[aria-label=\"Timer delay\"]').length"),0);
  const beforeDraft = JSON.stringify(scene());
  await button('Scene timer'); await field('Timer delay',4321);
  await button('Assets'); await button('Local logic');
  assert.equal(await evaluate("document.querySelectorAll('[aria-label=\"Timer delay\"]').length"),0);
  await button('Scene timer');
  assert.equal(await evaluate("document.querySelector('[aria-label=\"Timer delay\"]').value"),'5000');
  await button('Cancel');
  assert.equal(JSON.stringify(scene()),beforeDraft,'Leaving a timer draft must not create or mutate records');
  assert.equal(hello.scene_object_authoring.egg_export,true);
  assert.equal(hello.scene_object_authoring.export_requires_project_readiness,true);
  const exportReady = latest.valid && !latest.build_issues.length
    && latest.document.scenes.every(item => {
      const capability = latest.scene_capabilities[item.scene_id];
      return capability.egg_export && capability.export_ready
        && capability.export_readiness_scope === 'whole_project'
        && capability.export_profile_id === hello.package_export.v2_profile.profile_id;
    });
  assert.equal(exportReady,true,'This single-scene timer project must satisfy the advertised export profile');
  assert.equal(await evaluate("[...document.querySelectorAll('button')].find(e=>e.textContent.trim()==='Build').disabled"),!exportReady);
  assert.deepEqual(errors,[]);
  console.log('GUI timers: single-target selection, draft cancellation, paired mutations, ordered actions, timer lifetimes, active restart/cancel, undo/redo, save/reload and backend export readiness passed');
}).catch(error => { console.error(error); process.exitCode = 1; }).finally(() => {
  clearTimeout(watchdog); child?.kill(); window?.destroy(); fs.rmSync(temp,{recursive:true,force:true}); app.exit(process.exitCode || 0);
});
