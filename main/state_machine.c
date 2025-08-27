#include <stdio.h>
#include "esp_log.h"
#include "state_machine.h"
#include "event_handler.h"
#include "button_driver.h"
#include "timer.h"

static const char *TAG = "state_machine";

/* --- State & mode enums --- */
typedef enum
{
  STATE_INIT = 0,
  STATE_CLOCK,
  STATE_MENU,
  STATE_EDIT,
  STATE_RESTART,
} top_state_t;

typedef enum
{
  MENU_BROWSE = 0,
  MENU_EDITING,
} menu_substate_t;

typedef enum
{
  EDIT_NONE = 0,
  EDIT_TIMESCALE,
  EDIT_REALTIME,
  EDIT_MODELTIME,
} edit_mode_t;

/* --- Menu items --- */
static const char *menu_items[] = {
    "Set Real Time",
    "Set Model Time",
    "Set Time Scale",
    "WiFi + SNTP",
    "Other 1",
    "Other 2",
};
#define MENU_COUNT (sizeof(menu_items) / sizeof(menu_items[0]))
#define MENU_PAGE_ROWS 4

/* --- Application state --- */
static top_state_t state = STATE_CLOCK;
static menu_substate_t menu_sub = MENU_BROWSE;
static edit_mode_t edit_mode = EDIT_NONE;

/* menu UI context */
static int menu_selected = 0;
static int menu_scroll_top = 0;

/* timescale edit */
static uint32_t edit_timescale = 1;

/* datetime edit */
struct small_datetime
{
  int year, month, day, hour, min, sec;
};
static struct small_datetime edit_dt;
static int edit_cursor = 0; // 0..5, field index

/* forward declarations */
static void state_event_handler(void *handler_arg, esp_event_base_t base, int32_t id, void *event_data);
static void enter_state_clock(void);

void state_machine_init(void)
{
  ESP_LOGI(TAG, "Initializing state machine");

  state = STATE_INIT;
  menu_sub = MENU_BROWSE;
  edit_mode = EDIT_NONE;

  // Subscribe to button, ticks and timer state events
  events_subscribe(EVENT_BUTTON_PRESS, state_event_handler, NULL);
  events_subscribe(EVENT_MODEL_TICK, state_event_handler, NULL);
  events_subscribe(EVENT_TIMER_STATE_CHANGE, state_event_handler, NULL);
  events_subscribe(EVENT_EXIT_INIT_STATE, state_event_handler, NULL);

  ESP_LOGI(TAG, "State machine initialized");
}

static void state_event_handler(void *handler_arg, esp_event_base_t base, int32_t id, void *event_data)
{
  if (base != CUSTOM_EVENTS)
    return;

  if (id == EVENT_EXIT_INIT_STATE)
  {
    if (state == STATE_INIT)
      enter_state_clock();
  }
  else if (id == EVENT_BUTTON_PRESS)
  {
    if (state == STATE_INIT || state == STATE_RESTART)
      return; // ignore button press in init state

    button_t btn = *(button_t *)event_data;
    ESP_LOGI(TAG, "Button pressed: %d", btn);

    if (btn == BUTTON_START_STOP)
    {
      if (timer_is_running())
        timer_pause();
      else
        timer_resume();

      return; // pause/resume handled globally (state independent)
    }
    else if (btn == BUTTON_MENU)
    {
      if (state == STATE_CLOCK)
        enter_state_menu();
      else if (state == STATE_MENU)
        enter_state_clock();

      return; // menu handled globally (state independent)
    }

    if (state == STATE_CLOCK)
    {
    }
    else if (state == STATE_MENU)
    {
    }
    else if (state == STATE_EDIT)
    {
    }
  }
}

/* --- State enter/exit --- */
static void enter_state_clock(void)
{
  state = STATE_CLOCK;

  // Render one frame now
  // render_clock_screen_now();
}
