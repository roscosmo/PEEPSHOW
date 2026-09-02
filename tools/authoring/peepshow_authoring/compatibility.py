"""Deterministic V1 compatibility report for the implemented STATE subset."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path
from typing import Any

from .egg_format import parse_egg
from .project import ProjectBundle, ValidationIssue
from .target_profile import (
    TARGET_PROFILE_HASH,
    TARGET_PROFILE_SOURCE_REF,
    TargetProfileError,
    target_profile_for_id,
)


REPORT_SCHEMA_VERSION = 1
TOOL_VERSION = "0.1.0"
EMPTY_SHA256 = hashlib.sha256(b"").hexdigest()


def _package_path(value: str) -> str:
    path = Path(value)
    if path.is_absolute():
        return path.name or "project"
    return value


def _canonical_sha256(value: Any) -> str:
    payload = json.dumps(
        value,
        ensure_ascii=True,
        separators=(",", ":"),
        sort_keys=True,
    ).encode("ascii")
    return hashlib.sha256(payload).hexdigest()


def _source_checksum(bundle: ProjectBundle) -> str:
    if bundle.valid:
        return hashlib.sha256(bundle.canonical_bytes()).hexdigest()
    return _canonical_sha256(
        {
            "project": bundle.project,
            "scenes": bundle.scenes,
            "issues": [
                {
                    "code": issue.code,
                    "path": _package_path(issue.path),
                    "message": issue.message,
                }
                for issue in bundle.issues
            ],
        }
    )


def _validation_result(
    issue_id: str,
    issue: ValidationIssue,
    *,
    severity: str = "error",
    blocks_preview: bool = True,
    blocks_dev: bool = True,
    blocks_shipping: bool = True,
) -> dict[str, Any]:
    return {
        "issue_id": issue_id,
        "severity": severity,
        "category": "authoring_source",
        "package_path": _package_path(issue.path),
        "user_message": issue.message,
        "internal_detail_ref": issue.code,
        "blocks_authoring_preview": blocks_preview,
        "blocks_dev_package": blocks_dev,
        "blocks_shipping_package": blocks_shipping,
        "waiver_ref": None,
    }


def _pending_profile_result(build_profile: str) -> dict[str, Any]:
    shipping = build_profile == "shipping"
    return {
        "issue_id": "target-profile-pending",
        "severity": "error" if shipping else "advisory",
        "category": "capabilities",
        "package_path": "project.selected_target_profile",
        "user_message": (
            "The selected HW6 development profile is not shipping-authoritative yet."
        ),
        "internal_detail_ref": "TARGET_PROFILE_PENDING_FOR_SHIPPING",
        "blocks_authoring_preview": False,
        "blocks_dev_package": False,
        "blocks_shipping_package": True,
        "waiver_ref": None,
    }


def _scene_report(scene: dict[str, Any], valid: bool) -> dict[str, Any]:
    has_waiting = bool(scene.get("waiting_visuals"))
    has_sfx = any(
        action.get("kind") == "play_sfx"
        for route in scene.get("routes", [])
        for action in route.get("actions", [])
        if isinstance(action, dict)
    )
    interaction = scene.get("interaction_policy")
    if not isinstance(interaction, dict):
        interaction = {}
    return {
        "scene_id": scene.get("scene_id"),
        "scene_name": scene.get("display_name"),
        "scene_type": scene.get("scene_type"),
        "entry_ref": scene.get("entry_state"),
        "inactive_route_ref": interaction.get("inactive_route"),
        "failure_route_ref": None,
        "required_capabilities": ["audio.sampled_sfx"] if has_sfx else [],
        "optional_capabilities": ["display.waiting_visual_animation"] if has_waiting else [],
        "budget_status": "pending_validation" if valid else "blocked",
        "validation_status": "passed" if valid else "failed",
    }


def _waiting_reports(bundle: ProjectBundle) -> list[dict[str, Any]]:
    if not bundle.valid:
        return []
    reports: list[dict[str, Any]] = []
    for scene in bundle.scenes:
        fallback = bool(scene.get("reactive_wait_default", {}).get("hold_fallback_allowed"))
        for state in scene.get("states", []):
            reports.append(
                {
                    "state_path": f"{scene.get('scene_id')}.{state.get('state_id')}",
                    "preferred_visual_ref": state.get("waiting_visual_ref"),
                    "fallback_visual_ref": "held_frame" if fallback else None,
                    "compiler_profile_id": bundle.project.get("selected_target_profile"),
                    "admission_status": "pending_validation",
                    "abstract_utilization": None,
                    "selected_result": "target_admission_required",
                    "issue_refs": ["target-profile-pending"],
                }
            )
    return reports


def _interaction_report(bundle: ProjectBundle) -> dict[str, Any]:
    if not bundle.valid or not bundle.scenes:
        return {
            "target_timeout_policy_ref": None,
            "meaningful_activity_sources": [],
            "inactive_route": None,
            "inactive_target_scene": None,
            "inactive_waiting_visual_ref": None,
            "bounded_deferral_status": "unavailable",
            "contexts": [],
            "target_activation_gestures": [],
            "activation_gesture_consumed": True,
            "admission_status": "blocked",
            "issue_refs": [],
        }
    scene = bundle.scenes[0]
    policy = scene.get("interaction_policy")
    if not isinstance(policy, dict):
        policy = {}
    action_sources = {
        item.get("action_id"): item.get("logical_source")
        for item in scene.get("input_actions", [])
        if isinstance(item, dict)
    }
    meaningful = [
        action_sources.get(action_id, action_id)
        for action_id in policy.get("meaningful_activity_actions", [])
    ]
    return {
        "target_timeout_policy_ref": "target_owned",
        "meaningful_activity_sources": meaningful,
        "inactive_route": policy.get("inactive_route"),
        "inactive_target_scene": None,
        "inactive_waiting_visual_ref": scene.get("reactive_wait_default", {}).get("waiting_visual_ref"),
        "bounded_deferral_status": "admitted" if policy.get("bounded_deferrals") == [] else "blocked",
        "contexts": [],
        "target_activation_gestures": ["START"],
        "activation_gesture_consumed": True,
        "admission_status": "pending_validation",
        "issue_refs": ["target-profile-pending"],
    }


def build_compatibility_report(
    bundle: ProjectBundle,
    package_blob: bytes | None = None,
) -> dict[str, Any]:
    """Build a deterministic report without claiming unvalidated HW6 grants."""

    build_profile = bundle.project.get("validation", {}).get("build_profile", "development")
    source_checksum = _source_checksum(bundle)
    package = parse_egg(package_blob) if package_blob is not None else None
    package_checksum = package.sha256 if package is not None else None
    package_bytes = len(package_blob) if package_blob is not None else 0

    validation_results = [
        _validation_result(f"validation-{index + 1}", issue)
        for index, issue in enumerate(bundle.issues)
    ]
    if bundle.project:
        validation_results.append(_pending_profile_result(build_profile))

    counts = {name: 0 for name in ("fatal", "error", "warning", "advisory", "waived")}
    for result in validation_results:
        counts[result["severity"]] += 1
    blocking_count = sum(
        1
        for result in validation_results
        if result["blocks_dev_package"]
        or build_profile == "shipping"
        and result["blocks_shipping_package"]
    )
    if bundle.issues or build_profile == "shipping":
        report_status = "failed"
    else:
        report_status = "dev_only"

    profile_id = bundle.project.get("selected_target_profile")
    try:
        target_profile = target_profile_for_id(profile_id)
    except TargetProfileError:
        target_profile = None
    profile_descriptor = {
        "profile_id": profile_id,
        "profile_version": (
            target_profile["profile_version"] if target_profile is not None else None
        ),
        "profile_status": (
            target_profile["profile_status"]
            if target_profile is not None
            else "unsupported"
        ),
    }
    has_waiting = any(scene.get("waiting_visuals") for scene in bundle.scenes)
    capability_reports = []
    if has_waiting:
        waiting_scenes = [
            scene for scene in bundle.scenes if scene.get("waiting_visuals")
        ]
        fallback_declared = all(
            scene.get("reactive_wait_default", {}).get("hold_fallback_allowed") is True
            for scene in waiting_scenes
        )
        capability_reports.append(
            {
                "capability": "display.waiting_visual_animation",
                "requested_by": [scene.get("scene_id") for scene in waiting_scenes],
                "requirement_level": "optional" if fallback_declared else "required",
                "target_grant_status": "pending_validation",
                "fallback_declared": fallback_declared,
                "admission_status": "pending_validation",
                "issue_refs": ["target-profile-pending"],
            }
        )
    sfx_scenes = [
        scene
        for scene in bundle.scenes
        if any(
            action.get("kind") == "play_sfx"
            for route in scene.get("routes", [])
            for action in route.get("actions", [])
            if isinstance(action, dict)
        )
    ]
    if sfx_scenes:
        capability_reports.append(
            {
                "capability": "audio.sampled_sfx",
                "requested_by": [scene.get("scene_id") for scene in sfx_scenes],
                "requirement_level": "required",
                "target_grant_status": "pending_validation",
                "fallback_declared": False,
                "admission_status": "pending_validation",
                "issue_refs": ["target-profile-pending"],
            }
        )

    budget_names = (
        "runtime_logic",
        "rendering",
        "assets",
        "runtime_ram",
        "save_settings",
        "diagnostics",
        "input",
        "sensors",
        "audio",
        "communication",
        "time_power",
        "waiting_visual_sequences",
    )
    budgets = {name: {"status": "pending_validation"} for name in budget_names}
    package_limit = (
        int(target_profile["package"]["maximum_bytes"])
        if target_profile is not None
        else None
    )
    audio_limit = (
        int(target_profile["audio"]["sampled_sfx"]["maximum_bank_bytes"])
        if target_profile is not None
        else None
    )
    audio_bytes = sum(len(asset.adpcm) for asset in bundle.audio_assets)
    budgets["audio"] = {
        "used_bytes": audio_bytes,
        "limit_bytes": audio_limit,
        "status": (
            "passed"
            if bundle.valid and audio_limit is not None and audio_bytes <= audio_limit
            else "blocked" if bundle.issues else "pending_validation"
        ),
    }
    budgets["package_size"] = {
        "used_bytes": package_bytes,
        "limit_bytes": package_limit,
        "status": (
            "passed"
            if package is not None
            and package_limit is not None
            and package_bytes <= package_limit
            else "blocked" if bundle.issues else "pending_validation"
        ),
    }

    project_package = bundle.project.get("package", {})
    report: dict[str, Any] = {
        "report_schema_version": REPORT_SCHEMA_VERSION,
        "report_id": None,
        "report_status": report_status,
        "build_profile": "shipping" if build_profile == "shipping" else "dev_package",
        "generated_by": {
            "tool_name": "peepshow_authoring",
            "tool_version": TOOL_VERSION,
            "validator_version": 1,
            "package_compiler_version": 1,
            "schema_versions": ["peepshow.authoring.project:1", "peepshow.authoring.state_scene:1"],
        },
        "package": {
            "package_id": project_package.get("package_id"),
            "package_name": project_package.get("display_name"),
            "package_version": project_package.get("version"),
            "package_container_version": 1,
            "package_checksum": package_checksum,
            "source_manifest_checksum": source_checksum,
            "content_parameter_checksum": EMPTY_SHA256,
        },
        "authoring_source": {
            "templates": [],
            "authoring_kits": [],
            "prefabs": [],
            "behavior_graphs": [],
            "behavior_macros": [],
        },
        "target_profile": {
            **profile_descriptor,
            "profile_hash": (
                TARGET_PROFILE_HASH if target_profile is not None else None
            ),
            "source_evidence_refs": (
                [TARGET_PROFILE_SOURCE_REF] if target_profile is not None else []
            ),
        },
        "capability_registry": {"registry_version": 0, "registry_hash": EMPTY_SHA256},
        "result_summary": {
            "fatal_count": counts["fatal"],
            "error_count": counts["error"],
            "warning_count": counts["warning"],
            "advisory_count": counts["advisory"],
            "waived_count": counts["waived"],
            "blocking_count": blocking_count,
        },
        "scenes": [_scene_report(scene, bundle.valid) for scene in bundle.scenes],
        "capabilities": capability_reports,
        "budgets": budgets,
        "interaction_state": _interaction_report(bundle),
        "waiting_visual_admission": _waiting_reports(bundle),
        "validation_results": validation_results,
        "waivers": [],
        "artifacts": {
            "package_blob_ref": None,
            "package_blob_checksum": package_checksum,
            "compatibility_report_checksum": None,
            "deterministic_build_ref": None,
            "validation_log_ref": None,
        },
    }
    identity = _canonical_sha256(report)
    report["report_id"] = f"compat-{identity[:24]}"
    report["artifacts"]["compatibility_report_checksum"] = _canonical_sha256(report)
    return report
