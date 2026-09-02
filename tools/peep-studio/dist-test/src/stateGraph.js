"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.resolveStateExitSide = resolveStateExitSide;
exports.resolveStateEntryHandle = resolveStateEntryHandle;
exports.buildStateTransitionRoute = buildStateTransitionRoute;
exports.planStateTransitionRoutes = planStateTransitionRoutes;
exports.buildStateGraphModel = buildStateGraphModel;
exports.buildSceneFlowGraphModel = buildSceneFlowGraphModel;
const INPUT_LABELS = {
    BUTTON_A: "Button A",
    BUTTON_START: "Start",
    BUTTON_B: "Button B",
    BUTTON_L: "Left button",
    BUTTON_R: "Right button",
    JOY_LEFT: "Joystick left",
    JOY_RIGHT: "Joystick right",
    JOY_UP: "Joystick up",
    JOY_DOWN: "Joystick down",
};
function inputLabel(inputActions, actionRef) {
    const input = inputActions.find((item) => item.action_id === actionRef);
    return INPUT_LABELS[input?.logical_source ?? ""] ?? input?.logical_source ?? actionRef;
}
function inputKind(inputActions, actionRef) {
    const source = inputActions.find((item) => item.action_id === actionRef)?.logical_source ?? "";
    return source.startsWith("BUTTON_") || source.startsWith("JOY_") ? "physical" : "platform";
}
function inputSource(inputActions, actionRef) {
    return inputActions.find((item) => item.action_id === actionRef)?.logical_source ?? actionRef;
}
const PHYSICAL_TRIGGER_EXITS = {
    BUTTON_L: { side: "top", ratio: 0.183 },
    BUTTON_R: { side: "top", ratio: 0.817 },
    JOY_UP: { side: "left", ratio: 0.63 },
    JOY_UP_LEFT: { side: "left", ratio: 0.69 },
    JOY_UP_RIGHT: { side: "right", ratio: 0.72 },
    JOY_LEFT: { side: "left", ratio: 0.75 },
    JOY_RIGHT: { side: "bottom", ratio: 0.363 },
    JOY_DOWN: { side: "bottom", ratio: 0.24 },
    JOY_DOWN_LEFT: { side: "bottom", ratio: 0.16 },
    JOY_DOWN_RIGHT: { side: "bottom", ratio: 0.4 },
    BUTTON_START: { side: "bottom", ratio: 0.533 },
    BUTTON_A: { side: "right", ratio: 0.67 },
    BUTTON_B: { side: "bottom", ratio: 0.713 },
};
function countLabel(count, singular) {
    if (count === 0) {
        return "";
    }
    return `${count} ${singular}${count === 1 ? "" : "s"}`;
}
function displayRefName(ref, fallback) {
    if (ref === undefined || ref.length === 0) {
        return fallback;
    }
    const label = ref.replaceAll("_", " ");
    return label.charAt(0).toUpperCase() + label.slice(1);
}
function signedValue(value) {
    if (value === undefined) {
        return "";
    }
    return value > 0 ? `+${value}` : String(value);
}
function actionEffectLabel(action) {
    if (action.kind === "request_render") {
        return null;
    }
    if (action.kind === "set_variable") {
        const variableName = displayRefName(action.variable_ref, "variable");
        if (action.operation === "add") {
            return `${variableName}${action.value === undefined ? "" : ` ${signedValue(action.value)}`}`;
        }
        if (action.operation === "assign" || action.operation === "set") {
            return `${variableName}${action.value === undefined ? "" : ` = ${action.value}`}`;
        }
        return variableName;
    }
    if (action.kind === "play_sfx") {
        return "Play sound";
    }
    if (action.kind === "exit_to_shell") {
        return "Exit package";
    }
    if (action.kind === "transition_scene") {
        return "Open scene";
    }
    return "Advanced effect";
}
function routeEffectLabels(route) {
    return route.actions
        .map(actionEffectLabel)
        .filter((label) => label !== null);
}
function visibleActionCount(route) {
    return routeEffectLabels(route).length;
}
function routeLabel(route, inputActions) {
    const badges = [countLabel(route.guards.length, "rule"), countLabel(visibleActionCount(route), "effect")].filter(Boolean);
    return [inputLabel(inputActions, route.action_ref), ...badges].join(" - ");
}
function statePosition(state, index, columns, savedPositions) {
    return {
        x: savedPositions?.[state.state_id]?.x ?? (index % columns) * 340,
        y: savedPositions?.[state.state_id]?.y ?? Math.floor(index / columns) * 270,
    };
}
const STATE_CARD_ROUTING_WIDTH = 300;
const STATE_CARD_BASE_HEIGHT = 268;
const STATE_CARD_ROUTE_CLEARANCE = 72;
const STATE_CARD_ROUTE_LANE_STEP = 36;
const STATE_PLATFORM_OUTPUT_FIRST_Y = 104;
const STATE_OUTPUT_STEP_Y = 52;
function stateCardHeight(platformOutputCount) {
    return STATE_CARD_BASE_HEIGHT + (platformOutputCount ?? 0) * STATE_OUTPUT_STEP_Y;
}
function centerX(position) {
    return position.x + STATE_CARD_ROUTING_WIDTH / 2;
}
function centerY(position) {
    return position.y + stateCardHeight(position.platformOutputCount) / 2;
}
function resolveStateExitSide(sourcePosition, targetPosition) {
    if (targetPosition === undefined) {
        return "right";
    }
    return centerX(targetPosition) < centerX(sourcePosition) ? "left" : "right";
}
function resolveStateEntryHandle(sourcePosition, targetPosition) {
    if (targetPosition === undefined) {
        return "entry-top-left";
    }
    return resolveStateEntryHandleFromPoint({ x: centerX(sourcePosition), y: centerY(sourcePosition) }, targetPosition);
}
function resolveStateEntryHandleFromPoint(sourcePoint, targetPosition) {
    const verticalSide = sourcePoint.y > centerY(targetPosition) ? "bottom" : "top";
    const horizontalSlot = sourcePoint.x < centerX(targetPosition) ? "left" : "right";
    return `entry-${verticalSide}-${horizontalSlot}`;
}
function stateEntryHandleSide(handle) {
    return handle.startsWith("entry-bottom") ? "bottom" : "top";
}
function stateEntryHandlePoint(position, handle) {
    const ratio = handle.endsWith("-left") ? 0.08 : 0.92;
    return {
        x: position.x + STATE_CARD_ROUTING_WIDTH * ratio,
        y: stateEntryHandleSide(handle) === "bottom" ? position.y + stateCardHeight(position.platformOutputCount) : position.y,
    };
}
function stateOutputPoint(position, sourceSide, outputIndex, sourceRatio) {
    const height = stateCardHeight(position.platformOutputCount);
    if (sourceRatio !== undefined) {
        if (sourceSide === "top" || sourceSide === "bottom") {
            return {
                x: position.x + STATE_CARD_ROUTING_WIDTH * sourceRatio,
                y: sourceSide === "top" ? position.y : position.y + height,
            };
        }
        return {
            x: sourceSide === "left" ? position.x : position.x + STATE_CARD_ROUTING_WIDTH,
            y: position.y + height * sourceRatio,
        };
    }
    return {
        x: sourceSide === "left" ? position.x : position.x + STATE_CARD_ROUTING_WIDTH,
        y: position.y + STATE_PLATFORM_OUTPUT_FIRST_Y + outputIndex * STATE_OUTPUT_STEP_Y,
    };
}
function defaultStateLane(sourceX, sourceY, targetX, targetY, sourceSide) {
    if (sourceSide === "top" || sourceSide === "bottom") {
        const sameRow = Math.abs(sourceY - targetY) < 180;
        const naturallySeparated = sourceSide === "top"
            ? sourceY > targetY + 48
            : sourceY < targetY - 48;
        if (!sameRow && naturallySeparated) {
            return sourceY + (targetY - sourceY) / 2;
        }
        return sourceSide === "top"
            ? Math.min(sourceY, targetY) - STATE_CARD_ROUTE_CLEARANCE
            : Math.max(sourceY, targetY) + STATE_CARD_ROUTE_CLEARANCE;
    }
    const sameColumn = Math.abs(sourceX - targetX) < 180;
    const naturallySeparated = sourceSide === "right"
        ? sourceX < targetX - 48
        : sourceX > targetX + 48;
    if (!sameColumn && naturallySeparated) {
        return sourceX + (targetX - sourceX) / 2;
    }
    return sourceSide === "left"
        ? Math.min(sourceX, targetX) - STATE_CARD_ROUTE_CLEARANCE
        : Math.max(sourceX, targetX) + STATE_CARD_ROUTE_CLEARANCE;
}
function candidateLaneXs(sourceX, sourceY, targetX, targetY, sourceSide) {
    const verticalExit = sourceSide === "top" || sourceSide === "bottom";
    const outsideLane = verticalExit
        ? sourceSide === "top"
            ? Math.min(sourceY, targetY) - STATE_CARD_ROUTE_CLEARANCE
            : Math.max(sourceY, targetY) + STATE_CARD_ROUTE_CLEARANCE
        : sourceSide === "left"
            ? Math.min(sourceX, targetX) - STATE_CARD_ROUTE_CLEARANCE
            : Math.max(sourceX, targetX) + STATE_CARD_ROUTE_CLEARANCE;
    const lanes = new Set([defaultStateLane(sourceX, sourceY, targetX, targetY, sourceSide)]);
    for (let index = 0; index < 6; index += 1) {
        const negativeDirection = sourceSide === "left" || sourceSide === "top";
        lanes.add(outsideLane + (negativeDirection ? -1 : 1) * index * STATE_CARD_ROUTE_LANE_STEP);
    }
    return [...lanes];
}
function roundedPolylinePath(points, radius) {
    if (points.length === 0) {
        return "";
    }
    if (points.length === 1) {
        return `M ${points[0].x} ${points[0].y}`;
    }
    const commands = [`M ${points[0].x} ${points[0].y}`];
    for (let index = 1; index < points.length - 1; index += 1) {
        const previous = points[index - 1];
        const current = points[index];
        const next = points[index + 1];
        const previousDistance = Math.hypot(current.x - previous.x, current.y - previous.y);
        const nextDistance = Math.hypot(next.x - current.x, next.y - current.y);
        const cornerRadius = Math.min(radius, previousDistance / 2, nextDistance / 2);
        if (cornerRadius <= 0) {
            commands.push(`L ${current.x} ${current.y}`);
            continue;
        }
        const before = {
            x: current.x - ((current.x - previous.x) / previousDistance) * cornerRadius,
            y: current.y - ((current.y - previous.y) / previousDistance) * cornerRadius,
        };
        const after = {
            x: current.x + ((next.x - current.x) / nextDistance) * cornerRadius,
            y: current.y + ((next.y - current.y) / nextDistance) * cornerRadius,
        };
        commands.push(`L ${before.x} ${before.y}`);
        commands.push(`Q ${current.x} ${current.y} ${after.x} ${after.y}`);
    }
    const last = points[points.length - 1];
    commands.push(`L ${last.x} ${last.y}`);
    return commands.join(" ");
}
function compactRoutePoints(points) {
    return points.filter((point, index) => {
        const previous = points[index - 1];
        const next = points[index + 1];
        if (previous !== undefined && point.x === previous.x && point.y === previous.y) {
            return false;
        }
        if (previous === undefined || next === undefined) {
            return true;
        }
        return !(previous.x === point.x && point.x === next.x) && !(previous.y === point.y && point.y === next.y);
    });
}
function buildStateTransitionRoute({ sourceX, sourceY, targetX, targetY, sourceSide, targetSide, laneX, }) {
    const horizontalExit = sourceSide === "left" || sourceSide === "right";
    const sourceOutset = sourceSide === "left" || sourceSide === "top" ? -28 : 28;
    const entryOutset = targetSide === "top" ? -36 : 36;
    const resolvedLane = laneX ?? defaultStateLane(sourceX, sourceY, targetX, targetY, sourceSide);
    const entryY = targetY + entryOutset;
    const points = compactRoutePoints(horizontalExit
        ? [
            { x: sourceX, y: sourceY },
            { x: sourceX + sourceOutset, y: sourceY },
            { x: resolvedLane, y: sourceY },
            { x: resolvedLane, y: entryY },
            { x: targetX, y: entryY },
            { x: targetX, y: targetY },
        ]
        : [
            { x: sourceX, y: sourceY },
            { x: sourceX, y: sourceY + sourceOutset },
            { x: sourceX, y: resolvedLane },
            { x: targetX, y: resolvedLane },
            { x: targetX, y: entryY },
            { x: targetX, y: targetY },
        ]);
    return {
        path: roundedPolylinePath(points, 14),
        points,
    };
}
function routeSegments(points) {
    const segments = [];
    for (let index = 1; index < points.length; index += 1) {
        const previous = points[index - 1];
        const current = points[index];
        if (previous.x === current.x) {
            segments.push({
                orientation: "vertical",
                min: Math.min(previous.y, current.y),
                max: Math.max(previous.y, current.y),
                fixed: previous.x,
            });
        }
        else if (previous.y === current.y) {
            segments.push({
                orientation: "horizontal",
                min: Math.min(previous.x, current.x),
                max: Math.max(previous.x, current.x),
                fixed: previous.y,
            });
        }
    }
    return segments;
}
function intervalOverlap(leftMin, leftMax, rightMin, rightMax) {
    return Math.max(0, Math.min(leftMax, rightMax) - Math.max(leftMin, rightMin));
}
function overlappingSegmentScore(candidate, placed) {
    let score = 0;
    candidate.forEach((candidateSegment) => {
        placed.forEach((placedSegment) => {
            if (candidateSegment.orientation === placedSegment.orientation) {
                const fixedDistance = Math.abs(candidateSegment.fixed - placedSegment.fixed);
                if (fixedDistance <= 18) {
                    const overlap = intervalOverlap(candidateSegment.min, candidateSegment.max, placedSegment.min, placedSegment.max);
                    score += overlap * (22 - fixedDistance);
                }
                return;
            }
            const horizontal = candidateSegment.orientation === "horizontal" ? candidateSegment : placedSegment;
            const vertical = candidateSegment.orientation === "vertical" ? candidateSegment : placedSegment;
            const crosses = vertical.fixed >= horizontal.min
                && vertical.fixed <= horizontal.max
                && horizontal.fixed >= vertical.min
                && horizontal.fixed <= vertical.max;
            if (crosses) {
                score += 18;
            }
        });
    });
    return score;
}
function nodeCrossingScore(candidate, nodes, source, target) {
    let score = 0;
    nodes
        .filter((node) => node.id !== source && node.id !== target)
        .forEach((node) => {
        const left = node.x - 10;
        const right = node.x + STATE_CARD_ROUTING_WIDTH + 10;
        const top = node.y - 10;
        const bottom = node.y + stateCardHeight(node.platformOutputCount) + 10;
        candidate.forEach((segment) => {
            if (segment.orientation === "horizontal") {
                const crosses = segment.fixed >= top
                    && segment.fixed <= bottom
                    && intervalOverlap(segment.min, segment.max, left, right) > 0;
                if (crosses) {
                    score += 5000;
                }
                return;
            }
            const crosses = segment.fixed >= left
                && segment.fixed <= right
                && intervalOverlap(segment.min, segment.max, top, bottom) > 0;
            if (crosses) {
                score += 5000;
            }
        });
    });
    return score;
}
function planStateTransitionRoutes(requests, nodes) {
    const nodeById = new Map(nodes.map((node) => [node.id, node]));
    const placedSegments = [];
    const planned = {};
    const sortedRequests = [...requests].sort((left, right) => {
        const leftSource = nodeById.get(left.source);
        const leftTarget = nodeById.get(left.target);
        const rightSource = nodeById.get(right.source);
        const rightTarget = nodeById.get(right.target);
        const leftDistance = leftSource === undefined || leftTarget === undefined
            ? 0
            : Math.abs(centerX(leftSource) - centerX(leftTarget)) + Math.abs(centerY(leftSource) - centerY(leftTarget));
        const rightDistance = rightSource === undefined || rightTarget === undefined
            ? 0
            : Math.abs(centerX(rightSource) - centerX(rightTarget)) + Math.abs(centerY(rightSource) - centerY(rightTarget));
        return rightDistance - leftDistance || left.id.localeCompare(right.id);
    });
    sortedRequests.forEach((request) => {
        const sourcePosition = nodeById.get(request.source);
        const targetPosition = nodeById.get(request.target);
        if (sourcePosition === undefined || targetPosition === undefined) {
            return;
        }
        const preferredSide = request.sourceSide ?? resolveStateExitSide(sourcePosition, targetPosition);
        const candidateSides = request.sourceSide !== undefined
            ? [request.sourceSide]
            : preferredSide === "right" ? ["right", "left"] : ["left", "right"];
        let best;
        candidateSides.forEach((sourceSide) => {
            const sourcePoint = stateOutputPoint(sourcePosition, sourceSide, request.sourceOutputIndex, request.sourceRatio);
            const targetHandle = resolveStateEntryHandleFromPoint(sourcePoint, targetPosition);
            const targetPoint = stateEntryHandlePoint(targetPosition, targetHandle);
            const baseLaneX = defaultStateLane(sourcePoint.x, sourcePoint.y, targetPoint.x, targetPoint.y, sourceSide);
            candidateLaneXs(sourcePoint.x, sourcePoint.y, targetPoint.x, targetPoint.y, sourceSide).forEach((laneX) => {
                const route = buildStateTransitionRoute({
                    sourceX: sourcePoint.x,
                    sourceY: sourcePoint.y,
                    targetX: targetPoint.x,
                    targetY: targetPoint.y,
                    sourceSide,
                    targetSide: stateEntryHandleSide(targetHandle),
                    laneX,
                });
                const segments = routeSegments(route.points);
                const sidePenalty = sourceSide === preferredSide ? 0 : 24;
                const distancePenalty = Math.abs(laneX - baseLaneX) * 0.08;
                const score = sidePenalty
                    + distancePenalty
                    + overlappingSegmentScore(segments, placedSegments)
                    + nodeCrossingScore(segments, nodes, request.source, request.target);
                if (best === undefined || score < best.score) {
                    best = {
                        layout: { sourceSide, targetHandle, laneX },
                        segments,
                        score,
                    };
                }
            });
        });
        if (best !== undefined) {
            planned[request.id] = best.layout;
            placedSegments.push(...best.segments);
        }
    });
    return planned;
}
function buildStateGraphModel(scene, editor) {
    const states = scene?.states ?? [];
    const routes = scene?.routes ?? [];
    const inputActions = scene?.input_actions ?? [];
    const entryState = scene?.entry_state ?? null;
    const columns = Math.max(1, Math.ceil(Math.sqrt(states.length)));
    const stateIds = new Set(states.map((state) => state.state_id));
    const stateLabels = new Map(states.map((state) => [state.state_id, state.display_name]));
    const outputsByState = new Map();
    const variableRefsByState = new Map();
    const savedPositions = scene === null ? undefined : editor?.state_graph?.scenes?.[scene.scene_id]?.nodes;
    routes.forEach((route) => {
        const effectLabels = routeEffectLabels(route);
        route.from_states
            .filter((source) => stateIds.has(source) && (route.target_scene !== undefined || (route.target_state !== undefined && stateIds.has(route.target_state))))
            .forEach((source) => {
            const outputs = outputsByState.get(source) ?? [];
            const variableRefs = variableRefsByState.get(source) ?? new Set();
            const logicalSource = inputSource(inputActions, route.action_ref);
            const physicalExit = PHYSICAL_TRIGGER_EXITS[logicalSource];
            route.guards.forEach((guard) => variableRefs.add(guard.variable_ref));
            route.actions.forEach((action) => {
                if (action.kind === "set_variable" && action.variable_ref !== undefined) {
                    variableRefs.add(action.variable_ref);
                }
            });
            outputs.push({
                id: `${route.route_id}:${source}`,
                routeId: route.route_id,
                label: inputLabel(inputActions, route.action_ref),
                guardCount: route.guards.length,
                actionCount: effectLabels.length,
                effectLabels,
                logicalSource,
                triggerKind: inputKind(inputActions, route.action_ref),
                preferredExitSide: physicalExit?.side,
                exitRatio: physicalExit?.ratio,
                targetState: route.target_state,
                targetStateLabel: route.target_state === undefined ? undefined : stateLabels.get(route.target_state),
                targetScene: route.target_scene,
            });
            outputsByState.set(source, outputs);
            variableRefsByState.set(source, variableRefs);
        });
    });
    const nodes = states.map((state, index) => {
        const position = statePosition(state, index, columns, savedPositions);
        const outputs = outputsByState.get(state.state_id) ?? [];
        return {
            id: state.state_id,
            label: state.display_name,
            isEntry: state.state_id === entryState,
            variableTouchCount: variableRefsByState.get(state.state_id)?.size ?? 0,
            placementOverrideCount: state.placement_overrides?.length ?? 0,
            platformOutputCount: outputs.filter((output) => output.triggerKind === "platform").length,
            waitingVisualRef: state.waiting_visual_ref,
            outputs,
            x: position.x,
            y: position.y,
        };
    });
    const edges = routes.flatMap((route) => {
        const targetState = route.target_state;
        if (targetState === undefined || !stateIds.has(targetState)) {
            return [];
        }
        return route.from_states
            .filter((source) => stateIds.has(source))
            .map((source) => ({
            id: `${route.route_id}:${source}->${targetState}`,
            source,
            target: targetState,
            label: "",
            route,
            sourceHandle: `${route.route_id}:${source}`,
            effectLabels: routeEffectLabels(route),
        }));
    });
    return { nodes, edges };
}
function buildSceneFlowGraphModel(scenes, entrySceneId, editor) {
    const sceneIds = new Set(scenes.map((scene) => scene.scene_id));
    const edges = scenes.flatMap((scene) => (scene.routes ?? [])
        .filter((route) => route.target_scene !== undefined && sceneIds.has(route.target_scene))
        .map((route) => ({
        id: `${scene.scene_id}:${route.route_id}->${route.target_scene}`,
        source: scene.scene_id,
        target: route.target_scene,
        label: routeLabel(route, scene.input_actions ?? []),
        route,
    })));
    const outgoingCounts = new Map();
    edges.forEach((edge) => {
        outgoingCounts.set(edge.source, (outgoingCounts.get(edge.source) ?? 0) + 1);
    });
    const exitsByScene = new Map();
    scenes.forEach((scene) => {
        const exits = (scene.routes ?? [])
            .filter((route) => route.target_scene !== undefined && sceneIds.has(route.target_scene))
            .map((route) => ({
            id: `${scene.scene_id}:${route.route_id}`,
            routeId: route.route_id,
            label: routeLabel(route, scene.input_actions ?? []),
            targetScene: route.target_scene,
            guardCount: route.guards.length,
        }));
        exitsByScene.set(scene.scene_id, exits);
    });
    const incomingTargets = new Set(edges.map((edge) => edge.target));
    const columnY = new Map();
    const nodes = scenes.map((scene, index) => {
        const states = scene.states ?? [];
        const entryState = states.find((state) => state.state_id === scene.entry_state);
        const isEntry = scene.scene_id === entrySceneId;
        const column = isEntry || !incomingTargets.has(scene.scene_id) ? 0 : 1;
        const exits = exitsByScene.get(scene.scene_id) ?? [];
        const y = columnY.get(column) ?? 0;
        columnY.set(column, y + 430 + exits.length * 58);
        const savedPosition = editor?.scene_flow?.nodes?.[scene.scene_id];
        return {
            id: scene.scene_id,
            label: scene.display_name,
            isEntry,
            stateCount: states.length,
            routeCount: outgoingCounts.get(scene.scene_id) ?? 0,
            entryStateLabel: entryState?.display_name ?? scene.entry_state ?? "No start state",
            exits,
            usedLogicalSources: (scene.input_actions ?? []).map((action) => action.logical_source),
            x: savedPosition?.x ?? column * 360,
            y: savedPosition?.y ?? y,
        };
    });
    return { nodes, edges };
}
