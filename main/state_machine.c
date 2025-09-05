#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include "esp_log.h"
#include "state_machine.h"
#include "event_handler.h"
#include "button_driver.h"
#include "timer.h"
#include "menu/menu.h"
#include "menu/menu_table.h"

static const char *TAG = "state_machine";

static int lcd_test_iterator = 0;

/* forward declarations */
static void state_event_handler(void *handler_arg, esp_event_base_t base, int32_t id, void *event_data);
static void timer_pause(void);
static void timer_resume(void);

/* getters */
int get_lcd_test_iterator(void)
{
  return lcd_test_iterator;
}

/* Helpers */

/* State machine initialization */

void state_machine_init(void)
{
  ESP_LOGI(TAG, "Initializing state machine");

  state_ctx.state = STATE_INIT;
  state_ctx.edit_mode = EDIT_NONE;

  // Subscribe to button, ticks and timer state events
  events_subscribe(EVENT_BUTTON_PRESS, state_event_handler, NULL);
  events_subscribe(EVENT_BUTTON_LONG_PRESS, state_event_handler, NULL);
  events_subscribe(EVENT_BUTTON_REPEATED_PRESS, state_event_handler, NULL);
  events_subscribe(EVENT_BUTTON_RELEASE, state_event_handler, NULL);
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
    if (state_ctx.state == STATE_INIT)
      enter_state_clock();
    return;
  }

  if (id == EVENT_BUTTON_PRESS)
  {
    if (state_ctx.state == STATE_INIT || state_ctx.state == STATE_RESTART)
      return; // ignore button press in init state

    uint8_t btn = *(uint8_t *)event_data;
    ESP_LOGI(TAG, "Button pressed: %d", btn);

    if (btn == BUTTON_START_STOP)
    {
      if (timer_is_running())
        timer_pause();
      else
        timer_resume();

      return; // pause/resume handled globally (state independent)
    }

    if (btn == BUTTON_MENU)
    {
      switch (state_ctx.state)
      {
      case STATE_CLOCK:
        state_ctx.selected = 0;
        // ensure first visible
        for (int i = 0; i < get_menu_count(); ++i)
        {
          if (menu_entry_visible(menu_table[i]))
          {
            state_ctx.selected = i;
            break;
          }
        }
        state_ctx.scroll_top = 0;
        enter_state_menu();
        return;

      case STATE_MENU:
      case STATE_EDIT:
        // cancel edit and return to clock (your previous behavior reset edit state)
        active_editor = NULL;
        state_ctx.edit_mode = EDIT_NONE;
        state_ctx.edit_timescale = DEFAULT_TIMESCALE;
        state_ctx.edit_timestamp = 0;
        state_ctx.edit_cursor = 0;

        enter_state_clock();
        return;

      default:
        break;
      }
    }

    if (state_ctx.state == STATE_CLOCK)
    {
      return; // handled globally, other events are ignored
    }

    if (state_ctx.state == STATE_MENU)
    {
      // handle_menu_button(btn);

      switch (btn)
      {
      case BUTTON_UP:
        menu_move_up();
        events_post(EVENT_LCD_UPDATE, NULL, 0);

        break;
      case BUTTON_DOWN:
        menu_move_down();
        events_post(EVENT_LCD_UPDATE, NULL, 0);
        break;
      case BUTTON_OK:
        menu_select();
        events_post(EVENT_LCD_UPDATE, NULL, 0);
        break;
      case BUTTON_CANCEL:
        enter_state_clock();
        break;
      case BUTTON_LEFT:
      case BUTTON_RIGHT:
      default:
        break;
      }

      return;
    }

    if (state_ctx.state == STATE_EDIT)
    {
      //  When in edit, OK commits, CANCEL cancels; other input forwarded to active editor
      if (btn == BUTTON_OK)
      {
        if (active_editor && active_editor->apply)
          active_editor->apply();
        active_editor = NULL;
        enter_state_menu();
        return;
      }

      if (btn == BUTTON_CANCEL)
      {
        if (active_editor && active_editor->cancel)
          active_editor->cancel();
        active_editor = NULL;
        enter_state_menu();
        return;
      }

      // forward press to editor
      if (active_editor && active_editor->handle_event)
      {
        active_editor->handle_event(id, btn);
      }
      return;
    }

    if (state_ctx.state == STATE_LCD_TEST)
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

      return;
    }

    return;
  }

  if (id == EVENT_BUTTON_LONG_PRESS || id == EVENT_BUTTON_REPEATED_PRESS || id == EVENT_BUTTON_RELEASE)
  {
    if (state_ctx.state == STATE_EDIT && active_editor && active_editor->handle_event)
    {
      uint8_t btn = *(uint8_t *)event_data;
      active_editor->handle_event(id, btn);
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


/*static void enter_state_lcd_test(void)
{
  state_ctx.state = STATE_LCD_TEST;

  events_post(EVENT_LCD_UPDATE, NULL, 0);
}*/

