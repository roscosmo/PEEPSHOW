#ifndef UI_PAGE_STUB_COMMON_H
#define UI_PAGE_STUB_COMMON_H

#include "ui/ui_router.h"

#ifdef __cplusplus
extern "C" {
#endif

void UiPageStub_Enter(void);
uint8_t UiPageStub_ActionBackTo(ui_page_id_t back_page, const ui_action_evt_t *evt);
void UiPageStub_Render(const char *title, const char *line1, const char *line2);

#ifdef __cplusplus
}
#endif

#endif
