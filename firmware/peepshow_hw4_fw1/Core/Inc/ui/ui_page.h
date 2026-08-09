#ifndef UI_PAGE_H
#define UI_PAGE_H

#include <stdint.h>
#include "ui/ui_events.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ui_router_s;
typedef struct ui_router_s ui_router_t;

typedef struct ui_page_s ui_page_t;

typedef void (*ui_page_enter_fn_t)(ui_router_t *ui, const void *arg);
typedef uint8_t (*ui_page_input_policy_fn_t)(const ui_input_evt_t *evt);
typedef uint32_t (*ui_page_event_fn_t)(ui_router_t *ui, const ui_input_evt_t *evt);
typedef void (*ui_page_render_fn_t)(ui_router_t *ui);
typedef void (*ui_page_exit_fn_t)(ui_router_t *ui);

struct ui_page_s
{
  const char *name;
  ui_footer_t footer;
  ui_page_input_policy_fn_t input_policy;
  ui_page_enter_fn_t enter;
  ui_page_event_fn_t event;
  ui_page_render_fn_t render;
  ui_page_exit_fn_t exit;
};

#ifdef __cplusplus
}
#endif

#endif

