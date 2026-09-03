"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
const strict_1 = __importDefault(require("node:assert/strict"));
const stateGraph_1 = require("../src/stateGraph");
const scene = {
    scene_id: "menu",
    display_name: "Menu",
    scene_type: "STATE_SCENE",
    entry_state: "idle",
    states: [
        {
            state_id: "idle",
            display_name: "Idle",
            render_model_ref: "idle_visual",
            waiting_visual_ref: "idle_wait",
        },
        {
            state_id: "armed",
            display_name: "Armed",
            render_model_ref: "armed_visual",
            waiting_visual_ref: "armed_wait",
        },
    ],
    input_actions: [
        {
            action_id: "select",
            logical_source: "BUTTON_A",
        },
    ],
    routes: [
        {
            route_id: "press_a",
            action_ref: "select",
            from_states: ["idle"],
            guards: [{ variable_ref: "coins", operator: "gt", value: 0 }],
            actions: [
                { kind: "set_variable", variable_ref: "coins", operation: "add", value: -1 },
                { kind: "request_render" },
            ],
            target_state: "armed",
        },
    ],
};
const graph = (0, stateGraph_1.buildStateGraphModel)(scene);
const savedGraph = (0, stateGraph_1.buildStateGraphModel)(scene, {
    state_graph: {
        scenes: {
            menu: {
                nodes: {
                    armed: { x: -120, y: 220 },
                },
                routes: {
                    press_a: {
                        sources: {
                            idle: {
                                routing_version: 3,
                                target_handle: "entry-bottom-right",
                                target_side: "right",
                                rails: [{ axis: "x", value: 180 }, { axis: "y", value: 90 }],
                            },
                        },
                    },
                },
            },
        },
    },
});
const legacyRouteGraph = (0, stateGraph_1.buildStateGraphModel)(scene, {
    state_graph: {
        scenes: {
            menu: {
                nodes: {},
                routes: {
                    press_a: {
                        sources: {
                            idle: {
                                routing_version: 2,
                                waypoints: [{ x: 900, y: 900 }, { x: 920, y: 940 }],
                            },
                        },
                    },
                },
            },
        },
    },
});
strict_1.default.equal(graph.nodes.length, 2);
strict_1.default.equal(graph.endpoints.length, 1);
strict_1.default.equal(graph.endpoints[0]?.kind, "entry");
strict_1.default.equal(graph.entryEdge?.target, "idle");
strict_1.default.equal(graph.nodes[0]?.id, "idle");
strict_1.default.equal(graph.nodes[0]?.isEntry, true);
strict_1.default.equal(graph.nodes[0]?.outputs.length, 1);
strict_1.default.equal(graph.nodes[0]?.outputs[0]?.label, "Button A");
strict_1.default.equal(graph.nodes[0]?.outputs[0]?.guardCount, 1);
strict_1.default.equal(graph.nodes[0]?.outputs[0]?.actionCount, 1);
strict_1.default.equal(graph.nodes[0]?.outputs[0]?.effectLabels[0], "Coins -1");
strict_1.default.equal(graph.nodes[0]?.outputs[0]?.eventKind, "press");
strict_1.default.equal(graph.nodes[0]?.outputs[0]?.triggerKind, "physical");
strict_1.default.equal(graph.nodes[0]?.outputs[0]?.preferredExitSide, "right");
strict_1.default.equal(graph.nodes[0]?.outputs[0]?.exitRatio, 0.67);
strict_1.default.equal(graph.nodes[0]?.variableTouchCount, 1);
strict_1.default.equal(graph.nodes[1]?.isEntry, false);
strict_1.default.equal(graph.edges.length, 1);
strict_1.default.equal(graph.edges[0]?.source, "idle");
strict_1.default.equal(graph.edges[0]?.target, "armed");
strict_1.default.equal(graph.edges[0]?.label, "");
strict_1.default.equal(graph.edges[0]?.sourceHandle, "press_a:idle");
strict_1.default.equal(graph.edges[0]?.effectLabels[0], "Coins -1");
strict_1.default.equal(graph.edges[0]?.guards[0]?.variable_ref, "coins");
strict_1.default.equal(graph.edges[0]?.actions[0]?.kind, "set_variable");
strict_1.default.deepEqual(savedGraph.edges[0]?.rails, [{ axis: "x", value: 180 }, { axis: "y", value: 90 }]);
strict_1.default.equal(savedGraph.edges[0]?.targetHandle, "entry-bottom-right");
strict_1.default.equal(savedGraph.edges[0]?.targetSide, "right");
strict_1.default.deepEqual(legacyRouteGraph.edges[0]?.rails, []);
strict_1.default.equal(legacyRouteGraph.edges[0]?.targetHandle, undefined);
strict_1.default.equal(savedGraph.nodes.find((node) => node.id === "armed")?.x, -120);
strict_1.default.equal(savedGraph.nodes.find((node) => node.id === "armed")?.y, 220);
const lifecycleGraph = (0, stateGraph_1.buildStateGraphModel)({
    ...scene,
    input_actions: [
        ...scene.input_actions,
        { action_id: "hold_b", logical_source: "BUTTON_B", event_kind: "hold" },
    ],
    routes: [
        ...scene.routes,
        {
            route_id: "hold_b",
            action_ref: "hold_b",
            from_states: ["idle"],
            guards: [],
            actions: [],
            target_state: "armed",
        },
    ],
});
const holdOutput = lifecycleGraph.nodes[0]?.outputs.find((output) => output.routeId === "hold_b");
strict_1.default.equal(holdOutput?.label, "Button B held");
strict_1.default.equal(holdOutput?.eventKind, "hold");
strict_1.default.equal(lifecycleGraph.nodes[0]?.platformOutputCount, 1);
strict_1.default.equal((0, stateGraph_1.resolveStateExitSide)({ x: 0, y: 0 }, { x: 340, y: 0 }), "right");
strict_1.default.equal((0, stateGraph_1.resolveStateExitSide)({ x: 0, y: 0 }, { x: -120, y: 220 }), "left");
strict_1.default.equal((0, stateGraph_1.resolveStateEntryHandle)({ x: 0, y: 0 }, { x: 340, y: 0 }), "entry-top-left");
strict_1.default.equal((0, stateGraph_1.resolveStateEntryHandle)({ x: 0, y: 400 }, { x: 0, y: 0 }), "entry-bottom-right");
strict_1.default.deepEqual((0, stateGraph_1.stateEntryPortPoint)({ x: 100, y: 200 }, "entry-top-left", "top"), { x: 108, y: 200 });
strict_1.default.deepEqual((0, stateGraph_1.stateEntryPortPoint)({ x: 100, y: 200 }, "entry-top-left", "left"), { x: 100, y: 209 });
const verticalStackRoute = (0, stateGraph_1.buildStateTransitionRoute)({
    sourceX: 280,
    sourceY: 160,
    targetX: 140,
    targetY: 300,
    sourceSide: "right",
    targetSide: "top",
});
strict_1.default.equal(verticalStackRoute.points.some((point) => point.x > 340), true);
strict_1.default.equal(verticalStackRoute.points.at(-2)?.y, 252);
strict_1.default.equal(verticalStackRoute.path.startsWith("M 280 160"), true);
const verticalReturnRoute = (0, stateGraph_1.buildStateTransitionRoute)({
    sourceX: 280,
    sourceY: 460,
    targetX: 140,
    targetY: 210,
    sourceSide: "right",
    targetSide: "bottom",
});
strict_1.default.equal(verticalReturnRoute.points.some((point) => point.x > 340), true);
strict_1.default.equal(verticalReturnRoute.points.at(-2)?.y, 258);
const topTriggerRoute = (0, stateGraph_1.buildStateTransitionRoute)({
    sourceX: 66,
    sourceY: 0,
    targetX: 360,
    targetY: 300,
    sourceSide: "top",
    targetSide: "top",
});
strict_1.default.equal(topTriggerRoute.points[1]?.x, 66);
strict_1.default.equal((topTriggerRoute.points[1]?.y ?? 0) < 0, true);
const sideEntryRoute = (0, stateGraph_1.buildStateTransitionRoute)({
    sourceX: 40,
    sourceY: 100,
    targetX: 300,
    targetY: 200,
    sourceSide: "bottom",
    targetSide: "left",
});
strict_1.default.deepEqual(sideEntryRoute.points.at(-1), { x: 300, y: 200 });
strict_1.default.equal(sideEntryRoute.points.at(-2)?.y, 200);
strict_1.default.equal((sideEntryRoute.points.at(-2)?.x ?? 300) < 300, true);
strict_1.default.deepEqual(sideEntryRoute.rails, [{ axis: "y", value: 272 }, { axis: "x", value: 252 }]);
const movedSideEntryRails = (0, stateGraph_1.moveStateTransitionRouteSection)(sideEntryRoute.controlPoints, sideEntryRoute.controlPoints.length - 2, { x: 280, y: 140 }, "bottom", "left");
strict_1.default.notEqual(movedSideEntryRails, null);
const movedSideEntryRoute = (0, stateGraph_1.buildStateTransitionRoute)({
    sourceX: 40,
    sourceY: 100,
    targetX: 300,
    targetY: 200,
    sourceSide: "bottom",
    targetSide: "left",
    rails: movedSideEntryRails ?? undefined,
});
strict_1.default.equal(movedSideEntryRoute.points.some((point) => point.y === 140), true);
strict_1.default.deepEqual(movedSideEntryRoute.points.at(-1), { x: 300, y: 200 });
const manualRoute = (0, stateGraph_1.buildStateTransitionRoute)({
    sourceX: 280,
    sourceY: 160,
    targetX: 140,
    targetY: 300,
    sourceSide: "right",
    targetSide: "top",
    rails: [{ axis: "x", value: 410 }, { axis: "y", value: 264 }],
});
strict_1.default.equal(manualRoute.points.some((point) => point.x === 410 && point.y === 160), true);
strict_1.default.equal(manualRoute.points.some((point) => point.x === 410 && point.y === 264), true);
strict_1.default.equal(manualRoute.path.startsWith("M 280 160"), true);
strict_1.default.deepEqual(verticalStackRoute.rails, [{ axis: "x", value: 352 }, { axis: "y", value: 252 }]);
strict_1.default.equal(manualRoute.controlPoints.length, 5);
const automaticSections = (0, stateGraph_1.stateTransitionRouteSections)(verticalStackRoute.controlPoints);
strict_1.default.equal(automaticSections.length, 4);
strict_1.default.deepEqual(automaticSections.map((section) => section.orientation), ["horizontal", "vertical", "horizontal", "vertical"]);
const movedSourceRails = (0, stateGraph_1.moveStateTransitionRouteSection)(verticalStackRoute.controlPoints, 0, { x: 330, y: 110 }, "right", "top");
strict_1.default.notEqual(movedSourceRails, null);
const movedSourceRoute = (0, stateGraph_1.buildStateTransitionRoute)({
    sourceX: 280,
    sourceY: 160,
    targetX: 140,
    targetY: 300,
    sourceSide: "right",
    targetSide: "top",
    rails: movedSourceRails ?? undefined,
});
strict_1.default.equal(movedSourceRoute.points.some((point) => point.y === 110), true);
strict_1.default.equal(movedSourceRoute.points.slice(1).every((point, index) => {
    const previous = movedSourceRoute.points[index];
    return previous.x === point.x || previous.y === point.y;
}), true);
const movedTargetRails = (0, stateGraph_1.moveStateTransitionRouteSection)(verticalStackRoute.controlPoints, verticalStackRoute.controlPoints.length - 2, { x: 240, y: 330 }, "right", "top");
strict_1.default.notEqual(movedTargetRails, null);
const movedTargetSectionRoute = (0, stateGraph_1.buildStateTransitionRoute)({
    sourceX: 280,
    sourceY: 160,
    targetX: 140,
    targetY: 300,
    sourceSide: "right",
    targetSide: "top",
    rails: movedTargetRails ?? undefined,
});
strict_1.default.equal(movedTargetSectionRoute.points.some((point) => point.x === 240), true);
strict_1.default.deepEqual(movedTargetSectionRoute.points.at(-1), { x: 140, y: 300 });
const movedMiddleRails = (0, stateGraph_1.moveStateTransitionRouteSection)(verticalStackRoute.controlPoints, 1, { x: 460, y: 220 }, "right", "top");
strict_1.default.deepEqual(movedMiddleRails, [{ axis: "x", value: 460 }, { axis: "y", value: 252 }]);
const insertedSection = (0, stateGraph_1.insertStateTransitionRouteSection)(verticalStackRoute.controlPoints, 1, { x: 352, y: 212 }, "top");
strict_1.default.notEqual(insertedSection, null);
const insertedRoute = (0, stateGraph_1.buildStateTransitionRoute)({
    sourceX: 280,
    sourceY: 160,
    targetX: 140,
    targetY: 300,
    sourceSide: "right",
    targetSide: "top",
    rails: insertedSection ?? undefined,
});
strict_1.default.equal((0, stateGraph_1.stateTransitionRouteSections)(insertedRoute.controlPoints).length > automaticSections.length, true);
strict_1.default.equal((0, stateGraph_1.insertStateTransitionRouteSection)(verticalStackRoute.controlPoints, 1, { x: 352, y: 212 }, "top", 2), null);
const movedCardRoute = (0, stateGraph_1.buildStateTransitionRoute)({
    sourceX: 520,
    sourceY: 90,
    targetX: 180,
    targetY: 360,
    sourceSide: "right",
    targetSide: "top",
    rails: manualRoute.rails,
});
strict_1.default.equal((movedCardRoute.points[1]?.x ?? 0) > 520, true);
strict_1.default.equal(movedCardRoute.points.slice(1).every((point, index) => {
    const previous = movedCardRoute.points[index];
    return previous.x === point.x || previous.y === point.y;
}), true);
const movedTargetRoute = (0, stateGraph_1.buildStateTransitionRoute)({
    sourceX: 280,
    sourceY: 160,
    targetX: 140,
    targetY: 100,
    sourceSide: "right",
    targetSide: "top",
    rails: manualRoute.rails,
});
strict_1.default.equal((movedTargetRoute.points.at(-2)?.y ?? 100) <= 76, true);
const selfCrossingRoute = (0, stateGraph_1.buildStateTransitionRoute)({
    sourceX: 0,
    sourceY: 0,
    targetX: 100,
    targetY: 100,
    sourceSide: "right",
    targetSide: "top",
    rails: [
        { axis: "x", value: 200 },
        { axis: "y", value: 200 },
        { axis: "x", value: -100 },
        { axis: "y", value: 0 },
    ],
});
strict_1.default.deepEqual(selfCrossingRoute.points, [{ x: 0, y: 0 }, { x: 100, y: 0 }, { x: 100, y: 100 }]);
strict_1.default.equal((0, stateGraph_1.stateGuardDescription)(scene.routes[0].guards[0]), "Coins is greater than 0");
strict_1.default.equal((0, stateGraph_1.stateActionDescription)(scene.routes[0].actions[0]), "Coins -1");
strict_1.default.equal((0, stateGraph_1.stateActionDescription)(scene.routes[0].actions[1]), null);
const lanePlan = (0, stateGraph_1.planStateTransitionRoutes)([
    { id: "first", source: "top", target: "bottom", sourceOutputIndex: 0 },
    { id: "second", source: "top", target: "bottom", sourceOutputIndex: 1 },
], [
    { id: "top", x: 0, y: 0 },
    { id: "bottom", x: 0, y: 340 },
]);
strict_1.default.notEqual(lanePlan.first?.laneX, lanePlan.second?.laneX);
strict_1.default.equal(lanePlan.first?.sourceSide, "right");
strict_1.default.equal(lanePlan.second?.targetHandle.startsWith("entry-top"), true);
const fixedTriggerPlan = (0, stateGraph_1.planStateTransitionRoutes)([{ id: "shoulder", source: "top", target: "bottom", sourceOutputIndex: 0, sourceSide: "top", sourceRatio: 0.22 }], [
    { id: "top", x: 0, y: 0, platformOutputCount: 0 },
    { id: "bottom", x: 0, y: 420, platformOutputCount: 0 },
]);
strict_1.default.equal(fixedTriggerPlan.shoulder?.sourceSide, "top");
strict_1.default.equal(fixedTriggerPlan.shoulder?.targetHandle.startsWith("entry-top"), true);
const pinnedEntryPlan = (0, stateGraph_1.planStateTransitionRoutes)([{
        id: "pinned",
        source: "top",
        target: "bottom",
        sourceOutputIndex: 0,
        targetHandle: "entry-bottom-right",
        targetSide: "right",
    }], [
    { id: "top", x: 0, y: 0 },
    { id: "bottom", x: 0, y: 420 },
]);
strict_1.default.equal(pinnedEntryPlan.pinned?.targetHandle, "entry-bottom-right");
strict_1.default.equal(pinnedEntryPlan.pinned?.targetSide, "right");
const invalidRouteGraph = (0, stateGraph_1.buildStateGraphModel)({
    ...scene,
    routes: [{ ...scene.routes[0], target_state: "missing" }],
});
strict_1.default.equal(invalidRouteGraph.edges.length, 0);
const menuScene = {
    ...scene,
    scene_exits: [
        {
            scene_exit_id: "to_game",
            display_name: "Game",
            target_scene: "game",
        },
    ],
    routes: [
        {
            route_id: "menu_to_game",
            action_ref: "select",
            from_states: ["idle"],
            guards: [],
            actions: [],
            scene_exit_ref: "to_game",
            target_scene: "game",
        },
        {
            route_id: "menu_local",
            action_ref: "select",
            from_states: ["idle"],
            guards: [],
            actions: [],
            target_state: "armed",
        },
    ],
};
const sceneFlow = (0, stateGraph_1.buildSceneFlowGraphModel)([
    menuScene,
    {
        ...scene,
        scene_id: "game",
        display_name: "Game",
        routes: [],
    },
    {
        ...scene,
        scene_id: "summary",
        display_name: "Summary",
        routes: [
            {
                route_id: "summary_to_menu",
                action_ref: "select",
                from_states: ["idle"],
                guards: [],
                actions: [],
                target_scene: "menu",
            },
        ],
    },
], "menu");
const localWithSceneExit = (0, stateGraph_1.buildStateGraphModel)(menuScene);
strict_1.default.equal(localWithSceneExit.nodes.find((node) => node.id === "idle")?.outputs.length, 2);
strict_1.default.equal(localWithSceneExit.nodes.find((node) => node.id === "idle")?.outputs.some((output) => output.targetScene === "game"), true);
strict_1.default.equal(localWithSceneExit.endpoints.some((endpoint) => endpoint.sceneExitId === "to_game"), true);
strict_1.default.equal(localWithSceneExit.edges.length, 2);
strict_1.default.equal(localWithSceneExit.edges.some((edge) => edge.target === "scene-exit-to_game"), true);
strict_1.default.equal(sceneFlow.nodes.length, 3);
strict_1.default.equal(sceneFlow.nodes.find((node) => node.id === "menu")?.isEntry, true);
strict_1.default.equal(sceneFlow.nodes.find((node) => node.id === "menu")?.exits.length, 1);
strict_1.default.equal(sceneFlow.nodes.find((node) => node.id === "menu")?.exits[0]?.label, "Game");
strict_1.default.equal(sceneFlow.nodes.find((node) => node.id === "menu")?.exits[0]?.targetScene, "game");
strict_1.default.equal(sceneFlow.edges.length, 2);
strict_1.default.equal(sceneFlow.edges.some((edge) => edge.source === "menu" && edge.target === "game"), true);
strict_1.default.equal(sceneFlow.edges.some((edge) => edge.source === "summary" && edge.target === "menu"), true);
