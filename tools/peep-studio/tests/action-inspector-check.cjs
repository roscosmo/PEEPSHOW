const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const { app, BrowserWindow } = require("electron");
const output = path.resolve("dist/action-inspector-check");
fs.mkdirSync(output, { recursive: true });
app.setPath("userData", path.join(output, "profile"));
app.disableHardwareAcceleration();
app.on("window-all-closed", () => {});
const wait = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
let window;
const watchdog = setTimeout(() => app.exit(1), 30000);
app.whenReady().then(async () => {
  window = new BrowserWindow({ width: 800, height: 1000, show: false,
    webPreferences: { sandbox: true, backgroundThrottling: false } });
  const evaluate = (code) => window.webContents.executeJavaScript(code);
  for (const width of [260, 360, 520]) {
    await window.loadURL(`http://127.0.0.1:5174/tests/action-inspector.html?width=${width}`);
    await wait(700);
    const layout = await evaluate(`(() => {
      const first = document.querySelector('.effect-fields');
      const fields = [...first.querySelectorAll('select,input')].map(e => e.getBoundingClientRect().toJSON());
      return { fields, clipped: [...document.querySelectorAll('select,input,button')].some(e => {
        const r=e.getBoundingClientRect(); return r.left < 0 || r.right > ${width};
      }), labels: first.textContent, options: [...first.querySelector('select').options].map(o => o.text) };
    })()`);
    assert.equal(layout.clipped, false);
    assert(layout.options.includes('Set position'));
    assert(!layout.options.includes('Move object'));
    assert(layout.labels.includes('px from top'));
    assert(layout.fields[1].top >= layout.fields[0].bottom);
    assert(layout.fields[2].top >= layout.fields[1].bottom);
    assert.equal(layout.fields[2].top, layout.fields[3].top);
    fs.writeFileSync(path.join(output, `actions-${width}.png`), (await window.webContents.capturePage()).toPNG());
  }
  await evaluate(`(() => {
    const e=document.querySelector('[aria-label="Effect 1 X position"]');
    Object.getOwnPropertyDescriptor(HTMLInputElement.prototype,'value').set.call(e,'64');
    e.dispatchEvent(new Event('input',{bubbles:true}));
  })()`);
  await wait(100);
  let actions = JSON.parse(await evaluate('document.body.dataset.actions'));
  assert.equal(actions[1].x, 64); assert.equal(actions[1].y, 32);
  await evaluate(`document.querySelector('[aria-label="Move effect 1 later"]').click()`);
  await wait(100);
  actions = JSON.parse(await evaluate('document.body.dataset.actions'));
  assert.equal(actions[0].kind, 'request_render');
  assert.equal(actions[1].kind, 'set_variable');
  assert.equal(actions[2].kind, 'set_element_position');
  await evaluate(`document.querySelector('[aria-label="Delete effect 2"]').click()`);
  await wait(100);
  actions = JSON.parse(await evaluate('document.body.dataset.actions'));
  assert(!actions.some(a => a.kind === 'set_element_position'));
  await window.loadURL('http://127.0.0.1:5174/tests/action-inspector.html?readonly=true');
  await wait(300);
  assert(await evaluate("[...document.querySelectorAll('input,select,button')].every(e => e.disabled)"));
  process.stdout.write('Action inspector layout, edits, ordering, deletion and read-only checks passed\n');
}).then(() => { clearTimeout(watchdog); window?.destroy(); app.exit(0); })
  .catch(error => { process.stderr.write(error.stack + '\n'); clearTimeout(watchdog); window?.destroy(); app.exit(1); });
