#include <stdio.h>
#include "esp_log.h"
#include "state_machine.h"
#include "event_handler.h"
#include "button_driver.h"
#include "timer.h"
#include <time.h>
#include <sys/time.h>

static const char *TAG = "state_machine";

/* --- Menu items --- */
static const char *menu_items[] = {
    "Set Real Time",
    "Set Model Time",
    "Set Time Scale",
    "WiFi + SNTP",
    "Test LCD",
    "Other 2",
};
#define MENU_COUNT (sizeof(menu_items) / sizeof(menu_items[0]))
#define MENU_PAGE_ROWS 4

/* --- Application state --- */
static top_state_t state = STATE_INIT;
static menu_substate_t menu_sub = MENU_BROWSE; // Todo: Biztos kell ez ide?
static edit_mode_t edit_mode = EDIT_NONE;

/* menu UI context */
static int menu_selected = 0;
static int menu_scroll_top = 0;

/* timescale edit */
static uint32_t edit_timescale = 1;

/* datetime edit */
static uint32_t edit_timestamp;
static int edit_cursor = 0; // 0..5, field index

static int lcd_test_iterator = 0;

/* forward declarations */
static void state_event_handler(void *handler_arg, esp_event_base_t base, int32_t id, void *event_data);
static void enter_state_clock(void);
static void enter_state_menu(void);
static void enter_state_edit(void);
static void handle_menu_button(button_t btn);
static void handle_edit_button(button_t btn);
static void timer_pause(void);
static void timer_resume(void);

static void apply_real_time(uint32_t ts);
static void apply_model_time(uint32_t ts);
static void apply_timescale(uint32_t ts);

/* getters */

top_state_t get_top_state(void)
{
  return state;
}

menu_substate_t get_menu_substate(void)
{
  return menu_sub;
}

edit_mode_t get_edit_mode(void)
{
  return edit_mode;
}

int get_menu_selected(void)
{
  return menu_selected;
}

int get_menu_scroll_top(void)
{
  return menu_scroll_top;
}

uint32_t get_edit_timescale(void)
{
  return edit_timescale;
}

uint32_t get_edit_timestamp(void)
{
  return edit_timestamp;
}

void set_edit_timestamp(uint32_t ts)
{
  edit_timestamp = ts;
}

int get_edit_cursor(void)
{
  return edit_cursor;
}

int get_menu_count(void)
{
  return (int)MENU_COUNT;
}

const char *get_menu_item(int idx)
{
  if (idx < 0 || idx >= (int)MENU_COUNT)
    return NULL;
  return menu_items[idx]; // returns pointer to read-only string literal
}

int get_lcd_test_iterator(void)
{
  return lcd_test_iterator;
}

/* State machine initialization */

void state_machine_init(void)
{
  ESP_LOGI(TAG, "Initializing state machine");

  state = STATE_INIT;
  menu_sub = MENU_BROWSE;
  edit_mode = EDIT_NONE;

  // Subscribe to button, ticks and timer state events
  events_subscribe(EVENT_BUTTON_PRESS, state_event_handler, NULL);
  // events_subscribe(EVENT_MODEL_TICK, state_event_handler, NULL);
  // events_subscribe(EVENT_TIMER_STATE_CHANGE, state_event_handler, NULL);
  events_subscribe(EVENT_EXIT_INIT_STATE, state_event_handler, NULL);

  ESP_LOGI(TAG, "State machine initialized");
}

static void state_event_handler(void *handler_arg, esp_event_base_t base, int32_t id, void *event_data)
{
  if (base != CUSTOM_EVENTS)
    return;

  ESP_LOGI(TAG, "Base: %s, ID: %d", base, id);
  if (id == EVENT_EXIT_INIT_STATE)
  {
    ESP_LOGI(TAG, "Exiting init state");
    if (state == STATE_INIT)
      enter_state_clock();
  }
  else if (id == EVENT_BUTTON_PRESS)
  {
    if (state == STATE_INIT || state == STATE_RESTART)
      return; // ignore button press in init state

    uint8_t btn = *(uint8_t *)event_data;
    ESP_LOGI(TAG, "Button pressed: %d", btn);

    if (btn == BUTTON_START_STOP)
    {
      if (state == STATE_INIT || state == STATE_RESTART)
        return;

      if (timer_is_running())
        timer_pause();
      else
        timer_resume();

      return; // pause/resume handled globally (state independent)
    }
    else if (btn == BUTTON_MENU)
    {
      /*if (state == STATE_CLOCK)
        enter_state_menu();
      else if (state == STATE_MENU)
        enter_state_clock();*/
      switch (state)
      {
      case STATE_CLOCK:
        menu_selected = 0;
        menu_scroll_top = 0;
        enter_state_menu();
        break;
      
      case STATE_MENU:
      case STATE_EDIT:
        menu_selected = 0;
        menu_scroll_top = 0;
        edit_mode = EDIT_NONE;
        edit_timescale = DEFAULT_TIMESCALE;
        edit_timestamp = 0;
        edit_cursor = 0;

        enter_state_clock();
        break;
      
      default:
        break;
      }

      return; // menu handled globally (state independent)
    }
    
    if (state == STATE_CLOCK)
    {
      switch (btn)
      {
      case BUTTON_CANCEL:
      case BUTTON_OK:
      case BUTTON_LEFT:
      case BUTTON_RIGHT:
      case BUTTON_UP:
      case BUTTON_DOWN:
        // disabled in clock state
      default:
        break;
      }
    }
    else if (state == STATE_MENU)
    {
      handle_menu_button(btn);
    }
    else if (state == STATE_EDIT)
    {
      handle_edit_button(btn);
    }
    else if (state == STATE_LCD_TEST)
    {
      switch (btn)
      {
      case BUTTON_CANCEL:
        enter_state_menu();
        break;
      case BUTTON_UP:
        lcd_test_iterator++;
        if (lcd_test_iterator >= 16 - 3)
          lcd_test_iterator = 0;
        break;
      case BUTTON_DOWN:
        lcd_test_iterator--;
        if (lcd_test_iterator < 0)
          lcd_test_iterator = 16 - 3;
        break;
      case BUTTON_OK:
      case BUTTON_LEFT:
      case BUTTON_RIGHT:
        // disabled in clock state
      default:
        break;
      }
    }
  }
}

static void timer_pause(void)
{
  events_post(EVENT_TIMER_PAUSE, NULL, 0);
  events_post(EVENT_LCD_UPDATE, NULL, 0);
}

static void timer_resume(void)
{
  events_post(EVENT_TIMER_RESUME, NULL, 0);
  events_post(EVENT_LCD_UPDATE, NULL, 0);
}

/* --- State enter/exit --- */
static void enter_state_clock(void)
{
  state = STATE_CLOCK;

  events_post(EVENT_LCD_UPDATE, NULL, 0);
}

static void enter_state_menu(void)
{
  state = STATE_MENU;

  events_post(EVENT_LCD_UPDATE, NULL, 0);
}

static void enter_state_edit(void)
{
  state = STATE_EDIT;

  events_post(EVENT_LCD_UPDATE, NULL, 0);
}

static void enter_state_lcd_test(void)
{
  state = STATE_LCD_TEST;

  events_post(EVENT_LCD_UPDATE, NULL, 0);
}

static void handle_menu_button(button_t btn)
{
  if (btn == BUTTON_UP)
  {
    menu_selected--;
    int max_start = MENU_COUNT > MENU_PAGE_ROWS ? MENU_COUNT - MENU_PAGE_ROWS : 0;
    if (menu_selected < 0)
    {
      menu_selected = MENU_COUNT - 1;
      menu_scroll_top = max_start;
    }

    // scroll if selection moved above the window
    if (menu_selected < menu_scroll_top)
      menu_scroll_top = menu_selected;
    // if wrapped to end, ensure scroll shows the tail
    if (menu_scroll_top > max_start)
      menu_scroll_top = max_start;

    events_post(EVENT_LCD_UPDATE, NULL, 0);
  }
  else if (btn == BUTTON_DOWN)
  {
    menu_selected++;
    if (menu_selected >= MENU_COUNT)
    {
      menu_selected = 0;
      menu_scroll_top = 0;
    }

    // scroll if selection moved below the window
    if (menu_selected >= menu_scroll_top + MENU_PAGE_ROWS)
      menu_scroll_top = menu_selected - MENU_PAGE_ROWS + 1;

    // clamp
    int max_start = MENU_COUNT > MENU_PAGE_ROWS ? MENU_COUNT - MENU_PAGE_ROWS : 0;
    if (menu_scroll_top < 0)
      menu_scroll_top = 0;
    if (menu_scroll_top > max_start)
      menu_scroll_top = max_start;

    events_post(EVENT_LCD_UPDATE, NULL, 0);
  }
  else if (btn == BUTTON_OK)
  {
    // ESP_LOGI(TAG, "Menu selected: %d", menu_selected);
    if (menu_selected == MENU_ITEM_TEST_LCD) // Test LCD
    {
      enter_state_lcd_test();
    }
    else if (menu_selected == MENU_ITEM_REALTIME) // Set Real Time
    {
      edit_mode = EDIT_REALTIME;
      edit_timestamp = time(NULL);
      enter_state_edit();
    }
    else if (menu_selected == MENU_ITEM_MODELTIME) // Set Model Time
    {
      edit_mode = EDIT_MODELTIME;
      edit_timestamp = unix_ts;
      enter_state_edit();
    }
    else if (menu_selected == MENU_ITEM_TIMESCALE) // Set Time Scale
    {
      edit_mode = EDIT_TIMESCALE;
      edit_timescale = timer_get_timescale();
      enter_state_edit();
    }
  }
  else if (btn == BUTTON_CANCEL)
  {
    enter_state_clock();
  }
}

static void handle_edit_button(button_t btn)
{
  if (btn == BUTTON_CANCEL)
  {
    edit_mode = EDIT_NONE;
    enter_state_menu();
  }
  else if (btn == BUTTON_OK)
  {
    if (edit_mode == EDIT_REALTIME)
      apply_real_time(edit_timestamp);
    else if (edit_mode == EDIT_MODELTIME)
      apply_model_time(edit_timestamp);
    else if (edit_mode == EDIT_TIMESCALE)
      apply_timescale(edit_timescale);
    edit_mode = EDIT_NONE;
    enter_state_menu();
  }
  else if (edit_mode == EDIT_REALTIME || edit_mode == EDIT_MODELTIME)
  {
    switch (btn)
    {
    case BUTTON_LEFT:
      edit_cursor--;
      if (edit_cursor < 0)
        edit_cursor = 0;
      events_post(EVENT_LCD_UPDATE, NULL, 0);
      break;

    case BUTTON_RIGHT:
      edit_cursor++;
      if (edit_cursor > 5)
        edit_cursor = 5;
      events_post(EVENT_LCD_UPDATE, NULL, 0);
      break;

    case BUTTON_UP:
    case BUTTON_DOWN:
      int dir = btn == BUTTON_UP ? 1 : -1;
      struct tm tm;
      ts_to_tm(edit_timestamp, &tm);

      switch (edit_cursor)
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

      edit_timestamp = tm_to_ts(&tm);
      events_post(EVENT_LCD_UPDATE, NULL, 0);
      break;
    default:
      break;
    }
  }
  else if (edit_mode == EDIT_TIMESCALE)
  {
    switch (btn)
    {
    case BUTTON_LEFT:
    case BUTTON_RIGHT:
      // disabled
      break;

    case BUTTON_UP:
      edit_timescale++;
      if (edit_timescale > MAX_TIMESCALE)
        edit_timescale = MAX_TIMESCALE;
      events_post(EVENT_LCD_UPDATE, NULL, 0);
      break;

    case BUTTON_DOWN:
      edit_timescale--;
      if (edit_timescale < 1)
        edit_timescale = 1;
      events_post(EVENT_LCD_UPDATE, NULL, 0);
      break;
    
    default:
      break;
    }
  }
}

static void apply_real_time(uint32_t ts)
{
  struct timeval tv = {
      .tv_sec = ts,
      .tv_usec = 0};
  settimeofday(&tv, NULL);
}

static void apply_model_time(uint32_t ts)
{
  unix_ts = ts;
}

static void apply_timescale(uint32_t timescale)
{
  events_post(EVENT_TIMER_SCALE, &timescale, sizeof(timescale));
}