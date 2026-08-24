import {
  AlertTriangle,
  ArrowLeft,
  ArrowRight,
  Box,
  Check,
  ChevronRight,
  Circle,
  Download,
  FileCode2,
  FolderOpen,
  Hammer,
  LoaderCircle,
  MonitorDot,
  PackageCheck,
  Pause,
  Play,
  RotateCcw,
  Redo2,
  Save,
  SaveAll,
  StepForward,
  Undo2,
  X,
} from "lucide-react";
import { useCallback, useEffect, useMemo, useRef, useState } from "react";
import {
  SceneAuthoringInspector,
  StateGraphView,
  type SceneSelection,
} from "./SceneInspection";
import type {
  Framebuffer,
  PackageBuildResult,
  ProjectCommandResult,
  ProjectHistoryResult,
  ProjectSaveResult,
  PreviewSnapshot,
  ProjectLoadResult,
  SceneDocument,
  ServiceHello,
} from "./types";

const INPUTS = [
  { source: "BUTTON_L", label: "L", icon: ArrowLeft },
  { source: "BUTTON_A", label: "A", icon: Circle },
  { source: "BUTTON_B", label: "B", icon: Circle },
  { source: "BUTTON_R", label: "R", icon: ArrowRight },
] as const;

function errorText(error: unknown): string {
  return error instanceof Error ? error.message : String(error);
}

function FramebufferCanvas({ framebuffer }: { framebuffer: Framebuffer | null }) {
  const canvasRef = useRef<HTMLCanvasElement>(null);

  useEffect(() => {
    const canvas = canvasRef.current;
    if (canvas === null) {
      return;
    }
    const context = canvas.getContext("2d");
    if (context === null) {
      return;
    }

    const width = framebuffer?.width ?? 168;
    const height = framebuffer?.height ?? 144;
    canvas.width = width;
    canvas.height = height;
    context.fillStyle = "#eef1ed";
    context.fillRect(0, 0, width, height);

    if (framebuffer === null) {
      context.fillStyle = "#c7cec9";
      context.fillRect(12, 12, width - 24, 1);
      context.fillRect(12, height - 13, width - 24, 1);
      return;
    }

    const encoded = atob(framebuffer.data_base64);
    const image = context.createImageData(width, height);
    for (let y = 0; y < height; y += 1) {
      for (let x = 0; x < width; x += 1) {
        const source = encoded.charCodeAt(y * framebuffer.row_stride_bytes + (x >> 3));
        const black = (source & (0x80 >> (x & 7))) !== 0;
        const target = (y * width + x) * 4;
        const shade = black ? 17 : 242;
        image.data[target] = shade;
        image.data[target + 1] = black ? 20 : 244;
        image.data[target + 2] = black ? 18 : 240;
        image.data[target + 3] = 255;
      }
    }
    context.putImageData(image, 0, 0);
  }, [framebuffer]);

  return <canvas ref={canvasRef} className="preview-canvas" aria-label="168 by 144 display preview" />;
}

function StatusMark({ ok }: { ok: boolean }) {
  return ok ? <Check size={14} aria-hidden="true" /> : <X size={14} aria-hidden="true" />;
}

export default function App() {
  const bridge = window.peepStudio;
  const [service, setService] = useState<ServiceHello | null>(null);
  const [project, setProject] = useState<ProjectLoadResult | null>(null);
  const [preview, setPreview] = useState<PreviewSnapshot | null>(null);
  const [selectedScene, setSelectedScene] = useState<string | null>(null);
  const [projectPath, setProjectPath] = useState<string | null>(null);
  const [temporaryProject, setTemporaryProject] = useState(false);
  const [sceneSelection, setSceneSelection] = useState<SceneSelection>({ kind: "scene" });
  const [build, setBuild] = useState<PackageBuildResult | null>(null);
  const [dirty, setDirty] = useState(false);
  const [canUndo, setCanUndo] = useState(false);
  const [canRedo, setCanRedo] = useState(false);
  const [busy, setBusy] = useState<string | null>(null);
  const [playing, setPlaying] = useState(false);
  const [message, setMessage] = useState<string | null>(null);
  const previewRef = useRef<PreviewSnapshot | null>(null);
  const operationLock = useRef(false);

  useEffect(() => {
    previewRef.current = preview;
  }, [preview]);

  useEffect(() => {
    if (bridge === undefined) {
      setMessage("Electron bridge unavailable. Run the desktop shell to connect the authoring service.");
      return;
    }
    bridge
      .serviceRequest<ServiceHello>("service.hello", {})
      .then(setService)
      .catch((error) => setMessage(errorText(error)));
  }, [bridge]);

  const startPreview = useCallback(
    async (sceneId: string, revision = project?.project_revision) => {
      if (bridge === undefined || revision === undefined) {
        return;
      }
      setBusy("Starting preview");
      setPlaying(false);
      try {
        const result = await bridge.serviceRequest<PreviewSnapshot>("project.preview_reset", {
          project_revision: revision,
          scene_id: sceneId,
        });
        setSelectedScene(sceneId);
        setSceneSelection({ kind: "scene" });
        setPreview(result);
        setMessage(null);
      } catch (error) {
        setMessage(errorText(error));
      } finally {
        setBusy(null);
      }
    },
    [bridge, project?.project_revision],
  );

  const loadProject = useCallback(
    async (path: string) => {
      if (bridge === undefined) {
        return;
      }
      setBusy("Loading project");
      setPlaying(false);
      setBuild(null);
      setDirty(false);
      setCanUndo(false);
      setCanRedo(false);
      setProjectPath(path);
      setPreview(null);
      setSelectedScene(null);
      setSceneSelection({ kind: "scene" });
      try {
        const result = await bridge.serviceRequest<ProjectLoadResult>("project.load", { path });
        setProject(result);
        setDirty(result.dirty);
        setCanUndo(result.can_undo);
        setCanRedo(result.can_redo);
        setMessage(result.valid ? null : "Project validation failed. Review the issues panel.");
        if (result.valid) {
          await startPreview(result.summary.entry_scene, result.project_revision);
        }
      } catch (error) {
        setMessage(errorText(error));
      } finally {
        setBusy(null);
      }
    },
    [bridge, startPreview],
  );

  const openProject = async () => {
    if (bridge === undefined) {
      return;
    }
    const path = await bridge.openProject();
    if (path !== null) {
      setTemporaryProject(false);
      await loadProject(path);
    }
  };

  const openExample = async () => {
    if (bridge === undefined) {
      return;
    }
    setTemporaryProject(true);
    await loadProject(await bridge.openExampleProject());
    setMessage("Opened a temporary copy of the example project.");
  };

  const advancePreview = useCallback(
    async (elapsedMs = 250) => {
      const current = previewRef.current;
      if (bridge === undefined || current === null || operationLock.current) {
        return;
      }
      operationLock.current = true;
      try {
        const result = await bridge.serviceRequest<PreviewSnapshot>("project.preview_advance", {
          project_revision: current.project_revision,
          preview_revision: current.preview_revision,
          elapsed_ms: elapsedMs,
        });
        setPreview(result);
      } catch (error) {
        setPlaying(false);
        setMessage(errorText(error));
      } finally {
        operationLock.current = false;
      }
    },
    [bridge],
  );

  useEffect(() => {
    if (!playing) {
      return undefined;
    }
    const interval = window.setInterval(() => void advancePreview(250), 250);
    return () => window.clearInterval(interval);
  }, [advancePreview, playing]);

  const sendInput = async (logicalSource: string) => {
    const current = previewRef.current;
    if (bridge === undefined || current === null || operationLock.current) {
      return;
    }
    operationLock.current = true;
    try {
      const result = await bridge.serviceRequest<PreviewSnapshot>("project.preview_input", {
        project_revision: current.project_revision,
        preview_revision: current.preview_revision,
        logical_source: logicalSource,
      });
      setPreview(result);
      setMessage(null);
    } catch (error) {
      setMessage(errorText(error));
    } finally {
      operationLock.current = false;
    }
  };

  const buildPackage = async () => {
    if (bridge === undefined || project === null || !project.valid) {
      return;
    }
    setBusy("Building package");
    try {
      const result = await bridge.serviceRequest<PackageBuildResult>("project.build_package", {
        project_revision: project.project_revision,
      });
      setBuild(result);
      setMessage(`Built ${result.package.package_id}.egg`);
    } catch (error) {
      setMessage(errorText(error));
    } finally {
      setBusy(null);
    }
  };

  const saveProject = async () => {
    if (bridge === undefined || project === null || busy !== null) {
      return;
    }
    setBusy("Saving project");
    try {
      const result = await bridge.serviceRequest<ProjectSaveResult>("project.save", {
        project_revision: project.project_revision,
      });
      applyProjectResult(result);
      setMessage(`Saved ${result.saved_sources.length} scene source${result.saved_sources.length === 1 ? "" : "s"}.`);
    } catch (error) {
      setMessage(errorText(error));
    } finally {
      setBusy(null);
    }
  };

  const applyProjectResult = (result: ProjectHistoryResult | ProjectCommandResult | ProjectSaveResult) => {
    setProject((current) => (
      current === null
        ? null
        : {
            ...current,
            project_revision: result.project_revision,
            valid: result.valid,
            issues: result.issues,
            document: result.document,
            summary: result.summary,
          }
    ));
    setDirty(result.dirty);
    setCanUndo(result.can_undo);
    setCanRedo(result.can_redo);
    setPreview(null);
    setBuild(null);
  };

  const stepHistory = async (operation: "project.undo" | "project.redo") => {
    if (bridge === undefined || project === null || busy !== null) {
      return;
    }
    setBusy(operation === "project.undo" ? "Undoing" : "Redoing");
    setPlaying(false);
    try {
      const result = await bridge.serviceRequest<ProjectHistoryResult>(operation, {
        project_revision: project.project_revision,
      });
      applyProjectResult(result);
      setSceneSelection({ kind: "scene" });
      setMessage(operation === "project.undo" ? "Undid last edit." : "Redid edit.");
    } catch (error) {
      setMessage(errorText(error));
    } finally {
      setBusy(null);
    }
  };

  const saveProjectAs = async () => {
    if (bridge === undefined || project === null || projectPath === null || busy !== null) {
      return;
    }
    setBusy("Saving project as");
    try {
      if (dirty) {
        const saved = await bridge.serviceRequest<ProjectSaveResult>("project.save", {
          project_revision: project.project_revision,
        });
        applyProjectResult(saved);
      }
      const destination = await bridge.saveProjectAs(projectPath, project.source_name);
      if (destination !== null) {
        setTemporaryProject(false);
        await loadProject(destination);
        setMessage(`Saved project as ${destination}`);
      }
    } catch (error) {
      setMessage(errorText(error));
    } finally {
      setBusy(null);
    }
  };

  const renameState = async (sceneId: string, stateId: string, displayName: string) => {
    if (bridge === undefined || project === null || busy !== null) {
      return;
    }
    setBusy("Renaming state");
    setPlaying(false);
    try {
      const result = await bridge.serviceRequest<ProjectCommandResult>("project.apply_commands", {
        project_revision: project.project_revision,
        commands: [
          {
            kind: "state.rename",
            scene_id: sceneId,
            state_id: stateId,
            display_name: displayName,
          },
        ],
      });
      applyProjectResult(result);
      setSceneSelection({ kind: "state", id: stateId });
      setMessage("State renamed. Save to write it to the project.");
    } catch (error) {
      setMessage(errorText(error));
    } finally {
      setBusy(null);
    }
  };

  const exportPackage = async () => {
    if (bridge === undefined || build === null) {
      return;
    }
    const exported = await bridge.exportEgg(
      `${build.package.package_id}.egg`,
      build.package.blob_base64,
    );
    if (exported !== null) {
      setMessage(`Exported ${exported}`);
    }
  };

  const scenes: SceneDocument[] = project?.document?.scenes ?? [];
  const selectedSceneDocument = useMemo(
    () => scenes.find((scene) => scene.scene_id === selectedScene) ?? null,
    [scenes, selectedScene],
  );
  const connected = service !== null;

  return (
    <main className="studio-shell">
      <header className="app-toolbar">
        <div className="brand-block">
          <div className="brand-mark" aria-hidden="true">P</div>
          <div>
            <h1>Peep Studio</h1>
            <p>{project?.summary.project_name ?? "STATE authoring workbench"}</p>
          </div>
        </div>

        <div className="toolbar-actions">
          <span className={`service-state ${connected ? "connected" : "disconnected"}`}>
            <MonitorDot size={15} aria-hidden="true" />
            {connected ? `Service API ${service.service_api_version}` : "Service offline"}
          </span>
          <button className="button secondary" onClick={openExample} disabled={bridge === undefined || busy !== null}>
            <FileCode2 size={16} aria-hidden="true" />
            Open example
          </button>
          <button className="button secondary" onClick={openProject} disabled={bridge === undefined || busy !== null}>
            <FolderOpen size={16} aria-hidden="true" />
            Open project
          </button>
          <button className="button primary" onClick={buildPackage} disabled={!project?.valid || busy !== null}>
            <Hammer size={16} aria-hidden="true" />
            Build
          </button>
          <button className="icon-button" onClick={() => void stepHistory("project.undo")} disabled={!canUndo || project === null || busy !== null || service?.operations.includes("project.undo") !== true} title="Undo">
            <Undo2 size={18} aria-hidden="true" />
          </button>
          <button className="icon-button" onClick={() => void stepHistory("project.redo")} disabled={!canRedo || project === null || busy !== null || service?.operations.includes("project.redo") !== true} title="Redo">
            <Redo2 size={18} aria-hidden="true" />
          </button>
          <button className="button secondary" onClick={saveProject} disabled={!dirty || project === null || busy !== null || service?.operations.includes("project.save") !== true}>
            <Save size={16} aria-hidden="true" />
            Save
          </button>
          <button className="button secondary" onClick={saveProjectAs} disabled={project === null || projectPath === null || busy !== null || service?.operations.includes("project.save") !== true}>
            <SaveAll size={16} aria-hidden="true" />
            Save as
          </button>
          <button className="icon-button" onClick={exportPackage} disabled={build === null} title="Export .egg">
            <Download size={18} aria-hidden="true" />
          </button>
        </div>
      </header>

      <section className="workspace-grid">
        <aside className="project-pane">
          <div className="pane-heading">
            <span>Project</span>
            {project !== null && (
              <span className={`validation-state ${project.valid ? "valid" : "invalid"}`}>
                <StatusMark ok={project.valid} /> {project.valid ? "Valid" : "Invalid"}
              </span>
            )}
          </div>

          {project === null ? (
            <div className="empty-pane">
              <Box size={25} aria-hidden="true" />
              <strong>No project open</strong>
              <span>Open a .peepproj folder to inspect scenes and package assets.</span>
            </div>
          ) : (
            <>
              <dl className="project-facts">
                <div><dt>Package</dt><dd>{project.summary.package_id}</dd></div>
                <div><dt>Target</dt><dd>{project.summary.target_profile}</dd></div>
                <div><dt>Source</dt><dd>{temporaryProject ? "Example copy" : "Project"}</dd></div>
                <div><dt>Edits</dt><dd>{dirty ? "Unsaved" : "Clean"}</dd></div>
                <div><dt>Path</dt><dd title={projectPath ?? undefined}>{projectPath ?? "-"}</dd></div>
                <div><dt>Frames</dt><dd>{project.summary.asset_frame_count}</dd></div>
                <div><dt>Animations</dt><dd>{project.summary.animation_count}</dd></div>
              </dl>

              <div className="tree-label">Scenes</div>
              <nav className="scene-list" aria-label="Project scenes">
                {scenes.map((scene) => (
                  <button
                    key={scene.scene_id}
                    className={`scene-row ${selectedScene === scene.scene_id ? "selected" : ""}`}
                    onClick={() => void startPreview(scene.scene_id)}
                  >
                    <FileCode2 size={16} aria-hidden="true" />
                    <span>
                      <strong>{scene.display_name}</strong>
                      <small>{scene.scene_type}</small>
                    </span>
                    <ChevronRight size={15} aria-hidden="true" />
                  </button>
                ))}
              </nav>
            </>
          )}
        </aside>

        <section className="preview-pane">
          <div className="preview-heading">
            <div>
              <span className="section-kicker">Selected scene</span>
              <h2>{preview?.scene.display_name ?? "Display preview"}</h2>
            </div>
            {preview !== null && (
              <div className="timeline-readout">
                <span>Step {preview.timeline.step_index + 1}/{preview.timeline.step_count}</span>
                <strong>{preview.timeline.elapsed_ms} ms</strong>
              </div>
            )}
          </div>

          <div className="display-stage">
            <div className="panel-bezel">
              <FramebufferCanvas framebuffer={preview?.framebuffer ?? null} />
            </div>
            <div className="surface-label">Scene canvas preview</div>
          </div>

          <div className="transport-bar">
            <button className="icon-button" onClick={() => selectedScene !== null && void startPreview(selectedScene)} disabled={preview === null} title="Reset preview">
              <RotateCcw size={18} aria-hidden="true" />
            </button>
            <button className="icon-button transport-play" onClick={() => setPlaying((value) => !value)} disabled={preview === null} title={playing ? "Pause preview" : "Play preview"}>
              {playing ? <Pause size={19} aria-hidden="true" /> : <Play size={19} aria-hidden="true" />}
            </button>
            <button className="icon-button" onClick={() => void advancePreview(250)} disabled={preview === null} title="Advance 250 ms">
              <StepForward size={18} aria-hidden="true" />
            </button>
            <div className="transport-divider" />
            {INPUTS.map(({ source, label, icon: Icon }) => (
              <button key={source} className="input-button" onClick={() => void sendInput(source)} disabled={preview === null} title={`Send ${source}`}>
                <Icon size={16} aria-hidden="true" />
                {label}
              </button>
            ))}
          </div>
        </section>

        <section className="state-graph-pane">
          <div className="preview-heading graph-heading">
            <div>
              <span className="section-kicker">Per-scene STATE graph</span>
              <h2>{selectedSceneDocument?.display_name ?? "No STATE scene selected"}</h2>
            </div>
            <span className="future-graph-label">Package scene-flow graph: Stage 5</span>
          </div>
          <div className="graph-surface">
            <StateGraphView
              scene={selectedSceneDocument}
              selected={sceneSelection}
              onSelect={setSceneSelection}
            />
          </div>
        </section>

        <aside className="inspector-pane">
          <div className="pane-heading">Inspector</div>

          <section className="inspector-section">
            <h3>Runtime</h3>
            {preview === null ? (
              <p className="muted">Start a scene preview to inspect its runtime state.</p>
            ) : (
              <dl className="inspector-list">
                <div><dt>Scene</dt><dd>{preview.scene.scene_id}</dd></div>
                <div><dt>State</dt><dd>{preview.scene.state_id}</dd></div>
                <div><dt>Presentation</dt><dd>{preview.timeline.presentation_id}</dd></div>
                <div><dt>Quantum</dt><dd>{preview.timeline.phase_quantum_ms} ms</dd></div>
                <div><dt>Black pixels</dt><dd>{preview.framebuffer.black_pixel_count}</dd></div>
              </dl>
            )}
          </section>

          <SceneAuthoringInspector
            scene={selectedSceneDocument}
            selection={sceneSelection}
            onSelect={setSceneSelection}
            onRenameState={renameState}
            canEdit={service?.operations.includes("project.apply_commands") === true && busy === null}
          />

          <section className="inspector-section">
            <h3>Variables</h3>
            {preview === null || Object.keys(preview.variables).length === 0 ? (
              <p className="muted">No runtime variables.</p>
            ) : (
              <dl className="inspector-list">
                {Object.entries(preview.variables).map(([name, value]) => (
                  <div key={name}><dt>{name}</dt><dd>{value}</dd></div>
                ))}
              </dl>
            )}
          </section>

          <section className="inspector-section">
            <h3>Validation</h3>
            {project === null ? (
              <p className="muted">No validation result.</p>
            ) : project.issues.length === 0 ? (
              <div className="success-note"><PackageCheck size={17} aria-hidden="true" /> Project is package-ready.</div>
            ) : (
              <div className="issue-list">
                {project.issues.map((issue, index) => (
                  <div className="issue" key={`${issue.code}-${index}`}>
                    <AlertTriangle size={16} aria-hidden="true" />
                    <span><strong>{issue.code}</strong>{issue.message}<small>{issue.path}</small></span>
                  </div>
                ))}
              </div>
            )}
          </section>

          {build !== null && (
            <section className="inspector-section build-result">
              <h3>Last build</h3>
              <dl className="inspector-list">
                <div><dt>Size</dt><dd>{build.package.size_bytes} B</dd></div>
                <div><dt>Chunks</dt><dd>{build.package.chunk_count}</dd></div>
                <div><dt>Scenes</dt><dd>{build.package.scene_count}</dd></div>
              </dl>
              <code>{build.package.sha256.slice(0, 16)}...</code>
            </section>
          )}
        </aside>
      </section>

      <footer className="status-bar">
        <span>{busy !== null ? <><LoaderCircle className="spin" size={14} aria-hidden="true" /> {busy}</> : message ?? "Ready"}</span>
        <span>{project?.source_name ?? "No project"}</span>
      </footer>
    </main>
  );
}
