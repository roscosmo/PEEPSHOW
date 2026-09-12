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
      return {
        groupCount: first.querySelectorAll('.effect-field-group').length,
        hasMainGroup: !!first.querySelector('.effect-field-group-main'),
        hasCoordinates: !!first.querySelector('.effect-coordinate-fields'),
        clipped: [...document.querySelectorAll('select,input,button')].some(e => {
        const r=e.getBoundingClientRect(); return r.left < 0 || r.right > ${width};
        }),
        labels: first.textContent,
        options: [...first.querySelector('select').options].map(o => o.text)
      };
    })()`);
    assert.equal(layout.clipped, false);
    assert(layout.groupCount >= 1);
    assert(layout.hasMainGroup);
    assert(layout.hasCoordinates);
    assert(layout.options.includes('Set position'));
    assert(!layout.options.includes('Move object'));
    assert(layout.labels.includes('px from top'));
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
  for (const width of [260, 360, 520]) {
    await window.loadURL(`http://127.0.0.1:5174/tests/action-inspector.html?native&width=${width}`);
    await wait(400);
    const details = await evaluate(`(() => {
      const e = document.querySelector('.object-action-position');
      return { text: e.textContent, label: document.querySelector('[aria-label="Effect 1 object"]').selectedOptions[0].text,
        motion: document.querySelector('.effect-motion-card')?.textContent ?? '',
        clipped: [...e.querySelectorAll('dt,dd,p')].some(item => { const r=item.getBoundingClientRect(); return r.left < 0 || r.right > ${width}; }) };
    })()`);
    assert.equal(details.label, 'Wizard');
    assert.equal(details.clipped, false);
    assert(details.motion.includes('Relative movement'));
    assert(details.motion.includes("Adds to the object's stored position"));
    assert(details.text.includes('Marker right'));
    assert(details.text.includes('X 85, Y 40'));
    assert(details.text.includes('X 120, Y 40'));
    assert(details.text.includes('Stored position'));
    assert(details.text.includes('On-screen position'));
    assert(details.text.includes('X is fixed by this state; movement still updates the stored position.'));
    assert(details.text.includes('In Destination, Y is fixed by that state; movement still updates the stored position.'));
    fs.writeFileSync(path.join(output, `object-position-${width}.png`), (await window.webContents.capturePage()).toPNG());
  }
  await window.loadURL('http://127.0.0.1:5174/tests/action-inspector.html?native&duplicates&horizontal');
  await wait(300);
  assert.deepEqual(await evaluate(`[...document.querySelector('[aria-label="Effect 1 object"]').options].map(o => o.text)`), ['Wizard (wizard)', 'Wizard (wizard_2)']);
  assert(!(await evaluate("document.querySelector('.object-action-position').textContent")).includes('Y is fixed'));
  for (const unavailable of ['otherScene', 'noPreview']) {
    await window.loadURL(`http://127.0.0.1:5174/tests/action-inspector.html?native&${unavailable}`);
    await wait(300);
    const text = await evaluate("document.querySelector('.object-action-position').textContent");
    assert(text.includes('No current emulator position'));
    assert(!text.includes('X 85'));
    assert(!text.includes('active state'));
  }
  await window.loadURL('http://127.0.0.1:5174/tests/action-inspector.html?native&frames');
  await wait(300);
  const frameSelect = '[aria-label="Effect 1 sprite frame"]';
  assert.deepEqual(await evaluate(`[...document.querySelector(${JSON.stringify(frameSelect)}).options].map(o=>o.text)`),
    ['Wizard - Frame 1', 'Wizard - Walking']);
  await evaluate(`(() => { const e=document.querySelector(${JSON.stringify(frameSelect)});
    Object.getOwnPropertyDescriptor(HTMLSelectElement.prototype,'value').set.call(e,'internal.frame_2');
    e.dispatchEvent(new Event('change',{bubbles:true})); })()`);
  await wait(100);
  assert.equal(JSON.parse(await evaluate('document.body.dataset.actions'))[1].frame_ref,'internal.frame_2');
  process.stdout.write('Action inspector layout, edits, ordering, read-only, object names, frame labels/IDs, live positions and override checks passed\n');
}).then(() => { clearTimeout(watchdog); window?.destroy(); app.exit(0); })
  .catch(error => { process.stderr.write(error.stack + '\n'); clearTimeout(watchdog); window?.destroy(); app.exit(1); });
