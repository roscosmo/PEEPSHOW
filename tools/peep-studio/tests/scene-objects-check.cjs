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
fs.mkdirSync(output, { recursive: true });
app.setPath("userData", path.join(output, "profile"));
app.disableHardwareAcceleration();
app.on("window-all-closed", () => {});
let child, window, documentResult;
let nextId = 0;
const pending = new Map();
const mutations = [];
const wait = (ms) => new Promise(resolve => setTimeout(resolve, ms));
const watchdog = setTimeout(() => { child?.kill(); app.exit(1); }, 45000);
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
        assert(["object.set_defaults", "object_override.set", "object_override.clear", "object.bind_animation", "object.clear_animation"].includes(command.kind));
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
    documentResult = { ...result, ...migrated };
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
  assert.equal(await evaluate("document.querySelector('.scene-hierarchy-node.selected .base-branch code').textContent"),
    String(documentResult.placement_ownership.scenes.state_demo.objects.length));
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
  await evaluate(`[...document.querySelectorAll('.scene-hierarchy-node.selected .state-branch .hierarchy-branch-select')].find(e => e.textContent.includes(${JSON.stringify(stateId)})).click()`);
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
  const secondStateName = scene().states.find(state => state.state_id === secondStateId).display_name;
  await evaluate(`[...document.querySelectorAll('.scene-hierarchy-node.selected .state-branch .hierarchy-branch-select')].find(e => e.querySelector('strong')?.textContent === ${JSON.stringify(secondStateName)}).dispatchEvent(new MouseEvent('click', { bubbles: true, ctrlKey: true, detail: 1 }))`);
  await wait(300);
  await setControl("Object Y", 22);
  for (const id of [stateId, secondStateId]) {
    assert.equal(scene().states.find(state => state.state_id === id).object_overrides.find(item => item.object_ref === selectedId).y, 22);
  }
  await click('[aria-label="Object visible"]');
  for (const id of [stateId, secondStateId]) {
    assert.equal(scene().states.find(state => state.state_id === id).object_overrides.find(item => item.object_ref === selectedId).visible, false);
  }
  await click('[aria-label="Clear visible override"]');
  await setControl("Object frame", "marker.phase_b", "select");
  for (const id of [stateId, secondStateId]) {
    assert.equal(scene().states.find(state => state.state_id === id).object_overrides.find(item => item.object_ref === selectedId).visual_ref, "marker.phase_b");
  }
  await click('[aria-label="Clear visual_ref override"]');
  await click('[aria-label="Clear x override"]');
  assert.equal(override().x, undefined);
  assert.equal(override().y, 22);
  assert(await evaluate("document.querySelector('[aria-label=\"Object animation\"]').disabled"));
  // A rejected out-of-bounds value must not issue a command.
  const count = mutations.length;
  await setControl("Object Y", 9999);
  assert.equal(mutations.length, count);
  await button("Save");
  const savedScene = JSON.stringify(scene());
  await button("Open example");
  await wait(600);
  assert.equal(JSON.stringify(scene()), savedScene);
  assert.equal(await evaluate("document.querySelector('footer').textContent.includes('PROJECT_REVISION_STALE')"), false);
  await button("Local logic");
  assert(await evaluate("[...document.querySelectorAll('.state-graph-pane .react-flow__node')].every(e => !e.classList.contains('draggable'))"));
  // Changing selection to a legacy scene restores the existing graph editing controls.
  const legacy = documentResult.document.scenes.find(scene => scene.schema_version === 1);
  assert(legacy, "Fixture must contain both versions");
  await evaluate(`[...document.querySelectorAll('.scene-hierarchy-select')].find(e => e.textContent.includes(${JSON.stringify(legacy.display_name)})).click()`);
  await wait(400);
  assert(await evaluate("!!document.querySelector('.state-graph-pane .react-flow__node.draggable')"));
  assert(mutations.some(command => command.kind === "object_override.clear"));
  console.log("Mixed-version GUI editing checks passed; placement pixels:", pixelCount);
}).catch(error => { console.error(error); process.exitCode = 1; }).finally(() => {
  clearTimeout(watchdog);
  child?.kill(); window?.destroy();
  fs.rmSync(temporary, { recursive: true, force: true });
  app.exit(process.exitCode || 0);
});
