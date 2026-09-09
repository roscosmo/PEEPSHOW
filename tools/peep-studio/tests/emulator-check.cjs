const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const { app, BrowserWindow } = require("electron");
app.disableHardwareAcceleration();
app.on("window-all-closed", () => {});
const base = process.argv[2] ?? "http://127.0.0.1:5174";
const output = path.resolve("dist/emulator-check");
fs.mkdirSync(output, { recursive: true });
app.setPath("userData", path.join(output, "profile"));
const wait = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

async function check(width, viewport) {
  const window = new BrowserWindow({ width: viewport, height: 900, show: false,
    webPreferences: { sandbox: true, contextIsolation: true, backgroundThrottling: false } });
  const evaluate = (source) => window.webContents.executeJavaScript(source);
  const click = async (label) => {
    await evaluate(`document.querySelector('button[aria-label="${label}"]').click()`);
    await wait(30);
  };
  const dimensions = () => evaluate(`(() => {
    const canvas = document.querySelector('canvas');
    const r = canvas.getBoundingClientRect();
    const controls = [...document.querySelectorAll('.emulator-panel button')].filter(e => e.getClientRects().length);
    return { width: r.width, height: r.height, hierarchy: document.querySelector('#hierarchy').getBoundingClientRect().top,
      buttons: controls.length, overflow: document.querySelector('.project-pane').scrollWidth > ${width},
      clipped: controls.filter(e => { const r=e.getBoundingClientRect(); return r.left < 0 || r.right > ${width} + 1; }).map(e => e.ariaLabel),
      frame: canvas.toDataURL(), state: document.querySelector('.emulator-identity strong').textContent };
  })()`);
  try {
    await window.loadURL(`${base}/tests/emulator.html?width=${width}`);
    await evaluate("document.fonts.ready");
    await wait(250);
    const expanded = await dimensions();
    assert(Math.abs(expanded.width / expanded.height - 168 / 144) < 0.001, "Display retains the hardware aspect ratio");
    assert.equal(expanded.overflow, false);
    assert.deepEqual(expanded.clipped, []);
    assert.equal(expanded.buttons, 13); // Collapse + transport + four directions + five device buttons.
    await evaluate("window.originalCanvas = document.querySelector('canvas')");
    fs.writeFileSync(path.join(output, `expanded-${width}.png`), (await window.webContents.capturePage()).toPNG());
    await click("Play preview");
    await click("Collapse emulator controls");
    const before = JSON.parse(await evaluate("document.body.dataset.engine"));
    const collapsed = await dimensions();
    assert.equal(collapsed.buttons, 1);
    assert.equal(collapsed.width, expanded.width);
    assert.equal(collapsed.height, expanded.height);
    assert(collapsed.hierarchy < expanded.hierarchy - 80);
    await wait(220);
    const after = JSON.parse(await evaluate("document.body.dataset.engine"));
    assert(after.ticks > before.ticks, "Timer must keep advancing while collapsed");
    assert.equal(after.playing, true);
    assert.equal(after.resets, 0);
    const running = await dimensions();
    assert.notEqual(running.frame, collapsed.frame, "Hidden controls must not freeze the framebuffer");
    assert.equal(await evaluate("document.querySelector('canvas') === window.originalCanvas"), true);
    fs.writeFileSync(path.join(output, `collapsed-${width}.png`), (await window.webContents.capturePage()).toPNG());
    await click("Expand emulator controls");
    const restored = await dimensions();
    assert.equal(restored.width, expanded.width);
    assert.equal(restored.height, expanded.height);
    assert.equal(restored.buttons, 13);
    await click("Pause preview");
    const paused = JSON.parse(await evaluate("document.body.dataset.engine"));
    await click("Collapse emulator controls");
    await wait(220);
    assert.equal(JSON.parse(await evaluate("document.body.dataset.engine")).ticks, paused.ticks, "Collapse must not start a paused emulator");
    await click("Expand emulator controls");
    for (const label of ["Joystick up", "Joystick left", "Joystick right", "Joystick down", "Start", "Button L", "Button R", "Button B", "Button A"]) await click(label);
    const inputs = JSON.parse(await evaluate("document.body.dataset.engine")).inputs;
    assert.deepEqual(inputs, ["JOY_UP", "JOY_LEFT", "JOY_RIGHT", "JOY_DOWN", "BUTTON_START", "BUTTON_L", "BUTTON_R", "BUTTON_B", "BUTTON_A"]);
    return { width, viewport, display: [expanded.width, expanded.height], collapseAndPlayback: "passed", inputMapping: "passed" };
  } finally { window.destroy(); }
}
app.whenReady().then(async () => {
  const results = [];
  for (const [width, viewport] of [[260, 1440], [320, 1440], [520, 1440], [375, 375]]) results.push(await check(width, viewport));
  for (const width of [1440, 760]) {
    const window = new BrowserWindow({ width, height: 1000, show: false,
      webPreferences: { sandbox: true, contextIsolation: true, backgroundThrottling: false } });
    try {
      await window.loadURL(`${base}/`);
      await window.webContents.executeJavaScript("document.fonts.ready");
      await wait(300);
      const layout = await window.webContents.executeJavaScript(`(() => {
        const emulator = document.querySelector('.project-pane .emulator-panel');
        const canvas = emulator?.querySelector('canvas');
        return { emulator: emulator?.getBoundingClientRect().height, display: canvas?.getBoundingClientRect().width };
      })()`);
      assert(layout.emulator > 0 && layout.display > 0, "Actual application shell renders its emulator");
      fs.writeFileSync(path.join(output, `shell-${width}.png`), (await window.webContents.capturePage()).toPNG());
      results.push({ shellViewport: width, layout: "passed" });
    } finally { window.destroy(); }
  }
  console.log(JSON.stringify(results, null, 2));
  app.quit();
}).catch(error => { console.error(error); app.exit(1); });
