export type AudioEditVersion = {
  output_source_path: string;
  original_source_path: string;
  trim_start_ms: number;
  trim_end_ms: number;
  normalized: boolean;
  target_peak_dbfs: number;
};

function projectPath(value: unknown): value is string {
  return typeof value === "string"
    && value.length > 0
    && !value.startsWith("/")
    && !/^[A-Za-z]:/.test(value)
    && !value.replace(/\\/g, "/").split("/").includes("..");
}

function validVersion(value: unknown): value is AudioEditVersion {
  if (value === null || typeof value !== "object") return false;
  const record = value as Partial<AudioEditVersion>;
  return projectPath(record.output_source_path)
    && projectPath(record.original_source_path)
    && typeof record.trim_start_ms === "number"
    && Number.isFinite(record.trim_start_ms)
    && record.trim_start_ms >= 0
    && typeof record.trim_end_ms === "number"
    && Number.isFinite(record.trim_end_ms)
    && record.trim_end_ms > record.trim_start_ms
    && typeof record.normalized === "boolean"
    && typeof record.target_peak_dbfs === "number"
    && Number.isFinite(record.target_peak_dbfs);
}

export function parseAudioEditCatalog(text: string): AudioEditVersion[] {
  try {
    const parsed = JSON.parse(text) as { versions?: unknown };
    return Array.isArray(parsed?.versions) ? parsed.versions.filter(validVersion) : [];
  } catch {
    return [];
  }
}

export function serializeAudioEditCatalog(versions: AudioEditVersion[]): string {
  return `${JSON.stringify({
    schema_id: "peepshow.studio.audio_sources",
    schema_version: 1,
    versions,
  }, null, 2)}\n`;
}

export function findAudioEditVersion(versions: AudioEditVersion[], outputSourcePath: string): AudioEditVersion | null {
  const normalized = outputSourcePath.replace(/\\/g, "/");
  return versions.find((version) => version.output_source_path === normalized) ?? null;
}

export function upsertAudioEditVersion(versions: AudioEditVersion[], version: AudioEditVersion): AudioEditVersion[] {
  return [...versions.filter((item) => item.output_source_path !== version.output_source_path), version];
}
