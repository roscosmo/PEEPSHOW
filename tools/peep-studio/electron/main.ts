import { app, BrowserWindow, dialog, ipcMain, nativeImage } from "electron";
import { spawn, type ChildProcessWithoutNullStreams } from "node:child_process";
import { cp, mkdir, mkdtemp, readFile, stat, writeFile } from "node:fs/promises";
import os from "node:os";
import path from "node:path";
import readline from "node:readline";
import { readThumbnailAudio } from "./audioThumbnail.js";

const PROTOCOL_VERSION = 1;
const REQUEST_TIMEOUT_MS = 30_000;
const MAX_SOURCE_IMAGE_DIMENSION = 4096;
const MAX_GENERATED_PNG_BYTES = 16 * 1024 * 1024;

type FontAssetRecord = {
  font_id: string;
  display_name: string;
  source_path: string;
  source_format: "ttf" | "otf";
};

type PendingRequest = {
  resolve: (value: unknown) => void;
  reject: (reason: Error) => void;
  timeout: NodeJS.Timeout;
};

type ServiceResponse = {
  protocol_version: number;
  id: string;
  ok: boolean;
  result?: unknown;
  error?: {
    code?: string;
    message?: string;
    details?: unknown;
  };
};

class AuthoringSidecar {
  private process: ChildProcessWithoutNullStreams | null = null;
  private nextId = 1;
  private pending = new Map<string, PendingRequest>();
  private stderrTail: string[] = [];

  constructor(private readonly repositoryRoot: string) {}

  private start(): void {
    if (this.process !== null) {
      return;
    }

    const python = process.env.PEEPSHOW_PYTHON || "python";
    const tool = path.join(this.repositoryRoot, "tools", "authoring", "egg_tool.py");
    const child = spawn(python, ["-u", tool, "service"], {
      cwd: this.repositoryRoot,
      windowsHide: true,
      stdio: ["pipe", "pipe", "pipe"],
    });
    this.process = child;

    const output = readline.createInterface({ input: child.stdout });
    output.on("line", (line) => this.handleLine(line));
    child.stderr.setEncoding("utf8");
    child.stderr.on("data", (chunk: string) => {
      this.stderrTail.push(chunk.trim());
      this.stderrTail = this.stderrTail.slice(-8);
    });
    child.on("error", (error) => this.failAll(error));
    child.on("exit", (code, signal) => {
      this.process = null;
      this.failAll(
        new Error(
          `Authoring service exited (${signal ?? code ?? "unknown"}). ${this.stderrTail.join(" ")}`.trim(),
        ),
      );
    });
  }

  private handleLine(line: string): void {
    let response: ServiceResponse;
    try {
      response = JSON.parse(line) as ServiceResponse;
    } catch {
      this.failAll(new Error(`Authoring service returned invalid JSON: ${line.slice(0, 160)}`));
      return;
    }

    const pending = this.pending.get(response.id);
    if (pending === undefined) {
      return;
    }
    clearTimeout(pending.timeout);
    this.pending.delete(response.id);

    if (response.ok) {
      pending.resolve(response.result);
      return;
    }

    const code = response.error?.code ?? "SERVICE_ERROR";
    const message = response.error?.message ?? "Authoring service request failed";
    pending.reject(new Error(`${code}: ${message}`));
  }

  private failAll(error: Error): void {
    for (const request of this.pending.values()) {
      clearTimeout(request.timeout);
      request.reject(error);
    }
    this.pending.clear();
  }

  request(operation: string, params: Record<string, unknown>): Promise<unknown> {
    this.start();
    const child = this.process;
    if (child === null) {
      return Promise.reject(new Error("Authoring service did not start"));
    }

    const id = `studio-${this.nextId++}`;
    const message = JSON.stringify({
      protocol_version: PROTOCOL_VERSION,
      id,
      operation,
      params,
    });

    return new Promise((resolve, reject) => {
      const timeout = setTimeout(() => {
        this.pending.delete(id);
        reject(new Error(`Authoring service timed out during ${operation}`));
      }, REQUEST_TIMEOUT_MS);
      this.pending.set(id, { resolve, reject, timeout });
      child.stdin.write(`${message}\n`, (error) => {
        if (error !== null && error !== undefined) {
          clearTimeout(timeout);
          this.pending.delete(id);
          reject(error);
        }
      });
    });
  }

  stop(): void {
    const child = this.process;
    this.process = null;
    if (child !== null && !child.killed) {
      child.kill();
    }
  }
}

const repositoryRoot = path.resolve(__dirname, "..", "..", "..");
const sidecar = new AuthoringSidecar(repositoryRoot);
const exampleProject = path.join(repositoryRoot, "examples", "authoring", "state_slice.peepproj");

async function createWritableExampleCopy(): Promise<string> {
  const parent = await mkdtemp(path.join(os.tmpdir(), "peep-studio-example-"));
  const destination = path.join(parent, "menu_selection.peepproj");
  await cp(exampleProject, destination, { recursive: true });
  return destination;
}

async function pathExists(candidate: string): Promise<boolean> {
  try {
    await stat(candidate);
  } catch {
    return false;
  }
  return true;
}

function assetIdFromFilename(filename: string, fallback: string): string {
  const stem = path.basename(filename, path.extname(filename)).toLowerCase();
  const normalized = stem.replace(/[^a-z0-9_]+/g, "_").replace(/^_+|_+$/g, "");
  if (normalized === "") {
    return fallback;
  }
  const stable = /^[a-z]/.test(normalized) ? normalized : `${fallback}_${normalized}`;
  return stable.slice(0, 48);
}

function displayNameFromFilename(filename: string): string {
  const stem = path.basename(filename, path.extname(filename));
  const normalized = stem.replace(/[_-]+/g, " ").replace(/\s+/g, " ").trim();
  return (normalized === "" ? "Sprite" : normalized).slice(0, 64);
}

async function uniqueAssetPath(projectPath: string, sourcePath: string, fallbackAssetId: string): Promise<{ assetId: string; relativePath: string; destinationPath: string }> {
  const assetsRoot = path.join(projectPath, "assets");
  const baseAssetId = assetIdFromFilename(sourcePath, fallbackAssetId);
  const extension = path.extname(sourcePath).toLowerCase() || ".png";
  await mkdir(assetsRoot, { recursive: true });
  for (let index = 0; index < 1000; index += 1) {
    const suffix = index === 0 ? "" : `_${index + 1}`;
    const assetId = `${baseAssetId}${suffix}`;
    const filename = `${assetId}${extension}`;
    const destinationPath = path.join(assetsRoot, filename);
    if (!(await pathExists(destinationPath))) {
      return {
        assetId,
        relativePath: `assets/${filename}`,
        destinationPath,
      };
    }
  }
  throw new Error("Could not choose a unique asset filename");
}

function fontCatalogPath(projectRoot: string): string {
  return path.join(projectRoot, "assets", "fonts", "catalog.json");
}

async function readFontCatalog(projectRoot: string): Promise<FontAssetRecord[]> {
  try {
    const parsed = JSON.parse(await readFile(fontCatalogPath(projectRoot), "utf-8")) as unknown;
    if (parsed === null || typeof parsed !== "object" || !Array.isArray((parsed as { fonts?: unknown }).fonts)) {
      return [];
    }
    return (parsed as { fonts: unknown[] }).fonts.flatMap((font): FontAssetRecord[] => {
      if (font === null || typeof font !== "object") {
        return [];
      }
      const record = font as Partial<FontAssetRecord>;
      if (
        typeof record.font_id === "string"
        && typeof record.display_name === "string"
        && typeof record.source_path === "string"
        && (record.source_format === "ttf" || record.source_format === "otf")
      ) {
        return [{
          font_id: record.font_id,
          display_name: record.display_name,
          source_path: record.source_path,
          source_format: record.source_format,
        }];
      }
      return [];
    });
  } catch {
    return [];
  }
}

async function writeFontCatalog(projectRoot: string, fonts: FontAssetRecord[]): Promise<void> {
  const catalog = {
    schema_id: "peepshow.studio.font_assets",
    schema_version: 1,
    fonts,
  };
  await mkdir(path.dirname(fontCatalogPath(projectRoot)), { recursive: true });
  await writeFile(fontCatalogPath(projectRoot), `${JSON.stringify(catalog, null, 2)}\n`, "utf-8");
}

async function uniqueFontPath(projectRoot: string, sourcePath: string, existingIds: Set<string>): Promise<{ fontId: string; relativePath: string; destinationPath: string; sourceFormat: "ttf" | "otf" }> {
  const extension = path.extname(sourcePath).toLowerCase();
  if (extension !== ".ttf" && extension !== ".otf") {
    throw new Error("Font import supports .ttf and .otf files");
  }
  const fontsRoot = path.join(projectRoot, "assets", "fonts");
  const baseFontId = assetIdFromFilename(sourcePath, "font");
  await mkdir(fontsRoot, { recursive: true });
  for (let index = 0; index < 1000; index += 1) {
    const suffix = index === 0 ? "" : `_${index + 1}`;
    const fontId = `${baseFontId}${suffix}`;
    const filename = `${fontId}${extension}`;
    const destinationPath = path.join(fontsRoot, filename);
    if (!existingIds.has(fontId) && !(await pathExists(destinationPath))) {
      return {
        fontId,
        relativePath: `assets/fonts/${filename}`,
        destinationPath,
        sourceFormat: extension === ".ttf" ? "ttf" : "otf",
      };
    }
  }
  throw new Error("Could not choose a unique font filename");
}

function resolveProjectRelativePath(projectRoot: string, relativePath: string): string {
  if (path.isAbsolute(relativePath)) {
    throw new Error("Project asset path must be relative");
  }
  const resolved = path.resolve(projectRoot, relativePath);
  const relative = path.relative(projectRoot, resolved);
  if (relative.startsWith("..") || path.isAbsolute(relative)) {
    throw new Error("Project asset path escapes the project folder");
  }
  return resolved;
}

function sanitizeSpriteImage(image: Electron.NativeImage): Buffer {
  const size = image.getSize();
  const bitmap = Buffer.from(image.toBitmap());
  for (let index = 0; index < bitmap.length; index += 4) {
    const alpha = bitmap[index + 3] ?? 0;
    if (alpha === 0) {
      bitmap[index] = 255;
      bitmap[index + 1] = 255;
      bitmap[index + 2] = 255;
      continue;
    }
    const average = ((bitmap[index] ?? 0) + (bitmap[index + 1] ?? 0) + (bitmap[index + 2] ?? 0)) / 3;
    const value = average < 128 ? 0 : 255;
    bitmap[index] = value;
    bitmap[index + 1] = value;
    bitmap[index + 2] = value;
  }
  return nativeImage.createFromBitmap(bitmap, { width: size.width, height: size.height }).toPNG();
}

function pngBufferFromDataUrl(dataUrl: string): Buffer {
  const match = /^data:image\/png;base64,([A-Za-z0-9+/=]+)$/.exec(dataUrl);
  if (match === null) {
    throw new Error("Generated sprite data must be a PNG data URL");
  }
  const buffer = Buffer.from(match[1], "base64");
  if (buffer.length === 0 || buffer.length > MAX_GENERATED_PNG_BYTES) {
    throw new Error("Generated sprite PNG is empty or too large");
  }
  return buffer;
}

function createWindow(): void {
  const window = new BrowserWindow({
    width: 1480,
    height: 940,
    minWidth: 1040,
    minHeight: 700,
    backgroundColor: "#f3f5f4",
    show: false,
    title: "Peep Studio",
    webPreferences: {
      preload: path.join(__dirname, "preload.js"),
      contextIsolation: true,
      nodeIntegration: false,
      sandbox: true,
    },
  });

  window.setMenuBarVisibility(false);
  window.webContents.setWindowOpenHandler(() => ({ action: "deny" }));
  window.webContents.on("will-navigate", (event) => event.preventDefault());
  window.once("ready-to-show", () => window.show());

  if (app.isPackaged) {
    void window.loadFile(path.join(__dirname, "..", "dist", "index.html"));
  } else {
    void window.loadURL("http://127.0.0.1:5173");
  }
}

ipcMain.handle(
  "peep:service-request",
  (_event, operation: unknown, params: unknown) => {
    if (typeof operation !== "string" || params === null || typeof params !== "object") {
      throw new Error("Invalid service request from renderer");
    }
    return sidecar.request(operation, params as Record<string, unknown>);
  },
);

ipcMain.handle("peep:open-project", async () => {
  const result = await dialog.showOpenDialog({
    title: "Open Peep Studio project",
    defaultPath: exampleProject,
    properties: ["openDirectory"],
  });
  return result.canceled ? null : result.filePaths[0] ?? null;
});

ipcMain.handle("peep:choose-new-project-path", async () => {
  const result = await dialog.showSaveDialog({
    title: "Create Peep Studio project",
    defaultPath: path.join(app.getPath("documents"), "New Project.peepproj"),
    filters: [{ name: "Peep Studio project", extensions: ["peepproj"] }],
  });
  if (result.canceled || result.filePath === undefined) {
    return null;
  }
  return path.resolve(
    result.filePath.endsWith(".peepproj")
      ? result.filePath
      : `${result.filePath}.peepproj`,
  );
});

ipcMain.handle("peep:open-example", () => createWritableExampleCopy());

ipcMain.handle("peep:import-sprite-png", async (_event, projectPath: unknown) => {
  if (typeof projectPath !== "string") {
    throw new Error("Invalid sprite import request from renderer");
  }
  const projectRoot = path.resolve(projectPath);
  if (!projectRoot.endsWith(".peepproj")) {
    throw new Error("Sprite import target must be a .peepproj directory");
  }
  const result = await dialog.showOpenDialog({
    title: "Import sprite PNG",
    defaultPath: projectRoot,
    properties: ["openFile"],
    filters: [{ name: "PNG image", extensions: ["png"] }],
  });
  if (result.canceled || result.filePaths[0] === undefined) {
    return null;
  }
  const sourcePath = path.resolve(result.filePaths[0]);
  const image = nativeImage.createFromPath(sourcePath);
  const size = image.getSize();
  if (image.isEmpty() || size.width <= 0 || size.height <= 0) {
    throw new Error("Selected PNG could not be loaded");
  }
  if (size.width > MAX_SOURCE_IMAGE_DIMENSION || size.height > MAX_SOURCE_IMAGE_DIMENSION) {
    throw new Error(`Sprite source PNG must be no larger than ${MAX_SOURCE_IMAGE_DIMENSION}x${MAX_SOURCE_IMAGE_DIMENSION}`);
  }
  const destination = await uniqueAssetPath(projectRoot, sourcePath, "sprite");
  await writeFile(destination.destinationPath, sanitizeSpriteImage(image));
  return {
    assetId: destination.assetId,
    displayName: displayNameFromFilename(sourcePath),
    sourcePath: destination.relativePath,
    width: size.width,
    height: size.height,
  };
});

ipcMain.handle("peep:read-font-assets", async (_event, projectPath: unknown) => {
  if (typeof projectPath !== "string") {
    throw new Error("Invalid font catalog request from renderer");
  }
  const projectRoot = path.resolve(projectPath);
  if (!projectRoot.endsWith(".peepproj")) {
    throw new Error("Font catalog target must be a .peepproj directory");
  }
  return readFontCatalog(projectRoot);
});

ipcMain.handle("peep:import-font-asset", async (_event, projectPath: unknown) => {
  if (typeof projectPath !== "string") {
    throw new Error("Invalid font import request from renderer");
  }
  const projectRoot = path.resolve(projectPath);
  if (!projectRoot.endsWith(".peepproj")) {
    throw new Error("Font import target must be a .peepproj directory");
  }
  const result = await dialog.showOpenDialog({
    title: "Import font",
    defaultPath: projectRoot,
    properties: ["openFile"],
    filters: [{ name: "TrueType/OpenType font", extensions: ["ttf", "otf"] }],
  });
  if (result.canceled || result.filePaths[0] === undefined) {
    return null;
  }
  const sourcePath = path.resolve(result.filePaths[0]);
  const fonts = await readFontCatalog(projectRoot);
  const destination = await uniqueFontPath(projectRoot, sourcePath, new Set(fonts.map(font => font.font_id)));
  await cp(sourcePath, destination.destinationPath);
  const record: FontAssetRecord = {
    font_id: destination.fontId,
    display_name: displayNameFromFilename(sourcePath),
    source_path: destination.relativePath,
    source_format: destination.sourceFormat,
  };
  await writeFontCatalog(projectRoot, [...fonts, record]);
  return record;
});

ipcMain.handle("peep:font-asset-source", async (_event, projectPath: unknown, sourcePath: unknown) => {
  if (typeof projectPath !== "string" || typeof sourcePath !== "string") {
    throw new Error("Invalid font source request from renderer");
  }
  const projectRoot = path.resolve(projectPath);
  if (!projectRoot.endsWith(".peepproj")) {
    throw new Error("Font source target must be a .peepproj directory");
  }
  const resolved = resolveProjectRelativePath(projectRoot, sourcePath);
  const extension = path.extname(resolved).toLowerCase();
  if (extension !== ".ttf" && extension !== ".otf") {
    throw new Error("Font source must be a .ttf or .otf file");
  }
  const data = await readFile(resolved);
  const mime = extension === ".ttf" ? "font/ttf" : "font/otf";
  return {
    key: `${sourcePath}:${data.length}`,
    data: `data:${mime};base64,${data.toString("base64")}`,
  };
});

ipcMain.handle("peep:write-generated-sprite-png", async (_event, projectPath: unknown, requestedAssetId: unknown, pngDataUrl: unknown) => {
  if (typeof projectPath !== "string" || typeof requestedAssetId !== "string" || typeof pngDataUrl !== "string") {
    throw new Error("Invalid generated sprite request from renderer");
  }
  const projectRoot = path.resolve(projectPath);
  if (!projectRoot.endsWith(".peepproj")) {
    throw new Error("Generated sprite target must be a .peepproj directory");
  }
  const image = nativeImage.createFromBuffer(pngBufferFromDataUrl(pngDataUrl));
  const size = image.getSize();
  if (image.isEmpty() || size.width <= 0 || size.height <= 0) {
    throw new Error("Generated sprite PNG could not be loaded");
  }
  if (size.width > MAX_SOURCE_IMAGE_DIMENSION || size.height > MAX_SOURCE_IMAGE_DIMENSION) {
    throw new Error(`Generated sprite PNG must be no larger than ${MAX_SOURCE_IMAGE_DIMENSION}x${MAX_SOURCE_IMAGE_DIMENSION}`);
  }
  const destination = await uniqueAssetPath(projectRoot, `${requestedAssetId}.png`, "sprite");
  await writeFile(destination.destinationPath, sanitizeSpriteImage(image));
  return {
    assetId: destination.assetId,
    sourcePath: destination.relativePath,
    width: size.width,
    height: size.height,
  };
});

ipcMain.handle("peep:import-audio-wav", async (_event, projectPath: unknown) => {
  if (typeof projectPath !== "string") {
    throw new Error("Invalid audio import request from renderer");
  }
  const projectRoot = path.resolve(projectPath);
  if (!projectRoot.endsWith(".peepproj")) {
    throw new Error("Audio import target must be a .peepproj directory");
  }
  const result = await dialog.showOpenDialog({
    title: "Import sampled SFX WAV",
    defaultPath: projectRoot,
    properties: ["openFile"],
    filters: [{ name: "PCM WAV audio", extensions: ["wav"] }],
  });
  if (result.canceled || result.filePaths[0] === undefined) {
    return null;
  }
  const sourcePath = path.resolve(result.filePaths[0]);
  const destination = await uniqueAssetPath(projectRoot, sourcePath, "audio");
  await cp(sourcePath, destination.destinationPath);
  return {
    assetId: destination.assetId,
    sourcePath: destination.relativePath,
  };
});

ipcMain.handle("peep:audio-thumbnail-source", (_event, projectPath: unknown, sourcePath: unknown) =>
  readThumbnailAudio(projectPath, sourcePath));

ipcMain.handle("peep:save-project-as", async (_event, sourcePath: unknown, defaultName: unknown) => {
  if (typeof sourcePath !== "string" || typeof defaultName !== "string") {
    throw new Error("Invalid Save As request from renderer");
  }
  const source = path.resolve(sourcePath);
  if (!source.endsWith(".peepproj")) {
    throw new Error("Save As source must be a .peepproj directory");
  }
  const result = await dialog.showSaveDialog({
    title: "Save Peep Studio project as",
    defaultPath: path.join(
      app.getPath("documents"),
      defaultName.endsWith(".peepproj") ? defaultName : `${defaultName}.peepproj`,
    ),
    filters: [{ name: "Peep Studio project", extensions: ["peepproj"] }],
  });
  if (result.canceled || result.filePath === undefined) {
    return null;
  }
  const destination = path.resolve(
    result.filePath.endsWith(".peepproj") ? result.filePath : `${result.filePath}.peepproj`,
  );
  if (destination === source) {
    throw new Error("Choose a different project location for Save As");
  }
  if (await pathExists(destination)) {
    throw new Error("Choose a new project location. That path already exists.");
  }
  await cp(source, destination, { recursive: true });
  return destination;
});

ipcMain.handle(
  "peep:export-egg",
  async (_event, defaultName: unknown, blobBase64: unknown) => {
    if (typeof defaultName !== "string" || typeof blobBase64 !== "string") {
      throw new Error("Invalid .egg export request from renderer");
    }
    const result = await dialog.showSaveDialog({
      title: "Export .egg package",
      defaultPath: defaultName.endsWith(".egg") ? defaultName : `${defaultName}.egg`,
      filters: [{ name: "PeepShow package", extensions: ["egg"] }],
    });
    if (result.canceled || result.filePath === undefined) {
      return null;
    }
    await writeFile(result.filePath, Buffer.from(blobBase64, "base64"));
    return result.filePath;
  },
);

app.whenReady().then(() => {
  createWindow();
  app.on("activate", () => {
    if (BrowserWindow.getAllWindows().length === 0) {
      createWindow();
    }
  });
});

app.on("window-all-closed", () => {
  if (process.platform !== "darwin") {
    app.quit();
  }
});

app.on("will-quit", () => sidecar.stop());
