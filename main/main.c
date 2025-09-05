#include <stdio.h>
#include "esp_log.h"
#include "lcd_driver.h"
#include "timer.h"
#include "event_handler.h"
#include "esp_random.h"
#include "output_driver.h"
#include "button_driver.h"
#include "state_machine.h"
#include "storage.h"

static const char *TAG = "main";

void tick_logger_handler(void *handler_arg, esp_event_base_t base, int32_t id, void *event_data)
{
  uint32_t tick_val = *(uint32_t *)event_data;
  char buf[21];
  format_datetime(tick_val, buf, sizeof(buf));
  ESP_LOGD(TAG, "Tick event from handler: model time: %s", buf);
}

// ----------------------
// Main entry
// ----------------------
void app_main(void)
{
  storage_init();

  events_init();

  events_subscribe(EVENT_MODEL_TICK, tick_logger_handler, NULL);

  output_driver_init();

  button_init();

  ESP_LOGI(TAG, "Initializing Model Timer");
  timer_initialize();

  // init i2c and lcd
  i2c_initialize();
  lcd_initialize();

  state_machine_init();

  storage_load();

  vTaskDelay(pdMS_TO_TICKS(3000));

  //lcd_set_screen_state(LCD_SCREEN_START_SCREEN);
  events_post(EVENT_EXIT_INIT_STATE, NULL, 0);

  // Core 0 could run other system stuff here
  while (true)
  {
    vTaskDelay(pdMS_TO_TICKS(60 * 1000));

    storage_save();

    ESP_LOGI(TAG, "Heartbeat, real=%llu, model=%lu", time(NULL), unix_ts);
  }
}
