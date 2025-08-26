#include <stdio.h>
#include "esp_log.h"
#include "lcd_driver.h"
#include "timer.h"
#include "event_handler.h"
#include "esp_random.h"
#include "led_driver.h"
#include "button_driver.h"

static const char *TAG = "main";

void tick_logger_handler(void *handler_arg, esp_event_base_t base, int32_t id, void *event_data)
{
  uint32_t tick_val = *(uint32_t *)event_data;
  char buf[21];
  format_time(tick_val, buf, sizeof(buf));
  ESP_LOGD(TAG, "Tick event from handler: model time: %s", buf);
}

// ----------------------
// Main entry
// ----------------------
void app_main(void)
{
  events_init();

  events_subscribe(EVENT_MODEL_TICK, tick_logger_handler, NULL);

  led_driver_init();

  button_init();

  ESP_LOGI(TAG, "Initializing Model Timer");
  timer_initialize();

  // init i2c and lcd
  i2c_initialize();
  lcd_initialize();

  vTaskDelay(pdMS_TO_TICKS(3000));

  lcd_set_screen_state(LCD_SCREEN_START_SCREEN);

  // Core 0 could run other system stuff here
  while (true)
  {
    vTaskDelay(pdMS_TO_TICKS(10000));
    ESP_LOGI(TAG, "Heartbeat (Core 0), model_ts=%lu", unix_ts);

    if (timer_is_running())
    {
      events_post(EVENT_TIMER_PAUSE, NULL, 0);
    }
    else
    {
      // pick a semirandom timescale from allowed values
      uint32_t candidates[] = {1, 2, 6, 12, 20, 30, 60};
      uint8_t idx = esp_random() % (sizeof(candidates) / sizeof(candidates[0]));
      uint32_t ts = candidates[idx];

      events_post(EVENT_TIMER_SCALE, &ts, sizeof(ts));
      events_post(EVENT_TIMER_RESUME, NULL, 0);
    }
  }
}
