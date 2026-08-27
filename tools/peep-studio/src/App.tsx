import {
  AlertTriangle,
  ArrowDown,
  ArrowLeft,
  ArrowRight,
  ArrowUp,
  Box,
  Check,
  ChevronRight,
  Circle,
  Download,
  FileCode2,
  FolderOpen,
  Hammer,
  LoaderCircle,
  Maximize2,
  MonitorDot,
  Network,
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
import { useCallback, useEffect, useMemo, useRef, useState, type CSSProperties, type PointerEvent as ReactPointerEvent } from "react";
import { FramebufferCanvas } from "./FramebufferCanvas";
import {
  SceneAuthoringInspector,
  SceneFlowView,
  StateGraphView,
  type SceneSelection,
} from "./SceneInspection";
import type {
  Framebuffer,
  PackageBuildResult,
  ProjectCommandResult,
  ProjectHistoryResult,
  ProjectSceneThumbnailsResult,
  ProjectSaveResult,
  PreviewSnapshot,
  ProjectLoadResult,
  SceneDocument,
  ServiceHello,
} from "./types";

const INPUTS = [
  { source: "BUTTON_L", label: "L", icon: Circle },
  { source: "BUTTON_R", label: "R", icon: Circle },
  { source: "BUTTON_A", label: "A", icon: Circle },
  { source: "BUTTON_B", label: "B", icon: Circle },
] as const;

const JOYSTICK_INPUTS = [
  { source: "JOY_UP", label: "Up", icon: ArrowUp, className: "joy-up" },
  { source: "JOY_LEFT", label: "Left", icon: ArrowLeft, className: "joy-left" },
  { source: "JOY_RIGHT", label: "Right", icon: ArrowRight, className: "joy-right" },
  { source: "JOY_DOWN", label: "Down", icon: ArrowDown, className: "joy-down" },
] as const;

function errorText(error: unknown): string {
  return error instanceof Error ? error.message : String(error);
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
  const [workspaceMode, setWorkspaceMode] = useState<"scene-flow" | "logic" | "placement">("scene-flow");
  const [sceneThumbnails, setSceneThumbnails] = useState<Record<string, Framebuffer>>({});
  const [sceneFlowLayoutStatus, setSceneFlowLayoutStatus] = useState("No layout move yet");
  const [projectWidth, setProjectWidth] = useState(320);
  const [inspectorWidth, setInspectorWidth] = useState(390);
  const [message, setMessage] = useState<string | null>(null);
  const previewRef = useRef<PreviewSnapshot | null>(null);
  const projectRevisionRef = useRef<number | null>(null);
  const layoutSaveChain = useRef(Promise.resolve());
  const operationLock = useRef(false);

  useEffect(() => {
    previewRef.current = preview;
  }, [preview]);

  useEffect(() => {
    projectRevisionRef.current = project?.project_revision ?? null;
  }, [project?.project_revision]);

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
      setSceneThumbnails({});
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
        if (result.scene.scene_id !== selectedScene) {
          setSelectedScene(result.scene.scene_id);
          setSceneSelection({ kind: "scene" });
        }
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
      if (result.scene.scene_id !== selectedScene) {
        setSelectedScene(result.scene.scene_id);
        setSceneSelection({ kind: "scene" });
      }
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
      setMessage(`Saved ${result.saved_sources.length} source file${result.saved_sources.length === 1 ? "" : "s"}.`);
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
    setSceneThumbnails({});
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

  const setRouteTarget = async (sceneId: string, routeId: string, targetState: string) => {
    if (bridge === undefined || project === null || busy !== null) {
      return;
    }
    setBusy("Updating route");
    setPlaying(false);
    try {
      const result = await bridge.serviceRequest<ProjectCommandResult>("project.apply_commands", {
        project_revision: project.project_revision,
        commands: [
          {
            kind: "route.set_target",
            scene_id: sceneId,
            route_id: routeId,
            target_state: targetState,
          },
        ],
      });
      applyProjectResult(result);
      setSceneSelection({ kind: "route", id: routeId });
      setMessage("Route target updated. Save to write it to the project.");
    } catch (error) {
      setMessage(errorText(error));
    } finally {
      setBusy(null);
    }
  };

  const setRouteSceneTarget = async (sceneId: string, routeId: string, targetScene: string) => {
    if (bridge === undefined || project === null || busy !== null) {
      return;
    }
    setBusy("Updating scene exit");
    setPlaying(false);
    try {
      const result = await bridge.serviceRequest<ProjectCommandResult>("project.apply_commands", {
        project_revision: project.project_revision,
        commands: [
          {
            kind: "route.set_target",
            scene_id: sceneId,
            route_id: routeId,
            target_scene: targetScene,
          },
        ],
      });
      applyProjectResult(result);
      setSelectedScene(sceneId);
      setSceneSelection({ kind: "route", id: routeId });
      setMessage("Scene exit updated. Save to write it to the project.");
    } catch (error) {
      setMessage(errorText(error));
    } finally {
      setBusy(null);
    }
  };

  const addSceneExit = async (sceneId: string, logicalSource: string, targetScene: string) => {
    if (bridge === undefined || project === null || busy !== null) {
      return;
    }
    setBusy("Adding scene exit");
    setPlaying(false);
    try {
      const result = await bridge.serviceRequest<ProjectCommandResult>("project.apply_commands", {
        project_revision: project.project_revision,
        commands: [
          {
            kind: "route.add_scene_exit",
            scene_id: sceneId,
            logical_source: logicalSource,
            target_scene: targetScene,
          },
        ],
      });
      const applied = result.applied_commands[0];
      applyProjectResult(result);
      setSelectedScene(sceneId);
      setSceneSelection({ kind: "route", id: String(applied?.route_id ?? "") });
      setMessage("Scene exit added. Save to write it to the project.");
    } catch (error) {
      setMessage(errorText(error));
    } finally {
      setBusy(null);
    }
  };

  const moveSceneNode = async (sceneId: string, x: number, y: number) => {
    if (bridge === undefined || project === null) {
      return;
    }
    setSceneFlowLayoutStatus(`queued ${sceneId} @ ${Math.round(x)}, ${Math.round(y)}`);
    layoutSaveChain.current = layoutSaveChain.current.then(async () => {
      const revision = projectRevisionRef.current;
      if (revision === null) {
        setSceneFlowLayoutStatus(`skipped ${sceneId}: no project revision`);
        return;
      }
      const result = await bridge.serviceRequest<ProjectCommandResult>("project.apply_commands", {
        project_revision: revision,
        commands: [
          {
            kind: "editor.scene_flow.set_node_position",
            scene_id: sceneId,
            x,
            y,
          },
        ],
      });
      projectRevisionRef.current = result.project_revision;
      applyProjectResult(result);
      const applied = result.applied_commands[0];
      setSceneFlowLayoutStatus(
        `saved ${String(applied?.scene_id ?? sceneId)} @ ${String(applied?.x ?? x)}, ${String(applied?.y ?? y)} rev ${result.project_revision}`,
      );
      setMessage("Graph layout updated. Save to write it to the project.");
    }).catch((error) => {
      setSceneFlowLayoutStatus(`save failed: ${errorText(error)}`);
      setMessage(errorText(error));
    });
    await layoutSaveChain.current;
  };

  const setRouteGuard = async (
    sceneId: string,
    routeId: string,
    guardIndex: number,
    variableRef: string,
    operator: string,
    value: number,
  ) => {
    if (bridge === undefined || project === null || busy !== null) {
      return;
    }
    setBusy("Updating guard");
    setPlaying(false);
    try {
      const result = await bridge.serviceRequest<ProjectCommandResult>("project.apply_commands", {
        project_revision: project.project_revision,
        commands: [
          {
            kind: "route.set_guard",
            scene_id: sceneId,
            route_id: routeId,
            guard_index: guardIndex,
            variable_ref: variableRef,
            operator,
            value,
          },
        ],
      });
      applyProjectResult(result);
      setSceneSelection({ kind: "route", id: routeId });
      setMessage("Guard updated. Save to write it to the project.");
    } catch (error) {
      setMessage(errorText(error));
    } finally {
      setBusy(null);
    }
  };

  const setRouteAction = async (
    sceneId: string,
    routeId: string,
    actionIndex: number,
    action: Record<string, unknown>,
  ) => {
    if (bridge === undefined || project === null || busy !== null) {
      return;
    }
    setBusy("Updating action");
    setPlaying(false);
    try {
      const result = await bridge.serviceRequest<ProjectCommandResult>("project.apply_commands", {
        project_revision: project.project_revision,
        commands: [
          {
            kind: "route.set_action",
            scene_id: sceneId,
            route_id: routeId,
            action_index: actionIndex,
            action,
          },
        ],
      });
      applyProjectResult(result);
      setSceneSelection({ kind: "route", id: routeId });
      setMessage("Action updated. Save to write it to the project.");
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
  const projectRevision = project?.project_revision ?? null;
  const projectValid = project?.valid ?? false;
  const thumbnailsSupported = service?.operations.includes("project.scene_thumbnails") === true;

  useEffect(() => {
    if (bridge === undefined || projectRevision === null || !projectValid || !thumbnailsSupported) {
      setSceneThumbnails({});
      return undefined;
    }

    let cancelled = false;
    bridge
      .serviceRequest<ProjectSceneThumbnailsResult>("project.scene_thumbnails", {
        project_revision: projectRevision,
      })
      .then((result) => {
        if (cancelled || result.project_revision !== projectRevision) {
          return;
        }
        setSceneThumbnails(Object.fromEntries(result.thumbnails.map((item) => [item.scene_id, item.framebuffer])));
      })
      .catch((error) => {
        if (!cancelled) {
          setSceneThumbnails({});
          setMessage(errorText(error));
        }
      });

    return () => {
      cancelled = true;
    };
  }, [bridge, projectRevision, projectValid, thumbnailsSupported]);

  const selectedSceneDocument = useMemo(
    () => scenes.find((scene) => scene.scene_id === selectedScene) ?? null,
    [scenes, selectedScene],
  );
  const connected = service !== null;
  const startProjectResize = (event: ReactPointerEvent<HTMLDivElement>) => {
    event.preventDefault();
    const startX = event.clientX;
    const startWidth = projectWidth;
    const move = (moveEvent: PointerEvent) => {
      const next = Math.min(520, Math.max(260, startWidth + (moveEvent.clientX - startX)));
      setProjectWidth(next);
    };
    const stop = () => {
      window.removeEventListener("pointermove", move);
      window.removeEventListener("pointerup", stop);
    };
    window.addEventListener("pointermove", move);
    window.addEventListener("pointerup", stop);
  };
  const startInspectorResize = (event: ReactPointerEvent<HTMLDivElement>) => {
    event.preventDefault();
    const startX = event.clientX;
    const startWidth = inspectorWidth;
    const move = (moveEvent: PointerEvent) => {
      const next = Math.min(560, Math.max(320, startWidth - (moveEvent.clientX - startX)));
      setInspectorWidth(next);
    };
    const stop = () => {
      window.removeEventListener("pointermove", move);
      window.removeEventListener("pointerup", stop);
    };
    window.addEventListener("pointermove", move);
    window.addEventListener("pointerup", stop);
  };
  const renderInputControls = () => (
    <div className="input-controls">
      <div className="input-control-group">
        <span className="input-group-label">Joystick</span>
        <div className="joystick-pad" aria-label="Joystick cardinal inputs">
          {JOYSTICK_INPUTS.map(({ source, label, icon: Icon, className }) => (
            <button key={source} className={`joystick-button ${className}`} onClick={() => void sendInput(source)} disabled={preview === null} title={`Send ${source}`}>
              <Icon size={15} aria-hidden="true" />
              <span>{label}</span>
            </button>
          ))}
          <button className="joystick-nub" type="button" disabled title="Analog joystick preview is reserved for PROGRAM scenes" aria-label="Analog joystick preview reserved for PROGRAM scenes" />
        </div>
      </div>
      <div className="input-control-group">
        <span className="input-group-label">Buttons</span>
        <div className="trigger-buttons" aria-label="Trigger buttons">
          {INPUTS.map(({ source, label, icon: Icon }) => (
            <button key={source} className="input-button" onClick={() => void sendInput(source)} disabled={preview === null} title={`Send ${source}`}>
              <Icon size={13} aria-hidden="true" />
              {label}
            </button>
          ))}
        </div>
      </div>
    </div>
  );
  const renderPreviewPanel = (variant: "project" | "placement") => (
    <section className={`preview-pane ${variant === "placement" ? "preview-pane-large" : "preview-pane-compact"}`}>
      <div className="preview-heading">
        <div>
          <span className="section-kicker">Screen</span>
          <h2>{preview?.scene.display_name ?? "Display preview"}</h2>
        </div>
        <div className="preview-heading-tools">
          {preview !== null && (
            <div className="timeline-readout">
              <span>Step {preview.timeline.step_index + 1}/{preview.timeline.step_count}</span>
              <strong>{preview.timeline.elapsed_ms} ms</strong>
            </div>
          )}
          <div className="playback-controls" aria-label="Preview playback">
            <button className="icon-button" onClick={() => selectedScene !== null && void startPreview(selectedScene)} disabled={preview === null} title="Reset preview">
              <RotateCcw size={18} aria-hidden="true" />
            </button>
            <button className="icon-button transport-play" onClick={() => setPlaying((value) => !value)} disabled={preview === null} title={playing ? "Pause preview" : "Play preview"}>
              {playing ? <Pause size={19} aria-hidden="true" /> : <Play size={19} aria-hidden="true" />}
            </button>
            <button className="icon-button" onClick={() => void advancePreview(250)} disabled={preview === null} title="Advance 250 ms">
              <StepForward size={18} aria-hidden="true" />
            </button>
          </div>
        </div>
      </div>

      <div className="display-stage">
        <div className="panel-bezel">
          <FramebufferCanvas framebuffer={preview?.framebuffer ?? null} />
        </div>
      </div>

      <div className="transport-bar">
        {renderInputControls()}
      </div>
    </section>
  );

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

      <section
        className={`workspace-grid ${workspaceMode === "placement" ? "placement-mode" : workspaceMode === "scene-flow" ? "scene-flow-mode" : "logic-mode"}`}
        style={{ "--project-width": `${projectWidth}px`, "--inspector-width": `${inspectorWidth}px` } as CSSProperties}
      >
        <aside className="project-pane">
          <div className="pane-heading">
            <span>Project</span>
            {project !== null && (
              <span className={`validation-state ${project.valid ? "valid" : "invalid"}`}>
                <StatusMark ok={project.valid} /> {project.valid ? "Valid" : "Invalid"}
              </span>
            )}
          </div>

          {workspaceMode !== "placement" && renderPreviewPanel("project")}

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

        <div
          className="project-resize-handle"
          role="separator"
          aria-orientation="vertical"
          aria-label="Resize project bar"
          title="Drag to resize project bar"
          onPointerDown={startProjectResize}
        />

        {workspaceMode === "placement" && (
          <section className="placement-pane">
            <div className="preview-heading graph-heading">
              <div>
                <span className="section-kicker">Placement mode</span>
                <h2>{selectedSceneDocument?.display_name ?? "No scene selected"}</h2>
              </div>
              <button className="button secondary" type="button" onClick={() => setWorkspaceMode("logic")}>
                <Network size={16} aria-hidden="true" />
                Logic
              </button>
              <button className="button secondary" type="button" onClick={() => setWorkspaceMode("scene-flow")}>
                <Network size={16} aria-hidden="true" />
                Scene flow
              </button>
            </div>
            {renderPreviewPanel("placement")}
          </section>
        )}

        {workspaceMode === "scene-flow" && (
        <section className="scene-flow-pane">
          <div className="preview-heading graph-heading">
            <div>
              <span className="section-kicker">Scene flow</span>
              <h2>{project?.summary.project_name ?? "No project open"}</h2>
            </div>
            <div className="mode-actions">
              <button className="button secondary" type="button" onClick={() => setWorkspaceMode("logic")}>
                <Network size={16} aria-hidden="true" />
                Local logic
              </button>
              <button className="button secondary" type="button" onClick={() => setWorkspaceMode("placement")}>
                <Maximize2 size={16} aria-hidden="true" />
                Placement
              </button>
            </div>
          </div>
          <div className="graph-surface">
            <SceneFlowView
              scenes={scenes}
              entrySceneId={project?.summary.entry_scene ?? null}
              thumbnails={sceneThumbnails}
              editor={project?.document?.project?.editor}
              layoutStatus={sceneFlowLayoutStatus}
              selectedSceneId={selectedScene}
              selectedRouteId={sceneSelection.kind === "route" ? sceneSelection.id : null}
              onSelectScene={(sceneId) => void startPreview(sceneId)}
              onSelectSceneRoute={(sceneId, routeId) => {
                setSelectedScene(sceneId);
                setSceneSelection({ kind: "route", id: routeId });
              }}
              onAddSceneExit={(sceneId, logicalSource, targetScene) => {
                void addSceneExit(sceneId, logicalSource, targetScene);
              }}
              onMoveSceneNode={(sceneId, x, y) => {
                void moveSceneNode(sceneId, x, y);
              }}
              canEdit={service?.operations.includes("project.apply_commands") === true && busy === null}
            />
          </div>
        </section>
        )}

        {workspaceMode === "logic" && (
        <section className="state-graph-pane">
          <div className="preview-heading graph-heading">
            <div>
              <span className="section-kicker">Logic graph</span>
              <h2>{selectedSceneDocument?.display_name ?? "No scene selected"}</h2>
            </div>
            <div className="mode-actions">
              <button className="button secondary" type="button" onClick={() => setWorkspaceMode("scene-flow")}>
                <Network size={16} aria-hidden="true" />
                Scene flow
              </button>
              <button className="button secondary" type="button" onClick={() => setWorkspaceMode("placement")}>
                <Maximize2 size={16} aria-hidden="true" />
                Placement
              </button>
            </div>
          </div>
          <div className="graph-surface">
            <StateGraphView
              scene={selectedSceneDocument}
              selected={sceneSelection}
              onSelect={setSceneSelection}
            />
          </div>
        </section>
        )}

        <div
          className="inspector-resize-handle"
          role="separator"
          aria-orientation="vertical"
          aria-label="Resize inspector"
          title="Drag to resize inspector"
          onPointerDown={startInspectorResize}
        />

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
            scenes={scenes}
            selection={sceneSelection}
            onSelect={setSceneSelection}
            onRenameState={renameState}
            onSetRouteTarget={setRouteTarget}
            onSetRouteSceneTarget={setRouteSceneTarget}
            onSetRouteGuard={setRouteGuard}
            onSetRouteAction={setRouteAction}
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
