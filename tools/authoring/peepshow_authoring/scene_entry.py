"""Bounded, explicit fresh-entry graphs for host authoring and preview."""
from copy import deepcopy

NODE_LIMIT = 32
LIST_LIMIT = 8
CAPABILITY = {
    "host_editing": True, "host_preview": True, "runtime": False, "export": False,
    "requires_variable_model": "scoped_v1", "node_kinds": ["actions", "decision", "state"],
    "action_kinds": ["set_variable", "play_sfx"], "branch_policy": "first_match_then_default",
    "maximum_nodes": NODE_LIMIT, "maximum_actions_per_node": LIST_LIMIT,
    "maximum_branches_per_decision": LIST_LIMIT, "maximum_guards_per_branch": LIST_LIMIT,
    "maximum_actions_per_entry": NODE_LIMIT * LIST_LIMIT,
    "commands": ["scene.entry_graph.set", "scene.entry_graph.clear"],
    "cycles": False, "waits": False, "state_entry_actions": False,
}


def references(scene):
    """Action/guard records for shared reference protection."""
    graph = scene.get("entry_graph")
    nodes = graph.get("nodes") if isinstance(graph, dict) else None
    for node in nodes if isinstance(nodes, list) else []:
        if not isinstance(node, dict):
            continue
        if node.get("kind") == "actions" and isinstance(node.get("actions"), list):
            yield {"actions": [a for a in node["actions"] if isinstance(a, dict)], "guards": []}
        elif node.get("kind") == "decision" and isinstance(node.get("branches"), list):
            for branch in node["branches"]:
                if isinstance(branch, dict) and isinstance(branch.get("guards"), list):
                    yield {"actions": [], "guards": [g for g in branch["guards"] if isinstance(g, dict)]}


def validate(project, scene, cue_ids):
    from .project import ProjectCommandError, STABLE_ID
    from .scoped_variables import enabled, project_scene
    if "entry_graph" not in scene:
        return None

    def fail(message):
        raise ProjectCommandError("SCENE_ENTRY_INVALID", message)

    def fields(value, required, optional=()):
        if not isinstance(value, dict) or not required <= value.keys() or value.keys() - required - set(optional):
            fail("entry record has missing or unknown fields")

    def identifier(value):
        if not isinstance(value, str) or not STABLE_ID.fullmatch(value):
            fail("entry references require stable IDs")

    def bounded_list(value, minimum=0):
        if not isinstance(value, list) or not minimum <= len(value) <= LIST_LIMIT:
            fail(f"entry list must have {minimum}..{LIST_LIMIT} items")

    if not enabled(project) or scene.get("schema_version") != 2:
        fail("entry graphs require scoped_v1 and V2 scenes")
    graph = scene["entry_graph"]
    fields(graph, {"root", "nodes"}, {"layout"})
    identifier(graph["root"])
    if not isinstance(graph["nodes"], list) or not 1 <= len(graph["nodes"]) <= NODE_LIMIT:
        fail(f"entry graph must have 1..{NODE_LIMIT} nodes")
    nodes, edges = {}, {}
    states = scene.get("states")
    state_ids = {state.get("state_id") for state in states if isinstance(state, dict) and isinstance(state.get("state_id"), str)} if isinstance(states, list) else set()
    for node in graph["nodes"]:
        if not isinstance(node, dict):
            fail("entry node must be an object")
        identifier(node.get("node_id"))
        ident = node["node_id"]
        if ident in nodes:
            fail("duplicate entry node ID")
        nodes[ident] = node
        kind = node.get("kind")
        if kind == "state":
            fields(node, {"node_id", "kind", "state_ref"})
            identifier(node["state_ref"])
            if node["state_ref"] not in state_ids:
                fail("entry destination state does not exist")
            edges[ident] = []
        elif kind == "actions":
            fields(node, {"node_id", "kind", "actions", "next"})
            bounded_list(node["actions"])
            for action in node["actions"]:
                if not isinstance(action, dict):
                    fail("entry action must be an object")
                if action.get("kind") == "play_sfx":
                    fields(action, {"kind", "cue_ref"})
                    identifier(action["cue_ref"])
                    if action["cue_ref"] not in cue_ids:
                        fail("entry SFX cue does not exist")
                elif action.get("kind") == "set_variable":
                    required = {"kind", "variable_ref", "operation"}
                    if action.get("operation") != "reset":
                        required.add("value")
                    fields(action, required, {"variable_scope"})
                else:
                    fail("entry actions support only set_variable and play_sfx")
            edges[ident] = [node["next"]]
        elif kind == "decision":
            fields(node, {"node_id", "kind", "branches", "default"})
            bounded_list(node["branches"], 1)
            for branch in node["branches"]:
                fields(branch, {"guards", "next"})
                bounded_list(branch["guards"], 1)
                for guard in branch["guards"]:
                    fields(guard, {"variable_ref", "operator", "value"}, {"variable_scope"})
                    if guard["operator"] not in ("eq", "ne", "lt", "le", "gt", "ge"):
                        fail("unsupported entry comparison")
            edges[ident] = [b["next"] for b in node["branches"]] + [node["default"]]
        else:
            fail("unknown entry node kind")
        for target in edges[ident]:
            identifier(target)
    if graph["root"] not in nodes or any(target not in nodes for targets in edges.values() for target in targets):
        fail("entry root or edge targets a missing node")
    reached, pending = set(), [graph["root"]]
    while pending:
        ident = pending.pop()
        if ident not in reached:
            reached.add(ident)
            pending.extend(edges[ident])
    if len(reached) != len(nodes):
        fail("all entry nodes must be reachable from the root")
    indegree = {ident: 0 for ident in nodes}
    for targets in edges.values():
        for target in targets:
            indegree[target] += 1
    pending = [ident for ident, degree in indegree.items() if degree == 0]
    visited = 0
    while pending:
        ident = pending.pop()
        visited += 1
        for target in edges[ident]:
            indegree[target] -= 1
            if indegree[target] == 0:
                pending.append(target)
    if visited != len(nodes):
        fail("entry graphs cannot contain cycles")
    layout = graph.get("layout", {})
    if not isinstance(layout, dict) or layout.keys() - nodes.keys():
        fail("layout must address existing entry node IDs")
    for position in layout.values():
        fields(position, {"x", "y"})
        if any(type(v) is not int or not -100000 <= v <= 100000 for v in position.values()):
            fail("entry layout coordinates must be bounded integers")
    # Reuse the scoped-variable type/range/reference validator, not a second
    # authoring interpretation. This projection is never encoded for firmware.
    scratch = {**scene, "routes": list(references(scene)), "event_handlers": []}
    project_scene(project, scratch)
    return deepcopy(graph)


def apply_command(scenes, command):
    from .project import _require_command_fields, _command_scene
    kind = command["kind"]
    required = {"kind", "scene_id"} | ({"graph"} if kind.endswith(".set") else set())
    _require_command_fields(command, required, required | {"command_id"})
    scene = _command_scene(scenes, command["scene_id"])
    if kind.endswith(".set"):
        scene["entry_graph"] = deepcopy(command["graph"])
    else:
        scene.pop("entry_graph", None)
    return deepcopy(command)
