import { readFile, realpath, stat } from "node:fs/promises";
import path from "node:path";
import { createHash } from "node:crypto";

export async function readThumbnailAudio(projectPath: unknown, sourcePath: unknown) {
  if (typeof projectPath !== "string" || typeof sourcePath !== "string"
    || !projectPath.endsWith(".peepproj") || path.isAbsolute(sourcePath)
    || path.extname(sourcePath).toLowerCase() !== ".wav") throw new Error("Invalid waveform source");
  const root = await realpath(projectPath);
  const source = await realpath(path.resolve(root, sourcePath));
  const relative = path.relative(root, source);
  if (relative.startsWith(`..${path.sep}`) || relative === ".." || path.isAbsolute(relative)) {
    throw new Error("Waveform source must be inside the project");
  }
  const info = await stat(source);
  if (!info.isFile() || info.size > 32 * 1024 * 1024) throw new Error("Waveform source exceeds preview size limit");
  const bytes = await readFile(source);
  return { key: createHash("sha256").update(bytes).digest("hex"), data: bytes.toString("base64") };
}
