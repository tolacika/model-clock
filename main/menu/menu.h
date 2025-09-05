#ifndef MENU_H
#define MENU_H

#include <stdint.h>
#include <stdbool.h>

/* small forward declarations for editors */
typedef struct editor_s editor_t;

typedef enum {
  MENU_ACTION_NONE = 0,
  MENU_ACTION_EDIT,
  MENU_ACTION_FUNC,
  MENU_ACTION_SUBMENU,
} menu_action_t;

typedef struct menu_entry_s {
  const char *label;
  menu_action_t action;
  const editor_t *editor;        // non-NULL when action == MENU_ACTION_EDIT
  void (*func)(void *arg);       // callback for MENU_ACTION_FUNC
  void *arg;
  bool (*visible)(void);         // optional visibility predicate (can be NULL)
} menu_entry_t;

/* Editor API — implement as needed; state machine will call these */
struct editor_s {
  void (*begin)(void);                               // called when entering edit
  void (*handle_event)(int32_t event_id, uint8_t btn); // press/repeat/long/release
  void (*apply)(void);                               // commit changes
  void (*cancel)(void);                              // cancel changes
  const char *(*render_line)(int row);               // optional per-row text for LCD
};

typedef enum
{
  STATE_INIT = 0,
  STATE_CLOCK,
  STATE_MENU,
  STATE_EDIT,
  STATE_LCD_TEST,
  STATE_RESTART,
} app_state_t;

typedef enum
{
  EDIT_NONE = 0,
  EDIT_TIMESCALE,
  EDIT_REALTIME,
  EDIT_MODELTIME,
} edit_mode_t;

typedef struct
{
  app_state_t state;
  int8_t selected;
  int8_t scroll_top;
  edit_mode_t edit_mode;
  uint32_t edit_timestamp;
  uint32_t edit_timescale;
  int8_t edit_cursor;
} state_ctx_t;

extern state_ctx_t state_ctx;
extern const editor_t *active_editor;

edit_mode_t get_edit_mode(void);
int8_t get_menu_selected(void);
int8_t get_menu_scroll_top(void);
uint32_t get_edit_timescale(void);
uint32_t get_edit_timestamp(void);
int8_t get_edit_cursor(void);
const menu_entry_t *get_menu_item(int idx);

bool menu_entry_visible(const menu_entry_t *e);
void menu_clamp_scroll(void);
void menu_move_up(void);
void menu_move_down(void);
void menu_select(void);

void enter_state_clock(void);
void enter_state_menu(void);
void enter_state_edit(void);

#endif