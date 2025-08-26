#include "button_driver.h"
#include "event_handler.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_log.h"

static const char *TAG = "button_interrupt";
// List of button GPIOs
static const gpio_num_t button_gpios[8] = BUTTON_PINS;

static QueueHandle_t gpio_evt_queue = NULL;
// store last ISR tick per button
static TickType_t last_isr_tick[8] = {0};

// ISR
static void IRAM_ATTR gpio_isr_handler(void *arg)
{
  uint32_t gpio_num = (uint32_t)arg;
  TickType_t now_tick = xTaskGetTickCountFromISR();

  for (uint8_t i = 0; i < 8; i++)
  {
    if (button_gpios[i] == gpio_num)
    {
      if ((now_tick - last_isr_tick[i]) >= pdMS_TO_TICKS(BUTTON_DEBOUNCE_MS))
      {
        last_isr_tick[i] = now_tick;
        xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
      }
      break;
    }
  }
}

// Task to handle button presses
static void button_task(void *arg)
{
  uint32_t io_num;
  uint8_t button_num;

  while (1)
  {
    if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY))
    {
      // Only handle press (active low)
      if (gpio_get_level(io_num) == 0)
      {
        for (button_num = 0; button_num < BUTTON_COUNT; button_num++)
        {
          if (button_gpios[button_num] == io_num)
          {
            break;
          }
        }
        ESP_LOGI(TAG, "Button pressed on GPIO %d, button %d", io_num, button_num);
        events_post(EVENT_BUTTON_PRESS, &button_num, sizeof(button_num));
      }
    }
  }
}

void button_init(void)
{
  gpio_config_t io_conf = {
      .intr_type = GPIO_INTR_NEGEDGE, // trigger on falling edge (press)
      .mode = GPIO_MODE_INPUT,
      .pin_bit_mask = 0,
      .pull_up_en = GPIO_PULLUP_ENABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
  };
  for (int i = 0; i < 8; i++)
  {
    io_conf.pin_bit_mask |= (1ULL << button_gpios[i]);
  }
  gpio_config(&io_conf);

  // Create the event queue
  gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));

  // Create the button task
  xTaskCreatePinnedToCore(button_task, "button_task", 2048, NULL, 10, NULL, tskNO_AFFINITY);

  // Install ISR service
  gpio_install_isr_service(ESP_INTR_FLAG_LEVEL3);

  // Attach ISR for all buttons
  for (int i = 0; i < 8; i++)
  {
    gpio_isr_handler_add(button_gpios[i], gpio_isr_handler, (void *)button_gpios[i]);
  }

  ESP_LOGI(TAG, "Button handler initialized for 8 buttons");
}
