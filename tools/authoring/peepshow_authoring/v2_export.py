"""Conservative export subset of the hardware-tested HW6 resident V2 profile.

This checks decoded records, so compiler and public parser share the boundary.
Band bounds include hidden clips and every possible horizontal position. They
deliberately do not rely on pixel coincidences, occlusion or payload deduplication.
Firmware still performs exact owner-side admission before install/transactions.
"""
from copy import deepcopy
from math import gcd, lcm

from .target_profile import TARGET_PROFILE, TARGET_STATE_SCENE_EVENTS


PROFILE_ID = "hw6_v2_resident_v1"
LIMITS = {
    "package_bytes": TARGET_PROFILE["package"]["resident_prefix_bytes"],
    "scenes": 1, "objects": 12, "states": 8, "variables": 8,
    "bindings": TARGET_STATE_SCENE_EVENTS["binding_count_max"],
    "transitions": 16, "guards": 16, "actions": 32, "strings": 256,
    "animated_objects": 8, "clip_steps": 12, "clip_distinct_frames": 4,
    "combined_steps": 12, "chunks": 18, "payload_bytes": 10512,
    "band_rows": 28, "band_slot_bytes": 584, "canvas_width": 168,
    "tick_ms": 10, "frame_duration_max_ms": 2000,
}


def public_v2_export_profile():
    return {
        "profile_id": PROFILE_ID, "container_version": 2,
        "status": "development_restricted", "execution_model": "scene_objects",
        "limits": deepcopy(LIMITS), "interaction_modes": ["continuous"],
        "event_classes": ["input", "timer"], "audio": False,
        "scene_connections": False, "system_exit_actions": False,
        "mixed_execution_models": False, "shipping_build": False,
        "budget_method": "conservative_all_clips_and_positions",
        "timing_method": "derived_gcd_no_rounding",
        "device_exact_admission_required": True,
    }


def issue(code, path, message, scene_id=None):
    result = {"code": code, "path": path, "message": message}
    if scene_id is not None:
        result["scene_id"] = scene_id
    return result


def source_issues(bundle):
    issues = []
    if len(bundle.scenes) != 1 or bundle.scenes[0].get("schema_version") != 2:
        issues.append(issue("V2_SCENE_PROFILE", "scenes",
                            "V2 export requires exactly one version-2 scene; split extra or legacy scenes into another project."))
    if bundle.project.get("validation", {}).get("build_profile") == "shipping":
        issues.append(issue("V2_SHIPPING_UNAVAILABLE", "validation.build_profile",
                            "Restricted V2 export is development-only; select the development build profile."))
    if bundle.audio_assets or bundle.audio_cues:
        issues.append(issue("V2_AUDIO_UNSUPPORTED", "assets",
                            "Remove audio assets and cues for restricted V2 export; package audio is not supported in this profile."))
    for scene in bundle.scenes:
        if scene.get("schema_version") != 2:
            continue
        sid = scene["scene_id"]
        path = f"scenes[{sid}]"
        if not scene["objects"]:
            issues.append(issue("V2_OBJECTS_EMPTY", path + ".objects",
                                "Place at least one scene object before building.", sid))
        if scene["interaction_policy"]["mode"] != "continuous":
            issues.append(issue("V2_INTERACTION_UNSUPPORTED", path + ".interaction_policy.mode",
                                "Select continuous interaction for restricted V2 export.", sid))
        if scene.get("scene_exits"):
            issues.append(issue("V2_SCENE_EXIT_UNSUPPORTED", path + ".scene_exits",
                                "Remove scene exits for restricted V2 export.", sid))
    return issues


def package_admission(package, size):
    """Validate a structurally parsed V2 package against the export subset."""
    issues = []

    def limit(name, used, path, sid=None):
        if used > LIMITS[name]:
            issues.append(issue("V2_CAPACITY", path,
                f"V2 {name} use {used}, limit {LIMITS[name]}; reduce this content before export.", sid))

    limit("package_bytes", size, "package")
    limit("strings", len(package.strings), "package.strings")
    if len(package.scenes) != 1 or package.scenes[0].get("execution_model") != 2:
        issues.append(issue("V2_SCENE_PROFILE", "scenes",
                            "V2 export requires exactly one scene using scene-owned objects."))
        return {"issues": issues, "animation_budget": None}
    if package.audio_assets or package.audio_cues or any(c.chunk_type in {10, 11, 12} for c in package.chunks):
        issues.append(issue("V2_AUDIO_UNSUPPORTED", "assets", "V2 export does not support audio chunks."))
    scene = package.scenes[0]
    sid = scene["scene_id"]
    path = f"scenes[{sid}]"
    graph = scene["graph"]
    if graph["interaction_mode"] != 1:
        issues.append(issue("V2_INTERACTION_UNSUPPORTED", path + ".interaction_policy",
                            "V2 export requires continuous interaction.", sid))
    for name, used in (("objects", len(scene["objects"])), ("states", graph["state_count"]),
                       ("variables", graph["variable_count"]), ("bindings", graph["binding_count"]),
                       ("transitions", sum(max(1, len(r["source_state_indexes"])) for r in graph["routes"])),
                       ("guards", sum(len(r["guards"]) for r in graph["routes"])),
                       ("actions", sum(len(r["operations"]) for r in graph["routes"]))):
        limit(name, used, path + "." + name, sid)
    if any(binding["event_class"] not in {1, 2} for binding in graph["bindings"]):
        issues.append(issue("V2_EVENT_UNSUPPORTED", path + ".event_bindings",
                            "Only input and scoped timer bindings can be exported.", sid))
    for route in graph["routes"]:
        route_path = path + f".routes[{route['route_id']}]"
        if route["target_scene"] is not None:
            issues.append(issue("V2_SCENE_EXIT_UNSUPPORTED", route_path,
                                "Scene connections cannot be exported in the restricted V2 profile.", sid))
        for index, op in enumerate(route["operations"]):
            if op["kind"] not in {1, 9, 10, 11, 12}:
                issues.append(issue("V2_ACTION_UNSUPPORTED", route_path + f".actions[{index}]",
                    "Use object, variable or timer actions only; audio, render requests and shell exits are not supported in this export profile.", sid))

    animated = [(i, obj, package.animations[obj["clip_index"]])
                for i, obj in enumerate(scene["objects"]) if obj["clip_index"] != 0xffff]
    limit("animated_objects", len(animated), path + ".objects", sid)
    quantum, cycle, bands = 0, 1, set()
    for index, obj, clip in animated:
        obj_path = path + f".objects[{obj['object_id']}].animation_ref"
        durations = clip["frame_duration_ms"]
        limit("clip_steps", len(durations), obj_path, sid)
        limit("clip_distinct_frames", len(set(clip["frame_indexes"])), obj_path, sid)
        if any(d % LIMITS["tick_ms"] or d > LIMITS["frame_duration_max_ms"] for d in durations):
            issues.append(issue("V2_ANIMATION_TIMING", obj_path,
                "Use frame durations in whole 10 ms ticks, up to 2000 ms per frame; no timing is rounded for export.", sid))
        for duration in durations:
            quantum = gcd(quantum, duration)
        # Values are bounded by the wire parser (32 objects, finite clip lengths).
        cycle = lcm(cycle, sum(durations))
        positions = {obj["x"]}
        for overrides in scene["state_overrides"]:
            positions.update(o["x"] for o in overrides if o["object_index"] == index and o["mask"] & 1)
        for op in scene["object_operations"]:
            if op["object_index"] != index or not op["axis_mask"] & 1:
                continue
            if op["opcode"] == 2 and op["x"]:
                positions = range(LIMITS["canvas_width"] - obj["width"] + 1)
                break
            if op["opcode"] == 1:
                positions.add(max(0, min(op["x"], LIMITS["canvas_width"] - obj["width"])))
        # Logical X maps to panel rows (rotated display). Include all positions,
        # even mutually exclusive overrides; masked/hidden clips may reappear.
        for x in positions:
            bands.update(range(x // LIMITS["band_rows"], (x + obj["width"] - 1) // LIMITS["band_rows"] + 1))
    steps = cycle // quantum if quantum else 1
    limit("combined_steps", steps, path + ".objects.animation_ref", sid)
    chunks = steps * max(1, len(bands))
    if chunks > LIMITS["chunks"] or chunks * LIMITS["band_slot_bytes"] > LIMITS["payload_bytes"]:
        issues.append(issue("V2_LPBAM_BUDGET", path + ".objects",
            f"Conservative animation bound is {steps} steps, {chunks} chunks and {chunks * LIMITS['band_slot_bytes']} bytes; limits are 12 steps, 18 chunks and 10512 bytes. Align clip timings, reduce animated widths or restrict horizontal movement. Hidden clips count; firmware still checks exact payloads.", sid))
    return {"issues": issues, "animation_budget": {
        "method": "conservative_all_clips_and_positions", "quantum_ms": quantum,
        "cycle_ms": cycle if quantum else 0, "combined_steps": steps,
        "panel_bands": len(bands), "chunks_upper_bound": chunks,
        "payload_bytes_upper_bound": chunks * LIMITS["band_slot_bytes"],
        "exact_device_admission_required": True,
    }}


def package_issues(package, size):
    return package_admission(package, size)["issues"]
