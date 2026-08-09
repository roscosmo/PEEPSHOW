## Tiled Map Metadata (PeepShow Top-Down Engine)

This project uses **Tiled** as the authoritative source of gameplay metadata.

- **Aseprite** = pixel art + animation tags
- **Tiled** = tile properties + object/entity placement
- **STM32 runtime** loads exported map/tileset data into lightweight structs + bitflags

Target format:

- Top-down engine
- Base tile size: **8×8**
- Supports: collision, occlusion, camera zones, spawns, exits, day/night lighting


# Tile-Level Properties (Grid Semantics)

Tile properties define cheap, per-cell gameplay rules.

These should compile down to a **bitmask** at build-time.


## Collision & Movement

Property meanings:

- `solid` (bool)  
  Blocks player + entity movement

- `water` (bool)  
  Water tile (swim or blocked)

- `slow` (bool)  
  Slows movement (mud/grass)

Example:

    solid = true
    slow  = true


## Rendering & Occlusion

Used for correct top-down depth layering.

- `occluder` (bool)  
  Tile can draw in front of player when behind it

- `occlude_y_px` (int)  
  Height of solid base strip (default: 2px)

- `roof` (bool)  
  Roof tile (hidden when player is indoors)

Fence/sign example:

    solid        = true
    occluder     = true
    occlude_y_px = 2


## Emissive / Lighting Contribution

Tiles that glow at night.

- `emissive` (bool)  
  Tile emits light

- `emissive_strength` (int)  
  Brightness (0–255)

- `night_only` (bool)  
  Only active during night cycle

Lamp/window example:

    emissive           = true
    emissive_strength  = 180
    night_only         = true



# Object-Level Metadata (Entity Semantics)

Objects define non-grid gameplay elements.

These should export into lightweight runtime records.

Recommended object layers:

- `Spawns`
- `Triggers`
- `Lighting`
- `Camera`
- `Paths`


## Spawn Points

Object type: `Spawn`

Used for player, enemies, NPCs, items.

Properties:

- `kind` (string): `enemy`, `npc`, `item`, `player`
- `id` (string): Entity ID (e.g. `slime`)
- `facing` (string): `up/down/left/right`
- `respawn_s` (int): Respawn delay in seconds

Example:

    kind      = enemy
    id        = slime
    facing    = left
    respawn_s = 10



## Interactables

Object type: `Interact`

Used for signs, chests, scripted triggers.

Properties:

- `script_id` (string, preferred): Script key from `Assets/game_project/scripts.json`
- `dialogue_id` (string, preferred): Dialogue key from `Assets/game_project/dialogues.json`
- `action` (string, fallback): legacy action key
- `text_id` (int, fallback): legacy dialogue lookup ID

Build/runtime note:

- Map blob generation packs `Interact` references into object args:
  - `arg0` = hash(`script_id`) if present, else hash(`action`)
  - `arg1` = hash(`dialogue_id`) if present, else legacy text/dialog fallback
- Topdown runtime consumes these args on interact input and tracks last-seen
  script/dialogue refs for runtime event handling.

Sign example:

    action  = sign
    text_id = 12



## Exits / Map Transitions

Exits are handled as dedicated transition trigger objects.

Object type: `Exit`

Used for:

- Doors
- Cave entrances
- Warp points
- Map edge transitions

Properties:

- `target_map` (string)  
  Destination map file/ID

- `target_spawn` (string)  
  Spawn point name/ID inside the destination map

- `direction` (string, optional)  
  Player facing after transition (`up/down/left/right`)

- `fade` (bool, optional)  
  Enable fade-out/fade-in transition

- `locked` (bool, optional)  
  Exit disabled until unlocked by game logic

Example: Door Exit

    type         = Exit
    target_map   = town_inn
    target_spawn = entry_door
    direction    = up
    fade         = true

Example: Cave Entrance

    type         = Exit
    target_map   = cave_01
    target_spawn = cave_start
    fade         = true

Runtime behavior:

When the player overlaps an `Exit` object:

1. Load `target_map`
2. Place player at `target_spawn`
3. Apply facing direction + camera update
4. Perform fade transition if enabled



# Camera & Zoom Zones

Object type: `CameraZone` (rectangle)

Preferred method for zoom control (NOT per-tile).

Properties:

- `zoom` (int): Zoom factor (e.g. 2 = zoom in)
- `lerp` (float): Transition smoothing
- `lock` (bool): Prevent camera scrolling
- `priority` (int): Zone override priority

Example:

    zoom     = 2
    lerp     = 0.15
    priority = 10



# Indoor / Darkness Zones

Object type: `IndoorZone` (rectangle)

Used for interior lighting + roof hiding.

Properties:

- `hide_roofs` (bool): Hide roof tiles while inside
- `ambient` (int): Ambient brightness override
- `ambient_night` (int): Ambient override during night

Example:

    hide_roofs    = true
    ambient       = 160
    ambient_night = 120



# Dynamic Lights

Object type: `Light` (point object)

Used for lamps, torches, glowing objects.

Properties:

- `radius` (int): Light radius in pixels
- `intensity` (int): Brightness (0–255)
- `on_at` (enum): `always`, `night`, `dusk`

Example:

    radius    = 24
    intensity = 200
    on_at     = night



# Runtime Lighting Model (1-bit Friendly)

Lighting is implemented using:

- Global ambient brightness from day/night cycle
- + Contributions from nearby `Light` objects
- + IndoorZone overrides
- Converted to 1-bit using ordered dithering (Bayer matrix)

This provides a convincing night effect on Sharp Memory LCD without heavy CPU cost.



# Minimal Engine Metadata Set (Recommended)

Tile Bitflags:

- `solid`
- `water`
- `slow`
- `occluder`
- `roof`
- `emissive`

Object Types:

- `Spawn`
- `Interact`
- `Exit`
- `CameraZone`
- `IndoorZone`
- `Light`

This is the baseline schema for PeepShow maps.
