#ifndef UI_PAGE_STORAGE_H
#define UI_PAGE_STORAGE_H

#include "ui/ui_router.h"

#ifdef __cplusplus
extern "C" {
#endif

const ui_page_vtable_t *UiPageStorage_GetVTable(void);

#ifdef __cplusplus
}
#endif

#endif
