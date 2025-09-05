#include "menu.h"
#include "../timer.h"
#include "../button_driver.h"
#include "../event_handler.h"

extern void editor_general_cancel(void);

void timescale_begin(void)
{
  state_ctx.edit_mode = EDIT_TIMESCALE;
  state_ctx.edit_cursor = 0;
  state_ctx.edit_timescale = timer_get_timescale();

  enter_state_menu();
}

void timescale_handle(int32_t event_id, uint8_t btn)
{
  if (event_id == EVENT_BUTTON_PRESS || event_id == EVENT_BUTTON_REPEATED_PRESS)
  {
    if (state_ctx.edit_mode == EDIT_TIMESCALE)
    {
      if (btn == BUTTON_UP || btn == BUTTON_DOWN)
      {
        int dir = btn == BUTTON_UP ? 1 : -1;
        state_ctx.edit_timescale += dir;
        if (state_ctx.edit_timescale < 1)
          state_ctx.edit_timescale = 1;
        if (state_ctx.edit_timescale > MAX_TIMESCALE)
          state_ctx.edit_timescale = MAX_TIMESCALE;
        events_post(EVENT_LCD_UPDATE, NULL, 0);
      }
    }
  }
}

void timescale_apply(void)
{
  events_post(EVENT_TIMER_SCALE, &state_ctx.edit_timescale, sizeof(state_ctx.edit_timescale));
}

const char *timescale_render(int row)
{
  // return a string for LCD row (must point to static buffer)
  return NULL;
}

/* editor instance */
static const editor_t timescale_editor = {
    .begin = timescale_begin,
    .handle_event = timescale_handle,
    .apply = timescale_apply,
    .cancel = editor_general_cancel,
    .render_line = timescale_render};

/* final menu entry (exported symbol referenced by menu_table.c) */
const menu_entry_t menu_item_timescale = {
    .label = "Set Time Scale",
    .action = MENU_ACTION_EDIT,
    .editor = &timescale_editor,
    .func = NULL,
    .arg = NULL,
    .visible = NULL};