const fs = require("node:fs");
const path = require("node:path");
const { app, BrowserWindow } = require("electron");

app.disableHardwareAcceleration();
app.commandLine.appendSwitch("disable-gpu");
app.commandLine.appendSwitch("disable-software-rasterizer", "false");
app.on("window-all-closed", () => {});

const url = process.argv[2] ?? "http://127.0.0.1:5173/";
const outputRoot = path.resolve(process.argv[3] ?? "visual-output");
const userDataRoot = path.join(outputRoot, "electron-profile");

fs.mkdirSync(userDataRoot, { recursive: true });
app.setPath("userData", userDataRoot);
app.commandLine.appendSwitch("user-data-dir", userDataRoot);

const viewports = [
  { name: "desktop", width: 1440, height: 900 },
  { name: "compact", width: 760, height: 980 },
];

async function capture(viewport) {
  const window = new BrowserWindow({
    width: viewport.width,
    height: viewport.height,
    show: false,
    webPreferences: {
      contextIsolation: true,
      nodeIntegration: false,
      sandbox: true,
    },
  });
  await window.loadURL(url);
  await window.webContents.executeJavaScript("document.fonts.ready");
  await new Promise((resolve) => setTimeout(resolve, 400));

  const result = await window.webContents.executeJavaScript(`
    (() => {
      const pick = (selector) => {
        const element = document.querySelector(selector);
        if (!element) return null;
        const rect = element.getBoundingClientRect();
        return { width: rect.width, height: rect.height, top: rect.top, left: rect.left };
      };
      return {
        toolbar: pick(".app-toolbar"),
        project: pick(".project-pane"),
        preview: pick(".preview-pane"),
        graph: pick(".scene-flow-pane, .state-graph-pane"),
        inspector: pick(".inspector-pane"),
        overflowX: document.documentElement.scrollWidth > document.documentElement.clientWidth + 1,
      };
    })()
  `);

  const missing = Object.entries(result)
    .filter(([key, value]) => key !== "overflowX" && (value === null || value.width < 1 || value.height < 1))
    .map(([key]) => key);
  if (missing.length > 0) {
    throw new Error(`${viewport.name} missing visible panes: ${missing.join(", ")}`);
  }

  const image = await window.webContents.capturePage();
  fs.mkdirSync(outputRoot, { recursive: true });
  fs.writeFileSync(path.join(outputRoot, `${viewport.name}.png`), image.toPNG());
  window.destroy();
  return result;
}

app.whenReady()
  .then(async () => {
    const results = {};
    for (const viewport of viewports) {
      results[viewport.name] = await capture(viewport);
    }
    fs.writeFileSync(path.join(outputRoot, "layout.json"), `${JSON.stringify(results, null, 2)}\n`);
  })
  .then(() => app.quit())
  .catch((error) => {
    console.error(error);
    app.exit(1);
  });
