"""Conservative export subset of the hardware-tested HW6 resident V2 profile.

This checks decoded records, so compiler and public parser share the boundary.
Band bounds include hidden clips and every possible horizontal position. They
deliberately do not rely on pixel coincidences, occlusion or payload deduplication.
Firmware still performs exact owner-side admission before install/transactions.
"""
from copy import deepcopy
from math import gcd, lcm

from .target_profile import TARGET_PROFILE, TARGET_STATE_SCENE_EVENTS, TARGET_SAMPLED_SFX


PROFILE_ID = "hw6_v2_resident_v1"
LIMITS = {
    "package_bytes": TARGET_PROFILE["package"]["resident_prefix_bytes"],
    "scenes": 8, "objects": 12, "states": 8, "variables": 8,
    "bindings": TARGET_STATE_SCENE_EVENTS["binding_count_max"],
    "transitions": 16, "guards": 16, "actions": 32, "strings": 256,
    "animated_objects": 8, "clip_steps": 12, "clip_distinct_frames": 4,
    "combined_steps": 12, "chunks": 18, "payload_bytes": 10512,
    "band_rows": 28, "band_slot_bytes": 584, "canvas_width": 168,
    "tick_ms": 10, "frame_duration_max_ms": 2000,
    "audio_assets": TARGET_SAMPLED_SFX["maximum_assets"],
    "audio_cues": TARGET_SAMPLED_SFX["maximum_cues"],
}


def public_v2_audio_profile():
    return {
        "supported": True, "capability": "audio.sampled_sfx",
        "action_kinds": ["play_sfx"],
        "action_contexts": ["local_transition", "local_timer_handler", "scene_exit", "timer_scene_exit"],
        "scene_exit_commit": "after_destination_admission_and_commit",
        "scene_exit_rejection": "no_cues",
        "action_order": "authored_order", "sequential_playback": False,
        "audio_failure_after_commit": "report_without_scene_rollback_or_retry",
        "compiled_format": TARGET_SAMPLED_SFX["compiled_format"],
        "sample_rate_hz": TARGET_SAMPLED_SFX["sample_rate_hz"],
        "channels": TARGET_SAMPLED_SFX["channels"],
        "block_samples": TARGET_SAMPLED_SFX["block_samples"],
        "maximum_assets": LIMITS["audio_assets"], "maximum_cues": LIMITS["audio_cues"],
        "voice_limit": TARGET_SAMPLED_SFX["voice_limit"],
        "cue_volume": True, "cue_priority": True,
        "residency": "whole_package", "shared_package_limit_bytes": LIMITS["package_bytes"],
        "survives_local_state_change": True, "survives_same_package_scene_replacement": True,
        "package_suspend": "stop_and_discard", "package_resume": "new_requests_only",
        "package_exit_or_replacement": "stop_and_discard",
        "unsupported": ["music", "looping", "procedural_audio", "pause_resume",
                        "fades", "runtime_volume_controls", "nonresident_audio", "scene_exit_mutations"],
    }


def public_v2_text_profile():
    return {
        "supported": True, "scene_schema_versions": [2], "object_kind": "text",
        "font_ids": ["peepshow.system.8x8.basic.v1"],
        "character_set": "printable_ascii_plus_newline", "maximum_length": 256,
        "glyph_cell": {"width": 8, "height": 8},
        "scale": {"minimum": 1, "maximum": 8, "integer_only": True},
        "alignment": ["left", "center", "right"], "vertical_alignment": "top",
        "bounds": {"width": {"minimum": 1, "maximum": 168},
                   "height": {"minimum": 1, "maximum": 144}},
        "overflow": "reject", "line_breaks": "explicit_newline",
        "automatic_wrapping": False, "automatic_shrinking": False,
        "ink": "black", "background": "transparent", "baked_assets": False,
        "commands": ["object.add", "object.set_text", "object.set_defaults",
                     "object_override.set", "object_override.clear", "object.delete"],
        "set_text_fields": ["text", "font_id", "scale", "alignment", "width", "height"],
        "override_properties": ["x", "y", "visible"],
        "runtime_action_kinds": ["object.set_position", "object.move_by", "object.set_visibility"],
        "dynamic_content": False, "content_overrides": False,
        "layer_and_z_order": "object_definition",
        "device_exact_admission_required": True,
    }


def public_v2_export_profile():
    return {
        "profile_id": PROFILE_ID, "profile_revision": 6, "container_version": 2,
        "status": "development_restricted", "execution_model": "scene_objects",
        "limits": deepcopy(LIMITS), "interaction_modes": ["continuous"],
        "event_classes": ["input", "timer"], "audio": True,
        "calendar_schedules": {"event_type": "time.local_schedule", "maximum_per_scene": 1,
                               "scene_entry_only": True, "status": "available_pending_validation"},
        "audio_profile": public_v2_audio_profile(),
        "runtime_text": public_v2_text_profile(),
        "scene_connections": True, "scene_entry_modes": ["fresh_default"],
        "scene_exit_action_kinds": ["play_sfx"], "self_scene_exits": False,
        "system_exit_actions": False,
        "mixed_execution_models": False, "shipping_build": False,
        "budget_method": "conservative_all_clips_and_positions",
        "timing_method": "derived_gcd_no_rounding",
        "device_exact_admission_required": True,
        "admission_scope": "all_scenes_including_unreachable",
        "limit_scopes": {
            "package": ["package_bytes", "scenes", "strings", "audio_assets", "audio_cues"],
            "scene": ["objects", "states", "variables", "bindings", "transitions", "guards",
                      "actions", "animated_objects", "combined_steps", "chunks", "payload_bytes"],
            "clip": ["clip_steps", "clip_distinct_frames", "frame_duration_max_ms"],
            "display": ["band_rows", "band_slot_bytes", "canvas_width", "tick_ms"],
        },
    }


def issue(code, path, message, scene_id=None):
    result = {"code": code, "path": path, "message": message}
    if scene_id is not None:
        result["scene_id"] = scene_id
    return result


def source_issues(bundle):
    issues = []
    if not 1 <= len(bundle.scenes) <= LIMITS["scenes"] or any(
            scene.get("schema_version") != 2 for scene in bundle.scenes):
        issues.append(issue("V2_SCENE_PROFILE", "scenes",
                            "V2 export requires one to eight version-2 scenes; legacy scenes cannot be mixed in."))
    scene_ids = {scene["scene_id"] for scene in bundle.scenes}
    if len(scene_ids) != len(bundle.scenes) or bundle.project["entry_scene"] not in scene_ids:
        issues.append(issue("V2_SCENE_PROFILE", "scenes", "Scene IDs must be unique and the package entry must exist."))
    if bundle.project.get("validation", {}).get("build_profile") == "shipping":
        issues.append(issue("V2_SHIPPING_UNAVAILABLE", "validation.build_profile",
                            "Restricted V2 export is development-only; select the development build profile."))
    if (bundle.audio_assets or bundle.audio_cues) and not (
            1 <= len(bundle.audio_assets) <= LIMITS["audio_assets"] and
            1 <= len(bundle.audio_cues) <= LIMITS["audio_cues"]):
        issues.append(issue("V2_AUDIO_CATALOG", "assets",
                            "V2 audio requires 1..32 sampled assets and 1..64 cues, within the shared resident package budget."))
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
        for scene_exit in scene.get("scene_exits", []):
            if scene_exit["target_scene"] == sid or scene_exit["target_scene"] not in scene_ids:
                issues.append(issue("V2_SCENE_EXIT_UNSUPPORTED", path + ".scene_exits",
                                    "Scene exits must target another scene's default entry.", sid))
    return issues


def package_admission(package, size):
    """Validate a structurally parsed V2 package against the export subset."""
    issues = []
    for name, used in (("package_bytes", size), ("strings", len(package.strings)),
                       ("scenes", len(package.scenes))):
        if used > LIMITS[name]:
            issues.append(issue("V2_CAPACITY", "package." + name,
                                f"V2 {name} use {used}, limit {LIMITS[name]}; reduce this content before export."))
    if not package.scenes or any(scene.get("execution_model") != 2 for scene in package.scenes):
        issues.append(issue("V2_SCENE_PROFILE", "scenes", "V2 export requires only scenes using scene-owned objects."))
        return {"issues": issues, "animation_budget": None, "scene_animation_budgets": {}}
    if (package.audio_assets or package.audio_cues or any(c.chunk_type in {10, 11, 12} for c in package.chunks)) and not (
            1 <= len(package.audio_assets) <= LIMITS["audio_assets"] and
            1 <= len(package.audio_cues) <= LIMITS["audio_cues"]):
        issues.append(issue("V2_AUDIO_CATALOG", "assets",
                            "V2 audio requires 1..32 sampled assets and 1..64 cues, within the shared resident package budget."))
    scene_budgets = {}
    for scene in package.scenes:
        result = _scene_admission(package, scene)
        issues.extend(result["issues"])
        scene_budgets[scene["scene_id"]] = result["animation_budget"]
    return {"issues": issues,
            "animation_budget": next(iter(scene_budgets.values())) if len(package.scenes) == 1 else None,
            "scene_animation_budgets": scene_budgets}


def _scene_admission(package, scene):
    """Each scene owns a separate playback cycle; never combine scene clocks."""
    issues = []

    def limit(name, used, path, sid=None):
        if used > LIMITS[name]:
            issues.append(issue("V2_CAPACITY", path,
                f"V2 {name} use {used}, limit {LIMITS[name]}; reduce this content before export.", sid))

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
            if (route["target_scene"] == sid or route["target_scene"] not in
                    {candidate["scene_id"] for candidate in package.scenes} or
                    any(op["kind"] != 7 for op in route["operations"])):
                issues.append(issue("V2_SCENE_EXIT_UNSUPPORTED", route_path,
                                    "Exit to another scene's default entry with only play_sfx actions or no actions.", sid))
        for index, op in enumerate(route["operations"]):
            if op["kind"] not in {1, 7, 9, 10, 11, 12}:
                issues.append(issue("V2_ACTION_UNSUPPORTED", route_path + f".actions[{index}]",
                    "Use local object, variable, timer or play_sfx actions; render requests and shell exits are not supported in this export profile.", sid))

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
