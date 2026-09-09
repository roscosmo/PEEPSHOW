const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const readline = require("node:readline");
const { spawn } = require("node:child_process");
const { app, BrowserWindow, ipcMain } = require("electron");

const root = path.resolve(__dirname, "../../..");
const output = path.resolve("dist/hierarchy-check");
fs.mkdirSync(output, { recursive: true });
app.setPath("userData", path.join(output, "profile"));
app.disableHardwareAcceleration();
app.on("window-all-closed", () => {});
const pending = new Map();
const operations = [];
let nextId = 0;
let child;
let window;
const wait = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
const watchdog = setTimeout(() => { child?.kill(); app.exit(1); }, 30000);

app.whenReady().then(async () => {
  child = spawn(process.env.PEEPSHOW_PYTHON || "python", ["-u", "tools/authoring/egg_tool.py", "service"], {
    cwd: root, windowsHide: true, stdio: ["pipe", "pipe", "pipe"],
  });
  readline.createInterface({ input: child.stdout }).on("line", (line) => {
    const response = JSON.parse(line);
    const handler = pending.get(response.id);
    pending.delete(response.id);
    if (response.ok) handler?.resolve(response.result);
    else handler?.reject(new Error(response.error.message));
  });
  child.stderr.on("data", (data) => process.stderr.write(data));
  ipcMain.handle("hierarchy:example", () => path.join(root, "examples/authoring/state_transition_slice.peepproj"));
  ipcMain.handle("hierarchy:service", (_, operation, params) => {
    assert(!/apply_commands|save|create|undo|redo/.test(operation), "Test must not modify the example");
    operations.push(operation);
    const id = String(++nextId);
    return new Promise((resolve, reject) => {
      pending.set(id, { resolve, reject });
      child.stdin.write(JSON.stringify({ protocol_version: 1, id, operation, params }) + "\n");
    });
  });
  window = new BrowserWindow({ width: 1440, height: 900, show: false,
    webPreferences: { preload: path.join(__dirname, "hierarchy-preload.cjs"), contextIsolation: true, sandbox: true, backgroundThrottling: false } });
  const evaluate = (code) => window.webContents.executeJavaScript(code);
  const click = async (selector, double = false) => {
    await evaluate(`(() => {
      const e = document.querySelector(${JSON.stringify(selector)});
      if (!e) throw new Error('Missing control: ' + ${JSON.stringify(selector)});
      e.dispatchEvent(new MouseEvent('click', { bubbles: true, detail: 1 }));
      ${double ? "e.dispatchEvent(new MouseEvent('click', { bubbles: true, detail: 2 })); e.dispatchEvent(new MouseEvent('dblclick', { bubbles: true, detail: 2 }));" : ""}
    })()`);
    await wait(150);
  };
  await window.loadURL(process.argv[2] || "http://127.0.0.1:5174");
  await wait(500);
  await evaluate("[...document.querySelectorAll('button')].find(e => e.textContent.trim() === 'Open example').click()");
  for (let i = 0; i < 40; i++) {
    if (await evaluate("!!document.querySelector('.scene-hierarchy-select')")) break;
    await wait(100);
  }
  assert(await evaluate("!!document.querySelector('.project-hierarchy-root + .scene-hierarchy')"));
  assert.equal(await evaluate("[...document.querySelectorAll('.project-pane summary')].some(e => e.textContent === 'Scenes')"), false);
  const resets = operations.filter((op) => op === "project.preview_reset").length;
  await click('.scene-hierarchy-select');
  const selected = await evaluate("document.querySelector('.scene-hierarchy-node.selected .scene-hierarchy-select').textContent");
  await click('.project-hierarchy-root > .hierarchy-disclosure-control');
  assert.equal(await evaluate("!!document.querySelector('.scene-hierarchy')"), false);
  assert(await evaluate("!!document.querySelector('.emulator-panel canvas')"));
  await click('.project-hierarchy-root > .hierarchy-disclosure-control');
  assert.equal(await evaluate("document.querySelector('.scene-hierarchy-node.selected .scene-hierarchy-select').textContent"), selected);
  const expanded = await evaluate("document.querySelector('.scene-hierarchy-row button').ariaExpanded");
  await click('.scene-hierarchy-select', true);
  assert.notEqual(await evaluate("document.querySelector('.scene-hierarchy-row button').ariaExpanded"), expanded);
  assert(await evaluate("!!document.querySelector('.scene-flow-pane')"));
  assert.equal(operations.filter((op) => op === "project.preview_reset").length, resets);
  await click('.scene-hierarchy-select', true);
  const states = '.states-branch > .hierarchy-branch-row > .hierarchy-branch-select';
  await click(states);
  assert.equal(await evaluate("document.querySelector('.states-branch .hierarchy-disclosure-control').ariaExpanded"), 'true');
  await click(states, true);
  assert.equal(await evaluate("document.querySelector('.states-branch .hierarchy-disclosure-control').ariaExpanded"), 'false');
  await click('.react-flow__node-sceneCard', true);
  assert(await evaluate("!!document.querySelector('.state-graph-pane')"));
  for (const width of [1440, 760]) {
    window.setSize(width, 900);
    await wait(1000);
    assert(await evaluate("!!document.querySelector('.project-hierarchy-root')"));
    const metrics = await evaluate(`['.project-pane', '.inspector-pane'].map(s => {
      const e = document.querySelector(s), css = getComputedStyle(e);
      return { scrollbar: css.scrollbarWidth, gutter: css.scrollbarGutter, overflow: e.scrollWidth > e.clientWidth + 1 };
    })`);
    for (const metric of metrics) {
      assert.equal(metric.scrollbar, 'thin');
      assert.equal(metric.gutter, 'stable');
      assert.equal(metric.overflow, false);
    }
    fs.writeFileSync(path.join(output, `hierarchy-${width}.png`), (await window.webContents.capturePage()).toPNG());
  }
  process.stdout.write("Hierarchy selection, expansion, scene navigation and scrollbar checks passed\n");
}).then(() => {
  clearTimeout(watchdog); child?.kill(); window?.destroy(); app.exit(0);
}).catch((error) => {
  process.stderr.write(error.stack + "\n");
  clearTimeout(watchdog); child?.kill(); window?.destroy(); app.exit(1);
});
