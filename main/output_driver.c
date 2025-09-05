#include "output_driver.h"
#include "driver/gpio.h"
#include "event_handler.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "timer.h"
#include "esp_log.h"
#include "led_strip.h"
#include <string.h>
#include <stdlib.h>

#define CLOCK_CHANNEL_COUNT 3

static const char *TAG = "output_driver";

/* clock channel config */
typedef struct
{
  int pin;           // -1 = disabled
  bool enabled;      // enabled if pin >= 0 (and optionally toggled by config)
  uint32_t pulse_ms; // ON time per pulse
  uint32_t gap_ms;   // gap between pulses
  uint8_t count;     // number of pulses in a sequence
  const char *name;  // friendly name for logs
} clock_channel_t;

/* Pulse worker param — allocated on heap then freed by worker */
typedef struct
{
  int pin;
  uint32_t pulse_ms;
  uint32_t gap_ms;
  uint8_t count;
  const char *name;
} pulse_worker_arg_t;

/* discrete LED pins (active-high) */
static int pin_green = -1;
static int pin_red = -1;

/* clock channels array */
static clock_channel_t clock_channels[CLOCK_CHANNEL_COUNT];

/* neopixel strip handle (NULL if not present) */
static led_strip_handle_t strip = NULL;

/* current neopixel color stored so pulses (if implemented later) can restore it */
static uint8_t np_r = 0, np_g = 0, np_b = 0;
static SemaphoreHandle_t np_mutex = NULL;

/* forward declarations */
static inline void safe_gpio_set(int pin, int level);
static void _set_neopixel_rgb_locked(uint8_t r, uint8_t g, uint8_t b);

/* Externally invoked when timer state changes. Keep the handler minimal and fast. */
static void timer_state_handler(void *handler_arg, esp_event_base_t base, int32_t id, void *event_data)
{
  // fast check
  bool running = timer_is_running();

  // green on when running, red on when paused
  safe_gpio_set(pin_green, running ? 1 : 0);
  safe_gpio_set(pin_red, running ? 0 : 1);

  // set neopixel color to green/red and store it
  if (np_mutex)
    xSemaphoreTake(np_mutex, portMAX_DELAY);
  if (running)
  {
    _set_neopixel_rgb_locked(0, 255, 0); // green
  }
  else
  {
    _set_neopixel_rgb_locked(255, 0, 0); // red
  }
  if (np_mutex)
    xSemaphoreGive(np_mutex);
}

/* Worker task that executes pulse sequence for a single channel, then deletes itself */
static void pulse_worker(void *pv)
{
  pulse_worker_arg_t *a = (pulse_worker_arg_t *)pv;
  if (!a)
    vTaskDelete(NULL);

  ESP_LOGD(TAG, "Pulse worker started for %s pin=%d pulse=%ums gap=%ums count=%u",
           a->name, a->pin, (unsigned)a->pulse_ms, (unsigned)a->gap_ms, (unsigned)a->count);

  // Run sequence: turn on -> wait pulse_ms -> turn off -> gap -> repeat
  for (uint8_t i = 0; i < a->count; ++i)
  {
    if (a->pin >= 0)
    {
      safe_gpio_set(a->pin, 1);
    }
    vTaskDelay(pdMS_TO_TICKS(a->pulse_ms));
    if (a->pin >= 0)
    {
      safe_gpio_set(a->pin, 0);
    }
    if (i + 1 < a->count)
    {
      vTaskDelay(pdMS_TO_TICKS(a->gap_ms));
    }
  }

  ESP_LOGD(TAG, "Pulse worker finished for %s", a->name);

  // free args and exit
  vPortFree(a);
  vTaskDelete(NULL);
}

/* Minute tick handler: spawn a worker per enabled channel so pulses can run in parallel.
   Called on EVENT_MODEL_MINUTE_TICK.
*/
static void minute_tick_handler(void *handler_arg, esp_event_base_t base, int32_t id, void *event_data)
{
    // iterate channels and create a worker for each enabled channel with valid pin
    for (int i = 0; i < CLOCK_CHANNEL_COUNT; ++i) {
        clock_channel_t *ch = &clock_channels[i];
        if (!ch->enabled || ch->pin < 0) continue;

        // allocate args for worker
        pulse_worker_arg_t *arg = pvPortMalloc(sizeof(pulse_worker_arg_t));
        if (!arg) {
            ESP_LOGW(TAG, "Failed to allocate pulse_worker_arg for channel %s", ch->name);
            continue;
        }
        arg->pin = ch->pin;
        arg->pulse_ms = ch->pulse_ms;
        arg->gap_ms = ch->gap_ms;
        arg->count = ch->count;
        arg->name = ch->name;

        // spawn worker with small stack (task will delete itself)
        BaseType_t ok = xTaskCreatePinnedToCore(pulse_worker, "pulse_worker", 2048, arg, 8, NULL, tskNO_AFFINITY);
        if (ok != pdPASS) {
            ESP_LOGW(TAG, "Failed to create pulse worker for %s", ch->name);
            vPortFree(arg);
        }
    }
}

/* initialize clock channel table from CONFIG values and defaults */
static void init_clock_channels(void)
{
    // CH0 maps to the amber LED pin
    clock_channels[0].pin = (CONFIG_CLOCK_OUT_CH0_GPIO >= 0) ? CONFIG_CLOCK_OUT_CH0_GPIO : -1;
    clock_channels[0].enabled = (clock_channels[0].pin >= 0);
    clock_channels[0].pulse_ms = CONFIG_OUTPUT_CHANNEL_DEFAULT_PERIOD_MS;
    clock_channels[0].gap_ms   = CONFIG_OUTPUT_CHANNEL_DEFAULT_GAP_MS;
    clock_channels[0].count    = CONFIG_OUTPUT_CHANNEL_DEFAULT_PULSE_COUNT;
    clock_channels[0].name     = "CH0";

    // CH1
    clock_channels[1].pin = (CONFIG_CLOCK_OUT_CH1_GPIO >= 0) ? CONFIG_CLOCK_OUT_CH1_GPIO : -1;
    clock_channels[1].enabled = (clock_channels[1].pin >= 0);
    clock_channels[1].pulse_ms = CONFIG_OUTPUT_CHANNEL_DEFAULT_PERIOD_MS;
    clock_channels[1].gap_ms   = CONFIG_OUTPUT_CHANNEL_DEFAULT_GAP_MS;
    clock_channels[1].count    = CONFIG_OUTPUT_CHANNEL_DEFAULT_PULSE_COUNT;
    clock_channels[1].name     = "CH1";

    // CH2
    clock_channels[2].pin = (CONFIG_CLOCK_OUT_CH2_GPIO >= 0) ? CONFIG_CLOCK_OUT_CH2_GPIO : -1;
    clock_channels[2].enabled = (clock_channels[2].pin >= 0);
    clock_channels[2].pulse_ms = CONFIG_OUTPUT_CHANNEL_DEFAULT_PERIOD_MS;
    clock_channels[2].gap_ms   = CONFIG_OUTPUT_CHANNEL_DEFAULT_GAP_MS;
    clock_channels[2].count    = CONFIG_OUTPUT_CHANNEL_DEFAULT_PULSE_COUNT;
    clock_channels[2].name     = "CH2";
}

/* Initialize GPIOs and neopixel, subscribe to events */
void output_driver_init(void)
{
    ESP_LOGI(TAG, "Initializing output_driver");

    /* discrete LEDs */
    pin_green = (CONFIG_LED_GREEN_GPIO >= 0) ? CONFIG_LED_GREEN_GPIO : -1;
    pin_red   = (CONFIG_LED_RED_GPIO   >= 0) ? CONFIG_LED_RED_GPIO   : -1;

    init_clock_channels();

    /* configure output GPIOs: green/red and all clock pins */
    uint64_t pin_mask = 0;
    if (pin_green >= 0) pin_mask |= (1ULL << pin_green);
    if (pin_red   >= 0) pin_mask |= (1ULL << pin_red);
    for (int i = 0; i < CLOCK_CHANNEL_COUNT; ++i) {
        if (clock_channels[i].pin >= 0) pin_mask |= (1ULL << clock_channels[i].pin);
    }

    if (pin_mask) {
        gpio_config_t cfg = {
            .pin_bit_mask = pin_mask,
            .mode = GPIO_MODE_OUTPUT,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .intr_type = GPIO_INTR_DISABLE
        };
        gpio_config(&cfg);
        // default all outputs low (off)
        if (pin_green >= 0) safe_gpio_set(pin_green, 0);
        if (pin_red   >= 0) safe_gpio_set(pin_red,   0);
        for (int i = 0; i < CLOCK_CHANNEL_COUNT; ++i) {
            if (clock_channels[i].pin >= 0) safe_gpio_set(clock_channels[i].pin, 0);
        }
    }

    /* neopixel init */
    int np_pin = (CONFIG_NEOPIXEL_GPIO >= 0) ? CONFIG_NEOPIXEL_GPIO : -1;
    if (np_pin >= 0) {
        led_strip_config_t strip_cfg = {
            .strip_gpio_num = np_pin,
            .max_leds = 1,
        };
        led_strip_rmt_config_t rmt_cfg = {
            .resolution_hz = 10 * 1000 * 1000,
            .flags.with_dma = false,
        };
        if (led_strip_new_rmt_device(&strip_cfg, &rmt_cfg, &strip) == ESP_OK) {
            led_strip_clear(strip);
            ESP_LOGI(TAG, "NeoPixel initialized on GPIO %d", np_pin);
        } else {
            strip = NULL;
            ESP_LOGW(TAG, "NeoPixel initialization failed on GPIO %d", np_pin);
        }
    } else {
        strip = NULL;
    }

    /* mutex for neopixel color */
    np_mutex = xSemaphoreCreateMutex();

    /* subscribe to events */
    events_subscribe(EVENT_MODEL_MINUTE_TICK, minute_tick_handler, NULL);
    events_subscribe(EVENT_TIMER_STATE_CHANGE, timer_state_handler, NULL);

    /* ensure LEDs/neopixel reflect current timer state immediately */
    timer_state_handler(NULL, CUSTOM_EVENTS, EVENT_TIMER_STATE_CHANGE, NULL);

    ESP_LOGI(TAG, "output_driver initialized (CH0=%d CH1=%d CH2=%d)",
             clock_channels[0].pin, clock_channels[1].pin, clock_channels[2].pin);
}

/* Helper to set a GPIO safely (no-op when pin < 0). */
static inline void safe_gpio_set(int pin, int level)
{
  if (pin < 0)
    return;
  gpio_set_level((gpio_num_t)pin, level ? 1 : 0);
}

/* Helper to set neopixel color and remember the color.
   Caller should hold np_mutex when called if needed for atomicity.
*/
static void _set_neopixel_rgb_locked(uint8_t r, uint8_t g, uint8_t b)
{
  if (!strip)
    return;
  led_strip_set_pixel(strip, 0, r, g, b);
  led_strip_refresh(strip);
  np_r = r;
  np_g = g;
  np_b = b;
}