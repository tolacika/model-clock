#include "menu.h"
#include <sys/time.h>
#include <time.h>
#include "../timer.h"
#include "../button_driver.h"
#include "../event_handler.h"

extern void editor_general_cancel(void);

// Local editor implementation (wraps the existing edit fields)
static void realtime_begin(void)
{
  state_ctx.edit_mode = EDIT_REALTIME;
  state_ctx.edit_cursor = 0;
  state_ctx.edit_timestamp = time(NULL);
}

static void modeltime_begin(void)
{
  state_ctx.edit_mode = EDIT_MODELTIME;
  state_ctx.edit_cursor = 0;
  state_ctx.edit_timestamp = unix_ts;
}

static void datetime_handle(int32_t event_id, uint8_t btn)
{
  if (event_id == EVENT_BUTTON_PRESS)
  {
    if (state_ctx.edit_mode == EDIT_REALTIME || state_ctx.edit_mode == EDIT_MODELTIME)
    {
      if (btn == BUTTON_LEFT || btn == BUTTON_RIGHT)
      {
        state_ctx.edit_cursor += btn == BUTTON_LEFT ? -1 : 1;
        if (state_ctx.edit_cursor < 0)
          state_ctx.edit_cursor = 0;
        if (state_ctx.edit_cursor > 5)
          state_ctx.edit_cursor = 5;
        events_post(EVENT_LCD_UPDATE, NULL, 0);
      }
    }
  }

  if (event_id == EVENT_BUTTON_PRESS || event_id == EVENT_BUTTON_REPEATED_PRESS)
  {
    if (state_ctx.edit_mode == EDIT_REALTIME || state_ctx.edit_mode == EDIT_MODELTIME)
    {
      if (btn == BUTTON_UP || btn == BUTTON_DOWN)
      {
        int dir = btn == BUTTON_UP ? 1 : -1;
        struct tm tm;
        ts_to_tm(state_ctx.edit_timestamp, &tm);

        switch (state_ctx.edit_cursor)
        {
        case 0: // year
          tm.tm_year += dir;
          break;

        case 1: // month
          tm.tm_mon += dir;
          break;

        case 2: // day
          tm.tm_mday += dir;
          break;

        case 3: // hour
          tm.tm_hour += dir;
          break;

        case 4: // minute
          tm.tm_min += dir;
          break;

        case 5: // second
          tm.tm_sec += dir;
          break;

        default:
          break;
        }

        state_ctx.edit_timestamp = tm_to_ts(&tm);
        events_post(EVENT_LCD_UPDATE, NULL, 0);
      }
    }
  }
}

static void realtime_apply(void)
{
  struct timeval tv = {
      .tv_sec = state_ctx.edit_timestamp,
      .tv_usec = 0};
  settimeofday(&tv, NULL);
}

static void modeltime_apply(void)
{
  unix_ts = state_ctx.edit_timestamp;
}

static const char *realtime_render(int row)
{
  // return a string for LCD row (must point to static buffer)
  return NULL;
}

/* editor instance */
static const editor_t realtime_editor = {
    .begin = realtime_begin,
    .handle_event = datetime_handle,
    .apply = realtime_apply,
    .cancel = editor_general_cancel,
    .render_line = realtime_render};

/* final menu entry (exported symbol referenced by menu_table.c) */
const menu_entry_t menu_item_realtime = {
    .label = "Set Real Time",
    .action = MENU_ACTION_EDIT,
    .editor = &realtime_editor,
    .func = NULL,
    .arg = NULL,
    .visible = NULL};

/* editor instance */
static const editor_t modeltime_editor = {
    .begin = modeltime_begin,
    .handle_event = datetime_handle,
    .apply = modeltime_apply,
    .cancel = editor_general_cancel,
    .render_line = realtime_render};

/* final menu entry (exported symbol referenced by menu_table.c) */
const menu_entry_t menu_item_modeltime = {
    .label = "Set Real Time",
    .action = MENU_ACTION_EDIT,
    .editor = &modeltime_editor,
    .func = NULL,
    .arg = NULL,
    .visible = NULL};