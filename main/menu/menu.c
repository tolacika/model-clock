#include <stdio.h>
#include "menu.h"
#include "menu_table.h"
#include "../event_handler.h"

state_ctx_t state_ctx = {
    .state = STATE_INIT,
    .selected = 0,
    .scroll_top = 0,
    .edit_mode = EDIT_NONE,
    .edit_timestamp = 0,
    .edit_timescale = 0
};
const editor_t *active_editor = NULL;

edit_mode_t get_edit_mode(void)
{
  return state_ctx.edit_mode;
}

int8_t get_menu_selected(void)
{
  return state_ctx.selected;
}

int8_t get_menu_scroll_top(void)
{
  return state_ctx.scroll_top;
}

uint32_t get_edit_timescale(void)
{
  return state_ctx.edit_timescale;
}

uint32_t get_edit_timestamp(void)
{
  return state_ctx.edit_timestamp;
}

int8_t get_edit_cursor(void)
{
  return state_ctx.edit_cursor;
}

const menu_entry_t *get_menu_item(int idx)
{
  if (idx < 0 || idx >= menu_table_count)
    return NULL;
  return menu_table[idx]; // returns pointer to read-only string literal
}



/* --- State enter/exit --- */
void enter_state_clock(void)
{
  state_ctx.state = STATE_CLOCK;
  events_post(EVENT_LCD_UPDATE, NULL, 0);
}

void enter_state_menu(void)
{
  state_ctx.state = STATE_MENU;
  // ensure selected index points to a visible entry
  if (!menu_entry_visible(menu_table[state_ctx.selected]))
  {
    // find first visible
    for (int i = 0; i < get_menu_count(); ++i)
    {
      if (menu_entry_visible(menu_table[i]))
      {
        state_ctx.selected = i;
        break;
      }
    }
  }
  menu_clamp_scroll();
  events_post(EVENT_LCD_UPDATE, NULL, 0);
}

void enter_state_edit(void)
{
  state_ctx.state = STATE_EDIT;

  events_post(EVENT_LCD_UPDATE, NULL, 0);
}

/* Utility functions */

/* Check if menu entry is visible */
bool menu_entry_visible(const menu_entry_t *e)
{
  return (e->visible == NULL) ? true : e->visible();
}

/* Clamp scroll position */
void menu_clamp_scroll(void)
{
  int max_start = get_menu_count() > MENU_PAGE_ROWS ? get_menu_count() - MENU_PAGE_ROWS : 0;
  if (state_ctx.selected < state_ctx.scroll_top)
    state_ctx.scroll_top = state_ctx.selected;
  if (state_ctx.selected >= state_ctx.scroll_top + MENU_PAGE_ROWS)
    state_ctx.scroll_top = state_ctx.selected - MENU_PAGE_ROWS + 1;
  if (state_ctx.scroll_top < 0)
    state_ctx.scroll_top = 0;
  if (state_ctx.scroll_top > max_start)
    state_ctx.scroll_top = max_start;
}

/* Move selection up, skipping invisible items */
void menu_move_up(void)
{
  if (get_menu_count() == 0)
    return;
  //int prev = state_ctx.selected;
  int start = state_ctx.selected;
  do
  {
    state_ctx.selected--;
    if (state_ctx.selected < 0)
      state_ctx.selected = get_menu_count() - 1;
    if (menu_entry_visible(menu_table[state_ctx.selected]))
      break;
  } while (state_ctx.selected != start);

  menu_clamp_scroll();
}

/* Move selection down, skipping invisible items */
void menu_move_down(void)
{
  if (get_menu_count() == 0)
    return;
  //int prev = state_ctx.selected;
  int start = state_ctx.selected;
  do
  {
    state_ctx.selected++;
    if (state_ctx.selected >= get_menu_count())
      state_ctx.selected = 0;
    if (menu_entry_visible(menu_table[state_ctx.selected]))
      break;
  } while (state_ctx.selected != start);

  menu_clamp_scroll();
}

/* Enter edit for the selected menu entry if it has an editor;
   call the entry->editor->begin() if present and set active_editor.
   If entry is MENU_ACTION_FUNC, call its func.
*/
void menu_select(void)
{
  if (get_menu_count() == 0)
    return;

  const menu_entry_t *entry = menu_table[state_ctx.selected];
  if (!menu_entry_visible(entry))
    return;

  if (entry->action == MENU_ACTION_EDIT && entry->editor)
  {
    active_editor = entry->editor;
    if (active_editor->begin)
      active_editor->begin();
    enter_state_edit();
    return;
  }

  if (entry->action == MENU_ACTION_FUNC && entry->func)
  {
    entry->func(entry->arg);
    // function can post further events / change state as needed
    return;
  }

  // nothing for other actions
}