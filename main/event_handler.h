#ifndef EVENT_HANDLER_H
#define EVENT_HANDLER_H

#include <stdint.h>
#include "esp_event.h"

// Declare an event base
ESP_EVENT_DECLARE_BASE(CUSTOM_EVENTS); // declaration of the timer events family

// Event types
enum
{
  EVENT_MODEL_TICK,         // Event for model tick
  EVENT_MODEL_MINUTE_TICK,  // Event for model tick
  EVENT_BUTTON_PRESS,       // Event for button press
  EVENT_RESTART_REQUESTED,  // Event for restart requested
  EVENT_TIMER_RESUME,       // Event for timer resume
  EVENT_TIMER_PAUSE,        // Event for timer pause
  EVENT_TIMER_SCALE,        // Event for timer scale
  EVENT_TIMER_STATE_CHANGE, // Event for timer state change
  EVENT_LCD_UPDATE,         // Event for LCD update
  EVENT_EXIT_INIT_STATE,    // Event for exit init state
};

void events_init(void);

void events_post(int32_t event_id, const void *event_data, size_t event_data_size);

void events_subscribe(int32_t event_id, esp_event_handler_t event_handler, void *event_handler_arg);

#endif