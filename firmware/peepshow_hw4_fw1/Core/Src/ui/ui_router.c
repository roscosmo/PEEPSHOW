#include "ui/ui_router.h"
#include "display_renderer.h"
#include "th_mode.h"
#include <string.h>

static uint16_t UiRouter_VisibleRows(void)
{
  return 8U;
}

static void UiRouter_SnapshotCapture(ui_router_tree_frame_t *dst, const ui_router_t *src)
{
  if ((dst == 0) || (src == 0))
  {
    return;
  }

  (void)memset(dst, 0, sizeof(*dst));
  dst->mode = src->mode;
  dst->menu_depth = src->menu_depth;
  dst->tree_id = src->current_tree_id;
  dst->current_page = src->current_page;
  dst->current_page_arg = src->current_page_arg;
  if (src->menu_depth > 0U)
  {
    (void)memcpy(dst->menu_stack, src->menu_stack, sizeof(src->menu_stack));
  }
}

static void UiRouter_SnapshotRestore(ui_router_t *dst, const ui_router_tree_frame_t *src)
{
  if ((dst == 0) || (src == 0))
  {
    return;
  }

  dst->mode = src->mode;
  dst->menu_depth = src->menu_depth;
  dst->current_tree_id = src->tree_id;
  dst->current_page = src->current_page;
  dst->current_page_arg = src->current_page_arg;
  (void)memcpy(dst->menu_stack, src->menu_stack, sizeof(dst->menu_stack));
}

static uint8_t UiRouter_MenuPush(ui_router_t *ui, const ui_menu_t *menu)
{
  ui_menu_stack_frame_t *frame;

  if ((ui == 0) || (menu == 0))
  {
    return 0U;
  }

  if (ui->menu_depth >= UI_MENU_STACK_MAX)
  {
    return 0U;
  }

  frame = &ui->menu_stack[ui->menu_depth];
  (void)memset(frame, 0, sizeof(*frame));
  frame->menu = menu;
  ui->menu_depth++;
  return 1U;
}

static uint8_t UiRouter_MenuPop(ui_router_t *ui)
{
  if ((ui == 0) || (ui->menu_depth <= 1U))
  {
    return 0U;
  }

  ui->menu_depth--;
  return 1U;
}

static ui_menu_stack_frame_t *UiRouter_MenuTopMutable(ui_router_t *ui)
{
  if ((ui == 0) || (ui->menu_depth == 0U))
  {
    return 0;
  }

  return &ui->menu_stack[ui->menu_depth - 1U];
}

static const ui_menu_stack_frame_t *UiRouter_MenuTop(const ui_router_t *ui)
{
  if ((ui == 0) || (ui->menu_depth == 0U))
  {
    return 0;
  }

  return &ui->menu_stack[ui->menu_depth - 1U];
}

static void UiRouter_FooterLines(ui_footer_t footer, const char **line1, const char **line2)
{
  const char *l1 = 0;
  const char *l2 = 0;

  switch (footer)
  {
    case UI_FOOTER_A_SELECT_B_BACK:
      l1 = "JOY: NAV  A:OPEN";
      l2 = "B: BACK";
      break;
    case UI_FOOTER_A_START_B_BACK:
      l1 = "A:START";
      l2 = "B: BACK";
      break;
    case UI_FOOTER_A_TOGGLE_B_BACK:
      l1 = "A:TOGGLE";
      l2 = "B: BACK";
      break;
    case UI_FOOTER_A_RUN_B_BACK:
      l1 = "A:RUN";
      l2 = "B: BACK";
      break;
    case UI_FOOTER_NONE:
    default:
      l1 = 0;
      l2 = 0;
      break;
  }

  if (line1 != 0)
  {
    *line1 = l1;
  }
  if (line2 != 0)
  {
    *line2 = l2;
  }
}

static void UiRouter_RenderMenu(const ui_router_t *ui)
{
  const ui_menu_stack_frame_t *frame = UiRouter_MenuTop(ui);
  const ui_menu_t *menu;
  uint16_t y = 24U;
  uint16_t i;
  uint16_t start_idx;
  uint16_t visible_rows = UiRouter_VisibleRows();
  const char *footer_l1 = 0;
  const char *footer_l2 = 0;

  if ((frame == 0) || (frame->menu == 0))
  {
    return;
  }

  menu = frame->menu;
  start_idx = frame->scroll_offset;

  renderClear(RENDER_COLOR_WHITE);
  Render_SetModeIndicator(TH_MODE_STATIC);
  if (menu->title != 0)
  {
    renderDrawText(4U, 8U, menu->title, RENDER_LAYER_UI, RENDER_COLOR_BLACK);
  }

  for (i = 0U; i < visible_rows; i++)
  {
    uint16_t item_idx = (uint16_t)(start_idx + i);
    const char *label = 0;

    if (item_idx >= menu->count)
    {
      break;
    }

    if (menu->items[item_idx].label != 0)
    {
      label = menu->items[item_idx].label;
    }
    else if (menu->items[item_idx].label_fn != 0)
    {
      label = menu->items[item_idx].label_fn(menu->items[item_idx].arg);
    }
    else
    {
      label = "-";
    }

    if (item_idx == frame->selected_index)
    {
      renderDrawText(4U, y, ">", RENDER_LAYER_UI, RENDER_COLOR_BLACK);
      UiRouter_FooterLines(menu->items[item_idx].footer, &footer_l1, &footer_l2);
    }
    renderDrawText(12U, y, label, RENDER_LAYER_UI, RENDER_COLOR_BLACK);
    y = (uint16_t)(y + 10U);
  }

  if (frame->scroll_offset > 0U)
  {
    renderDrawText((uint16_t)(RENDER_WIDTH - 12U), 24U, "^", RENDER_LAYER_UI, RENDER_COLOR_BLACK);
  }
  if ((uint16_t)(frame->scroll_offset + visible_rows) < menu->count)
  {
    renderDrawText((uint16_t)(RENDER_WIDTH - 12U), 94U, "v", RENDER_LAYER_UI, RENDER_COLOR_BLACK);
  }

  if ((footer_l1 == 0) && (footer_l2 == 0))
  {
    UiRouter_FooterLines(menu->footer, &footer_l1, &footer_l2);
  }

  if (footer_l1 != 0)
  {
    renderDrawText(4U, (uint16_t)(RENDER_HEIGHT - 20U), footer_l1, RENDER_LAYER_UI, RENDER_COLOR_BLACK);
  }
  if (footer_l2 != 0)
  {
    renderDrawText(4U, (uint16_t)(RENDER_HEIGHT - 12U), footer_l2, RENDER_LAYER_UI, RENDER_COLOR_BLACK);
  }
}

void UiRouter_Init(ui_router_t *ui, const ui_menu_t *root_menu)
{
  UiRouter_Reset(ui, root_menu);
}

void UiRouter_Reset(ui_router_t *ui, const ui_menu_t *root_menu)
{
  ui_router_action_handler_t prev_handler;

  if ((ui == 0) || (root_menu == 0))
  {
    return;
  }

  prev_handler = ui->action_handler;
  (void)memset(ui, 0, sizeof(*ui));
  ui->action_handler = prev_handler;
  ui->mode = UI_MODE_MENU;
  ui->dirty = 1U;
  ui->tree_depth = 0U;
  ui->current_tree_id = (ui_menu_tree_id_t)UI_TREE_ID_SYSTEM_ROOT;
  ui->state_version = 1U;
  (void)UiRouter_MenuPush(ui, root_menu);
}

void UiRouter_SetActionHandler(ui_router_t *ui, ui_router_action_handler_t handler)
{
  if (ui == 0)
  {
    return;
  }

  ui->action_handler = handler;
}

void UiRouter_OpenPage(ui_router_t *ui, const ui_page_t *page, const void *arg)
{
  if ((ui == 0) || (page == 0))
  {
    return;
  }

  if ((ui->mode == UI_MODE_PAGE) && (ui->current_page != 0) && (ui->current_page->exit != 0))
  {
    ui->current_page->exit(ui);
  }

  ui->current_page = page;
  ui->current_page_arg = arg;
  ui->mode = UI_MODE_PAGE;
  if (ui->current_page->enter != 0)
  {
    ui->current_page->enter(ui, ui->current_page_arg);
  }
  ui->dirty = 1U;
  ui->state_version++;
}

uint8_t UiRouter_OpenTree(ui_router_t *ui, ui_menu_tree_id_t tree_id, const ui_menu_t *root_menu)
{
  if ((ui == 0) || (root_menu == 0))
  {
    return 0U;
  }

  if ((ui->mode == UI_MODE_PAGE) && (ui->current_page != 0) && (ui->current_page->exit != 0))
  {
    ui->current_page->exit(ui);
  }

  ui->mode = UI_MODE_MENU;
  ui->menu_depth = 0U;
  ui->current_page = 0;
  ui->current_page_arg = 0;
  ui->tree_depth = 0U;
  ui->current_tree_id = tree_id;
  (void)memset(ui->menu_stack, 0, sizeof(ui->menu_stack));
  (void)memset(ui->tree_stack, 0, sizeof(ui->tree_stack));
  if (UiRouter_MenuPush(ui, root_menu) == 0U)
  {
    return 0U;
  }
  ui->dirty = 1U;
  ui->state_version++;
  return 1U;
}

uint8_t UiRouter_PushTree(ui_router_t *ui, ui_menu_tree_id_t tree_id, const ui_menu_t *root_menu)
{
  if ((ui == 0) || (root_menu == 0))
  {
    return 0U;
  }

  if (ui->tree_depth >= UI_TREE_STACK_MAX)
  {
    return 0U;
  }

  UiRouter_SnapshotCapture(&ui->tree_stack[ui->tree_depth], ui);
  ui->tree_depth++;

  ui->mode = UI_MODE_MENU;
  ui->menu_depth = 0U;
  ui->current_tree_id = tree_id;
  ui->current_page = 0;
  ui->current_page_arg = 0;
  (void)memset(ui->menu_stack, 0, sizeof(ui->menu_stack));
  if (UiRouter_MenuPush(ui, root_menu) == 0U)
  {
    ui->tree_depth--;
    UiRouter_SnapshotRestore(ui, &ui->tree_stack[ui->tree_depth]);
    return 0U;
  }

  ui->dirty = 1U;
  ui->state_version++;
  return 1U;
}

uint8_t UiRouter_PopTree(ui_router_t *ui)
{
  if ((ui == 0) || (ui->tree_depth == 0U))
  {
    return 0U;
  }

  if ((ui->mode == UI_MODE_PAGE) && (ui->current_page != 0) && (ui->current_page->exit != 0))
  {
    ui->current_page->exit(ui);
  }

  ui->tree_depth--;
  UiRouter_SnapshotRestore(ui, &ui->tree_stack[ui->tree_depth]);
  ui->dirty = 1U;
  ui->state_version++;
  return 1U;
}

uint32_t UiRouter_HandleEvent(ui_router_t *ui, const ui_input_evt_t *evt)
{
  uint32_t result = UI_EVT_RESULT_NONE;

  if ((ui == 0) || (evt == 0))
  {
    return result;
  }

  if (ui->mode == UI_MODE_PAGE)
  {
    if ((ui->current_page != 0) &&
        (ui->current_page->input_policy != 0) &&
        (ui->current_page->input_policy(evt) == 0U))
    {
      return UI_EVT_RESULT_NONE;
    }

    uint8_t is_back_evt = ((evt->evt == UI_EVT_BACK) || (evt->evt == UI_EVT_LONG_BACK)) ? 1U : 0U;

    if ((ui->current_page != 0) && (ui->current_page->event != 0))
    {
      result = ui->current_page->event(ui, evt);
    }

    if (is_back_evt != 0U)
    {
      if ((result & UI_EVT_RESULT_CONSUME_BACK) == 0U)
      {
        if ((ui->current_page != 0) && (ui->current_page->exit != 0))
        {
          ui->current_page->exit(ui);
        }

        ui->current_page = 0;
        ui->current_page_arg = 0;
        ui->mode = UI_MODE_MENU;
        result |= (UI_EVT_RESULT_HANDLED | UI_EVT_RESULT_DIRTY);
      }
      else
      {
        result |= UI_EVT_RESULT_HANDLED;
      }
    }

    if ((result & UI_EVT_RESULT_REQUEST_EXIT) != 0U)
    {
      if ((ui->current_page != 0) && (ui->current_page->exit != 0))
      {
        ui->current_page->exit(ui);
      }
      ui->current_page = 0;
      ui->current_page_arg = 0;
      ui->mode = UI_MODE_MENU;
      result |= UI_EVT_RESULT_DIRTY;
    }

    if ((result & UI_EVT_RESULT_HANDLED) != 0U)
    {
      ui->state_version++;
    }
    if ((result & UI_EVT_RESULT_DIRTY) != 0U)
    {
      ui->dirty = 1U;
    }

    return result;
  }

  {
    ui_menu_stack_frame_t *frame = UiRouter_MenuTopMutable(ui);
    const ui_menu_item_t *item = 0;

    if ((frame == 0) || (frame->menu == 0) || (frame->menu->count == 0U))
    {
      return result;
    }

    if (frame->selected_index >= frame->menu->count)
    {
      frame->selected_index = (uint16_t)(frame->menu->count - 1U);
    }

    switch (evt->evt)
    {
      case UI_EVT_UP:
      case UI_EVT_LEFT:
        if (frame->selected_index > 0U)
        {
          frame->selected_index--;
          if (frame->selected_index < frame->scroll_offset)
          {
            frame->scroll_offset = frame->selected_index;
          }
          result = (UI_EVT_RESULT_HANDLED | UI_EVT_RESULT_DIRTY);
        }
        break;
      case UI_EVT_DOWN:
      case UI_EVT_RIGHT:
        if ((uint32_t)frame->selected_index + 1U < (uint32_t)frame->menu->count)
        {
          uint16_t visible_rows = UiRouter_VisibleRows();
          frame->selected_index++;
          if (frame->selected_index >= (uint16_t)(frame->scroll_offset + visible_rows))
          {
            frame->scroll_offset = (uint16_t)(frame->selected_index - visible_rows + 1U);
          }
          result = (UI_EVT_RESULT_HANDLED | UI_EVT_RESULT_DIRTY);
        }
        break;
      case UI_EVT_BACK:
      case UI_EVT_LONG_BACK:
        if (UiRouter_MenuPop(ui) != 0U)
        {
          result = (UI_EVT_RESULT_HANDLED | UI_EVT_RESULT_DIRTY);
        }
        else if (UiRouter_PopTree(ui) != 0U)
        {
          result = (UI_EVT_RESULT_HANDLED | UI_EVT_RESULT_DIRTY);
        }
        break;
      case UI_EVT_SELECT:
      case UI_EVT_LONG_SELECT:
        item = &frame->menu->items[frame->selected_index];
        if (item->type == UI_MENU_ITEM_SUBMENU)
        {
          if ((item->target.submenu != 0) && (UiRouter_MenuPush(ui, item->target.submenu) != 0U))
          {
            result = (UI_EVT_RESULT_HANDLED | UI_EVT_RESULT_DIRTY);
          }
        }
        else if (item->type == UI_MENU_ITEM_PAGE)
        {
          if (item->target.page != 0)
          {
            ui->current_page = item->target.page;
            ui->current_page_arg = item->arg;
            ui->mode = UI_MODE_PAGE;
            if (ui->current_page->enter != 0)
            {
              ui->current_page->enter(ui, ui->current_page_arg);
            }
            result = (UI_EVT_RESULT_HANDLED | UI_EVT_RESULT_DIRTY);
          }
        }
        else if (item->type == UI_MENU_ITEM_ACTION)
        {
          if (ui->action_handler != 0)
          {
            result |= ui->action_handler(ui, item->target.action.action_id, item->target.action.arg0);
          }
        }
        break;
      default:
        break;
    }
  }

  if ((result & UI_EVT_RESULT_HANDLED) != 0U)
  {
    ui->state_version++;
  }
  if ((result & UI_EVT_RESULT_DIRTY) != 0U)
  {
    ui->dirty = 1U;
  }

  return result;
}

void UiRouter_Render(ui_router_t *ui)
{
  if (ui == 0)
  {
    return;
  }

  if ((ui->mode == UI_MODE_PAGE) && (ui->current_page != 0) && (ui->current_page->render != 0))
  {
    ui->current_page->render(ui);
  }
  else
  {
    UiRouter_RenderMenu(ui);
  }
}

uint8_t UiRouter_IsDirty(const ui_router_t *ui)
{
  if (ui == 0)
  {
    return 0U;
  }

  return ui->dirty;
}

void UiRouter_ClearDirty(ui_router_t *ui)
{
  if (ui == 0)
  {
    return;
  }

  ui->dirty = 0U;
}

uint32_t UiRouter_StateVersion(const ui_router_t *ui)
{
  if (ui == 0)
  {
    return 0U;
  }

  return ui->state_version;
}

const ui_menu_stack_frame_t *UiRouter_CurrentMenu(const ui_router_t *ui)
{
  return UiRouter_MenuTop(ui);
}

const ui_page_t *UiRouter_CurrentPage(const ui_router_t *ui)
{
  if (ui == 0)
  {
    return 0;
  }

  return ui->current_page;
}

ui_menu_tree_id_t UiRouter_CurrentTree(const ui_router_t *ui)
{
  if (ui == 0)
  {
    return (ui_menu_tree_id_t)UI_TREE_ID_SYSTEM_ROOT;
  }

  return ui->current_tree_id;
}

