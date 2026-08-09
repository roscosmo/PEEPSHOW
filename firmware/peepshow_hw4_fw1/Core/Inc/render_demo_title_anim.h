#ifndef RENDER_DEMO_TITLE_ANIM_H
#define RENDER_DEMO_TITLE_ANIM_H

#include <stdint.h>

#include "game_runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

void RenderDemoTitleAnim_Reset(void);
uint8_t RenderDemoTitleAnim_HandleControl(const game_runtime_input_t *input,
                                          uint8_t *request_exit_to_static,
                                          game_runtime_audio_cue_t *audio_cue_out);
void RenderDemoTitleAnim_Update(const app_sensor_snapshot_t *sensor_snapshot, uint32_t dt_ms);
void RenderDemoTitleAnim_DrawFrame(const app_sensor_snapshot_t *sensor_snapshot);

#ifdef __cplusplus
}
#endif

#endif /* RENDER_DEMO_TITLE_ANIM_H */
