#ifndef UI_PAGE_JOY_COMMON_H
#define UI_PAGE_JOY_COMMON_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void UiPageJoyCommon_ResetDoneActions(void);
uint8_t UiPageJoyCommon_GetDoneRecalMode(void);
void UiPageJoyCommon_SetDoneRecalMode(uint8_t enabled);
uint8_t UiPageJoyCommon_GetDoneRecalNextEnter(void);
void UiPageJoyCommon_SetDoneRecalNextEnter(uint8_t enabled);

#ifdef __cplusplus
}
#endif

#endif


