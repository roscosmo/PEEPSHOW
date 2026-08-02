#ifndef UI_PAGE_JOY_TARGET_H
#define UI_PAGE_JOY_TARGET_H

#include "ui/ui_router.h"

#ifdef __cplusplus
extern "C" {
#endif

const ui_page_vtable_t *UiPageJoyTarget_GetVTable(void);
void UiPageJoyTarget_ResetDoneActions(void);

#ifdef __cplusplus
}
#endif

#endif
