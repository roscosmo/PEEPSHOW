#ifndef RENDER_DEMO_H
#define RENDER_DEMO_H

#include "app_threadx.h"

#ifdef __cplusplus
extern "C" {
#endif

void RenderDemo_Reset(void);
void RenderDemo_ToggleBackground(void);
void RenderDemo_ToggleCube(void);
void RenderDemo_DrawFrame(const app_sensor_snapshot_t *sensor_snapshot);

#ifdef __cplusplus
}
#endif

#endif /* RENDER_DEMO_H */
