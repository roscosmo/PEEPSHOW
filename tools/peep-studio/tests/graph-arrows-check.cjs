const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const { app, BrowserWindow } = require("electron");

app.disableHardwareAcceleration();
app.on("window-all-closed", () => {});
const base = process.argv[2] ?? "http://127.0.0.1:5173";
const output = path.resolve(process.argv[3] ?? "dist/graph-arrow-check");
fs.mkdirSync(output, { recursive: true });
app.setPath("userData", path.join(output, "profile"));
const pause = (ms = 100) => new Promise((resolve) => setTimeout(resolve, ms));

async function inspect(window) {
  return window.webContents.executeJavaScript(`(() => {
    const arrows = [...document.querySelectorAll('.react-flow__edge .state-transition-arrow')];
    return arrows.map((arrow) => {
      const coordinates = arrow.getAttribute('d').match(/-?\\d+(?:\\.\\d+)?(?:e[-+]?\\d+)?/gi).map(Number);
      const matrix = arrow.getScreenCTM();
      const tip = new DOMPoint(coordinates[0], coordinates[1]).matrixTransform(matrix);
      const center = new DOMPoint((coordinates[0] + coordinates[2] + coordinates[4]) / 3,
        (coordinates[1] + coordinates[3] + coordinates[5]) / 3).matrixTransform(matrix);
      const hit = document.elementFromPoint(center.x, center.y);
      const headWidth = Math.hypot(coordinates[2] - coordinates[4], coordinates[3] - coordinates[5]);
      const headDepth = Math.hypot(coordinates[0] - (coordinates[2] + coordinates[4]) / 2,
        coordinates[1] - (coordinates[3] + coordinates[5]) / 2);
      return { id: arrow.closest('.react-flow__edge').dataset.id, tip: { x: tip.x, y: tip.y },
        headWidth, headDepth,
        center: { x: center.x, y: center.y }, hitId: hit?.dataset.edgeId,
        hitClass: hit?.className, color: getComputedStyle(arrow).fill };
    });
  })()`);
}

async function click(window, point) {
  const position = { x: Math.round(point.x), y: Math.round(point.y) };
  window.webContents.sendInputEvent({ type: "mouseMove", ...position });
  window.webContents.sendInputEvent({ type: "mouseDown", button: "left", clickCount: 1, ...position });
  window.webContents.sendInputEvent({ type: "mouseUp", button: "left", clickCount: 1, ...position });
  await pause();
}

async function drag(window, start, end) {
  window.webContents.sendInputEvent({ type: "mouseMove", x: Math.round(start.x), y: Math.round(start.y) });
  window.webContents.sendInputEvent({ type: "mouseDown", button: "left", clickCount: 1, x: Math.round(start.x), y: Math.round(start.y) });
  for (let step = 1; step <= 8; step++) {
    window.webContents.sendInputEvent({ type: "mouseMove", button: "left", modifiers: ["leftButtonDown"], x: Math.round(start.x + (end.x - start.x) * step / 8),
      y: Math.round(start.y + (end.y - start.y) * step / 8) });
    await pause(20);
  }
  window.webContents.sendInputEvent({ type: "mouseUp", button: "left", clickCount: 1, x: Math.round(end.x), y: Math.round(end.y) });
  await pause();
}

async function check(graph, width, height) {
  const window = new BrowserWindow({ width, height, show: false, webPreferences: { contextIsolation: true, sandbox: true, backgroundThrottling: false } });
  const errors = [];
  window.webContents.on("console-message", (event) => { if (event.level === "error") errors.push(event.message); });
  try {
    await window.loadURL(`${base}/tests/graph-arrows.html?graph=${graph}`);
    await window.webContents.executeJavaScript("document.fonts.ready");
    let arrows = [];
    let previousGeometry = "";
    let stableFrames = 0;
    for (let attempt = 0; attempt < 30; attempt++) {
      await pause();
      arrows = await inspect(window);
      const geometry = JSON.stringify(arrows.map(({ id, tip }) => ({ id, tip })));
      stableFrames = geometry === previousGeometry ? stableFrames + 1 : 0;
      previousGeometry = geometry;
      if (stableFrames >= 4 && arrows.length === (graph === "scene" ? 5 : 3) && arrows.every((arrow) => arrow.hitId === arrow.id)) break;
    }
    await pause(400);
    arrows = await inspect(window);
    fs.writeFileSync(path.join(output, `${graph}-${width}.png`), (await window.webContents.capturePage()).toPNG());
    assert.equal(arrows.length, graph === "scene" ? 5 : 3, `${graph}: every transition has an arrow`);
    for (const arrow of arrows) {
      assert(Math.abs(arrow.headWidth - 12) < 0.001, `${graph}: rendered arrowhead shrank`);
      assert(Math.abs(arrow.headDepth - 10) < 0.001, `${graph}: rendered arrowhead depth changed`);
      assert.equal(arrow.hitId, arrow.id, `${graph}: arrow hit is covered: ${JSON.stringify(arrow)}`);
      await click(window, arrow.center);
      const result = await window.webContents.executeJavaScript("JSON.parse(document.body.dataset.result || 'null')");
      assert(result, `${graph}: clicking the visible arrow must select it: ${JSON.stringify(arrow)}`);
      if (graph === "scene") assert(arrow.id.includes(result.selected), JSON.stringify({ arrow, result }));
      else if (arrow.id.startsWith("scene-entry")) assert.equal(result.kind, "scene");
      else assert(arrow.id.includes(result.id), JSON.stringify({ arrow, result }));
    }
    const blue = arrows.find((arrow) => !arrow.id.startsWith("scene-entry") && !arrow.id.startsWith("package-entry"));
    await click(window, blue.center);
    const section = await window.webContents.executeJavaScript(`(() => {
      const buttons = [...document.querySelectorAll('.state-route-section')];
      const button = buttons.find((item) => {
        const r = item.getBoundingClientRect();
        return document.elementFromPoint(r.x + r.width / 2, r.y + r.height / 2) === item;
      });
      if (!button) return null;
      const r = button.getBoundingClientRect();
      return { x: r.x + r.width / 2, y: r.y + r.height / 2, horizontal: button.classList.contains('state-route-section-horizontal') };
    })()`);
    assert(section, `${graph}: selected route retains draggable orthogonal sections`);
    await drag(window, section, { x: section.x + (section.horizontal ? 0 : 24), y: section.y + (section.horizontal ? 24 : 0) });
    const savedEditor = await window.webContents.executeJavaScript("JSON.parse(document.body.dataset.editor)");
    const moved = graph === "scene"
      ? savedEditor.scene_flow.routes?.first?.["scene_exit:first-menu"]
      : savedEditor.state_graph.scenes.menu.routes["from-first"].sources.first;
    assert(moved?.rails?.length > 0, `${graph}: section drag commits rails: ${JSON.stringify({ section, moved })}`);
    const targetId = graph === "scene" ? "menu" : "destination";
    const beforeMove = (await inspect(window)).find((arrow) => arrow.id === blue.id);
    await window.webContents.executeJavaScript(`window.dispatchEvent(new CustomEvent('fixture-move-node', { detail: { graph: '${graph}', id: '${targetId}' } }))`);
    await pause();
    const movedEditor = await window.webContents.executeJavaScript("JSON.parse(document.body.dataset.editor)");
    assert.deepEqual(graph === "scene" ? movedEditor.scene_flow.routes : movedEditor.state_graph.scenes.menu.routes,
      graph === "scene" ? savedEditor.scene_flow.routes : savedEditor.state_graph.scenes.menu.routes,
      "Moving a node must not rewrite saved route layouts");
    const afterMove = (await inspect(window)).find((arrow) => arrow.id === blue.id);
    assert(Math.hypot(afterMove.tip.x - beforeMove.tip.x, afterMove.tip.y - beforeMove.tip.y) > 10,
      `Arrow must follow the moved socket: ${JSON.stringify({ graph, beforeMove, afterMove })}`);
    if (graph === "state") {
      const destination = await window.webContents.executeJavaScript(`(() => {
        const p = document.querySelector('[data-id="destination"] [data-handleid="entry-bottom-right:right"]').getBoundingClientRect();
        return { x: p.x + p.width / 2, y: p.y + p.height / 2 };
      })()`);
      const current = (await inspect(window)).find((arrow) => arrow.id === blue.id);
      await drag(window, current.center, destination);
      const pinned = await window.webContents.executeJavaScript("JSON.parse(document.body.dataset.result)");
      assert.equal(pinned.handle, "entry-bottom-right");
      assert.equal(pinned.side, "right");
      assert.deepEqual(pinned.rails, moved.rails, "Retargeting preserves manual rails");
      const green = (await inspect(window)).find((arrow) => arrow.id.startsWith("scene-entry"));
      await drag(window, green.center, destination);
      const entry = await window.webContents.executeJavaScript("JSON.parse(document.body.dataset.editor).state_graph.scenes.menu.entry");
      assert.equal(entry.target_handle, "entry-bottom-right");
      assert.equal(entry.target_side, "right");
      const otherState = await window.webContents.executeJavaScript(`(() => {
        const p = document.querySelector('[data-id="first"] [data-handleid="entry-bottom-left:left"]').getBoundingClientRect();
        return { x: p.x + p.width / 2, y: p.y + p.height / 2 };
      })()`);
      const entryArrow = (await inspect(window)).find((arrow) => arrow.id.startsWith("scene-entry"));
      await drag(window, entryArrow.center, otherState);
      const changedEntry = (await inspect(window)).find((arrow) => arrow.id === "scene-entry->first");
      assert(changedEntry, "The green entry arrow must remain draggable to a different state");
    }
    assert.deepEqual(errors, []);
    return { graph, width, arrows: arrows.map(({ id, tip }) => ({ id, tip })), selection: "passed", sectionDrag: "passed", nodePositionUpdate: "passed" };
  } finally {
    window.destroy();
  }
}

app.whenReady().then(async () => {
  const results = [];
  for (const [width, height] of [[1440, 1000], [760, 980]]) {
    for (const graph of ["scene", "state"]) results.push(await check(graph, width, height));
  }
  fs.writeFileSync(path.join(output, "results.json"), JSON.stringify(results, null, 2));
  console.log(JSON.stringify(results, null, 2));
  app.quit();
}).catch((error) => { console.error(error); app.exit(1); });
