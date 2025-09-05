#include "button_driver.h"
#include "event_handler.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

#define GPIO_EVT_QUEUE_SIZE 16

static const char *TAG = "button_driver";
// List of button GPIOs
static const int button_gpio_values[BUTTON__COUNT] = {
    CONFIG_BUTTON_START_STOP_GPIO,
    CONFIG_BUTTON_MENU_GPIO,
    CONFIG_BUTTON_LEFT_GPIO,
    CONFIG_BUTTON_RIGHT_GPIO,
    CONFIG_BUTTON_UP_GPIO,
    CONFIG_BUTTON_DOWN_GPIO,
    CONFIG_BUTTON_CANCEL_GPIO,
    CONFIG_BUTTON_OK_GPIO,
};

/* internal state */
static QueueHandle_t gpio_evt_queue = NULL;
static TickType_t last_isr_tick[BUTTON__COUNT];
static uint32_t cfg_longpress_ms = CONFIG_BUTTON_LONG_PRESS_MS;
static uint32_t cfg_repeat_ms = CONFIG_BUTTON_REPEAT_DELAY_MS;

/* forward declaration */
static int gpio_to_button_idx(gpio_num_t gpio);

/* ISR: minimal work, apply coarse debounce, push gpio number to queue */
static void IRAM_ATTR gpio_isr_handler(void *arg)
{
  uint32_t gpio_num = (uint32_t)arg;
  TickType_t now_tick = xTaskGetTickCountFromISR();

  int idx = gpio_to_button_idx((gpio_num_t)gpio_num);
  if (idx < 0)
    return;

  if ((now_tick - last_isr_tick[idx]) >= pdMS_TO_TICKS(CONFIG_BUTTON_ISR_DEBOUNCE_MS))
  {
    last_isr_tick[idx] = now_tick;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (gpio_evt_queue)
    {
      xQueueSendFromISR(gpio_evt_queue, &gpio_num, &xHigherPriorityTaskWoken);
      if (xHigherPriorityTaskWoken)
        portYIELD_FROM_ISR();
    }
  }
}

/* Main button task:
 * - receives gpio numbers from ISR
 * - validates press (active-low check + settle delay)
 * - posts EVENT_BUTTON_PRESS for all buttons
 * - for UP/DOWN:
 *     wait longpress_ms; if still held -> post EVENT_BUTTON_LONG_PRESS
 *     then post EVENT_BUTTON_REPEAT every repeat_ms until release
 *     on release post EVENT_BUTTON_RELEASE
 */
static void button_task(void *pv)
{
  uint32_t io_num;
  while (1) {
    if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY) == pdTRUE) {
      int idx = gpio_to_button_idx((gpio_num_t)io_num);
      if (idx < 0) continue;

      // Small settle to filter bounce more thoroughly
      vTaskDelay(pdMS_TO_TICKS(CONFIG_BUTTON_TASK_DEBOUNCE_MS));

      // Confirm it's still a press (active-low)
      int lvl = gpio_get_level((gpio_num_t)io_num);
      if (lvl != 0) {
        // spurious ISR or release -- ignore (we only handle press here)
        continue;
      }

      // Post immediate press event (single-step)
      uint8_t button_id = (uint8_t)idx;
      events_post(EVENT_BUTTON_PRESS, &button_id, sizeof(button_id));

      // Only UP/DOWN have longpress/repeat/release sequence
      if (idx == BUTTON_UP || idx == BUTTON_DOWN) {
        // wait for longpress timeout while polling for release
        TickType_t start = xTaskGetTickCount();
        TickType_t deadline = start + pdMS_TO_TICKS(cfg_longpress_ms);
        bool still_pressed = true;
        while (xTaskGetTickCount() < deadline) {
          vTaskDelay(pdMS_TO_TICKS(20));
          int lvl2 = gpio_get_level((gpio_num_t)io_num);
          if (lvl2 != 0) { still_pressed = false; break; } // released before longpress
        }

        if (!still_pressed) {
          // released before longpress -> nothing else to post here
          continue;
        }

        // long press detected
        events_post(EVENT_BUTTON_LONG_PRESS, &button_id, sizeof(button_id));

        // begin repeating until release
        while (1) {
          vTaskDelay(pdMS_TO_TICKS(cfg_repeat_ms));
          int lvl3 = gpio_get_level((gpio_num_t)io_num);
          if (lvl3 != 0) {
            // released -> break and post release event
            events_post(EVENT_BUTTON_RELEASE, &button_id, sizeof(button_id));
            break;
          }
          // still held -> send repeat
          events_post(EVENT_BUTTON_REPEATED_PRESS, &button_id, sizeof(button_id));
        }
      }

      // For other buttons we don't track release/long/repeat
    }
  }
}

void button_init(void)
{
    // prepare pin mask
  uint64_t pin_mask = 0;
  for (int i = 0; i < BUTTON__COUNT; ++i) {
    if (button_gpio_values[i] >= 0) {
      pin_mask |= (1ULL << button_gpio_values[i]);
    }
  }
  if (pin_mask == 0) {
    ESP_LOGW(TAG, "No buttons configured (all -1). Skipping init.");
    return;
  }

  gpio_config_t io_conf = {
      .intr_type = GPIO_INTR_NEGEDGE, // detect press (active-low falling edge)
      .mode = GPIO_MODE_INPUT,
      .pin_bit_mask = pin_mask,
      .pull_up_en = GPIO_PULLUP_ENABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
  };
  gpio_config(&io_conf);

  // Create the event queue
  gpio_evt_queue = xQueueCreate(GPIO_EVT_QUEUE_SIZE, sizeof(uint32_t));

  // initialize last_isr_tick
  for (int i = 0; i < BUTTON__COUNT; ++i) last_isr_tick[i] = 0;

  // install ISR service and add handlers only for configured pins
  gpio_install_isr_service(ESP_INTR_FLAG_LEVEL3);
  for (int i = 0; i < BUTTON__COUNT; ++i) {
    if (button_gpio_values[i] >= 0) {
      gpio_isr_handler_add((gpio_num_t)button_gpio_values[i], gpio_isr_handler, (void *)button_gpio_values[i]);
    }
  }

  // Create the button task
  xTaskCreatePinnedToCore(button_task, "button_task", 4096, NULL, 10, NULL, 1);

  ESP_LOGI(TAG, "Button handler initialized for 8 buttons");
}

/* helper: find button index by gpio number, or -1 if not found */
static int gpio_to_button_idx(gpio_num_t gpio)
{
  for (int i = 0; i < BUTTON__COUNT; ++i) {
    if (button_gpio_values[i] >= 0 && (gpio_num_t)button_gpio_values[i] == gpio) return i;
  }
  return -1;
}