const assert = require("node:assert/strict");
const fs = require("node:fs");
const os = require("node:os");
const path = require("node:path");
const readline = require("node:readline");
const { spawn } = require("node:child_process");
const { app, BrowserWindow, ipcMain } = require("electron");

const root = path.resolve(__dirname, "../../..");
const output = path.resolve("dist/scene-objects-check");
const temporary = fs.mkdtempSync(path.join(os.tmpdir(), "peep-scene-objects-"));
const fixture = path.join(temporary, "mixed.peepproj");
fs.cpSync(path.join(root, "tools/authoring/peepshow_authoring/test_project.peepproj"), fixture, { recursive: true });
// A dedicated self-loop in the disposable fixture lets repeated inputs exercise ordered writes.
const sourcePath = path.join(fixture, 'scenes/state_demo.state.json');
const source = JSON.parse(fs.readFileSync(sourcePath, 'utf8'));
source.input_actions.push({ action_id: 'test_move', logical_source: 'BUTTON_B' });
source.routes.push({ route_id: 'test_move', action_ref: 'test_move', from_states: ['center'],
  target_state: 'center', guards: [], actions: [{ kind: 'request_render' }] });
source.reactive_wait_default.event_interests.push('test_move');
source.interaction_policy.meaningful_activity_actions.push('test_move');
fs.writeFileSync(sourcePath, JSON.stringify(source));
fs.mkdirSync(output, { recursive: true });
app.setPath("userData", path.join(output, "profile"));
app.disableHardwareAcceleration();
app.on("window-all-closed", () => {});
let child, window, documentResult;
let nextId = 0;
const pending = new Map();
const mutations = [];
const wait = (ms) => new Promise(resolve => setTimeout(resolve, ms));
const watchdog = setTimeout(() => { child?.kill(); app.exit(1); }, 90000);
function request(operation, params) {
  const id = String(++nextId);
  return new Promise((resolve, reject) => {
    pending.set(id, { resolve, reject });
    child.stdin.write(JSON.stringify({ protocol_version: 1, id, operation, params }) + "\n");
  });
}
app.whenReady().then(async () => {
  child = spawn(process.env.PEEPSHOW_PYTHON || "python", ["-u", "tools/authoring/egg_tool.py", "service"], {
    cwd: root, windowsHide: true, stdio: ["pipe", "pipe", "pipe"],
  });
  readline.createInterface({ input: child.stdout }).on("line", line => {
    const response = JSON.parse(line), handler = pending.get(response.id);
    pending.delete(response.id);
    if (response.ok) handler?.resolve(response.result);
    else handler?.reject(new Error(JSON.stringify(response.error)));
  });
  child.stderr.on("data", data => process.stderr.write(data));
  ipcMain.handle("hierarchy:example", () => fixture);
  ipcMain.handle("hierarchy:service", async (_, operation, params) => {
    if (/create|migration/.test(operation)) {
      mutations.push(operation);
      throw new Error("Unexpected UI mutation: " + operation);
    }
    if (operation === "project.load") assert.equal(params.path, fixture);
    if (operation === "project.apply_commands") {
      for (const command of params.commands) {
        assert(["object_actions.set", "object.add", "object.delete", "object.set_defaults", "object_override.set", "object_override.clear", "object.bind_animation", "object.clear_animation"].includes(command.kind));
        assert.equal(command.scene_id, "state_demo");
      }
      mutations.push(...params.commands);
    }
    const result = await request(operation, params).catch(error => { throw new Error(`${operation}: ${error.message}`); });
    if (["project.apply_commands", "project.undo", "project.redo", "project.save"].includes(operation)) documentResult = result;
    if (operation !== "project.load") return result;
    if (result.document.scenes.some(scene => scene.schema_version === 2)) { documentResult = result; return result; }
    // Fixture preparation is explicit and isolated from the application and user projects.
    const migration = { project_revision: result.project_revision, scene_id: "state_demo", accept_continuous_animation: true };
    const plan = await request("project.object_migration_preview", migration);
    assert(plan.can_apply, JSON.stringify(plan));
    const migrated = await request("project.object_migration_apply", { ...migration, source_revision: plan.plan.source_revision });
    const route = migrated.document.scenes.find(scene => scene.scene_id === 'state_demo').routes.find(route => route.target_state);
    const referenced = await request('project.apply_commands', { project_revision: migrated.project_revision,
      commands: [{ kind: 'object_actions.set', scene_id: 'state_demo', owner_kind: 'route', owner_id: route.route_id,
        actions: [...route.actions, { kind: 'object.set_visibility', object_ref: 'marker', visible: true }] }] });
    documentResult = { ...result, ...referenced };
    return documentResult;
  });
  window = new BrowserWindow({ width: 1440, height: 900, show: false,
    webPreferences: { preload: path.join(__dirname, "hierarchy-preload.cjs"), contextIsolation: true, sandbox: true, backgroundThrottling: false, offscreen: true } });
  const evaluate = code => window.webContents.executeJavaScript(code).catch(error => { throw new Error(`${code.slice(0, 350)}: ${error.message}`); });
  const click = async selector => {
    await evaluate(`(() => { const e = document.querySelector(${JSON.stringify(selector)}); if (!e) throw new Error('Missing ${selector}'); e.click(); })()`);
    await wait(250);
  };
  const button = async text => {
    await evaluate(`(() => { const e = [...document.querySelectorAll('button')].find(e => e.textContent.trim() === ${JSON.stringify(text)}); if (!e) throw new Error('Missing button ${text}'); e.click(); })()`);
    await wait(300);
  };
  await window.loadURL(process.argv[2] || "http://127.0.0.1:5174");
  await wait(600);
  await button("Open example");
  for (let i = 0; i < 60; i++) {
    if (await evaluate("!!document.querySelector('.host-preview-notice')")) break;
    await wait(100);
  }
  assert(await evaluate("!!document.querySelector('.host-preview-notice')"));
  assert(await evaluate("[...document.querySelectorAll('button')].find(e => e.textContent.trim() === 'Build').disabled"));
  assert.equal(await evaluate("document.querySelectorAll('.scene-hierarchy-node.selected .native-object-branch').length"),
    documentResult.placement_ownership.scenes.state_demo.objects.length);
  await click('.scene-hierarchy-node.selected .placement-tree-object');
  assert(await evaluate("!document.querySelector('[aria-label=\"Object X\"]').disabled"));
  assert(await evaluate("!!document.querySelector('.placement-element-box.selected')"));
  assert(await evaluate("document.querySelector('.emulator-timeline').textContent.includes('Scene time')"));
  assert.equal(await evaluate("document.body.textContent.includes('NaN')"), false);
  const pixelCount = await evaluate(`(() => { const c = document.querySelector('.panel-bezel canvas');
    const p = c.getContext('2d').getImageData(0,0,c.width,c.height).data;
    let dark=0; for(let i=0;i<p.length;i+=4) if(p[i]<80 && p[i+1]<80 && p[i+2]<80) dark++; return dark; })()`);
  assert(pixelCount > 0, "Placement framebuffer must contain rendered objects");
  const scene = () => documentResult.document.scenes.find(scene => scene.scene_id === "state_demo");
  const selectedId = await evaluate("document.querySelector('.placement-element-box.selected span').textContent");
  const object = () => scene().objects.find(object => object.object_id === selectedId);
  const setControl = async (label, value, tag = "input") => {
    await evaluate(`(() => {
      const e = document.querySelector('[aria-label="${label}"]'); e.focus();
      Object.getOwnPropertyDescriptor(${tag === "select" ? "HTMLSelectElement" : "HTMLInputElement"}.prototype, 'value').set.call(e, ${JSON.stringify(String(value))});
      e.dispatchEvent(new Event('${tag === "select" ? "change" : "input"}', { bubbles: true }));
    })()`);
    await wait(80);
    await evaluate(`document.querySelector('[aria-label="${label}"]').dispatchEvent(new FocusEvent('focusout', { bubbles: true }))`);
    await wait(450);
  };
  const originalX = object().defaults.x;
  await button('Delete object');
  assert(object(), 'An object referenced by a transition must survive a refused delete');
  assert(await evaluate("document.querySelector('footer').textContent.includes('OBJECT_IN_USE')"));
  await setControl("Object X", originalX - 2);
  assert.equal(object().defaults.x, originalX - 2);
  await click('button[title="Undo"]');
  assert.equal(object().defaults.x, originalX);
  await click('button[title="Redo"]');
  assert.equal(object().defaults.x, originalX - 2);
  const clip = object().animation_ref;
  assert(clip);
  await setControl("Object animation", "", "select");
  assert.equal(object().animation_ref, undefined);
  await setControl("Object animation", clip, "select");
  assert.equal(object().animation_ref, clip);
  for (const width of [1440, 760]) {
    window.setSize(width, 900);
    window.webContents.invalidate();
    await wait(800);
    assert(await evaluate("document.querySelector('.host-preview-notice').getBoundingClientRect().bottom <= document.querySelector('.workspace-grid').getBoundingClientRect().top + 1"));
    fs.writeFileSync(path.join(output, `objects-${width}.png`), (await window.webContents.capturePage()).toPNG());
  }
  window.setSize(1440, 900);
  await wait(300);
  const stateId = Object.keys(documentResult.placement_ownership.scenes.state_demo.states)[1];
  await setControl('Editing', stateId, 'select');
  await wait(500);
  const expected = documentResult.placement_ownership.scenes.state_demo.states[stateId].resolved_elements;
  const actual = await evaluate("[...document.querySelectorAll('.placement-element-box')].map(e => e.title)");
  for (const element of expected) assert(actual.includes(`${element.element_id}: ${element.x},${element.y} ${element.width}x${element.height}`));
  const override = () => scene().states.find(state => state.state_id === stateId).object_overrides.find(item => item.object_ref === selectedId);
  await setControl("Object X", originalX - 4);
  await setControl("Object Y", 20);
  assert.equal(override().x, originalX - 4);
  assert.equal(override().y, 20);
  const secondStateId = scene().states.find(state => state.state_id !== stateId).state_id;
  await setControl('Editing', secondStateId, 'select');
  await wait(300);
  await setControl("Object Y", 22);
  assert.equal(override().y, 20, 'Selecting a different target must not edit the preceding state');
  for (const id of [secondStateId]) {
    assert.equal(scene().states.find(state => state.state_id === id).object_overrides.find(item => item.object_ref === selectedId).y, 22);
  }
  await click('[aria-label="Object visible"]');
  for (const id of [secondStateId]) {
    assert.equal(scene().states.find(state => state.state_id === id).object_overrides.find(item => item.object_ref === selectedId).visible, false);
  }
  await click('[aria-label="Use scene default for visibility"]');
  await setControl("Object frame", "marker.phase_b", "select");
  for (const id of [secondStateId]) {
    assert.equal(scene().states.find(state => state.state_id === id).object_overrides.find(item => item.object_ref === selectedId).visual_ref, "marker.phase_b");
  }
  await click('[aria-label="Use scene default for frame"]');
  await click('[aria-label="Use scene default for X"]');
  assert.equal(override().x, originalX - 4);
  assert.equal(override().y, 20);
  assert(await evaluate("document.querySelector('[aria-label=\"Object animation\"]').disabled"));
  // A rejected out-of-bounds value must not issue a command.
  const count = mutations.length;
  await setControl("Object Y", 9999);
  assert.equal(mutations.length, count);
  const pointer = async (selector, type, x, y) => {
    await evaluate(`(() => {
      const overlay = document.querySelector('.placement-screen-overlay').getBoundingClientRect();
      const target = ${selector === "window" ? "window" : `document.querySelector(${JSON.stringify(selector)})`};
      target.dispatchEvent(new PointerEvent(${JSON.stringify(type)}, { bubbles: true, button: 0, pointerId: 1,
        clientX: overlay.left + ${x + 0.5} / 168 * overlay.width, clientY: overlay.top + ${y + 0.5} / 144 * overlay.height }));
    })()`);
    await wait(100);
  };
  // New geometry is authored through the same two-point tools, with exact visibility scope.
  await click('.placement-tool-palette .primitive-outline_rect');
  await pointer('.placement-screen-overlay', 'pointerdown', 30, 40);
  await pointer('window', 'pointermove', 40, 50);
  await pointer('window', 'pointerup', 40, 50);
  await wait(400);
  const createdId = mutations.findLast(command => command.kind === 'object.add').object.object_id;
  const created = () => scene().objects.find(item => item.object_id === createdId);
  assert.equal(created().defaults.visible, false);
  for (const state of scene().states) {
    const change = state.object_overrides.find(item => item.object_ref === createdId);
    assert.equal(change?.visible === true, state.state_id === secondStateId);
  }
  await click('[aria-label="Select and move objects"]');
  await pointer('.placement-element-box.selected', 'pointerdown', 35, 45);
  await pointer('window', 'pointermove', 45, 45);
  await pointer('window', 'pointerup', 45, 45);
  await wait(450);
  for (const id of [secondStateId]) {
    const change = scene().states.find(state => state.state_id === id).object_overrides.find(item => item.object_ref === createdId);
    assert.equal(change.x, 40);
    assert.equal(change.y, undefined, 'Horizontal movement must retain Y inheritance');
  }
  await evaluate("document.querySelector('.placement-element-box.selected').dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowRight', bubbles: true}))");
  await wait(450);
  assert.equal(scene().states.find(state => state.state_id === secondStateId).object_overrides.find(item => item.object_ref === createdId).x, 41);
  window.webContents.invalidate();
  await wait(250);
  fs.writeFileSync(path.join(output, 'canvas-scoped-object.png'), (await window.webContents.capturePage()).toPNG());
  await button('Remove from selected states');
  assert(created(), 'State removal must not delete the scene-owned object');
  for (const id of [secondStateId]) assert.equal(scene().states.find(state => state.state_id === id).object_overrides.find(item => item.object_ref === createdId).visible, false);
  await setControl('Editing', '', 'select');
  await button('Delete object');
  assert.equal(created(), undefined);
  assert(scene().states.every(state => !state.object_overrides.some(item => item.object_ref === createdId)));
  await click('button[title="Undo"]');
  assert(created(), 'Undo restores the object and its overrides');
  await click('button[title="Redo"]');
  assert.equal(created(), undefined);
  await click('[aria-label="Add sprite"]');
  await click('.placement-sprite-picker-group button');
  const spriteAdded = mutations.findLast(command => command.kind === 'object.add');
  assert.equal(spriteAdded.object.kind, 'sprite');
  assert.equal(spriteAdded.visible_in_states, undefined);
  assert.equal(scene().objects.find(item => item.object_id === spriteAdded.object.object_id).defaults.visible, true);
  assert(!scene().waiting_visuals, 'Sprite creation must not fabricate legacy animation records');
  const sprite = () => scene().objects.find(item => item.object_id === spriteAdded.object.object_id);
  const spriteX = sprite().defaults.x, spriteY = sprite().defaults.y;
  await pointer('.placement-element-box.selected', 'pointerdown', spriteX + 2, spriteY + 2);
  await pointer('window', 'pointermove', spriteX + 7, spriteY + 9);
  await pointer('window', 'pointerup', spriteX + 7, spriteY + 9);
  await wait(450);
  assert.equal(sprite().defaults.x, spriteX + 5);
  assert.equal(sprite().defaults.y, spriteY + 7);
  await button("Save");
  const savedScene = JSON.stringify(scene());
  await button("Open example");
  await wait(600);
  assert.equal(JSON.stringify(scene()), savedScene);
  assert.equal(await evaluate("document.querySelector('footer').textContent.includes('PROJECT_REVISION_STALE')"), false);
  await button("Local logic");
  assert(await evaluate("!!document.querySelector('.state-graph-pane .react-flow__node.draggable')"));
  await click('.state-graph-pane .react-flow__pane');
  const overviewCount = await evaluate("[...document.querySelectorAll('.scene-overview-card > div')].find(e => e.querySelector('span')?.textContent === 'Scene objects').querySelector('strong').textContent");
  assert.equal(Number(overviewCount), scene().objects.length);
  assert(await evaluate("![...document.querySelectorAll('.inspector-section h3')].some(e => ['Screen layouts','Waiting animations'].includes(e.textContent.trim()))"));
  window.webContents.invalidate(); await wait(200);
  fs.writeFileSync(path.join(output,'v2-scene-inspector.png'),(await window.webContents.capturePage()).toPNG());
  await evaluate("document.querySelector('.react-flow__node[data-id=\"center\"]').dispatchEvent(new MouseEvent('click',{bubbles:true}))"); await wait(200);
  assert.equal(Number(await evaluate("[...document.querySelectorAll('.state-summary-card > div')].find(e => e.querySelector('span')?.textContent === 'Objects overridden').querySelector('strong').textContent")), scene().states.find(s=>s.state_id==='center').object_overrides.length);
  assert(await evaluate("!document.querySelector('.selected-record').textContent.includes('Waiting animation')"));
  const selectTestRoute = () => click('.react-flow__node[data-id="center"] [aria-label="B trigger, configured"]');
  await selectTestRoute();
  const actionRoute = () => scene().routes.find(route => route.route_id === 'test_move');
  await setControl('Add effect', 'object.move_by', 'select');
  await setControl('Effect 1 object', spriteAdded.object.object_id, 'select');
  await setControl('Effect 1 dx', 4);
  await click('[aria-label="Effect 1 use dy"]');
  await setControl('Effect 1 dy', 3);
  assert.deepEqual(actionRoute().actions[1], { kind: 'object.move_by', object_ref: spriteAdded.object.object_id, dx: 4, dy: 3 });
  const runInputs = async (count) => {
    const project_revision = documentResult.project_revision;
    let snapshot = await request('project.preview_reset', { project_revision, scene_id: 'state_demo', state_id: 'center' });
    for (let i = 0; i < count; i++) snapshot = await request('project.preview_input', {
      project_revision, preview_revision: snapshot.preview_revision, logical_source: 'BUTTON_B',
    });
    return snapshot.objects.find(object => object.object_id === spriteAdded.object.object_id);
  };
  let moved = await runInputs(2);
  assert.equal(moved.underlying.x, sprite().defaults.x + 8);
  assert.equal(moved.underlying.y, sprite().defaults.y - 6);
  await setControl('Add effect', 'object.set_position', 'select');
  await setControl('Effect 2 object', spriteAdded.object.object_id, 'select');
  await setControl('Effect 2 x', 30);
  moved = await runInputs(1);
  assert.equal(moved.underlying.x, 30);
  await click('[aria-label="Move effect 2 earlier"]');
  moved = await runInputs(1);
  assert.equal(moved.underlying.x, 34);
  assert.equal(actionRoute().actions[0].kind, 'request_render', 'Hidden actions retain their position');
  await click('[aria-label="Delete effect 1"]');
  assert.equal(actionRoute().actions.length, 2);
  await click('button[title="Undo"]');
  assert.equal(actionRoute().actions.length, 3);
  await click('button[title="Redo"]');
  assert.equal(actionRoute().actions.length, 2);
  await selectTestRoute();
  await setControl('Add effect', 'object.clear_frame', 'select');
  assert.equal(actionRoute().actions.at(-1).kind, 'object.clear_frame');
  await setControl('Add effect', 'object.set_frame', 'select');
  assert.equal(actionRoute().actions.at(-1).kind, 'object.set_frame');
  assert(actionRoute().actions.at(-1).frame_ref);
  await setControl('Add effect', 'object.set_visibility', 'select');
  await setControl('Effect 4 object', spriteAdded.object.object_id, 'select');
  await click('.action-editor-list .logic-toggle input');
  moved = await runInputs(1);
  assert.equal(moved.underlying.visible, false);
  await click('.react-flow__node[data-id="center"] [aria-label="A trigger, configured"]');
  assert(await evaluate("[...(document.querySelector('[aria-label=\"Add effect\"]')?.options ?? [])].every(option => !option.value || option.value === 'play_sfx')"));
  await selectTestRoute();
  for (const width of [1440, 760]) {
    window.setSize(width, 1000); window.webContents.invalidate(); await wait(500);
    assert(await evaluate("[...document.querySelectorAll('.inspector-pane input,.inspector-pane select')].every(e => e.getBoundingClientRect().right <= document.querySelector('.inspector-pane').getBoundingClientRect().right)"));
    fs.writeFileSync(path.join(output, `object-actions-${width}.png`), (await window.webContents.capturePage()).toPNG());
  }
  window.setSize(1440, 900);
  await button('Save');
  const savedActions = JSON.stringify(actionRoute().actions);
  await button('Open example'); await wait(500);
  assert.equal(JSON.stringify(actionRoute().actions), savedActions);
  await button('Local logic');
  // Changing selection to a legacy scene restores the existing graph editing controls.
  const legacy = documentResult.document.scenes.find(scene => scene.schema_version === 1);
  assert(legacy, "Fixture must contain both versions");
  await evaluate(`[...document.querySelectorAll('.scene-hierarchy-select')].find(e => e.textContent.includes(${JSON.stringify(legacy.display_name)})).click()`);
  await wait(400);
  assert(await evaluate("!!document.querySelector('.state-graph-pane .react-flow__node.draggable')"));
  await click('.state-graph-pane .react-flow__pane');
  assert(await evaluate("['Screen layouts','Waiting animations'].every(label => [...document.querySelectorAll('.inspector-section h3')].some(e=>e.textContent.trim()===label))"));
  assert(await evaluate("[...document.querySelectorAll('.scene-overview-card span')].some(e=>e.textContent==='Screen items')"));
  assert(mutations.some(command => command.kind === "object_override.clear"));
  console.log("Mixed-version GUI editing checks passed; placement pixels:", pixelCount);
}).catch(error => { console.error(error); process.exitCode = 1; }).finally(() => {
  clearTimeout(watchdog);
  child?.kill(); window?.destroy();
  fs.rmSync(temporary, { recursive: true, force: true });
  app.exit(process.exitCode || 0);
});
