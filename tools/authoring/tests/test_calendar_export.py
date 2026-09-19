"""Public source/export and deterministic host calendar execution."""
from copy import deepcopy
from dataclasses import replace
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from build_calendar_export_fixture import calendar_export_bundle, write_project
from peepshow_authoring.compiler import build_egg, build_preview_package
from peepshow_authoring.egg_format import parse_egg
from peepshow_authoring.preview import StateScenePreview
from peepshow_authoring.project import load_project, save_project
from peepshow_authoring.protocol import ServiceRequest
from peepshow_authoring.service import AuthoringService


class CalendarExportTests(unittest.TestCase):
    def preview(self, mode="daily", seconds=0, day_offset=0):
        bundle = calendar_export_bundle(mode, seconds, day_offset)
        return StateScenePreview(build_preview_package(bundle), bundle.scenes[0]["scene_id"])

    def test_public_roundtrip_modes(self):
        for mode, offset in (("daily", 0), ("today_offset", 1), ("next_occurrence", 0)):
            bundle = calendar_export_bundle(mode, 43200, offset)
            package = parse_egg(build_egg(bundle))
            self.assertEqual(bundle.scenes[0]["event_bindings"][0]["configuration"],
                             package.scenes[0]["graph"]["event_bindings"][0]["configuration"])

    def test_unset_midnight_local_transition_and_no_replay(self):
        p = self.preview()
        self.assertEqual((), p.advance(600000))
        p.set_local_time("2026-09-19T23:59:40")
        p.advance(10000)
        p.apply_input("BUTTON_A")
        self.assertEqual((), p.advance(9999))
        result = p.advance(1)
        self.assertEqual(1, len(result))
        self.assertTrue(result[0].accepted)
        self.assertEqual("time.local_schedule", result[0].logical_source)
        p.apply_input("BUTTON_A")
        self.assertEqual((), p.advance(600000))

    def test_suspension_keeps_calendar_but_pauses_scene_elapsed(self):
        p = self.preview()
        p.set_local_time("2026-09-19T23:59:40")
        p.advance(5000)
        p.suspend()
        self.assertEqual((), p.advance(30000))
        self.assertEqual(5000, p.snapshot()["timeline"]["elapsed_ms"])
        p.resume()
        self.assertEqual(1, len(p.advance(0)))
        self.assertEqual((), p.advance(0))

    def test_clock_edits_skip_and_do_not_replay(self):
        p = self.preview()
        p.set_local_time("2026-09-19T23:59:40")
        p.set_local_time("2026-09-20T00:01:00")
        self.assertEqual((), p.advance(0))
        p.set_local_time("2026-09-20T23:59:59")
        self.assertEqual(1, len(p.advance(1000)))
        p.set_local_time("2026-09-20T23:59:59")
        self.assertEqual((), p.advance(1000))

    def test_multi_day_suspension_coalesces_and_keeps_relative_time_paused(self):
        p = self.preview()
        p.set_local_time("2026-09-19T23:59:40")
        p.suspend()
        for _ in range(300):
            self.assertEqual((), p.advance(600000))
        p.resume()
        self.assertEqual(1, len(p.advance(0)))
        self.assertEqual((), p.advance(0))
        self.assertEqual(0, p.snapshot()["timeline"]["elapsed_ms"])

    def test_once_past_and_next_and_fixed_resolution(self):
        p = self.preview("today_offset", 43200)
        p.set_local_time("2026-09-19T12:00:00")
        self.assertEqual((), p.advance(600000))
        p = self.preview("next_occurrence", 43200)
        p.set_local_time("2026-09-19T12:00:00")
        deadline = p._calendar_deadline
        p.set_local_time("2026-09-20T11:59:59")
        self.assertEqual(deadline, p._calendar_deadline)
        self.assertEqual(1, len(p.advance(1000)))
        self.assertEqual((), p.advance(600000))

    def test_false_guard_consumes_once(self):
        bundle = calendar_export_bundle("next_occurrence", 43200)
        scene = deepcopy(bundle.scenes[0])
        scene["variables"] = [{"variable_id": "allow", "type": "int", "initial": 0,
                               "minimum": 0, "maximum": 1}]
        scene["event_handlers"][0]["guards"] = [{"variable_ref": "allow", "operator": "eq", "value": 1}]
        p = StateScenePreview(build_preview_package(replace(bundle, scenes=(scene,))), scene["scene_id"])
        p.set_local_time("2026-09-19T11:59:59")
        events = p.advance(1000)
        self.assertEqual(1, len(events))
        self.assertFalse(events[0].accepted)
        p._variables[0] = 1
        self.assertEqual((), p.advance(600000))

    def test_validation_and_save_reload(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp) / "calendar.peepproj"
            bundle = write_project(root)
            self.assertTrue(bundle.valid)
            for config in ({"mode": "daily", "time_of_day_seconds": 86400},
                           {"mode": [], "time_of_day_seconds": 0},
                           {"mode": "daily", "time_of_day_seconds": True},
                           {"mode": "next_occurrence", "time_of_day_seconds": 0, "day_offset": 1}):
                scene = deepcopy(bundle.scenes[0])
                scene["event_bindings"][0]["configuration"] = config
                save_project(replace(bundle, scenes=(scene,)))
                self.assertFalse(load_project(root).valid)

    def test_public_service_build_and_preview(self):
        with tempfile.TemporaryDirectory() as temp:
            bundle = write_project(Path(temp) / "calendar.peepproj")
            service = AuthoringService()
            def call(op, **params):
                return service.handle(ServiceRequest("test", op, params))
            loaded = call("project.load", path=str(bundle.root))
            self.assertTrue(loaded["scene_capabilities"][bundle.scenes[0]["scene_id"]]["calendar_schedules"]["supported"])
            rev = loaded["project_revision"]
            call("project.build_package", project_revision=rev)
            result = call("project.preview_reset", project_revision=rev, scene_id=bundle.scenes[0]["scene_id"])
            for operation, fields in (
                ("project.preview_set_local_time", {"local_time": "2026-09-19T23:59:40"}),
                ("project.preview_suspend", {}),
                ("project.preview_advance", {"elapsed_ms": 30000}),
                ("project.preview_resume", {}),
            ):
                result = call(operation, project_revision=rev, preview_revision=result["preview_revision"], **fields)
            self.assertEqual(1, len(result["timer_events"]))
