/* Compile-only target ABI measurement; never linked into firmware. */
#include "ps_scene_objects.h"
#include "ps_scene_object_graph.h"

uint8_t ps_object_bank_size[sizeof(ps_scene_objects_t)];
uint8_t ps_object_stage_size[sizeof(ps_scene_objects_stage_t)];
uint8_t ps_object_snapshot_size[sizeof(ps_scene_objects_snapshot_t)];
uint8_t ps_object_graph_size[sizeof(ps_scene_object_graph_t)];
uint8_t ps_object_graph_stage_size[sizeof(ps_scene_object_graph_stage_t)];
uint8_t ps_object_graph_effects_size[sizeof(ps_scene_object_effects_t)];
