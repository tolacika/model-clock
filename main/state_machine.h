#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

/* --- State & mode enums --- */
typedef enum
{
  STATE_INIT = 0,
  STATE_CLOCK,
  STATE_MENU,
  STATE_EDIT,
  STATE_LCD_TEST,
  STATE_RESTART,
} top_state_t;

typedef enum
{
  MENU_BROWSE = 0,
  MENU_EDITING,
} menu_substate_t;

typedef enum
{
  MENU_ITEM_NONE = -1,
  MENU_ITEM_REALTIME,
  MENU_ITEM_MODELTIME,
  MENU_ITEM_TIMESCALE,
  MENU_ITEM_WIFI,
  MENU_ITEM_TEST_LCD,
  MENU_ITEM_OTHER_2,
} menu_item_t;

typedef enum
{
  EDIT_NONE = 0,
  EDIT_TIMESCALE,
  EDIT_REALTIME,
  EDIT_MODELTIME,
} edit_mode_t;

void state_machine_init(void);

top_state_t get_top_state(void);
menu_substate_t get_menu_substate(void);
edit_mode_t get_edit_mode(void);
int get_menu_selected(void);
int get_menu_scroll_top(void);
uint32_t get_edit_timescale(void);
uint32_t get_edit_timestamp(void);
void set_edit_timestamp(uint32_t ts);
int get_edit_cursor(void);
int get_menu_count(void);
const char *get_menu_item(int idx);
int get_lcd_test_iterator(void);

#endif