import { app, BrowserWindow, dialog, ipcMain, nativeImage } from "electron";
import { spawn, type ChildProcessWithoutNullStreams } from "node:child_process";
import { cp, mkdir, mkdtemp, stat, writeFile } from "node:fs/promises";
import os from "node:os";
import path from "node:path";
import readline from "node:readline";

const PROTOCOL_VERSION = 1;
const REQUEST_TIMEOUT_MS = 30_000;

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
  const destination = path.join(parent, "state_slice.peepproj");
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

function assetIdFromFilename(filename: string): string {
  const stem = path.basename(filename, path.extname(filename)).toLowerCase();
  const normalized = stem.replace(/[^a-z0-9_]+/g, "_").replace(/^_+|_+$/g, "");
  return normalized === "" ? "sprite" : normalized.slice(0, 48);
}

function displayNameFromFilename(filename: string): string {
  const stem = path.basename(filename, path.extname(filename));
  const normalized = stem.replace(/[_-]+/g, " ").replace(/\s+/g, " ").trim();
  return (normalized === "" ? "Sprite" : normalized).slice(0, 64);
}

async function uniqueAssetPath(projectPath: string, sourcePath: string): Promise<{ assetId: string; relativePath: string; destinationPath: string }> {
  const assetsRoot = path.join(projectPath, "assets");
  const baseAssetId = assetIdFromFilename(sourcePath);
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
  if (size.width > 168 || size.height > 144) {
    throw new Error("First-pass sprite import only supports full-image frames up to 168x144");
  }
  const destination = await uniqueAssetPath(projectRoot, sourcePath);
  await writeFile(destination.destinationPath, sanitizeSpriteImage(image));
  return {
    assetId: destination.assetId,
    displayName: displayNameFromFilename(sourcePath),
    sourcePath: destination.relativePath,
    width: size.width,
    height: size.height,
  };
});

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
