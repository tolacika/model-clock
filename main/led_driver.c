#include "led_driver.h"
#include "driver/gpio.h"
#include "event_handler.h"
#include "timer.h"
#include "esp_log.h"
#include "led_strip.h"

static const char *TAG = "led_driver";

static int target_led_states[LED_COUNT] = {0, 0, 0};
static int current_led_states[LED_COUNT] = {0, 0, 0};
static const gpio_num_t led_pins[LED_COUNT] = {LED_GREEN_PIN, LED_AMBER_PIN, LED_RED_PIN};
static led_strip_handle_t led_strip;
static uint8_t current_colors[3] = {0, 0, 0};
static uint8_t target_colors[3] = {0, 0, 0};

void led_event_handler(void *handler_arg, esp_event_base_t base, int32_t id, void *event_data)
{
  ESP_LOGI(TAG, "Prio: %d, Core: %d", uxTaskPriorityGet(NULL), xPortGetCoreID());
  if (base == CUSTOM_EVENTS && id == EVENT_TIMER_STATE_CHANGE)
  {
    if (timer_is_running())
    {
      target_led_states[LED_GREEN] = 1;
      target_led_states[LED_RED] = 0;
      target_colors[0] = 0;
      target_colors[1] = 16;
      target_colors[2] = 0;
    }
    else
    {
      target_led_states[LED_GREEN] = 0;
      target_led_states[LED_RED] = 1;
      target_colors[0] = 16;
      target_colors[1] = 0;
      target_colors[2] = 0;
    }
  }
}

void status_led_task(void *pvParameter)
{
  while (1)
  {
    for (int i = 0; i < LED_COUNT; i++)
    {
      if (target_led_states[i] != current_led_states[i])
      {
        current_led_states[i] = target_led_states[i];

        gpio_set_level(led_pins[i], current_led_states[i]);
      }
    }

    if (target_colors[0] != current_colors[0] || target_colors[1] != current_colors[1] || target_colors[2] != current_colors[2])
    {
      current_colors[0] = target_colors[0];
      current_colors[1] = target_colors[1];
      current_colors[2] = target_colors[2];

      led_strip_set_pixel(led_strip, 0, current_colors[0], current_colors[1], current_colors[2]);
      led_strip_refresh(led_strip);
    }

    vTaskDelay(pdMS_TO_TICKS(100)); // Adjust delay as needed
  }
}

void led_driver_init(void)
{
  if (timer_is_running())
  {
    target_led_states[LED_GREEN] = 1;
    target_led_states[LED_RED] = 0;
    target_colors[0] = 0;
    target_colors[1] = 16;
    target_colors[2] = 0;
  }
  else
  {
    target_led_states[LED_GREEN] = 0;
    target_led_states[LED_RED] = 1;
    target_colors[0] = 16;
    target_colors[1] = 0;
    target_colors[2] = 0;
  }

  for (int i = 0; i < LED_COUNT; i++)
  {
    gpio_set_direction(led_pins[i], GPIO_MODE_OUTPUT);
    gpio_set_level(led_pins[i], target_led_states[i]);
  }

  /* LED strip initialization with the GPIO and pixels number*/
  led_strip_config_t strip_config = {
      .strip_gpio_num = LED_NEOPIXEL_PIN,
      .max_leds = 1, // at least one LED on board
  };

  led_strip_rmt_config_t rmt_config = {
      .resolution_hz = 10 * 1000 * 1000, // 10MHz
      .flags.with_dma = false,
  };
  ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));

  led_strip_clear(led_strip);

  xTaskCreatePinnedToCore(status_led_task, "status_led_task", 2048, NULL, 8, NULL, tskNO_AFFINITY);

  events_subscribe(EVENT_TIMER_STATE_CHANGE, led_event_handler, NULL);
}