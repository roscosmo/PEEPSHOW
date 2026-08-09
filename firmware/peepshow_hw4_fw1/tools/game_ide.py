#!/usr/bin/env python3
"""
PeepShow Game IDE (v1 scaffold)

This is a lightweight authoring shell for game project domains:
- pages
- pet_menu_slots
- maps
- controller/camera/input profiles
- entities
- dialogues
- scripts
- items
- game modes (stored under package `modes` in Assets/game_package/manifest.example.json)

It uses JSON records under Assets/game_project and runs the validator.
"""

from __future__ import annotations

import json
import tkinter as tk
from pathlib import Path
from tkinter import messagebox
from tkinter import ttk
from typing import Any

from validate_game_project import find_repo_root, validate_project


DOMAIN_FILES = {
    "maps": "maps.json",
    "pages": "pages.json",
    "pet_menu_slots": "pet_menu_slots.json",
    "entities": "entities.json",
    "dialogues": "dialogues.json",
    "scripts": "scripts.json",
    "items": "items.json",
    "controller_profiles": "controller_profiles.json",
    "camera_profiles": "camera_profiles.json",
    "input_profiles": "input_profiles.json",
}

PACKAGE_MODES_DOMAIN = "package_modes"
PACKAGE_MANIFEST_REL = Path("Assets") / "game_package" / "manifest.example.json"
TREE_ROOT_IID = "root:game_package"

DOMAIN_ORDER = [
    "pages",
    "pet_menu_slots",
    "maps",
    "controller_profiles",
    "camera_profiles",
    "input_profiles",
    "entities",
    "dialogues",
    "scripts",
    "items",
    PACKAGE_MODES_DOMAIN,
]
TREE_DOMAIN_ORDER = [
    PACKAGE_MODES_DOMAIN,
    "pet_menu_slots",
    "pages",
    "maps",
    "controller_profiles",
    "camera_profiles",
    "input_profiles",
    "entities",
    "dialogues",
    "scripts",
    "items",
]
DOMAIN_LABELS = {
    "pages": "Pages",
    "pet_menu_slots": "Pet Menu Slots",
    "maps": "Maps",
    "controller_profiles": "Controller Profiles",
    "camera_profiles": "Camera Profiles",
    "input_profiles": "Input Profiles",
    "entities": "Entities",
    "dialogues": "Dialogues",
    "scripts": "Scripts",
    "items": "Items",
    PACKAGE_MODES_DOMAIN: "Game Modes",
}
PROFILE_DOMAINS = ("controller_profiles", "camera_profiles", "input_profiles")
PROFILE_FORM_FIELDS_BY_DOMAIN: dict[str, list[tuple[str, str]]] = {
    "controller_profiles": [
        ("id", "Profile Key"),
        ("display_name", "Profile Name"),
        ("profile_id", "Internal Profile ID"),
        ("topdown_render_scale", "Topdown Scale"),
        ("topdown_tile_present_mode", "Topdown Present Mode"),
        ("move_speed_px_s", "Move Speed (px/s)"),
        ("move_accel_px_s2", "Move Accel (px/s^2)"),
        ("move_decel_px_s2", "Move Decel (px/s^2)"),
    ],
    "camera_profiles": [
        ("id", "Profile Key"),
        ("display_name", "Profile Name"),
        ("profile_id", "Internal Profile ID"),
        ("camera_deadzone_w_px", "Camera Deadzone W (px)"),
        ("camera_deadzone_h_px", "Camera Deadzone H (px)"),
        ("camera_follow_permille", "Camera Follow (permille)"),
        ("camera_max_speed_px_s", "Camera Max Speed (px/s)"),
        ("camera_lookahead_x_px", "Camera Lookahead X (px)"),
        ("camera_lookahead_y_px", "Camera Lookahead Y (px)"),
    ],
    "input_profiles": [
        ("id", "Profile Key"),
        ("display_name", "Profile Name"),
        ("profile_id", "Internal Profile ID"),
        ("input_deadzone_permille", "Input Deadzone (permille)"),
        ("input_flags", "Input Flags (bitmask)"),
    ],
}
PROFILE_MODE_SYNC_FIELDS: dict[str, tuple[str, ...]] = {
    "controller_profiles": (
        "topdown_render_scale",
        "topdown_tile_present_mode",
        "move_speed_px_s",
        "move_accel_px_s2",
        "move_decel_px_s2",
    ),
    "camera_profiles": (
        "camera_deadzone_w_px",
        "camera_deadzone_h_px",
        "camera_follow_permille",
        "camera_max_speed_px_s",
        "camera_lookahead_x_px",
        "camera_lookahead_y_px",
    ),
    "input_profiles": (
        "input_deadzone_permille",
        "input_flags",
    ),
}

ENTITY_FORM_FIELDS = [
    ("id", "ID"),
    ("display_name", "Display Name"),
    ("category", "Category"),
    ("sprite_set_id", "Sprite Set ID"),
    ("animation_set_id", "Animation Set ID"),
    ("controller_type", "Controller Type"),
    ("collision_profile_id", "Collision Profile ID"),
    ("dialogue_id", "Dialogue ID"),
    ("ability_set_id", "Ability Set ID"),
    ("loot_table_id", "Loot Table ID"),
    ("ai_profile_id", "AI Profile ID"),
]

ENTITY_CATEGORIES = (
    "player",
    "npc",
    "enemy",
    "boss",
    "pickup",
    "interactable",
)

MAP_FORM_FIELDS = [
    ("id", "ID"),
    ("display_name", "Display Name"),
    ("tiled_map", "Tiled Map Path"),
    ("map_asset_id", "Map Asset ID"),
    ("tileset_asset_id", "Tileset Asset ID"),
    ("music_asset_id", "Music Asset ID"),
]

DIALOGUE_FORM_FIELDS = [
    ("id", "ID"),
    ("start_node_id", "Start Node ID"),
]

SCRIPT_FORM_FIELDS = [
    ("id", "ID"),
    ("trigger", "Trigger"),
]

ITEM_FORM_FIELDS = [
    ("id", "ID"),
    ("name", "Name"),
    ("category", "Category"),
    ("max_stack", "Max Stack"),
    ("value", "Value"),
    ("description", "Description"),
    ("use_effect_id", "Use Effect ID"),
]

ITEM_CATEGORIES = (
    "consumable",
    "key_item",
    "material",
    "equipment",
    "currency",
)

PACKAGE_MODE_FORM_FIELDS = [
    ("id", "Mode Key"),
    ("display_name", "Mode Name"),
    ("description", "Description"),
    ("map_ref", "Map"),
    ("controller_profile_ref", "Controller Profile"),
    ("camera_profile_ref", "Camera Profile"),
    ("input_profile_ref", "Input Profile"),
    ("runtime_kind", "Runtime Kind"),
    ("scene_lifecycle", "Resume Behavior"),
    ("mode_id", "Internal Mode ID"),
    ("backend_id", "Backend ID"),
    ("resume_domain_id", "Resume Domain (Internal)"),
    ("music_asset_id", "Music Asset ID"),
    ("sfx_interact_asset_id", "SFX Interact Asset ID"),
    ("sfx_confirm_asset_id", "SFX Confirm Asset ID"),
    ("sfx_error_asset_id", "SFX Error Asset ID"),
]
PACKAGE_MODE_TEXT_FIELDS = {"id", "display_name", "description"}
PACKAGE_MODE_BASIC_FIELD_KEYS = (
    "id",
    "display_name",
    "description",
    "map_ref",
    "controller_profile_ref",
    "camera_profile_ref",
    "input_profile_ref",
    "runtime_kind",
    "scene_lifecycle",
)
PACKAGE_MODE_ADVANCED_FIELD_KEYS = tuple(
    key for key, _ in PACKAGE_MODE_FORM_FIELDS if key not in PACKAGE_MODE_BASIC_FIELD_KEYS
)
PACKAGE_MODE_FORM_LABELS = dict(PACKAGE_MODE_FORM_FIELDS)

PACKAGE_MODE_ENUM_OPTIONS: dict[str, tuple[str, ...]] = {
    "runtime_kind": ("Realtime (1)", "Static (2)"),
    "scene_lifecycle": ("Persistent (1)", "Transient (2)", "Default (0)"),
}
PACKAGE_MODE_ENUM_VALUE_LABELS: dict[str, dict[int, str]] = {
    "runtime_kind": {1: "Realtime (1)", 2: "Static (2)"},
    "scene_lifecycle": {1: "Persistent (1)", 2: "Transient (2)", 0: "Default (0)"},
}

PAGE_FORM_FIELDS = [
    ("id", "Page Key"),
    ("display_name", "Page Name"),
    ("page_id", "Internal Page ID"),
    ("route_kind", "Route Kind"),
    ("native_page_key", "Native Page"),
    ("native_tree_key", "Native Tree"),
]
PAGE_ROUTE_KIND_OPTIONS = ("native_page", "native_tree")
PAGE_NATIVE_PAGE_OPTIONS = (
    "home",
    "pet",
    "battery_stats",
    "audio_levels",
    "lis2",
    "lis2_steps",
    "joy_cal",
    "joy_target",
)
PAGE_NATIVE_TREE_OPTIONS = ("system_root", "pet_feed")

PET_MENU_SLOT_FORM_FIELDS = [
    ("id", "Slot Key"),
    ("display_name", "Slot Name"),
    ("slot_index", "Slot Index"),
    ("icon_action_id", "Icon Action ID"),
    ("select_kind", "Select Behavior"),
    ("mode_ref", "Target Mode"),
    ("page_ref", "Target Page"),
    ("status_kind", "Status Kind"),
    ("status_source", "Status Source"),
    ("status_base_icon_action_id", "Status Base Icon"),
]
PET_MENU_SLOT_ENUM_OPTIONS: dict[str, tuple[str, ...]] = {
    "select_kind": ("none", "feed", "play", "start_game", "options", "launch_mode", "open_page"),
    "status_kind": ("none", "bool", "level4"),
    "status_source": ("none", "battery"),
}

DOMAIN_TEMPLATES: dict[str, dict[str, Any]] = {
    "pages": {
        "id": "new_page",
        "display_name": "New Page",
        "page_id": 1,
        "route_kind": "native_page",
        "native_page_key": "home",
        "native_tree_key": "",
    },
    "pet_menu_slots": {
        "id": "slot_0",
        "display_name": "Menu Slot",
        "slot_index": 0,
        "icon_action_id": 0,
        "select_kind": "none",
        "mode_key": "",
        "page_key": "",
        "status_kind": "none",
        "status_source": "none",
        "status_base_icon_action_id": 0,
    },
    "maps": {
        "id": "new_map",
        "display_name": "New Map",
        "tiled_map": "Assets/maps/new_map.json",
        "map_asset_id": 0,
        "tileset_asset_id": 0,
        "music_asset_id": 0,
    },
    "entities": {
        "id": "new_entity",
        "display_name": "New Entity",
        "category": "npc",
        "sprite_set_id": "",
        "animation_set_id": "",
        "controller_type": "",
        "collision_profile_id": "",
        "dialogue_id": "",
        "ability_set_id": "",
        "loot_table_id": "",
        "ai_profile_id": "",
        "event_hooks": [],
    },
    "dialogues": {
        "id": "new_dialogue",
        "start_node_id": "start",
        "nodes": [
            {
                "id": "start",
                "speaker_id": "",
                "text": "",
                "choices": [],
            }
        ],
    },
    "scripts": {
        "id": "new_script",
        "trigger": "on_interact",
        "actions": [],
    },
    "items": {
        "id": "new_item",
        "name": "New Item",
        "category": "consumable",
        "max_stack": 1,
        "value": 0,
        "description": "",
        "use_effect_id": "",
    },
    "controller_profiles": {
        "id": "topdown_player",
        "display_name": "Topdown Player",
        "profile_id": 2,
        "topdown_render_scale": 2,
        "topdown_tile_present_mode": 0,
        "move_speed_px_s": 72,
        "move_accel_px_s2": 480,
        "move_decel_px_s2": 640,
    },
    "camera_profiles": {
        "id": "topdown_follow",
        "display_name": "Topdown Follow",
        "profile_id": 4,
        "camera_deadzone_w_px": 24,
        "camera_deadzone_h_px": 24,
        "camera_follow_permille": 280,
        "camera_max_speed_px_s": 240,
        "camera_lookahead_x_px": 16,
        "camera_lookahead_y_px": 16,
    },
    "input_profiles": {
        "id": "default_input",
        "display_name": "Default Input",
        "profile_id": 1,
        "input_deadzone_permille": 150,
        "input_flags": 3,
    },
    PACKAGE_MODES_DOMAIN: {
        "id": "mode_1",
        "display_name": "New Game Mode",
        "description": "",
        "mode_id": 1,
        "runtime_kind": 1,
        "backend_id": 3,
        "controller_profile_key": "topdown_player",
        "camera_profile_key": "topdown_follow",
        "input_profile_key": "default_input",
        "scene_map_id": 0,
        "scene_tileset_id": 0,
        "music_asset_id": 0,
        "sfx_interact_asset_id": 0,
        "sfx_confirm_asset_id": 0,
        "sfx_error_asset_id": 0,
        "scene_lifecycle": 2,
        "resume_domain_id": 0,
    },
}


def load_json(path: Path, default_value: Any) -> Any:
    if not path.is_file():
        return default_value
    return json.loads(path.read_text(encoding="utf-8"))


def deep_copy_json(value: Any) -> Any:
    return json.loads(json.dumps(value))


class GameIdeApp(tk.Tk):
    def __init__(self, repo_root: Path) -> None:
        super().__init__()
        self.repo_root = repo_root
        self.gp_dir = self.repo_root / "Assets" / "game_project"
        self.package_manifest_path = self.repo_root / PACKAGE_MANIFEST_REL
        self.title("PeepShow Game IDE (v1)")
        self.geometry("1200x760")
        self.minsize(1000, 640)

        self.domain_data: dict[str, list[dict[str, Any]]] = {}
        self.package_manifest: dict[str, Any] = {}
        self.active_domain: str | None = None
        self.active_index: int | None = None
        self._suspend_tree_select = False
        self.entity_form_vars: dict[str, tk.StringVar] = {
            key: tk.StringVar(value="") for key, _ in ENTITY_FORM_FIELDS
        }
        self.entity_form_widgets: list[Any] = []
        self.entity_form_status_var = tk.StringVar(
            value="Select an entity record to edit with form controls."
        )
        self.map_form_vars: dict[str, tk.StringVar] = {
            key: tk.StringVar(value="") for key, _ in MAP_FORM_FIELDS
        }
        self.map_form_widgets: list[Any] = []
        self.map_form_status_var = tk.StringVar(
            value="Select a map record to edit with form controls."
        )
        self.dialogue_form_vars: dict[str, tk.StringVar] = {
            key: tk.StringVar(value="") for key, _ in DIALOGUE_FORM_FIELDS
        }
        self.dialogue_form_widgets: list[Any] = []
        self.dialogue_form_status_var = tk.StringVar(
            value="Select a dialogue record to edit with form controls."
        )
        self.script_form_vars: dict[str, tk.StringVar] = {
            key: tk.StringVar(value="") for key, _ in SCRIPT_FORM_FIELDS
        }
        self.script_form_widgets: list[Any] = []
        self.script_form_status_var = tk.StringVar(
            value="Select a script record to edit with form controls."
        )
        self.item_form_vars: dict[str, tk.StringVar] = {
            key: tk.StringVar(value="") for key, _ in ITEM_FORM_FIELDS
        }
        self.item_form_widgets: list[Any] = []
        self.item_form_status_var = tk.StringVar(
            value="Select an item record to edit with form controls."
        )
        self.package_mode_form_vars: dict[str, tk.StringVar] = {
            key: tk.StringVar(value="") for key, _ in PACKAGE_MODE_FORM_FIELDS
        }
        self.package_mode_form_widgets: list[Any] = []
        self.package_mode_form_widget_by_key: dict[str, Any] = {}
        self.package_mode_show_advanced_var = tk.BooleanVar(value=False)
        self.package_mode_form_status_var = tk.StringVar(
            value="Select a game mode record to edit with form controls."
        )
        self.page_form_vars: dict[str, tk.StringVar] = {
            key: tk.StringVar(value="") for key, _ in PAGE_FORM_FIELDS
        }
        self.page_form_widgets: list[Any] = []
        self.page_form_widget_by_key: dict[str, Any] = {}
        self.page_form_status_var = tk.StringVar(
            value="Select a page record to edit with form controls."
        )
        self.pet_menu_slot_form_vars: dict[str, tk.StringVar] = {
            key: tk.StringVar(value="") for key, _ in PET_MENU_SLOT_FORM_FIELDS
        }
        self.pet_menu_slot_form_widgets: list[Any] = []
        self.pet_menu_slot_form_widget_by_key: dict[str, Any] = {}
        self.pet_menu_slot_form_status_var = tk.StringVar(
            value="Select a pet menu slot record to edit with form controls."
        )
        self.profile_form_vars: dict[str, dict[str, tk.StringVar]] = {
            domain: {key: tk.StringVar(value="") for key, _ in PROFILE_FORM_FIELDS_BY_DOMAIN[domain]}
            for domain in PROFILE_DOMAINS
        }
        self.profile_form_widgets: dict[str, list[Any]] = {domain: [] for domain in PROFILE_DOMAINS}
        self.profile_form_status_vars: dict[str, tk.StringVar] = {
            domain: tk.StringVar(value=self._profile_form_default_status(domain))
            for domain in PROFILE_DOMAINS
        }
        self.map_form_box: ttk.LabelFrame | None = None
        self.dialogue_form_box: ttk.LabelFrame | None = None
        self.script_form_box: ttk.LabelFrame | None = None
        self.item_form_box: ttk.LabelFrame | None = None
        self.entity_form_box: ttk.LabelFrame | None = None
        self.package_mode_form_box: ttk.LabelFrame | None = None
        self.package_mode_advanced_box: ttk.LabelFrame | None = None
        self.package_mode_advanced_toggle: ttk.Checkbutton | None = None
        self.page_form_box: ttk.LabelFrame | None = None
        self.pet_menu_slot_form_box: ttk.LabelFrame | None = None
        self.profile_form_boxes: dict[str, ttk.LabelFrame] = {}

        self._build_ui()
        self._load_all()
        self._rebuild_tree()
        self._set_status("Ready.")

    def _build_ui(self) -> None:
        self.columnconfigure(0, weight=0)
        self.columnconfigure(1, weight=1)
        self.rowconfigure(0, weight=1)
        self.rowconfigure(1, weight=0)

        left = ttk.Frame(self, padding=8)
        left.grid(row=0, column=0, sticky="nsw")
        left.rowconfigure(1, weight=1)

        ttk.Label(left, text="Project Tree", font=("", 11, "bold")).grid(row=0, column=0, sticky="w")
        self.tree = ttk.Treeview(left, show="tree", height=30)
        self.tree.grid(row=1, column=0, sticky="nsew", pady=(6, 6))
        self.tree.bind("<<TreeviewSelect>>", self._on_tree_select)

        btns = ttk.Frame(left)
        btns.grid(row=2, column=0, sticky="ew")
        btns.columnconfigure(0, weight=1)
        btns.columnconfigure(1, weight=1)
        ttk.Button(btns, text="Add", command=self._add_record).grid(row=0, column=0, sticky="ew", padx=(0, 4))
        ttk.Button(btns, text="Delete", command=self._delete_record).grid(row=0, column=1, sticky="ew", padx=(4, 0))

        right = ttk.Frame(self, padding=8)
        right.grid(row=0, column=1, sticky="nsew")
        right.columnconfigure(0, weight=1)
        right.rowconfigure(3, weight=1)

        self.record_title = ttk.Label(right, text="Record", font=("", 11, "bold"))
        self.record_title.grid(row=0, column=0, sticky="w")

        self.entity_form_box = ttk.LabelFrame(right, text="Entity Form (v1)", padding=8)
        self.entity_form_box.grid(row=1, column=0, sticky="ew", pady=(8, 8))
        self.entity_form_box.columnconfigure(1, weight=1)
        for row, (key, label) in enumerate(ENTITY_FORM_FIELDS):
            ttk.Label(self.entity_form_box, text=label).grid(row=row, column=0, sticky="w", padx=(0, 8), pady=2)
            if key == "category":
                widget = ttk.Combobox(
                    self.entity_form_box,
                    textvariable=self.entity_form_vars[key],
                    values=ENTITY_CATEGORIES,
                    state="readonly",
                )
            else:
                widget = ttk.Entry(self.entity_form_box, textvariable=self.entity_form_vars[key])
            widget.grid(row=row, column=1, sticky="ew", pady=2)
            self.entity_form_widgets.append(widget)

        entity_form_actions = ttk.Frame(self.entity_form_box)
        entity_form_actions.grid(row=len(ENTITY_FORM_FIELDS), column=0, columnspan=2, sticky="ew", pady=(8, 2))
        entity_form_actions.columnconfigure(0, weight=1)
        entity_form_actions.columnconfigure(1, weight=1)
        ttk.Button(
            entity_form_actions,
            text="Load Record -> Form",
            command=self._load_entity_form_from_active,
        ).grid(row=0, column=0, sticky="ew", padx=(0, 4))
        ttk.Button(
            entity_form_actions,
            text="Apply Form -> Record",
            command=self._apply_entity_form_to_active,
        ).grid(row=0, column=1, sticky="ew", padx=(4, 0))

        ttk.Label(
            self.entity_form_box,
            textvariable=self.entity_form_status_var,
            foreground="#555555",
        ).grid(
            row=len(ENTITY_FORM_FIELDS) + 1,
            column=0,
            columnspan=2,
            sticky="w",
            pady=(4, 0),
        )

        self.package_mode_form_box = ttk.LabelFrame(right, text="Game Mode Form (v1)", padding=8)
        self.package_mode_form_box.grid(row=1, column=0, sticky="ew", pady=(8, 8))
        self.package_mode_form_box.columnconfigure(1, weight=1)
        basic_grid = ttk.Frame(self.package_mode_form_box)
        basic_grid.grid(row=0, column=0, columnspan=2, sticky="ew")
        basic_grid.columnconfigure(1, weight=1)
        for row, key in enumerate(PACKAGE_MODE_BASIC_FIELD_KEYS):
            label = PACKAGE_MODE_FORM_LABELS[key]
            ttk.Label(basic_grid, text=label).grid(row=row, column=0, sticky="w", padx=(0, 8), pady=2)
            widget = self._create_package_mode_widget(basic_grid, key)
            widget.grid(row=row, column=1, sticky="ew", pady=2)
            self.package_mode_form_widget_by_key[key] = widget
            self.package_mode_form_widgets.append(widget)

        self.package_mode_advanced_toggle = ttk.Checkbutton(
            self.package_mode_form_box,
            text="Show Advanced Fields",
            variable=self.package_mode_show_advanced_var,
            command=self._toggle_package_mode_advanced,
        )
        self.package_mode_advanced_toggle.grid(row=1, column=0, columnspan=2, sticky="w", pady=(6, 2))

        self.package_mode_advanced_box = ttk.LabelFrame(
            self.package_mode_form_box, text="Advanced (Internal Tuning)", padding=8
        )
        self.package_mode_advanced_box.grid(row=2, column=0, columnspan=2, sticky="ew", pady=(0, 6))
        self.package_mode_advanced_box.columnconfigure(1, weight=1)
        for row, key in enumerate(PACKAGE_MODE_ADVANCED_FIELD_KEYS):
            label = PACKAGE_MODE_FORM_LABELS[key]
            ttk.Label(self.package_mode_advanced_box, text=label).grid(
                row=row, column=0, sticky="w", padx=(0, 8), pady=2
            )
            widget = self._create_package_mode_widget(self.package_mode_advanced_box, key)
            widget.grid(row=row, column=1, sticky="ew", pady=2)
            self.package_mode_form_widget_by_key[key] = widget
            self.package_mode_form_widgets.append(widget)
        self._toggle_package_mode_advanced()

        package_mode_form_actions = ttk.Frame(self.package_mode_form_box)
        package_mode_form_actions.grid(row=3, column=0, columnspan=2, sticky="ew", pady=(8, 2))
        package_mode_form_actions.columnconfigure(0, weight=1)
        package_mode_form_actions.columnconfigure(1, weight=1)
        ttk.Button(
            package_mode_form_actions,
            text="Load Record -> Form",
            command=self._load_package_mode_form_from_active,
        ).grid(row=0, column=0, sticky="ew", padx=(0, 4))
        ttk.Button(
            package_mode_form_actions,
            text="Apply Form -> Record",
            command=self._apply_package_mode_form_to_active,
        ).grid(row=0, column=1, sticky="ew", padx=(4, 0))

        ttk.Label(
            self.package_mode_form_box,
            textvariable=self.package_mode_form_status_var,
            foreground="#555555",
        ).grid(row=4, column=0, columnspan=2, sticky="w", pady=(4, 0))

        self._build_map_form(right)
        self._build_dialogue_form(right)
        self._build_script_form(right)
        self._build_item_form(right)
        self._build_page_form(right)
        self._build_pet_menu_slot_form(right)
        for domain in PROFILE_DOMAINS:
            self._build_profile_form(right, domain)

        ttk.Label(right, text="Edit JSON for selected record:").grid(row=2, column=0, sticky="w", pady=(2, 4))

        self.editor = tk.Text(right, wrap="none", undo=True)
        self.editor.grid(row=3, column=0, sticky="nsew")

        xscroll = ttk.Scrollbar(right, orient="horizontal", command=self.editor.xview)
        xscroll.grid(row=4, column=0, sticky="ew")
        yscroll = ttk.Scrollbar(right, orient="vertical", command=self.editor.yview)
        yscroll.grid(row=3, column=1, sticky="ns")
        self.editor.configure(xscrollcommand=xscroll.set, yscrollcommand=yscroll.set)

        actions = ttk.Frame(right)
        actions.grid(row=5, column=0, sticky="ew", pady=(8, 0))
        for col in range(4):
            actions.columnconfigure(col, weight=1)
        ttk.Button(actions, text="Apply Record", command=self._apply_record).grid(row=0, column=0, sticky="ew", padx=(0, 4))
        ttk.Button(actions, text="Save All", command=self._save_all).grid(row=0, column=1, sticky="ew", padx=4)
        ttk.Button(actions, text="Validate", command=self._validate).grid(row=0, column=2, sticky="ew", padx=4)
        ttk.Button(actions, text="Reload", command=self._reload).grid(row=0, column=3, sticky="ew", padx=(4, 0))

        self.status_var = tk.StringVar(value="")
        status = ttk.Label(self, textvariable=self.status_var, anchor="w")
        status.grid(row=1, column=0, columnspan=2, sticky="ew", padx=8, pady=(0, 8))
        self._set_entity_form_enabled(False)
        self._set_map_form_enabled(False)
        self._set_dialogue_form_enabled(False)
        self._set_script_form_enabled(False)
        self._set_item_form_enabled(False)
        self._set_package_mode_form_enabled(False)
        self._set_page_form_enabled(False)
        self._set_pet_menu_slot_form_enabled(False)
        for domain in PROFILE_DOMAINS:
            self._set_profile_form_enabled(domain, False)
        self._show_active_form(None, None)

    def _set_status(self, text: str) -> None:
        self.status_var.set(text)

    def _set_entity_form_enabled(self, enabled: bool) -> None:
        for widget in self.entity_form_widgets:
            if isinstance(widget, ttk.Combobox):
                widget.configure(state="readonly" if enabled else "disabled")
            else:
                widget.configure(state="normal" if enabled else "disabled")

    def _set_map_form_enabled(self, enabled: bool) -> None:
        for widget in self.map_form_widgets:
            widget.configure(state="normal" if enabled else "disabled")

    def _set_dialogue_form_enabled(self, enabled: bool) -> None:
        for widget in self.dialogue_form_widgets:
            widget.configure(state="normal" if enabled else "disabled")

    def _set_script_form_enabled(self, enabled: bool) -> None:
        for widget in self.script_form_widgets:
            widget.configure(state="normal" if enabled else "disabled")

    def _set_item_form_enabled(self, enabled: bool) -> None:
        for widget in self.item_form_widgets:
            if isinstance(widget, ttk.Combobox):
                widget.configure(state="readonly" if enabled else "disabled")
            else:
                widget.configure(state="normal" if enabled else "disabled")

    def _set_package_mode_form_enabled(self, enabled: bool) -> None:
        for widget in self.package_mode_form_widgets:
            if isinstance(widget, ttk.Combobox):
                widget.configure(state="readonly" if enabled else "disabled")
            else:
                widget.configure(state="normal" if enabled else "disabled")
        if self.package_mode_advanced_toggle is not None:
            self.package_mode_advanced_toggle.configure(state="normal" if enabled else "disabled")

    def _set_page_form_enabled(self, enabled: bool) -> None:
        for widget in self.page_form_widgets:
            if isinstance(widget, ttk.Combobox):
                widget.configure(state="readonly" if enabled else "disabled")
            else:
                widget.configure(state="normal" if enabled else "disabled")

    def _set_pet_menu_slot_form_enabled(self, enabled: bool) -> None:
        for widget in self.pet_menu_slot_form_widgets:
            if isinstance(widget, ttk.Combobox):
                widget.configure(state="readonly" if enabled else "disabled")
            else:
                widget.configure(state="normal" if enabled else "disabled")

    def _set_profile_form_enabled(self, domain: str, enabled: bool) -> None:
        for widget in self.profile_form_widgets.get(domain, []):
            widget.configure(state="normal" if enabled else "disabled")

    def _toggle_package_mode_advanced(self) -> None:
        if self.package_mode_advanced_box is None:
            return
        if self.package_mode_show_advanced_var.get():
            self.package_mode_advanced_box.grid()
        else:
            self.package_mode_advanced_box.grid_remove()

    def _create_package_mode_widget(self, parent: ttk.Widget, key: str) -> Any:
        if key == "map_ref":
            widget = ttk.Combobox(
                parent,
                textvariable=self.package_mode_form_vars[key],
                values=self._map_ref_values(),
                state="readonly",
            )
            return widget
        if key.endswith("_profile_ref"):
            profile_domain = self._profile_domain_for_mode_ref_key(key)
            widget = ttk.Combobox(
                parent,
                textvariable=self.package_mode_form_vars[key],
                values=self._profile_ref_values(profile_domain),
                state="readonly",
            )
            return widget
        enum_options = PACKAGE_MODE_ENUM_OPTIONS.get(key)
        if enum_options is not None:
            return ttk.Combobox(
                parent,
                textvariable=self.package_mode_form_vars[key],
                values=enum_options,
                state="readonly",
            )
        return ttk.Entry(parent, textvariable=self.package_mode_form_vars[key])

    def _profile_form_default_status(self, domain: str) -> str:
        return f"Select a {self._domain_label(domain).lower()} record to edit with form controls."

    def _build_profile_form(self, parent: ttk.Frame, domain: str) -> None:
        fields = PROFILE_FORM_FIELDS_BY_DOMAIN[domain]
        box = ttk.LabelFrame(parent, text=f"{self._domain_label(domain)} Form (v1)", padding=8)
        box.grid(row=1, column=0, sticky="ew", pady=(8, 8))
        box.columnconfigure(1, weight=1)
        self.profile_form_boxes[domain] = box

        for row, (key, label) in enumerate(fields):
            ttk.Label(box, text=label).grid(row=row, column=0, sticky="w", padx=(0, 8), pady=2)
            widget = ttk.Entry(box, textvariable=self.profile_form_vars[domain][key])
            widget.grid(row=row, column=1, sticky="ew", pady=2)
            self.profile_form_widgets[domain].append(widget)

        actions = ttk.Frame(box)
        actions.grid(row=len(fields), column=0, columnspan=2, sticky="ew", pady=(8, 2))
        actions.columnconfigure(0, weight=1)
        actions.columnconfigure(1, weight=1)
        ttk.Button(
            actions,
            text="Load Record -> Form",
            command=lambda d=domain: self._load_profile_form_from_active(d),
        ).grid(row=0, column=0, sticky="ew", padx=(0, 4))
        ttk.Button(
            actions,
            text="Apply Form -> Record",
            command=lambda d=domain: self._apply_profile_form_to_active(d),
        ).grid(row=0, column=1, sticky="ew", padx=(4, 0))

        ttk.Label(
            box,
            textvariable=self.profile_form_status_vars[domain],
            foreground="#555555",
        ).grid(row=len(fields) + 1, column=0, columnspan=2, sticky="w", pady=(4, 0))

    def _build_map_form(self, parent: ttk.Frame) -> None:
        self.map_form_box = ttk.LabelFrame(parent, text="Map Form (v1)", padding=8)
        self.map_form_box.grid(row=1, column=0, sticky="ew", pady=(8, 8))
        self.map_form_box.columnconfigure(1, weight=1)
        for row, (key, label) in enumerate(MAP_FORM_FIELDS):
            ttk.Label(self.map_form_box, text=label).grid(row=row, column=0, sticky="w", padx=(0, 8), pady=2)
            widget = ttk.Entry(self.map_form_box, textvariable=self.map_form_vars[key])
            widget.grid(row=row, column=1, sticky="ew", pady=2)
            self.map_form_widgets.append(widget)

        actions = ttk.Frame(self.map_form_box)
        actions.grid(row=len(MAP_FORM_FIELDS), column=0, columnspan=2, sticky="ew", pady=(8, 2))
        actions.columnconfigure(0, weight=1)
        actions.columnconfigure(1, weight=1)
        ttk.Button(actions, text="Load Record -> Form", command=self._load_map_form_from_active).grid(
            row=0, column=0, sticky="ew", padx=(0, 4)
        )
        ttk.Button(actions, text="Apply Form -> Record", command=self._apply_map_form_to_active).grid(
            row=0, column=1, sticky="ew", padx=(4, 0)
        )
        ttk.Label(
            self.map_form_box,
            textvariable=self.map_form_status_var,
            foreground="#555555",
        ).grid(row=len(MAP_FORM_FIELDS) + 1, column=0, columnspan=2, sticky="w", pady=(4, 0))

    def _build_dialogue_form(self, parent: ttk.Frame) -> None:
        self.dialogue_form_box = ttk.LabelFrame(parent, text="Dialogue Form (v1)", padding=8)
        self.dialogue_form_box.grid(row=1, column=0, sticky="ew", pady=(8, 8))
        self.dialogue_form_box.columnconfigure(1, weight=1)
        for row, (key, label) in enumerate(DIALOGUE_FORM_FIELDS):
            ttk.Label(self.dialogue_form_box, text=label).grid(row=row, column=0, sticky="w", padx=(0, 8), pady=2)
            widget = ttk.Entry(self.dialogue_form_box, textvariable=self.dialogue_form_vars[key])
            widget.grid(row=row, column=1, sticky="ew", pady=2)
            self.dialogue_form_widgets.append(widget)

        actions = ttk.Frame(self.dialogue_form_box)
        actions.grid(row=len(DIALOGUE_FORM_FIELDS), column=0, columnspan=2, sticky="ew", pady=(8, 2))
        actions.columnconfigure(0, weight=1)
        actions.columnconfigure(1, weight=1)
        ttk.Button(actions, text="Load Record -> Form", command=self._load_dialogue_form_from_active).grid(
            row=0, column=0, sticky="ew", padx=(0, 4)
        )
        ttk.Button(actions, text="Apply Form -> Record", command=self._apply_dialogue_form_to_active).grid(
            row=0, column=1, sticky="ew", padx=(4, 0)
        )
        ttk.Label(
            self.dialogue_form_box,
            textvariable=self.dialogue_form_status_var,
            foreground="#555555",
        ).grid(row=len(DIALOGUE_FORM_FIELDS) + 1, column=0, columnspan=2, sticky="w", pady=(4, 0))

    def _build_script_form(self, parent: ttk.Frame) -> None:
        self.script_form_box = ttk.LabelFrame(parent, text="Script Form (v1)", padding=8)
        self.script_form_box.grid(row=1, column=0, sticky="ew", pady=(8, 8))
        self.script_form_box.columnconfigure(1, weight=1)
        for row, (key, label) in enumerate(SCRIPT_FORM_FIELDS):
            ttk.Label(self.script_form_box, text=label).grid(row=row, column=0, sticky="w", padx=(0, 8), pady=2)
            widget = ttk.Entry(self.script_form_box, textvariable=self.script_form_vars[key])
            widget.grid(row=row, column=1, sticky="ew", pady=2)
            self.script_form_widgets.append(widget)

        actions = ttk.Frame(self.script_form_box)
        actions.grid(row=len(SCRIPT_FORM_FIELDS), column=0, columnspan=2, sticky="ew", pady=(8, 2))
        actions.columnconfigure(0, weight=1)
        actions.columnconfigure(1, weight=1)
        ttk.Button(actions, text="Load Record -> Form", command=self._load_script_form_from_active).grid(
            row=0, column=0, sticky="ew", padx=(0, 4)
        )
        ttk.Button(actions, text="Apply Form -> Record", command=self._apply_script_form_to_active).grid(
            row=0, column=1, sticky="ew", padx=(4, 0)
        )
        ttk.Label(
            self.script_form_box,
            textvariable=self.script_form_status_var,
            foreground="#555555",
        ).grid(row=len(SCRIPT_FORM_FIELDS) + 1, column=0, columnspan=2, sticky="w", pady=(4, 0))

    def _build_item_form(self, parent: ttk.Frame) -> None:
        self.item_form_box = ttk.LabelFrame(parent, text="Item Form (v1)", padding=8)
        self.item_form_box.grid(row=1, column=0, sticky="ew", pady=(8, 8))
        self.item_form_box.columnconfigure(1, weight=1)
        for row, (key, label) in enumerate(ITEM_FORM_FIELDS):
            ttk.Label(self.item_form_box, text=label).grid(row=row, column=0, sticky="w", padx=(0, 8), pady=2)
            if key == "category":
                widget: Any = ttk.Combobox(
                    self.item_form_box,
                    textvariable=self.item_form_vars[key],
                    values=ITEM_CATEGORIES,
                    state="readonly",
                )
            else:
                widget = ttk.Entry(self.item_form_box, textvariable=self.item_form_vars[key])
            widget.grid(row=row, column=1, sticky="ew", pady=2)
            self.item_form_widgets.append(widget)

        actions = ttk.Frame(self.item_form_box)
        actions.grid(row=len(ITEM_FORM_FIELDS), column=0, columnspan=2, sticky="ew", pady=(8, 2))
        actions.columnconfigure(0, weight=1)
        actions.columnconfigure(1, weight=1)
        ttk.Button(actions, text="Load Record -> Form", command=self._load_item_form_from_active).grid(
            row=0, column=0, sticky="ew", padx=(0, 4)
        )
        ttk.Button(actions, text="Apply Form -> Record", command=self._apply_item_form_to_active).grid(
            row=0, column=1, sticky="ew", padx=(4, 0)
        )
        ttk.Label(
            self.item_form_box,
            textvariable=self.item_form_status_var,
            foreground="#555555",
        ).grid(row=len(ITEM_FORM_FIELDS) + 1, column=0, columnspan=2, sticky="w", pady=(4, 0))

    def _build_page_form(self, parent: ttk.Frame) -> None:
        self.page_form_box = ttk.LabelFrame(parent, text="Page Form (v1)", padding=8)
        self.page_form_box.grid(row=1, column=0, sticky="ew", pady=(8, 8))
        self.page_form_box.columnconfigure(1, weight=1)

        for row, (key, label) in enumerate(PAGE_FORM_FIELDS):
            ttk.Label(self.page_form_box, text=label).grid(row=row, column=0, sticky="w", padx=(0, 8), pady=2)
            if key == "route_kind":
                widget: Any = ttk.Combobox(
                    self.page_form_box,
                    textvariable=self.page_form_vars[key],
                    values=PAGE_ROUTE_KIND_OPTIONS,
                    state="readonly",
                )
            elif key == "native_page_key":
                widget = ttk.Combobox(
                    self.page_form_box,
                    textvariable=self.page_form_vars[key],
                    values=PAGE_NATIVE_PAGE_OPTIONS,
                    state="readonly",
                )
            elif key == "native_tree_key":
                widget = ttk.Combobox(
                    self.page_form_box,
                    textvariable=self.page_form_vars[key],
                    values=PAGE_NATIVE_TREE_OPTIONS,
                    state="readonly",
                )
            else:
                widget = ttk.Entry(self.page_form_box, textvariable=self.page_form_vars[key])
            widget.grid(row=row, column=1, sticky="ew", pady=2)
            self.page_form_widgets.append(widget)
            self.page_form_widget_by_key[key] = widget

        actions = ttk.Frame(self.page_form_box)
        actions.grid(row=len(PAGE_FORM_FIELDS), column=0, columnspan=2, sticky="ew", pady=(8, 2))
        actions.columnconfigure(0, weight=1)
        actions.columnconfigure(1, weight=1)
        ttk.Button(
            actions,
            text="Load Record -> Form",
            command=self._load_page_form_from_active,
        ).grid(row=0, column=0, sticky="ew", padx=(0, 4))
        ttk.Button(
            actions,
            text="Apply Form -> Record",
            command=self._apply_page_form_to_active,
        ).grid(row=0, column=1, sticky="ew", padx=(4, 0))

        ttk.Label(
            self.page_form_box,
            textvariable=self.page_form_status_var,
            foreground="#555555",
        ).grid(row=len(PAGE_FORM_FIELDS) + 1, column=0, columnspan=2, sticky="w", pady=(4, 0))

    def _build_pet_menu_slot_form(self, parent: ttk.Frame) -> None:
        self.pet_menu_slot_form_box = ttk.LabelFrame(parent, text="Pet Menu Slot Form (v1)", padding=8)
        self.pet_menu_slot_form_box.grid(row=1, column=0, sticky="ew", pady=(8, 8))
        self.pet_menu_slot_form_box.columnconfigure(1, weight=1)

        for row, (key, label) in enumerate(PET_MENU_SLOT_FORM_FIELDS):
            ttk.Label(self.pet_menu_slot_form_box, text=label).grid(
                row=row, column=0, sticky="w", padx=(0, 8), pady=2
            )
            if key in PET_MENU_SLOT_ENUM_OPTIONS:
                widget = ttk.Combobox(
                    self.pet_menu_slot_form_box,
                    textvariable=self.pet_menu_slot_form_vars[key],
                    values=PET_MENU_SLOT_ENUM_OPTIONS[key],
                    state="readonly",
                )
            elif key == "mode_ref":
                widget = ttk.Combobox(
                    self.pet_menu_slot_form_box,
                    textvariable=self.pet_menu_slot_form_vars[key],
                    values=self._mode_ref_values(),
                    state="readonly",
                )
            elif key == "page_ref":
                widget = ttk.Combobox(
                    self.pet_menu_slot_form_box,
                    textvariable=self.pet_menu_slot_form_vars[key],
                    values=self._page_ref_values(),
                    state="readonly",
                )
            else:
                widget = ttk.Entry(self.pet_menu_slot_form_box, textvariable=self.pet_menu_slot_form_vars[key])
            widget.grid(row=row, column=1, sticky="ew", pady=2)
            self.pet_menu_slot_form_widgets.append(widget)
            self.pet_menu_slot_form_widget_by_key[key] = widget

        actions = ttk.Frame(self.pet_menu_slot_form_box)
        actions.grid(row=len(PET_MENU_SLOT_FORM_FIELDS), column=0, columnspan=2, sticky="ew", pady=(8, 2))
        actions.columnconfigure(0, weight=1)
        actions.columnconfigure(1, weight=1)
        ttk.Button(
            actions,
            text="Load Record -> Form",
            command=self._load_pet_menu_slot_form_from_active,
        ).grid(row=0, column=0, sticky="ew", padx=(0, 4))
        ttk.Button(
            actions,
            text="Apply Form -> Record",
            command=self._apply_pet_menu_slot_form_to_active,
        ).grid(row=0, column=1, sticky="ew", padx=(4, 0))

        ttk.Label(
            self.pet_menu_slot_form_box,
            textvariable=self.pet_menu_slot_form_status_var,
            foreground="#555555",
        ).grid(row=len(PET_MENU_SLOT_FORM_FIELDS) + 1, column=0, columnspan=2, sticky="w", pady=(4, 0))

    def _parse_form_int_value(self, raw_value: str) -> int | None:
        token = raw_value.strip()
        if token == "":
            return None
        try:
            return int(token, 10)
        except ValueError:
            l_paren = token.rfind("(")
            r_paren = token.rfind(")")
            if l_paren >= 0 and r_paren > l_paren:
                maybe_num = token[l_paren + 1 : r_paren].strip()
                try:
                    return int(maybe_num, 10)
                except ValueError:
                    return None
            return None

    def _show_active_form(self, domain: str | None, idx: int | None) -> None:
        active_key = domain if idx is not None else None
        form_by_domain = {
            "maps": self.map_form_box,
            "dialogues": self.dialogue_form_box,
            "scripts": self.script_form_box,
            "items": self.item_form_box,
            "entities": self.entity_form_box,
            PACKAGE_MODES_DOMAIN: self.package_mode_form_box,
            "pages": self.page_form_box,
            "pet_menu_slots": self.pet_menu_slot_form_box,
        }
        for profile_domain, profile_box in self.profile_form_boxes.items():
            form_by_domain[profile_domain] = profile_box
        for form_domain, form_box in form_by_domain.items():
            if form_box is None:
                continue
            if active_key == form_domain:
                form_box.grid()
            else:
                form_box.grid_remove()

    def _domain_label(self, domain: str) -> str:
        return DOMAIN_LABELS.get(domain, domain)

    def _maps_by_asset_id(self) -> dict[int, dict[str, Any]]:
        out: dict[int, dict[str, Any]] = {}
        for rec in self.domain_data.get("maps", []):
            map_asset_id = rec.get("map_asset_id")
            if isinstance(map_asset_id, int) and map_asset_id > 0:
                out[map_asset_id] = rec
        return out

    def _map_ref_values(self) -> list[str]:
        values: list[str] = []
        for map_asset_id, rec in self._maps_by_asset_id().items():
            display_name = str(rec.get("display_name", "")).strip()
            rec_id = str(rec.get("id", "")).strip()
            map_name = display_name or rec_id or f"Map {map_asset_id}"
            values.append(f"{map_name} ({map_asset_id})")
        return values

    def _map_ref_label_for_asset_id(self, scene_map_id: int | None) -> str:
        if not isinstance(scene_map_id, int) or scene_map_id <= 0:
            return ""
        rec = self._maps_by_asset_id().get(scene_map_id)
        if rec is None:
            return f"Unknown Map ({scene_map_id})"
        display_name = str(rec.get("display_name", "")).strip()
        rec_id = str(rec.get("id", "")).strip()
        map_name = display_name or rec_id or f"Map {scene_map_id}"
        return f"{map_name} ({scene_map_id})"

    def _map_from_ref_label(self, label: str) -> dict[str, Any] | None:
        token = label.strip()
        if token == "":
            return None
        l_paren = token.rfind("(")
        r_paren = token.rfind(")")
        if l_paren >= 0 and r_paren > l_paren:
            maybe_num = token[l_paren + 1 : r_paren].strip()
            try:
                map_asset_id = int(maybe_num, 10)
            except ValueError:
                map_asset_id = 0
            if map_asset_id > 0:
                return self._maps_by_asset_id().get(map_asset_id)
        for rec in self._maps_by_asset_id().values():
            display_name = str(rec.get("display_name", "")).strip()
            rec_id = str(rec.get("id", "")).strip()
            if token == display_name or token == rec_id:
                return rec
        return None

    def _profile_domain_for_mode_ref_key(self, ref_key: str) -> str:
        mapping = {
            "controller_profile_ref": "controller_profiles",
            "camera_profile_ref": "camera_profiles",
            "input_profile_ref": "input_profiles",
        }
        return mapping.get(ref_key, "")

    def _profiles_by_profile_id(self, domain: str) -> dict[int, dict[str, Any]]:
        out: dict[int, dict[str, Any]] = {}
        for rec in self.domain_data.get(domain, []):
            profile_id = rec.get("profile_id")
            if isinstance(profile_id, int) and profile_id > 0:
                out[profile_id] = rec
        return out

    def _profile_ref_values(self, domain: str) -> list[str]:
        values: list[str] = []
        for profile_id, rec in self._profiles_by_profile_id(domain).items():
            display_name = str(rec.get("display_name", "")).strip()
            rec_id = str(rec.get("id", "")).strip()
            profile_name = display_name or rec_id or f"Profile {profile_id}"
            values.append(f"{profile_name} ({profile_id})")
        return values

    def _modes_by_key(self) -> dict[str, dict[str, Any]]:
        out: dict[str, dict[str, Any]] = {}
        for rec in self.domain_data.get(PACKAGE_MODES_DOMAIN, []):
            mode_key = str(rec.get("id", "")).strip()
            if mode_key != "":
                out[mode_key] = rec
        return out

    def _mode_ref_values(self) -> list[str]:
        values: list[str] = []
        for mode_key, rec in self._modes_by_key().items():
            display_name = str(rec.get("display_name", "")).strip()
            mode_name = display_name or mode_key
            values.append(f"{mode_name} ({mode_key})")
        return values

    def _mode_ref_label_for_key(self, mode_key: str | None) -> str:
        key = "" if mode_key is None else str(mode_key).strip()
        if key == "":
            return ""
        rec = self._modes_by_key().get(key)
        if rec is None:
            return f"Unknown Mode ({key})"
        display_name = str(rec.get("display_name", "")).strip()
        mode_name = display_name or key
        return f"{mode_name} ({key})"

    def _mode_from_ref_label(self, label: str) -> dict[str, Any] | None:
        token = label.strip()
        if token == "":
            return None
        l_paren = token.rfind("(")
        r_paren = token.rfind(")")
        if l_paren >= 0 and r_paren > l_paren:
            mode_key = token[l_paren + 1 : r_paren].strip()
            rec = self._modes_by_key().get(mode_key)
            if rec is not None:
                return rec
        for mode_key, rec in self._modes_by_key().items():
            display_name = str(rec.get("display_name", "")).strip()
            if token == display_name or token == mode_key:
                return rec
        return None

    def _pages_by_key(self) -> dict[str, dict[str, Any]]:
        out: dict[str, dict[str, Any]] = {}
        for rec in self.domain_data.get("pages", []):
            page_key = str(rec.get("id", "")).strip()
            if page_key != "":
                out[page_key] = rec
        return out

    def _page_ref_values(self) -> list[str]:
        values: list[str] = []
        for page_key, rec in self._pages_by_key().items():
            display_name = str(rec.get("display_name", "")).strip()
            page_name = display_name or page_key
            values.append(f"{page_name} ({page_key})")
        return values

    def _page_ref_label_for_key(self, page_key: str | None) -> str:
        key = "" if page_key is None else str(page_key).strip()
        if key == "":
            return ""
        rec = self._pages_by_key().get(key)
        if rec is None:
            return f"Unknown Page ({key})"
        display_name = str(rec.get("display_name", "")).strip()
        page_name = display_name or key
        return f"{page_name} ({key})"

    def _page_from_ref_label(self, label: str) -> dict[str, Any] | None:
        token = label.strip()
        if token == "":
            return None
        l_paren = token.rfind("(")
        r_paren = token.rfind(")")
        if l_paren >= 0 and r_paren > l_paren:
            page_key = token[l_paren + 1 : r_paren].strip()
            rec = self._pages_by_key().get(page_key)
            if rec is not None:
                return rec
        for page_key, rec in self._pages_by_key().items():
            display_name = str(rec.get("display_name", "")).strip()
            if token == display_name or token == page_key:
                return rec
        return None

    def _profile_ref_label_for_id(self, domain: str, profile_id: int | None) -> str:
        if not isinstance(profile_id, int) or profile_id <= 0:
            return ""
        rec = self._profiles_by_profile_id(domain).get(profile_id)
        if rec is None:
            return f"Unknown Profile ({profile_id})"
        display_name = str(rec.get("display_name", "")).strip()
        rec_id = str(rec.get("id", "")).strip()
        profile_name = display_name or rec_id or f"Profile {profile_id}"
        return f"{profile_name} ({profile_id})"

    def _profile_from_ref_label(self, domain: str, label: str) -> dict[str, Any] | None:
        token = label.strip()
        if token == "":
            return None
        l_paren = token.rfind("(")
        r_paren = token.rfind(")")
        if l_paren >= 0 and r_paren > l_paren:
            maybe_num = token[l_paren + 1 : r_paren].strip()
            try:
                profile_id = int(maybe_num, 10)
            except ValueError:
                profile_id = 0
            if profile_id > 0:
                return self._profiles_by_profile_id(domain).get(profile_id)
        for rec in self._profiles_by_profile_id(domain).values():
            display_name = str(rec.get("display_name", "")).strip()
            rec_id = str(rec.get("id", "")).strip()
            if token == display_name or token == rec_id:
                return rec
        return None

    def _mode_profile_key_field(self, profile_domain: str) -> str:
        mapping = {
            "controller_profiles": "controller_profile_key",
            "camera_profiles": "camera_profile_key",
            "input_profiles": "input_profile_key",
        }
        return mapping.get(profile_domain, "")

    def _mode_profile_id_field(self, profile_domain: str) -> str:
        mapping = {
            "controller_profiles": "controller_profile_id",
            "camera_profiles": "camera_profile_id",
            "input_profiles": "",
        }
        return mapping.get(profile_domain, "")

    def _clear_mode_profile_derived_fields(self, mode_record: dict[str, Any], profile_domain: str) -> None:
        for field in PROFILE_MODE_SYNC_FIELDS.get(profile_domain, ()):
            mode_record.pop(field, None)
        id_field = self._mode_profile_id_field(profile_domain)
        if id_field != "":
            mode_record.pop(id_field, None)

    def _set_mode_profile_reference(
        self, mode_record: dict[str, Any], profile_domain: str, profile_rec: dict[str, Any]
    ) -> None:
        key_field = self._mode_profile_key_field(profile_domain)
        profile_key = str(profile_rec.get("id", "")).strip()
        if key_field != "" and profile_key != "":
            mode_record[key_field] = profile_key
        self._clear_mode_profile_derived_fields(mode_record, profile_domain)

    def _record_tree_label(self, domain: str, record: dict[str, Any], idx: int) -> str:
        if domain == PACKAGE_MODES_DOMAIN:
            mode_id = record.get("mode_id")
            mode_key = str(record.get("id", "")).strip()
            if mode_key == "":
                mode_key = f"mode_{mode_id}" if isinstance(mode_id, int) else f"mode_{idx + 1}"
            display_name = str(record.get("display_name", "")).strip()
            if display_name == "":
                display_name = f"Mode {mode_id}" if isinstance(mode_id, int) else f"Mode {idx + 1}"
            lifecycle = record.get("scene_lifecycle")
            lifecycle_label = ""
            if lifecycle == 1:
                lifecycle_label = " [Persistent]"
            elif lifecycle == 2:
                lifecycle_label = " [Transient]"
            runtime_kind = record.get("runtime_kind")
            runtime_label = ""
            if runtime_kind == 1:
                runtime_label = " [Realtime]"
            elif runtime_kind == 2:
                runtime_label = " [Static]"
            return f"{display_name} ({mode_key}){runtime_label}{lifecycle_label}"

        rec_id = str(record.get("id", "")).strip() or f"{domain}_{idx + 1}"
        display_name = str(record.get("display_name", "")).strip()
        if display_name == "":
            display_name = str(record.get("name", "")).strip()
        if display_name and display_name != rec_id:
            return f"{display_name} ({rec_id})"
        return rec_id

    def _clear_entity_form(self) -> None:
        for key, _label in ENTITY_FORM_FIELDS:
            self.entity_form_vars[key].set("")

    def _clear_map_form(self) -> None:
        for key, _label in MAP_FORM_FIELDS:
            self.map_form_vars[key].set("")

    def _clear_dialogue_form(self) -> None:
        for key, _label in DIALOGUE_FORM_FIELDS:
            self.dialogue_form_vars[key].set("")

    def _clear_script_form(self) -> None:
        for key, _label in SCRIPT_FORM_FIELDS:
            self.script_form_vars[key].set("")

    def _clear_item_form(self) -> None:
        for key, _label in ITEM_FORM_FIELDS:
            self.item_form_vars[key].set("")

    def _clear_package_mode_form(self) -> None:
        for key, _label in PACKAGE_MODE_FORM_FIELDS:
            self.package_mode_form_vars[key].set("")

    def _clear_page_form(self) -> None:
        for key, _label in PAGE_FORM_FIELDS:
            self.page_form_vars[key].set("")

    def _clear_pet_menu_slot_form(self) -> None:
        for key, _label in PET_MENU_SLOT_FORM_FIELDS:
            self.pet_menu_slot_form_vars[key].set("")

    def _clear_profile_form(self, domain: str) -> None:
        for key, _label in PROFILE_FORM_FIELDS_BY_DOMAIN[domain]:
            self.profile_form_vars[domain][key].set("")

    def _refresh_entity_form_for_selection(self, domain: str | None, idx: int | None) -> None:
        if domain != "entities" or idx is None:
            self._clear_entity_form()
            self._set_entity_form_enabled(False)
            self.entity_form_status_var.set("Select an entity record to edit with form controls.")
            return
        records = self.domain_data.get("entities", [])
        if idx < 0 or idx >= len(records):
            self._clear_entity_form()
            self._set_entity_form_enabled(False)
            self.entity_form_status_var.set("Select an entity record to edit with form controls.")
            return
        record = records[idx]
        for key, _label in ENTITY_FORM_FIELDS:
            value = record.get(key, "")
            self.entity_form_vars[key].set("" if value is None else str(value))
        self._set_entity_form_enabled(True)
        self.entity_form_status_var.set("Entity form is synced to selected record.")

    def _refresh_map_form_for_selection(self, domain: str | None, idx: int | None) -> None:
        if domain != "maps" or idx is None:
            self._clear_map_form()
            self._set_map_form_enabled(False)
            self.map_form_status_var.set("Select a map record to edit with form controls.")
            return
        records = self.domain_data.get("maps", [])
        if idx < 0 or idx >= len(records):
            self._clear_map_form()
            self._set_map_form_enabled(False)
            self.map_form_status_var.set("Select a map record to edit with form controls.")
            return
        record = records[idx]
        for key, _label in MAP_FORM_FIELDS:
            value = record.get(key, "")
            self.map_form_vars[key].set("" if value is None else str(value))
        self._set_map_form_enabled(True)
        self.map_form_status_var.set("Map form is synced to selected record.")

    def _refresh_dialogue_form_for_selection(self, domain: str | None, idx: int | None) -> None:
        if domain != "dialogues" or idx is None:
            self._clear_dialogue_form()
            self._set_dialogue_form_enabled(False)
            self.dialogue_form_status_var.set("Select a dialogue record to edit with form controls.")
            return
        records = self.domain_data.get("dialogues", [])
        if idx < 0 or idx >= len(records):
            self._clear_dialogue_form()
            self._set_dialogue_form_enabled(False)
            self.dialogue_form_status_var.set("Select a dialogue record to edit with form controls.")
            return
        record = records[idx]
        for key, _label in DIALOGUE_FORM_FIELDS:
            value = record.get(key, "")
            self.dialogue_form_vars[key].set("" if value is None else str(value))
        self._set_dialogue_form_enabled(True)
        self.dialogue_form_status_var.set("Dialogue form is synced to selected record.")

    def _refresh_script_form_for_selection(self, domain: str | None, idx: int | None) -> None:
        if domain != "scripts" or idx is None:
            self._clear_script_form()
            self._set_script_form_enabled(False)
            self.script_form_status_var.set("Select a script record to edit with form controls.")
            return
        records = self.domain_data.get("scripts", [])
        if idx < 0 or idx >= len(records):
            self._clear_script_form()
            self._set_script_form_enabled(False)
            self.script_form_status_var.set("Select a script record to edit with form controls.")
            return
        record = records[idx]
        for key, _label in SCRIPT_FORM_FIELDS:
            value = record.get(key, "")
            self.script_form_vars[key].set("" if value is None else str(value))
        self._set_script_form_enabled(True)
        self.script_form_status_var.set("Script form is synced to selected record.")

    def _refresh_item_form_for_selection(self, domain: str | None, idx: int | None) -> None:
        if domain != "items" or idx is None:
            self._clear_item_form()
            self._set_item_form_enabled(False)
            self.item_form_status_var.set("Select an item record to edit with form controls.")
            return
        records = self.domain_data.get("items", [])
        if idx < 0 or idx >= len(records):
            self._clear_item_form()
            self._set_item_form_enabled(False)
            self.item_form_status_var.set("Select an item record to edit with form controls.")
            return
        record = records[idx]
        for key, _label in ITEM_FORM_FIELDS:
            value = record.get(key, "")
            self.item_form_vars[key].set("" if value is None else str(value))
        self._set_item_form_enabled(True)
        self.item_form_status_var.set("Item form is synced to selected record.")

    def _refresh_package_mode_form_for_selection(self, domain: str | None, idx: int | None) -> None:
        for key in ("map_ref", "controller_profile_ref", "camera_profile_ref", "input_profile_ref"):
            widget = self.package_mode_form_widget_by_key.get(key)
            if not isinstance(widget, ttk.Combobox):
                continue
            if key == "map_ref":
                widget.configure(values=self._map_ref_values())
            else:
                profile_domain = self._profile_domain_for_mode_ref_key(key)
                widget.configure(values=self._profile_ref_values(profile_domain))

        if domain != PACKAGE_MODES_DOMAIN or idx is None:
            self._clear_package_mode_form()
            self._set_package_mode_form_enabled(False)
            self.package_mode_form_status_var.set(
                "Select a game mode record to edit with form controls."
            )
            return
        records = self.domain_data.get(PACKAGE_MODES_DOMAIN, [])
        if idx < 0 or idx >= len(records):
            self._clear_package_mode_form()
            self._set_package_mode_form_enabled(False)
            self.package_mode_form_status_var.set(
                "Select a game mode record to edit with form controls."
            )
            return
        record = records[idx]
        mode_id = record.get("mode_id")
        for key, _label in PACKAGE_MODE_FORM_FIELDS:
            if key == "map_ref":
                value = self._map_ref_label_for_asset_id(record.get("scene_map_id"))
            elif key == "controller_profile_ref":
                value = ""
                profile_key = str(record.get("controller_profile_key", "")).strip()
                if profile_key:
                    rec = self.domain_data.get("controller_profiles", [])
                    match = next((x for x in rec if str(x.get("id", "")).strip() == profile_key), None)
                    if isinstance(match, dict):
                        value = self._profile_ref_label_for_id("controller_profiles", match.get("profile_id"))
                if value == "":
                    value = self._profile_ref_label_for_id(
                        "controller_profiles", record.get("controller_profile_id")
                    )
            elif key == "camera_profile_ref":
                value = ""
                profile_key = str(record.get("camera_profile_key", "")).strip()
                if profile_key:
                    rec = self.domain_data.get("camera_profiles", [])
                    match = next((x for x in rec if str(x.get("id", "")).strip() == profile_key), None)
                    if isinstance(match, dict):
                        value = self._profile_ref_label_for_id("camera_profiles", match.get("profile_id"))
                if value == "":
                    value = self._profile_ref_label_for_id("camera_profiles", record.get("camera_profile_id"))
            elif key == "input_profile_ref":
                value = ""
                profile_key = str(record.get("input_profile_key", "")).strip()
                if profile_key:
                    rec = self.domain_data.get("input_profiles", [])
                    match = next((x for x in rec if str(x.get("id", "")).strip() == profile_key), None)
                    if isinstance(match, dict):
                        value = self._profile_ref_label_for_id("input_profiles", match.get("profile_id"))
                if value == "":
                    deadzone = record.get("input_deadzone_permille")
                    flags = record.get("input_flags")
                    for input_rec in self.domain_data.get("input_profiles", []):
                        if (
                            isinstance(input_rec, dict)
                            and input_rec.get("input_deadzone_permille") == deadzone
                            and input_rec.get("input_flags") == flags
                        ):
                            value = self._profile_ref_label_for_id(
                                "input_profiles", input_rec.get("profile_id")
                            )
                            break
            elif key == "id":
                value = record.get(key, "")
                if (value is None or str(value).strip() == "") and isinstance(mode_id, int):
                    value = f"mode_{mode_id}"
            elif key == "display_name":
                value = record.get(key, "")
                if (value is None or str(value).strip() == "") and isinstance(mode_id, int):
                    value = f"Mode {mode_id}"
            elif key in PACKAGE_MODE_TEXT_FIELDS:
                value = record.get(key, "")
            else:
                value = record.get(key, 0)
                if isinstance(value, int):
                    value = PACKAGE_MODE_ENUM_VALUE_LABELS.get(key, {}).get(value, value)
            self.package_mode_form_vars[key].set("" if value is None else str(value))
        self._set_package_mode_form_enabled(True)
        self.package_mode_form_status_var.set("Game mode form is synced to selected record.")

    def _refresh_profile_forms_for_selection(self, domain: str | None, idx: int | None) -> None:
        for profile_domain in PROFILE_DOMAINS:
            if domain != profile_domain or idx is None:
                self._clear_profile_form(profile_domain)
                self._set_profile_form_enabled(profile_domain, False)
                self.profile_form_status_vars[profile_domain].set(
                    self._profile_form_default_status(profile_domain)
                )
                continue
            records = self.domain_data.get(profile_domain, [])
            if idx < 0 or idx >= len(records):
                self._clear_profile_form(profile_domain)
                self._set_profile_form_enabled(profile_domain, False)
                self.profile_form_status_vars[profile_domain].set(
                    self._profile_form_default_status(profile_domain)
                )
                continue
            record = records[idx]
            for key, _label in PROFILE_FORM_FIELDS_BY_DOMAIN[profile_domain]:
                value = record.get(key, "")
                self.profile_form_vars[profile_domain][key].set("" if value is None else str(value))
            self._set_profile_form_enabled(profile_domain, True)
            self.profile_form_status_vars[profile_domain].set(
                f"{self._domain_label(profile_domain)} form is synced to selected record."
            )

    def _refresh_page_form_for_selection(self, domain: str | None, idx: int | None) -> None:
        if domain != "pages" or idx is None:
            self._clear_page_form()
            self._set_page_form_enabled(False)
            self.page_form_status_var.set("Select a page record to edit with form controls.")
            return
        records = self.domain_data.get("pages", [])
        if idx < 0 or idx >= len(records):
            self._clear_page_form()
            self._set_page_form_enabled(False)
            self.page_form_status_var.set("Select a page record to edit with form controls.")
            return
        record = records[idx]
        for key, _label in PAGE_FORM_FIELDS:
            value = record.get(key, "")
            self.page_form_vars[key].set("" if value is None else str(value))
        self._set_page_form_enabled(True)
        self.page_form_status_var.set("Page form is synced to selected record.")

    def _refresh_pet_menu_slot_form_for_selection(self, domain: str | None, idx: int | None) -> None:
        for key in ("mode_ref", "page_ref"):
            widget = self.pet_menu_slot_form_widget_by_key.get(key)
            if not isinstance(widget, ttk.Combobox):
                continue
            if key == "mode_ref":
                widget.configure(values=self._mode_ref_values())
            else:
                widget.configure(values=self._page_ref_values())

        if domain != "pet_menu_slots" or idx is None:
            self._clear_pet_menu_slot_form()
            self._set_pet_menu_slot_form_enabled(False)
            self.pet_menu_slot_form_status_var.set(
                "Select a pet menu slot record to edit with form controls."
            )
            return
        records = self.domain_data.get("pet_menu_slots", [])
        if idx < 0 or idx >= len(records):
            self._clear_pet_menu_slot_form()
            self._set_pet_menu_slot_form_enabled(False)
            self.pet_menu_slot_form_status_var.set(
                "Select a pet menu slot record to edit with form controls."
            )
            return
        record = records[idx]
        for key, _label in PET_MENU_SLOT_FORM_FIELDS:
            if key == "mode_ref":
                value = self._mode_ref_label_for_key(record.get("mode_key"))
            elif key == "page_ref":
                value = self._page_ref_label_for_key(record.get("page_key"))
            else:
                value = record.get(key, "")
            self.pet_menu_slot_form_vars[key].set("" if value is None else str(value))
        self._set_pet_menu_slot_form_enabled(True)
        self.pet_menu_slot_form_status_var.set("Pet menu slot form is synced to selected record.")

    def _refresh_forms_for_selection(self, domain: str | None, idx: int | None) -> None:
        self._show_active_form(domain, idx)
        self._refresh_map_form_for_selection(domain, idx)
        self._refresh_dialogue_form_for_selection(domain, idx)
        self._refresh_script_form_for_selection(domain, idx)
        self._refresh_item_form_for_selection(domain, idx)
        self._refresh_entity_form_for_selection(domain, idx)
        self._refresh_package_mode_form_for_selection(domain, idx)
        self._refresh_page_form_for_selection(domain, idx)
        self._refresh_pet_menu_slot_form_for_selection(domain, idx)
        self._refresh_profile_forms_for_selection(domain, idx)

    def _rebuild_tree_and_select(self, domain: str | None, idx: int | None) -> None:
        self._suspend_tree_select = True
        try:
            self._rebuild_tree()
            if domain is not None and idx is not None:
                token = f"record:{domain}:{idx}"
                self.tree.selection_set(token)
                self.tree.focus(token)
        finally:
            self._suspend_tree_select = False

    def _load_entity_form_from_active(self) -> None:
        if self.active_domain != "entities" or self.active_index is None:
            self._set_status("Select an entity record before loading the form.")
            return
        raw = self.editor.get("1.0", tk.END).strip()
        if raw == "":
            messagebox.showerror("Invalid record", "Record JSON cannot be empty.")
            return
        try:
            parsed = json.loads(raw)
        except Exception as exc:
            messagebox.showerror("Invalid JSON", f"Could not parse record JSON:\n{exc}")
            return
        if not isinstance(parsed, dict):
            messagebox.showerror("Invalid record", "Record root must be a JSON object.")
            return
        self.domain_data["entities"][self.active_index] = parsed
        self._refresh_entity_form_for_selection("entities", self.active_index)
        self._set_status("Loaded active entity record into form.")

    def _apply_entity_form_to_active(self) -> None:
        if self.active_domain != "entities" or self.active_index is None:
            self._set_status("Select an entity record before applying form values.")
            return
        records = self.domain_data.get("entities", [])
        idx = self.active_index
        if idx < 0 or idx >= len(records):
            self._set_status("Selected entity record is out of range.")
            return

        updated = dict(records[idx])
        for key, _label in ENTITY_FORM_FIELDS:
            updated[key] = self.entity_form_vars[key].get().strip()

        rec_id = str(updated.get("id", "")).strip()
        if rec_id == "":
            messagebox.showerror("Invalid entity", "Entity 'id' cannot be empty.")
            return
        for other_ix, other in enumerate(records):
            if other_ix == idx:
                continue
            if str(other.get("id", "")).strip() == rec_id:
                messagebox.showerror("Duplicate entity ID", f"Entity id '{rec_id}' is already in use.")
                return

        records[idx] = updated
        self._rebuild_tree_and_select("entities", idx)
        self._load_record_into_editor("entities", idx)
        self._refresh_entity_form_for_selection("entities", idx)
        self._set_status("Applied entity form values to record.")

    def _load_package_mode_form_from_active(self) -> None:
        if self.active_domain != PACKAGE_MODES_DOMAIN or self.active_index is None:
            self._set_status("Select a game mode record before loading the form.")
            return
        raw = self.editor.get("1.0", tk.END).strip()
        if raw == "":
            messagebox.showerror("Invalid record", "Record JSON cannot be empty.")
            return
        try:
            parsed = json.loads(raw)
        except Exception as exc:
            messagebox.showerror("Invalid JSON", f"Could not parse record JSON:\n{exc}")
            return
        if not isinstance(parsed, dict):
            messagebox.showerror("Invalid record", "Record root must be a JSON object.")
            return
        self.domain_data[PACKAGE_MODES_DOMAIN][self.active_index] = parsed
        self._refresh_package_mode_form_for_selection(PACKAGE_MODES_DOMAIN, self.active_index)
        self._set_status("Loaded active game mode record into form.")

    def _load_profile_form_from_active(self, domain: str) -> None:
        if self.active_domain != domain or self.active_index is None:
            self._set_status(f"Select a {self._domain_label(domain)} record before loading the form.")
            return
        raw = self.editor.get("1.0", tk.END).strip()
        if raw == "":
            messagebox.showerror("Invalid record", "Record JSON cannot be empty.")
            return
        try:
            parsed = json.loads(raw)
        except Exception as exc:
            messagebox.showerror("Invalid JSON", f"Could not parse record JSON:\n{exc}")
            return
        if not isinstance(parsed, dict):
            messagebox.showerror("Invalid record", "Record root must be a JSON object.")
            return
        self.domain_data[domain][self.active_index] = parsed
        self._refresh_profile_forms_for_selection(domain, self.active_index)
        self._set_status(f"Loaded active {self._domain_label(domain).lower()} record into form.")

    def _load_page_form_from_active(self) -> None:
        if self.active_domain != "pages" or self.active_index is None:
            self._set_status("Select a page record before loading the form.")
            return
        raw = self.editor.get("1.0", tk.END).strip()
        if raw == "":
            messagebox.showerror("Invalid record", "Record JSON cannot be empty.")
            return
        try:
            parsed = json.loads(raw)
        except Exception as exc:
            messagebox.showerror("Invalid JSON", f"Could not parse record JSON:\n{exc}")
            return
        if not isinstance(parsed, dict):
            messagebox.showerror("Invalid record", "Record root must be a JSON object.")
            return
        self.domain_data["pages"][self.active_index] = parsed
        self._refresh_page_form_for_selection("pages", self.active_index)
        self._set_status("Loaded active page record into form.")

    def _load_pet_menu_slot_form_from_active(self) -> None:
        if self.active_domain != "pet_menu_slots" or self.active_index is None:
            self._set_status("Select a pet menu slot record before loading the form.")
            return
        raw = self.editor.get("1.0", tk.END).strip()
        if raw == "":
            messagebox.showerror("Invalid record", "Record JSON cannot be empty.")
            return
        try:
            parsed = json.loads(raw)
        except Exception as exc:
            messagebox.showerror("Invalid JSON", f"Could not parse record JSON:\n{exc}")
            return
        if not isinstance(parsed, dict):
            messagebox.showerror("Invalid record", "Record root must be a JSON object.")
            return
        self.domain_data["pet_menu_slots"][self.active_index] = parsed
        self._refresh_pet_menu_slot_form_for_selection("pet_menu_slots", self.active_index)
        self._set_status("Loaded active pet menu slot record into form.")

    def _apply_map_form_to_active(self) -> None:
        if self.active_domain != "maps" or self.active_index is None:
            self._set_status("Select a map record before applying form values.")
            return
        records = self.domain_data.get("maps", [])
        idx = self.active_index
        if idx < 0 or idx >= len(records):
            self._set_status("Selected map record is out of range.")
            return

        updated = dict(records[idx])
        for key, _label in MAP_FORM_FIELDS:
            raw_value = self.map_form_vars[key].get().strip()
            if key in {"id", "display_name", "tiled_map"}:
                updated[key] = raw_value
            else:
                parsed_int = self._parse_form_int_value(raw_value)
                if parsed_int is None:
                    messagebox.showerror("Invalid map", f"Field '{key}' must be an integer.")
                    return
                updated[key] = parsed_int

        rec_id = str(updated.get("id", "")).strip()
        if rec_id == "":
            messagebox.showerror("Invalid map", "Field 'id' cannot be empty.")
            return
        for int_key in ("map_asset_id", "tileset_asset_id", "music_asset_id"):
            value = updated.get(int_key)
            if not isinstance(value, int) or value < 0:
                messagebox.showerror("Invalid map", f"Field '{int_key}' must be integer >= 0.")
                return

        for other_ix, other in enumerate(records):
            if other_ix == idx:
                continue
            if str(other.get("id", "")).strip() == rec_id:
                messagebox.showerror("Duplicate map ID", f"id '{rec_id}' is already in use.")
                return

        records[idx] = updated
        self._rebuild_tree_and_select("maps", idx)
        self._load_record_into_editor("maps", idx)
        self._refresh_map_form_for_selection("maps", idx)
        self._set_status("Applied map form values to record.")

    def _apply_dialogue_form_to_active(self) -> None:
        if self.active_domain != "dialogues" or self.active_index is None:
            self._set_status("Select a dialogue record before applying form values.")
            return
        records = self.domain_data.get("dialogues", [])
        idx = self.active_index
        if idx < 0 or idx >= len(records):
            self._set_status("Selected dialogue record is out of range.")
            return

        updated = dict(records[idx])
        for key, _label in DIALOGUE_FORM_FIELDS:
            updated[key] = self.dialogue_form_vars[key].get().strip()

        rec_id = str(updated.get("id", "")).strip()
        if rec_id == "":
            messagebox.showerror("Invalid dialogue", "Field 'id' cannot be empty.")
            return
        start_node_id = str(updated.get("start_node_id", "")).strip()
        if start_node_id == "":
            messagebox.showerror("Invalid dialogue", "Field 'start_node_id' cannot be empty.")
            return
        for other_ix, other in enumerate(records):
            if other_ix == idx:
                continue
            if str(other.get("id", "")).strip() == rec_id:
                messagebox.showerror("Duplicate dialogue ID", f"id '{rec_id}' is already in use.")
                return

        records[idx] = updated
        self._rebuild_tree_and_select("dialogues", idx)
        self._load_record_into_editor("dialogues", idx)
        self._refresh_dialogue_form_for_selection("dialogues", idx)
        self._set_status("Applied dialogue form values to record.")

    def _apply_script_form_to_active(self) -> None:
        if self.active_domain != "scripts" or self.active_index is None:
            self._set_status("Select a script record before applying form values.")
            return
        records = self.domain_data.get("scripts", [])
        idx = self.active_index
        if idx < 0 or idx >= len(records):
            self._set_status("Selected script record is out of range.")
            return

        updated = dict(records[idx])
        for key, _label in SCRIPT_FORM_FIELDS:
            updated[key] = self.script_form_vars[key].get().strip()

        rec_id = str(updated.get("id", "")).strip()
        if rec_id == "":
            messagebox.showerror("Invalid script", "Field 'id' cannot be empty.")
            return
        trigger = str(updated.get("trigger", "")).strip()
        if trigger == "":
            messagebox.showerror("Invalid script", "Field 'trigger' cannot be empty.")
            return
        for other_ix, other in enumerate(records):
            if other_ix == idx:
                continue
            if str(other.get("id", "")).strip() == rec_id:
                messagebox.showerror("Duplicate script ID", f"id '{rec_id}' is already in use.")
                return

        records[idx] = updated
        self._rebuild_tree_and_select("scripts", idx)
        self._load_record_into_editor("scripts", idx)
        self._refresh_script_form_for_selection("scripts", idx)
        self._set_status("Applied script form values to record.")

    def _apply_item_form_to_active(self) -> None:
        if self.active_domain != "items" or self.active_index is None:
            self._set_status("Select an item record before applying form values.")
            return
        records = self.domain_data.get("items", [])
        idx = self.active_index
        if idx < 0 or idx >= len(records):
            self._set_status("Selected item record is out of range.")
            return

        updated = dict(records[idx])
        for key, _label in ITEM_FORM_FIELDS:
            raw_value = self.item_form_vars[key].get().strip()
            if key in {"id", "name", "category", "description", "use_effect_id"}:
                updated[key] = raw_value
            else:
                parsed_int = self._parse_form_int_value(raw_value)
                if parsed_int is None:
                    messagebox.showerror("Invalid item", f"Field '{key}' must be an integer.")
                    return
                updated[key] = parsed_int

        rec_id = str(updated.get("id", "")).strip()
        if rec_id == "":
            messagebox.showerror("Invalid item", "Field 'id' cannot be empty.")
            return
        if str(updated.get("name", "")).strip() == "":
            messagebox.showerror("Invalid item", "Field 'name' cannot be empty.")
            return
        category = str(updated.get("category", "")).strip()
        if category == "":
            messagebox.showerror("Invalid item", "Field 'category' cannot be empty.")
            return
        for int_key in ("max_stack", "value"):
            value = updated.get(int_key)
            if not isinstance(value, int):
                messagebox.showerror("Invalid item", f"Field '{int_key}' must be an integer.")
                return
        for other_ix, other in enumerate(records):
            if other_ix == idx:
                continue
            if str(other.get("id", "")).strip() == rec_id:
                messagebox.showerror("Duplicate item ID", f"id '{rec_id}' is already in use.")
                return

        records[idx] = updated
        self._rebuild_tree_and_select("items", idx)
        self._load_record_into_editor("items", idx)
        self._refresh_item_form_for_selection("items", idx)
        self._set_status("Applied item form values to record.")

    def _apply_package_mode_form_to_active(self) -> None:
        if self.active_domain != PACKAGE_MODES_DOMAIN or self.active_index is None:
            self._set_status("Select a game mode record before applying form values.")
            return
        records = self.domain_data.get(PACKAGE_MODES_DOMAIN, [])
        idx = self.active_index
        if idx < 0 or idx >= len(records):
            self._set_status("Selected game mode record is out of range.")
            return

        updated = dict(records[idx])
        for key, _label in PACKAGE_MODE_FORM_FIELDS:
            raw_value = self.package_mode_form_vars[key].get().strip()
            if key == "map_ref":
                map_rec = self._map_from_ref_label(raw_value)
                if map_rec is None:
                    messagebox.showerror("Invalid game mode", "Field 'Map' must select a valid map.")
                    return
                map_asset_id = map_rec.get("map_asset_id")
                tileset_asset_id = map_rec.get("tileset_asset_id")
                if not isinstance(map_asset_id, int) or map_asset_id <= 0:
                    messagebox.showerror("Invalid game mode", "Selected map has invalid map_asset_id.")
                    return
                if not isinstance(tileset_asset_id, int) or tileset_asset_id <= 0:
                    messagebox.showerror("Invalid game mode", "Selected map has invalid tileset_asset_id.")
                    return
                updated["scene_map_id"] = map_asset_id
                updated["scene_tileset_id"] = tileset_asset_id
                map_music = map_rec.get("music_asset_id")
                if isinstance(map_music, int) and map_music >= 0:
                    updated["music_asset_id"] = map_music
            elif key == "controller_profile_ref":
                profile_rec = self._profile_from_ref_label("controller_profiles", raw_value)
                if profile_rec is None:
                    messagebox.showerror("Invalid game mode", "Field 'Controller Profile' must select a valid profile.")
                    return
                profile_id = profile_rec.get("profile_id")
                if not isinstance(profile_id, int) or profile_id <= 0:
                    messagebox.showerror("Invalid game mode", "Selected controller profile has invalid profile_id.")
                    return
                self._set_mode_profile_reference(updated, "controller_profiles", profile_rec)
            elif key == "camera_profile_ref":
                profile_rec = self._profile_from_ref_label("camera_profiles", raw_value)
                if profile_rec is None:
                    messagebox.showerror("Invalid game mode", "Field 'Camera Profile' must select a valid profile.")
                    return
                profile_id = profile_rec.get("profile_id")
                if not isinstance(profile_id, int) or profile_id <= 0:
                    messagebox.showerror("Invalid game mode", "Selected camera profile has invalid profile_id.")
                    return
                self._set_mode_profile_reference(updated, "camera_profiles", profile_rec)
            elif key == "input_profile_ref":
                profile_rec = self._profile_from_ref_label("input_profiles", raw_value)
                if profile_rec is None:
                    messagebox.showerror("Invalid game mode", "Field 'Input Profile' must select a valid profile.")
                    return
                deadzone = profile_rec.get("input_deadzone_permille")
                flags = profile_rec.get("input_flags")
                if not isinstance(deadzone, int) or deadzone < 0:
                    messagebox.showerror("Invalid game mode", "Selected input profile has invalid deadzone.")
                    return
                if not isinstance(flags, int) or flags < 0:
                    messagebox.showerror("Invalid game mode", "Selected input profile has invalid flags.")
                    return
                self._set_mode_profile_reference(updated, "input_profiles", profile_rec)
            elif key in PACKAGE_MODE_TEXT_FIELDS:
                if key in {"id", "display_name"} and raw_value == "":
                    messagebox.showerror("Invalid game mode", f"Field '{key}' cannot be empty.")
                    return
                updated[key] = raw_value
            else:
                if raw_value == "":
                    messagebox.showerror("Invalid game mode", f"Field '{key}' cannot be empty.")
                    return
                parsed_int = self._parse_form_int_value(raw_value)
                if parsed_int is None:
                    messagebox.showerror("Invalid game mode", f"Field '{key}' must be an integer.")
                    return
                updated[key] = parsed_int

        mode_id = updated.get("mode_id")
        if not isinstance(mode_id, int) or mode_id <= 0:
            messagebox.showerror("Invalid game mode", "Field 'mode_id' must be integer > 0.")
            return
        mode_key = str(updated.get("id", "")).strip()
        if mode_key == "":
            messagebox.showerror("Invalid game mode", "Field 'id' cannot be empty.")
            return
        for other_ix, other in enumerate(records):
            if other_ix == idx:
                continue
            other_mode_id = other.get("mode_id")
            if isinstance(other_mode_id, int) and other_mode_id == mode_id:
                messagebox.showerror("Duplicate mode_id", f"mode_id '{mode_id}' is already in use.")
                return
            if str(other.get("id", "")).strip() == mode_key:
                messagebox.showerror("Duplicate mode key", f"id '{mode_key}' is already in use.")
                return

        records[idx] = updated
        self._rebuild_tree_and_select(PACKAGE_MODES_DOMAIN, idx)
        self._load_record_into_editor(PACKAGE_MODES_DOMAIN, idx)
        self._refresh_package_mode_form_for_selection(PACKAGE_MODES_DOMAIN, idx)
        self._set_status("Applied game mode form values to record.")

    def _apply_page_form_to_active(self) -> None:
        if self.active_domain != "pages" or self.active_index is None:
            self._set_status("Select a page record before applying form values.")
            return
        records = self.domain_data.get("pages", [])
        idx = self.active_index
        if idx < 0 or idx >= len(records):
            self._set_status("Selected page record is out of range.")
            return

        updated = dict(records[idx])
        for key, _label in PAGE_FORM_FIELDS:
            raw_value = self.page_form_vars[key].get().strip()
            if key in {"id", "display_name", "route_kind", "native_page_key", "native_tree_key"}:
                updated[key] = raw_value
                continue
            parsed_int = self._parse_form_int_value(raw_value)
            if parsed_int is None:
                messagebox.showerror("Invalid page", f"Field '{key}' must be an integer.")
                return
            updated[key] = parsed_int

        rec_id = str(updated.get("id", "")).strip()
        if rec_id == "":
            messagebox.showerror("Invalid page", "Field 'id' cannot be empty.")
            return
        page_id = updated.get("page_id")
        if not isinstance(page_id, int) or page_id <= 0:
            messagebox.showerror("Invalid page", "Field 'page_id' must be integer > 0.")
            return
        route_kind = str(updated.get("route_kind", "")).strip()
        if route_kind not in PAGE_ROUTE_KIND_OPTIONS:
            messagebox.showerror("Invalid page", "Field 'route_kind' must be native_page or native_tree.")
            return
        if route_kind == "native_page":
            native_page_key = str(updated.get("native_page_key", "")).strip()
            if native_page_key == "":
                messagebox.showerror("Invalid page", "native_page route requires 'native_page_key'.")
                return
            updated["native_tree_key"] = ""
        else:
            native_tree_key = str(updated.get("native_tree_key", "")).strip()
            if native_tree_key == "":
                messagebox.showerror("Invalid page", "native_tree route requires 'native_tree_key'.")
                return
            updated["native_page_key"] = ""

        for other_ix, other in enumerate(records):
            if other_ix == idx:
                continue
            if str(other.get("id", "")).strip() == rec_id:
                messagebox.showerror("Duplicate page key", f"id '{rec_id}' is already in use.")
                return
            if other.get("page_id") == page_id:
                messagebox.showerror("Duplicate page_id", f"page_id '{page_id}' is already in use.")
                return

        records[idx] = updated
        self._rebuild_tree_and_select("pages", idx)
        self._load_record_into_editor("pages", idx)
        self._refresh_page_form_for_selection("pages", idx)
        self._set_status("Applied page form values to record.")

    def _apply_pet_menu_slot_form_to_active(self) -> None:
        if self.active_domain != "pet_menu_slots" or self.active_index is None:
            self._set_status("Select a pet menu slot record before applying form values.")
            return
        records = self.domain_data.get("pet_menu_slots", [])
        idx = self.active_index
        if idx < 0 or idx >= len(records):
            self._set_status("Selected pet menu slot record is out of range.")
            return

        updated = dict(records[idx])
        for key, _label in PET_MENU_SLOT_FORM_FIELDS:
            raw_value = self.pet_menu_slot_form_vars[key].get().strip()
            if key in {"id", "display_name", "select_kind", "status_kind", "status_source"}:
                updated[key] = raw_value
            elif key == "mode_ref":
                mode_rec = self._mode_from_ref_label(raw_value)
                updated["mode_key"] = "" if mode_rec is None else str(mode_rec.get("id", "")).strip()
            elif key == "page_ref":
                page_rec = self._page_from_ref_label(raw_value)
                updated["page_key"] = "" if page_rec is None else str(page_rec.get("id", "")).strip()
            else:
                parsed_int = self._parse_form_int_value(raw_value)
                if parsed_int is None:
                    messagebox.showerror("Invalid slot", f"Field '{key}' must be an integer.")
                    return
                updated[key] = parsed_int

        rec_id = str(updated.get("id", "")).strip()
        if rec_id == "":
            messagebox.showerror("Invalid slot", "Field 'id' cannot be empty.")
            return

        slot_index = updated.get("slot_index")
        if not isinstance(slot_index, int) or slot_index < 0 or slot_index >= 10:
            messagebox.showerror("Invalid slot", "slot_index must be in range 0..9.")
            return
        icon_action_id = updated.get("icon_action_id")
        if not isinstance(icon_action_id, int) or icon_action_id < 0 or icon_action_id >= 10:
            messagebox.showerror("Invalid slot", "icon_action_id must be in range 0..9.")
            return
        if str(updated.get("select_kind", "")).strip() not in PET_MENU_SLOT_ENUM_OPTIONS["select_kind"]:
            messagebox.showerror("Invalid slot", "select_kind is invalid.")
            return
        if str(updated.get("status_kind", "")).strip() not in PET_MENU_SLOT_ENUM_OPTIONS["status_kind"]:
            messagebox.showerror("Invalid slot", "status_kind is invalid.")
            return
        if str(updated.get("status_source", "")).strip() not in PET_MENU_SLOT_ENUM_OPTIONS["status_source"]:
            messagebox.showerror("Invalid slot", "status_source is invalid.")
            return
        if str(updated.get("select_kind", "")).strip() == "launch_mode":
            if str(updated.get("mode_key", "")).strip() == "":
                messagebox.showerror("Invalid slot", "launch_mode requires a target mode.")
                return
        if str(updated.get("select_kind", "")).strip() == "open_page":
            if str(updated.get("page_key", "")).strip() == "":
                messagebox.showerror("Invalid slot", "open_page requires a target page.")
                return
        if str(updated.get("status_kind", "")).strip() == "none":
            updated["status_source"] = "none"
        else:
            if str(updated.get("status_source", "")).strip() != "battery":
                messagebox.showerror("Invalid slot", "status_kind requires status_source=battery.")
                return

        for other_ix, other in enumerate(records):
            if other_ix == idx:
                continue
            if str(other.get("id", "")).strip() == rec_id:
                messagebox.showerror("Duplicate slot key", f"id '{rec_id}' is already in use.")
                return
            if other.get("slot_index") == slot_index:
                messagebox.showerror("Duplicate slot index", f"slot_index '{slot_index}' is already in use.")
                return

        records[idx] = updated
        self._rebuild_tree_and_select("pet_menu_slots", idx)
        self._load_record_into_editor("pet_menu_slots", idx)
        self._refresh_pet_menu_slot_form_for_selection("pet_menu_slots", idx)
        self._set_status("Applied pet menu slot form values to record.")

    def _apply_profile_form_to_active(self, domain: str) -> None:
        if self.active_domain != domain or self.active_index is None:
            self._set_status(f"Select a {self._domain_label(domain)} record before applying form values.")
            return
        records = self.domain_data.get(domain, [])
        idx = self.active_index
        if idx < 0 or idx >= len(records):
            self._set_status(f"Selected {self._domain_label(domain).lower()} record is out of range.")
            return

        updated = dict(records[idx])
        for key, _label in PROFILE_FORM_FIELDS_BY_DOMAIN[domain]:
            raw_value = self.profile_form_vars[domain][key].get().strip()
            if key in {"id", "display_name"}:
                if key == "id" and raw_value == "":
                    messagebox.showerror("Invalid profile", "Field 'id' cannot be empty.")
                    return
                if key == "display_name" and raw_value == "":
                    updated.pop(key, None)
                else:
                    updated[key] = raw_value
                continue
            if raw_value == "":
                if key == "profile_id":
                    messagebox.showerror("Invalid profile", "Field 'profile_id' cannot be empty.")
                    return
                updated.pop(key, None)
                continue
            parsed_int = self._parse_form_int_value(raw_value)
            if parsed_int is None:
                messagebox.showerror("Invalid profile", f"Field '{key}' must be an integer.")
                return
            updated[key] = parsed_int

        profile_id = updated.get("profile_id")
        if not isinstance(profile_id, int) or profile_id <= 0:
            messagebox.showerror("Invalid profile", "Field 'profile_id' must be integer > 0.")
            return
        rec_id = str(updated.get("id", "")).strip()
        if rec_id == "":
            messagebox.showerror("Invalid profile", "Field 'id' cannot be empty.")
            return

        for other_ix, other in enumerate(records):
            if other_ix == idx:
                continue
            other_profile_id = other.get("profile_id")
            if isinstance(other_profile_id, int) and other_profile_id == profile_id:
                messagebox.showerror("Duplicate profile_id", f"profile_id '{profile_id}' is already in use.")
                return
            if str(other.get("id", "")).strip() == rec_id:
                messagebox.showerror("Duplicate profile key", f"id '{rec_id}' is already in use.")
                return

        records[idx] = updated
        self._rebuild_tree_and_select(domain, idx)
        self._load_record_into_editor(domain, idx)
        self._refresh_profile_forms_for_selection(domain, idx)
        self._set_status(f"Applied {self._domain_label(domain).lower()} form values to record.")

    def _domain_path(self, domain: str) -> Path:
        if domain == PACKAGE_MODES_DOMAIN:
            return self.package_manifest_path
        return self.gp_dir / DOMAIN_FILES[domain]

    def _load_all(self) -> None:
        self.domain_data = {}
        self.package_manifest = {}
        for domain in DOMAIN_ORDER:
            if domain == PACKAGE_MODES_DOMAIN:
                raw_manifest = load_json(self.package_manifest_path, {})
                if not isinstance(raw_manifest, dict):
                    raise ValueError(f"{self.package_manifest_path.as_posix()} must contain a JSON object.")
                self.package_manifest = raw_manifest
                raw_modes = raw_manifest.get("modes", [])
                if not isinstance(raw_modes, list):
                    raise ValueError(
                        f"{self.package_manifest_path.as_posix()} field 'modes' must contain a JSON array."
                    )
                cleaned_modes: list[dict[str, Any]] = []
                for item in raw_modes:
                    if isinstance(item, dict):
                        cleaned_modes.append(item)
                self.domain_data[domain] = cleaned_modes
            else:
                raw = load_json(self._domain_path(domain), [])
                if not isinstance(raw, list):
                    raise ValueError(f"{self._domain_path(domain).as_posix()} must contain a JSON array.")
                cleaned: list[dict[str, Any]] = []
                for item in raw:
                    if isinstance(item, dict):
                        cleaned.append(item)
                self.domain_data[domain] = cleaned

    def _reload(self) -> None:
        self._load_all()
        self.active_domain = None
        self.active_index = None
        self._rebuild_tree()
        self.editor.delete("1.0", tk.END)
        self.record_title.configure(text="Record")
        self._refresh_forms_for_selection(None, None)
        self._set_status("Reloaded from disk.")

    def _rebuild_tree(self) -> None:
        self.tree.delete(*self.tree.get_children())
        self.tree.insert("", "end", iid=TREE_ROOT_IID, text="Game Package")
        for domain in TREE_DOMAIN_ORDER:
            parent_id = f"domain:{domain}"
            tree_parent = TREE_ROOT_IID if domain in (PACKAGE_MODES_DOMAIN, "pet_menu_slots") else ""
            self.tree.insert(tree_parent, "end", iid=parent_id, text=self._domain_label(domain))
            for idx, record in enumerate(self.domain_data.get(domain, [])):
                rec_label = self._record_tree_label(domain, record, idx)
                self.tree.insert(parent_id, "end", iid=f"record:{domain}:{idx}", text=rec_label)
            self.tree.item(parent_id, open=True)
        self.tree.item(TREE_ROOT_IID, open=True)

    def _current_selection(self) -> tuple[str | None, int | None]:
        selected = self.tree.selection()
        if not selected:
            return None, None
        token = selected[0]
        if token == TREE_ROOT_IID:
            return None, None
        if token.startswith("domain:"):
            return token.split(":", 1)[1], None
        if not token.startswith("record:"):
            return None, None
        parts = token.split(":")
        if len(parts) != 3:
            return None, None
        domain = parts[1]
        try:
            idx = int(parts[2])
        except ValueError:
            return None, None
        return domain, idx

    def _on_tree_select(self, _event: Any) -> None:
        if self._suspend_tree_select:
            return
        if not self._commit_active_record():
            return
        domain, idx = self._current_selection()
        self.active_domain = domain
        self.active_index = idx
        if domain is None or idx is None:
            self.editor.delete("1.0", tk.END)
            if domain is None:
                self.record_title.configure(text="Record")
            else:
                self.record_title.configure(text=f"{self._domain_label(domain)} (select a record)")
            self._refresh_forms_for_selection(domain, idx)
            return
        self._load_record_into_editor(domain, idx)
        self._refresh_forms_for_selection(domain, idx)

    def _load_record_into_editor(self, domain: str, idx: int) -> None:
        records = self.domain_data.get(domain, [])
        if idx < 0 or idx >= len(records):
            self.editor.delete("1.0", tk.END)
            self.record_title.configure(text="Record")
            return
        record = records[idx]
        if domain == PACKAGE_MODES_DOMAIN:
            mode_id = record.get("mode_id", "<unnamed>")
            mode_key = str(record.get("id", "")).strip()
            display_name = str(record.get("display_name", "")).strip()
            if display_name == "":
                display_name = f"Mode {mode_id}"
            if mode_key == "":
                mode_key = f"mode_{mode_id}" if isinstance(mode_id, int) else "<unnamed>"
            title_id = f"{display_name} ({mode_key})"
        else:
            title_id = record.get("id", "<unnamed>")
        self.record_title.configure(text=f"{self._domain_label(domain)}: {title_id}")
        self.editor.delete("1.0", tk.END)
        self.editor.insert("1.0", json.dumps(record, indent=2, ensure_ascii=True))

    def _load_map_form_from_active(self) -> None:
        if self.active_domain != "maps" or self.active_index is None:
            self._set_status("Select a map record before loading the form.")
            return
        raw = self.editor.get("1.0", tk.END).strip()
        if raw == "":
            messagebox.showerror("Invalid record", "Record JSON cannot be empty.")
            return
        try:
            parsed = json.loads(raw)
        except Exception as exc:
            messagebox.showerror("Invalid JSON", f"Could not parse record JSON:\n{exc}")
            return
        if not isinstance(parsed, dict):
            messagebox.showerror("Invalid record", "Record root must be a JSON object.")
            return
        self.domain_data["maps"][self.active_index] = parsed
        self._refresh_map_form_for_selection("maps", self.active_index)
        self._set_status("Loaded active map record into form.")

    def _load_dialogue_form_from_active(self) -> None:
        if self.active_domain != "dialogues" or self.active_index is None:
            self._set_status("Select a dialogue record before loading the form.")
            return
        raw = self.editor.get("1.0", tk.END).strip()
        if raw == "":
            messagebox.showerror("Invalid record", "Record JSON cannot be empty.")
            return
        try:
            parsed = json.loads(raw)
        except Exception as exc:
            messagebox.showerror("Invalid JSON", f"Could not parse record JSON:\n{exc}")
            return
        if not isinstance(parsed, dict):
            messagebox.showerror("Invalid record", "Record root must be a JSON object.")
            return
        self.domain_data["dialogues"][self.active_index] = parsed
        self._refresh_dialogue_form_for_selection("dialogues", self.active_index)
        self._set_status("Loaded active dialogue record into form.")

    def _load_script_form_from_active(self) -> None:
        if self.active_domain != "scripts" or self.active_index is None:
            self._set_status("Select a script record before loading the form.")
            return
        raw = self.editor.get("1.0", tk.END).strip()
        if raw == "":
            messagebox.showerror("Invalid record", "Record JSON cannot be empty.")
            return
        try:
            parsed = json.loads(raw)
        except Exception as exc:
            messagebox.showerror("Invalid JSON", f"Could not parse record JSON:\n{exc}")
            return
        if not isinstance(parsed, dict):
            messagebox.showerror("Invalid record", "Record root must be a JSON object.")
            return
        self.domain_data["scripts"][self.active_index] = parsed
        self._refresh_script_form_for_selection("scripts", self.active_index)
        self._set_status("Loaded active script record into form.")

    def _load_item_form_from_active(self) -> None:
        if self.active_domain != "items" or self.active_index is None:
            self._set_status("Select an item record before loading the form.")
            return
        raw = self.editor.get("1.0", tk.END).strip()
        if raw == "":
            messagebox.showerror("Invalid record", "Record JSON cannot be empty.")
            return
        try:
            parsed = json.loads(raw)
        except Exception as exc:
            messagebox.showerror("Invalid JSON", f"Could not parse record JSON:\n{exc}")
            return
        if not isinstance(parsed, dict):
            messagebox.showerror("Invalid record", "Record root must be a JSON object.")
            return
        self.domain_data["items"][self.active_index] = parsed
        self._refresh_item_form_for_selection("items", self.active_index)
        self._set_status("Loaded active item record into form.")

    def _commit_active_record(self) -> bool:
        if self.active_domain is None or self.active_index is None:
            return True
        records = self.domain_data.get(self.active_domain, [])
        if self.active_index < 0 or self.active_index >= len(records):
            return True
        raw = self.editor.get("1.0", tk.END).strip()
        if raw == "":
            messagebox.showerror("Invalid record", "Record JSON cannot be empty.")
            return False
        try:
            parsed = json.loads(raw)
        except Exception as exc:
            messagebox.showerror("Invalid JSON", f"Could not parse record JSON:\n{exc}")
            return False
        if not isinstance(parsed, dict):
            messagebox.showerror("Invalid record", "Record root must be a JSON object.")
            return False
        if self.active_domain == PACKAGE_MODES_DOMAIN:
            mode_id = parsed.get("mode_id")
            if not isinstance(mode_id, int) or mode_id <= 0:
                messagebox.showerror("Invalid record", "Game mode record must include integer 'mode_id' > 0.")
                return False
            mode_key = str(parsed.get("id", "")).strip()
            if mode_key == "":
                messagebox.showerror("Invalid record", "Game mode record must include non-empty string 'id'.")
                return False
            for other_ix, other in enumerate(records):
                if other_ix == self.active_index:
                    continue
                other_mode_id = other.get("mode_id")
                if isinstance(other_mode_id, int) and other_mode_id == mode_id:
                    messagebox.showerror("Duplicate mode_id", f"mode_id '{mode_id}' is already in use.")
                    return False
                if str(other.get("id", "")).strip() == mode_key:
                    messagebox.showerror("Duplicate mode key", f"id '{mode_key}' is already in use.")
                    return False
        elif self.active_domain in PROFILE_DOMAINS:
            profile_id = parsed.get("profile_id")
            if not isinstance(profile_id, int) or profile_id <= 0:
                messagebox.showerror(
                    "Invalid record",
                    f"{self._domain_label(self.active_domain)} record must include integer 'profile_id' > 0.",
                )
                return False
            for other_ix, other in enumerate(records):
                if other_ix == self.active_index:
                    continue
                other_profile_id = other.get("profile_id")
                if isinstance(other_profile_id, int) and other_profile_id == profile_id:
                    messagebox.showerror("Duplicate profile_id", f"profile_id '{profile_id}' is already in use.")
                    return False
        else:
            rec_id = str(parsed.get("id", "")).strip()
            if rec_id == "":
                messagebox.showerror("Invalid record", "Record must include a non-empty 'id'.")
                return False
        records[self.active_index] = parsed
        return True

    def _pick_domain_for_add(self) -> str:
        domain, idx = self._current_selection()
        if domain is None:
            return PACKAGE_MODES_DOMAIN
        if idx is None:
            return domain
        return domain

    def _next_id(self, domain: str, base_id: str) -> str:
        existing = {str(x.get("id", "")).strip() for x in self.domain_data.get(domain, [])}
        if base_id not in existing:
            return base_id
        suffix = 2
        while True:
            candidate = f"{base_id}_{suffix}"
            if candidate not in existing:
                return candidate
            suffix += 1

    def _next_mode_id(self) -> int:
        existing: set[int] = set()
        for rec in self.domain_data.get(PACKAGE_MODES_DOMAIN, []):
            mode_id = rec.get("mode_id")
            if isinstance(mode_id, int) and mode_id > 0:
                existing.add(mode_id)
        mode_id = 1
        while mode_id in existing:
            mode_id += 1
        return mode_id

    def _next_mode_key(self, mode_id: int) -> str:
        existing = {
            str(x.get("id", "")).strip()
            for x in self.domain_data.get(PACKAGE_MODES_DOMAIN, [])
        }
        candidate = f"mode_{mode_id}"
        if candidate not in existing:
            return candidate
        suffix = 2
        while True:
            candidate = f"mode_{mode_id}_{suffix}"
            if candidate not in existing:
                return candidate
            suffix += 1

    def _next_profile_id(self, domain: str) -> int:
        existing: set[int] = set()
        for rec in self.domain_data.get(domain, []):
            profile_id = rec.get("profile_id")
            if isinstance(profile_id, int) and profile_id > 0:
                existing.add(profile_id)
        profile_id = 1
        while profile_id in existing:
            profile_id += 1
        return profile_id

    def _next_page_id(self) -> int:
        existing: set[int] = set()
        for rec in self.domain_data.get("pages", []):
            page_id = rec.get("page_id")
            if isinstance(page_id, int) and page_id > 0:
                existing.add(page_id)
        next_id = 1
        while next_id in existing:
            next_id += 1
        return next_id

    def _next_pet_menu_slot_index(self) -> int:
        used: set[int] = set()
        for rec in self.domain_data.get("pet_menu_slots", []):
            slot_index = rec.get("slot_index")
            if isinstance(slot_index, int) and slot_index >= 0:
                used.add(slot_index)
        for slot_index in range(10):
            if slot_index not in used:
                return slot_index
        return -1

    def _add_record(self) -> None:
        if not self._commit_active_record():
            return
        domain = self._pick_domain_for_add()
        tpl = deep_copy_json(DOMAIN_TEMPLATES[domain])
        if domain == PACKAGE_MODES_DOMAIN:
            tpl["mode_id"] = self._next_mode_id()
            tpl["id"] = self._next_mode_key(tpl["mode_id"])
            tpl["display_name"] = f"Mode {tpl['mode_id']}"
            if "description" not in tpl:
                tpl["description"] = ""
        elif domain == "pages":
            page_id = self._next_page_id()
            tpl["page_id"] = page_id
            tpl["id"] = self._next_id(domain, f"page_{page_id}")
            tpl["display_name"] = f"Page {page_id}"
        elif domain == "pet_menu_slots":
            slot_index = self._next_pet_menu_slot_index()
            if slot_index < 0:
                messagebox.showerror("No free slot", "All 10 pet menu slots are already in use.")
                return
            tpl["slot_index"] = slot_index
            tpl["icon_action_id"] = slot_index
            tpl["id"] = self._next_id(domain, f"slot_{slot_index}")
            tpl["display_name"] = f"Slot {slot_index}"
            tpl["mode_key"] = ""
            tpl["page_key"] = ""
        elif domain in PROFILE_DOMAINS:
            tpl["profile_id"] = self._next_profile_id(domain)
        else:
            base_id = str(tpl.get("id", f"new_{domain}"))
            tpl["id"] = self._next_id(domain, base_id)
        self.domain_data[domain].append(tpl)
        idx = len(self.domain_data[domain]) - 1
        self._rebuild_tree_and_select(domain, idx)
        self.active_domain = domain
        self.active_index = idx
        self._load_record_into_editor(domain, idx)
        self._refresh_forms_for_selection(domain, idx)
        if domain == PACKAGE_MODES_DOMAIN:
            mode_name = str(tpl.get("display_name", "")).strip()
            if mode_name == "":
                mode_name = f"Mode {tpl['mode_id']}"
            self._set_status(
                f"Added new record: {self._domain_label(domain)}: "
                f"{mode_name} ({tpl.get('id', '')})"
            )
        else:
            self._set_status(f"Added new record: {self._domain_label(domain)}: {tpl['id']}")

    def _delete_record(self) -> None:
        domain, idx = self._current_selection()
        if domain is None or idx is None:
            self._set_status("Select a record to delete.")
            return
        if idx < 0 or idx >= len(self.domain_data.get(domain, [])):
            self._set_status("Invalid record selection.")
            return
        if domain == PACKAGE_MODES_DOMAIN:
            rec = self.domain_data[domain][idx]
            mode_id = rec.get("mode_id", "")
            mode_key = str(rec.get("id", "")).strip() or f"mode_{mode_id}"
            display_name = str(rec.get("display_name", "")).strip() or f"Mode {mode_id}"
            rec_id = f"{display_name} ({mode_key})"
        else:
            rec_id = str(self.domain_data[domain][idx].get("id", ""))
        confirm = messagebox.askyesno("Delete record", f"Delete {self._domain_label(domain)}: {rec_id}?")
        if not confirm:
            return
        del self.domain_data[domain][idx]
        self.active_domain = None
        self.active_index = None
        self.editor.delete("1.0", tk.END)
        self.record_title.configure(text="Record")
        self._rebuild_tree()
        self._refresh_forms_for_selection(None, None)
        self._set_status(f"Deleted record: {self._domain_label(domain)}: {rec_id}")

    def _apply_record(self) -> None:
        if self._commit_active_record():
            self._set_status("Record applied.")

    def _save_all(self) -> None:
        if not self._commit_active_record():
            return
        self.gp_dir.mkdir(parents=True, exist_ok=True)
        for domain in DOMAIN_ORDER:
            if domain == PACKAGE_MODES_DOMAIN:
                continue
            path = self._domain_path(domain)
            payload = self.domain_data.get(domain, [])
            path.write_text(json.dumps(payload, indent=2, ensure_ascii=True) + "\n", encoding="utf-8")
        manifest_payload = (
            deep_copy_json(self.package_manifest) if isinstance(self.package_manifest, dict) else {}
        )
        if not isinstance(manifest_payload, dict):
            manifest_payload = {}
        manifest_payload["modes"] = self.domain_data.get(PACKAGE_MODES_DOMAIN, [])
        self.package_manifest_path.parent.mkdir(parents=True, exist_ok=True)
        self.package_manifest_path.write_text(
            json.dumps(manifest_payload, indent=2, ensure_ascii=True) + "\n",
            encoding="utf-8",
        )
        self.package_manifest = manifest_payload
        self._set_status("Saved all domain files.")

    def _validate(self) -> None:
        if not self._commit_active_record():
            return
        # Validate in-memory view by writing once before check.
        self._save_all()
        errors, warnings = validate_project(self.repo_root)
        if errors:
            text = "\n".join(f"- {msg}" for msg in errors)
            messagebox.showerror("Validation failed", text)
            self._set_status(f"Validation failed: {len(errors)} error(s).")
            return
        if warnings:
            text = "\n".join(f"- {msg}" for msg in warnings)
            messagebox.showwarning("Validation warnings", text)
            self._set_status(f"Validation passed with {len(warnings)} warning(s).")
            return
        messagebox.showinfo("Validation passed", "Project data and Tiled references are valid.")
        self._set_status("Validation passed.")


def main() -> int:
    repo_root = find_repo_root(Path(__file__).resolve())
    app = GameIdeApp(repo_root)
    app.mainloop()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
