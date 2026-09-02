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
            },
        },
    },
});
strict_1.default.equal(graph.nodes.length, 2);
strict_1.default.equal(graph.nodes[0]?.id, "idle");
strict_1.default.equal(graph.nodes[0]?.isEntry, true);
strict_1.default.equal(graph.nodes[0]?.outputs.length, 1);
strict_1.default.equal(graph.nodes[0]?.outputs[0]?.label, "Button A");
strict_1.default.equal(graph.nodes[0]?.outputs[0]?.guardCount, 1);
strict_1.default.equal(graph.nodes[0]?.outputs[0]?.actionCount, 1);
strict_1.default.equal(graph.nodes[0]?.outputs[0]?.effectLabels[0], "Coins -1");
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
strict_1.default.equal(savedGraph.nodes.find((node) => node.id === "armed")?.x, -120);
strict_1.default.equal(savedGraph.nodes.find((node) => node.id === "armed")?.y, 220);
strict_1.default.equal((0, stateGraph_1.resolveStateExitSide)({ x: 0, y: 0 }, { x: 340, y: 0 }), "right");
strict_1.default.equal((0, stateGraph_1.resolveStateExitSide)({ x: 0, y: 0 }, { x: -120, y: 220 }), "left");
strict_1.default.equal((0, stateGraph_1.resolveStateEntryHandle)({ x: 0, y: 0 }, { x: 340, y: 0 }), "entry-top-left");
strict_1.default.equal((0, stateGraph_1.resolveStateEntryHandle)({ x: 0, y: 400 }, { x: 0, y: 0 }), "entry-bottom-right");
const verticalStackRoute = (0, stateGraph_1.buildStateTransitionRoute)({
    sourceX: 280,
    sourceY: 160,
    targetX: 140,
    targetY: 300,
    sourceSide: "right",
    targetSide: "top",
});
strict_1.default.equal(verticalStackRoute.points.some((point) => point.x > 340), true);
strict_1.default.equal(verticalStackRoute.points.at(-2)?.y, 264);
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
strict_1.default.equal(verticalReturnRoute.points.at(-2)?.y, 246);
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
const invalidRouteGraph = (0, stateGraph_1.buildStateGraphModel)({
    ...scene,
    routes: [{ ...scene.routes[0], target_state: "missing" }],
});
strict_1.default.equal(invalidRouteGraph.edges.length, 0);
const menuScene = {
    ...scene,
    routes: [
        {
            route_id: "menu_to_game",
            action_ref: "select",
            from_states: ["idle"],
            guards: [],
            actions: [],
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
strict_1.default.equal(localWithSceneExit.edges.length, 1);
strict_1.default.equal(sceneFlow.nodes.length, 3);
strict_1.default.equal(sceneFlow.nodes.find((node) => node.id === "menu")?.isEntry, true);
strict_1.default.equal(sceneFlow.nodes.find((node) => node.id === "menu")?.exits.length, 1);
strict_1.default.equal(sceneFlow.nodes.find((node) => node.id === "menu")?.exits[0]?.label, "Button A");
strict_1.default.equal(sceneFlow.nodes.find((node) => node.id === "menu")?.exits[0]?.targetScene, "game");
strict_1.default.equal(sceneFlow.edges.length, 2);
strict_1.default.equal(sceneFlow.edges.some((edge) => edge.source === "menu" && edge.target === "game"), true);
strict_1.default.equal(sceneFlow.edges.some((edge) => edge.source === "summary" && edge.target === "menu"), true);
