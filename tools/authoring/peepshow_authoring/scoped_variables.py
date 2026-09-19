"""Opt-in host variable model; its flattened projection is never executable export."""
from copy import deepcopy

MODEL = "scoped_v1"
LIMIT = 32
CAPABILITY = {
    "model": MODEL, "host_editing": True, "host_preview": True,
    "runtime": False, "export": False, "entry_graphs": True,
    "scopes": ["scene", "package"], "value_types": ["int32", "bool"],
    "operations": ["assign", "add", "reset"], "arithmetic": "clamp",
    "maximum_combined_variables_per_scene": LIMIT,
    "commands": ["project.variables.enable", "package_variable.add",
                 "package_variable.update", "package_variable.delete"],
}


def enabled(project):
    return project.get("variable_model") == MODEL


def declarations(records, path):
    from .project import ProjectCommandError, STABLE_ID
    if not isinstance(records, list) or len(records) > LIMIT:
        raise ProjectCommandError("VARIABLE_BUDGET", f"{path}: expected at most {LIMIT} variables")
    seen = set()
    for record in records:
        if not isinstance(record, dict):
            raise ProjectCommandError("VARIABLE_TYPE_INVALID", f"{path}: declaration must be an object")
        ident = record.get("variable_id")
        if not isinstance(ident, str) or not STABLE_ID.fullmatch(ident) or ident in seen:
            raise ProjectCommandError("VARIABLE_ID_INVALID", f"{path}: invalid or duplicate ID")
        seen.add(ident)
        boolean = record.get("value_type") == "bool"
        fields = {"variable_id", "value_type", "initial"} | (set() if boolean else {"minimum", "maximum"})
        if set(record) != fields or record.get("value_type") not in ("bool", "int32"):
            raise ProjectCommandError("VARIABLE_TYPE_INVALID", f"{path}.{ident}: invalid declaration")
        if boolean:
            valid = type(record["initial"]) is bool
        else:
            values = [record[key] for key in ("minimum", "initial", "maximum")]
            valid = all(type(value) is int for value in values) and -2147483648 <= values[0] <= values[1] <= values[2] <= 2147483647
        if not valid:
            raise ProjectCommandError("VARIABLE_RANGE_INVALID", f"{path}.{ident}: invalid type, range or initial value")


def project_check(project):
    from .project import ProjectCommandError
    if "variable_model" in project and not enabled(project):
        raise ProjectCommandError("VARIABLE_MODEL_UNSUPPORTED", "unknown variable model")
    if "package_variables" in project and not enabled(project):
        raise ProjectCommandError("VARIABLE_MODEL_REQUIRED", "package variables require scoped_v1")
    if enabled(project):
        declarations(project.get("package_variables", []), "package_variables")


def project_scene(project, scene):
    """Return validated legacy-compatible host IR and alias-to-source metadata."""
    from .project import ProjectCommandError
    if not enabled(project):
        return scene, {}
    if scene.get("schema_version") != 2:
        raise ProjectCommandError("VARIABLE_MODEL_UNSUPPORTED", "scoped_v1 requires V2 scenes")
    result = deepcopy(scene)
    definitions = {}
    aliases = {}
    result["variables"] = []
    for scope, records in (("scene", scene.get("variables")), ("package", project.get("package_variables", []))):
        declarations(records, scope)
        for record in records:
            key = (scope, record["variable_id"])
            alias = f"scoped.{len(aliases)}"
            definitions[key] = (alias, record)
            aliases[alias] = {"scope": scope, **record}
            result["variables"].append({
                "variable_id": alias, "value_type": "int32", "initial": int(record["initial"]),
                "minimum": record.get("minimum", 0), "maximum": record.get("maximum", 1),
            })
    if len(aliases) > LIMIT:
        raise ProjectCommandError("VARIABLE_BUDGET", f"scene and package variables together exceed {LIMIT}")
    for collection in ("routes", "event_handlers"):
        if not isinstance(result.get(collection, []), list):
            raise ProjectCommandError("PROJECT_TYPE_INVALID", f"{collection} must be an array")
    for route in [*result.get("routes", []), *result.get("event_handlers", [])]:
        if not isinstance(route, dict):
            continue
        for collection in ("guards", "actions"):
            if not isinstance(route.get(collection, []), list):
                raise ProjectCommandError("PROJECT_TYPE_INVALID", f"{collection} must be an array")
            for item in route.get(collection, []):
                if not isinstance(item, dict) or (collection == "actions" and item.get("kind") != "set_variable"):
                    continue
                if item.get("operation") == "guard" and collection == "actions":
                    raise ProjectCommandError("VARIABLE_OPERATION_INVALID", "guard is not a variable action")
                scope = item.get("variable_scope", "scene")
                ref = item.get("variable_ref")
                if not isinstance(scope, str) or not isinstance(ref, str) or (scope, ref) not in definitions:
                    raise ProjectCommandError("VARIABLE_REFERENCE_INVALID", "unknown scope-qualified variable")
                alias, record = definitions[(scope, ref)]
                boolean = record["value_type"] == "bool"
                operation = item.get("operation") if collection == "actions" else "guard"
                if operation not in ("assign", "add", "reset", "guard") or (boolean and operation == "add"):
                    raise ProjectCommandError("VARIABLE_OPERATION_INVALID", "unsupported variable operation")
                if operation == "reset":
                    if "value" in item:
                        raise ProjectCommandError("VARIABLE_OPERATION_INVALID", "reset has no value operand")
                    item["operation"] = "assign"
                    item["value"] = record["initial"]
                value = item.get("value")
                if type(value) is not (bool if boolean else int):
                    raise ProjectCommandError("VARIABLE_VALUE_INVALID", "operand must match declared variable type")
                if boolean and operation == "guard" and item.get("operator") not in ("eq", "ne"):
                    raise ProjectCommandError("VARIABLE_OPERATION_INVALID", "boolean comparison supports eq/ne only")
                if not boolean and not -2147483648 <= value <= 2147483647:
                    raise ProjectCommandError("VARIABLE_VALUE_INVALID", "operand must fit int32")
                if operation == "assign" and not boolean and not record["minimum"] <= value <= record["maximum"]:
                    raise ProjectCommandError("VARIABLE_VALUE_INVALID", "assignment is outside declared bounds")
                item.pop("variable_scope", None)
                item["variable_ref"] = alias
                item["value"] = int(value)
    return result, aliases


def apply_command(project, scenes, command):
    from .project import ProjectCommandError, _require_command_fields
    kind = command["kind"]
    if kind == "project.variables.enable":
        _require_command_fields(command, {"kind"}, {"kind", "command_id"})
        project["variable_model"] = MODEL
        project.setdefault("package_variables", [])
        return {"kind": kind, "variable_model": MODEL}
    if not enabled(project):
        raise ProjectCommandError("VARIABLE_MODEL_REQUIRED", "enable scoped_v1 first")
    delete = kind == "package_variable.delete"
    field = "variable_id" if delete else "variable"
    _require_command_fields(command, {"kind", field}, {"kind", field, "command_id"})
    value = command[field]
    if not delete:
        declarations([value], "command.variable")
    ident = value if delete else value["variable_id"]
    records = project.setdefault("package_variables", [])
    existing = next((r for r in records if r["variable_id"] == ident), None)
    if kind == "package_variable.add":
        if existing is not None:
            raise ProjectCommandError("PROJECT_ID_DUPLICATE", "package variable already exists")
        records.append(deepcopy(value))
    else:
        if existing is None:
            raise ProjectCommandError("COMMAND_TARGET_UNKNOWN", "unknown package variable")
        if delete:
            from .scene_entry import references
            for scene in scenes:
                for route in [*scene.get("routes", []), *scene.get("event_handlers", []), *references(scene)]:
                    if any(item.get("variable_scope", "scene") == "package" and item.get("variable_ref") == ident
                           for item in [*route.get("guards", []), *route.get("actions", [])]):
                        raise ProjectCommandError("COMMAND_TARGET_IN_USE", "package variable is referenced")
            records.remove(existing)
        else:
            records[records.index(existing)] = deepcopy(value)
    declarations(records, "package_variables")
    return {"kind": kind, field: deepcopy(value)}
