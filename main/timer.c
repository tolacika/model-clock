#include "timer.h"
#include "esp_log.h"
#include "driver/gptimer.h"
#include "event_handler.h"

static const char *TAG = "model_timer";

volatile uint32_t unix_ts = DEFAULT_UNIX_TS; // Start: 2025-01-01 00:00:00
QueueHandle_t tick_queue;

static gptimer_handle_t gptimer = NULL;
static uint32_t current_timescale = DEFAULT_TIMESCALE; // default 1:2
static bool timer_running = false;

// Pause timer
void timer_pause(void);
// Resume timer
void timer_resume(void);
// Set timescale
void timer_set_timescale(uint32_t new_timescale);

// ----------------------
// ISR callback
// ----------------------
static bool IRAM_ATTR timer_isr_callback(
    gptimer_handle_t timer,
    const gptimer_alarm_event_data_t *edata,
    void *user_data)
{
  // Increment one model-second
  unix_ts++;

  // Notify worker task(s) via queue
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  uint32_t ts = unix_ts;
  xQueueSendFromISR(tick_queue, &ts, &xHigherPriorityTaskWoken);

  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);

  return true;
}

// ----------------------
// Tick consumer task
// ----------------------
void tick_consumer_task(void *pvParams)
{
  uint32_t tick_val;
  struct tm tm_info;

  while (true)
  {
    if (xQueueReceive(tick_queue, &tick_val, portMAX_DELAY))
    {
      ts_to_tm(tick_val, &tm_info);
      events_post(EVENT_MODEL_TICK, &tick_val, sizeof(tick_val));
      if (timer_running && tm_info.tm_sec == 0)
      {
        events_post(EVENT_MODEL_MINUTE_TICK, &tick_val, sizeof(tick_val));
      }
    }
  }
}

void timer_event_handler(void *handler_arg, esp_event_base_t base, int32_t id, void *event_data)
{
  ESP_LOGI(TAG, "Prio: %d, Core: %d", uxTaskPriorityGet(NULL), xPortGetCoreID());
  if (base == CUSTOM_EVENTS)
  {
    switch (id)
    {
    case EVENT_TIMER_RESUME:
      timer_resume();
      break;
    case EVENT_TIMER_PAUSE:
      timer_pause();
      break;
    case EVENT_TIMER_SCALE:
      uint32_t new_timescale = *(uint32_t *)event_data;
      timer_set_timescale(new_timescale);
      break;
    default:
      break;
    }
  }
}

void timer_initialize(void)
{
  setenv("TZ", "UTC", 1);
  tzset();

  tick_queue = xQueueCreate(10, sizeof(uint32_t));
  if (tick_queue == NULL)
  {
    ESP_LOGE(TAG, "Failed to create tick queue");
    return;
  }

  // gptimer_handle_t gptimer = NULL;
  gptimer_config_t timer_config = {
      .clk_src = GPTIMER_CLK_SRC_DEFAULT,
      .direction = GPTIMER_COUNT_UP,
      .resolution_hz = TIMER_RES_HZ,
  };
  ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &gptimer));

  gptimer_event_callbacks_t cbs = {
      .on_alarm = timer_isr_callback,
  };
  ESP_ERROR_CHECK(gptimer_register_event_callbacks(gptimer, &cbs, NULL));
  ESP_LOGI(TAG, "Timer initialized, default TIMESCALE=%d", current_timescale);

  // configure alarm
  timer_set_timescale(current_timescale);
  ESP_ERROR_CHECK(gptimer_enable(gptimer));

  // Launch consumer task on core 1
  xTaskCreatePinnedToCore(tick_consumer_task, "tick_task", 4096, NULL, 5, NULL, 1);

  // Subscribe to events
  events_subscribe(EVENT_TIMER_RESUME, timer_event_handler, NULL);
  events_subscribe(EVENT_TIMER_PAUSE, timer_event_handler, NULL);
  events_subscribe(EVENT_TIMER_SCALE, timer_event_handler, NULL);
}

// Converts unix timestamp to formatted string in "YYYY-MM-DD HH:MM:SS" format
void format_datetime_lcd(time_t ts, char *out, size_t out_sz)
{
  struct tm tm_info;
  ts_to_tm(ts, &tm_info);
  strftime(out, out_sz, "%Y-%m-%d  %H:%M:%S", &tm_info);
}

void format_datetime(time_t ts, char *out, size_t out_sz)
{
  struct tm tm_info;
  ts_to_tm(ts, &tm_info);
  strftime(out, out_sz, "%Y-%m-%d %H:%M:%S", &tm_info);
}

// Convert UNIX timestamp → tm
void ts_to_tm(uint32_t unix_ts, struct tm *out)
{
  time_t t = unix_ts;
  gmtime_r(&t, out); // use localtime_r if you want timezone
}

// Convert tm → UNIX timestamp
uint32_t tm_to_ts(struct tm *in)
{
  return (uint32_t)mktime(in); // normalizes fields too
}

// Sets the timer timescale
void timer_set_timescale(uint32_t new_timescale)
{
  if (new_timescale == 0 || new_timescale > MAX_TIMESCALE)
    return;
  current_timescale = new_timescale;

  gptimer_alarm_config_t alarm = {
      .alarm_count = TIMER_RES_HZ / current_timescale,
      .reload_count = 0,
      .flags.auto_reload_on_alarm = true,
  };
  ESP_ERROR_CHECK(gptimer_set_alarm_action(gptimer, &alarm));

  ESP_LOGI(TAG, "Timescale set to 1:%d", current_timescale);
}

uint32_t timer_get_timescale(void)
{
  return current_timescale;
}

// Pauses the timer
void timer_pause(void)
{
  if (timer_running)
  {
    ESP_ERROR_CHECK(gptimer_stop(gptimer));
    timer_running = false;
    ESP_LOGI(TAG, "Timer paused");
    events_post(EVENT_TIMER_STATE_CHANGE, NULL, 0);
  }
}

// Resumes the timer
void timer_resume(void)
{
  if (!timer_running)
  {
    ESP_ERROR_CHECK(gptimer_start(gptimer));
    timer_running = true;
    ESP_LOGI(TAG, "Timer resumed (scale=%d)", current_timescale);
    events_post(EVENT_TIMER_STATE_CHANGE, NULL, 0);
  }
}

// Checks if the timer is running
bool timer_is_running(void)
{
  return timer_running;
}