#include "storage.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "event_handler.h"
#include "timer.h"
#include <sys/time.h>

static const char *TAG = "storage";

void storage_load(void)
{
  storage_data_t data = {
      .model_ts = DEFAULT_UNIX_TS,
      .real_ts = DEFAULT_REAL_TS,
      .timescale = DEFAULT_TIMESCALE};
  storage_read(&data);

  events_post(EVENT_TIMER_SCALE, &data.timescale, sizeof(data.timescale));

  unix_ts = data.model_ts;

  struct timeval tv = {
      .tv_sec = data.real_ts,
      .tv_usec = 0};
  settimeofday(&tv, NULL);

  ESP_LOGI(TAG, "Loaded model_ts: %lu, real_ts: %lu, timescale: %lu", data.model_ts, data.real_ts, data.timescale);
}

void storage_save(void)
{
  storage_data_t data = {
      .model_ts = unix_ts,
      .real_ts = time(NULL),
      .timescale = timer_get_timescale()};

  storage_write(&data);

  ESP_LOGI(TAG, "Saved model_ts: %lu, real_ts: %lu, timescale: %lu", data.model_ts, data.real_ts, data.timescale);
}

void storage_init(void)
{
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
  {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);
}

void storage_write(storage_data_t *data)
{
  nvs_handle handle;
  ESP_ERROR_CHECK(nvs_open("storage", NVS_READWRITE, &handle));
  ESP_ERROR_CHECK(nvs_set_u32(handle, "model_ts", data->model_ts));
  ESP_ERROR_CHECK(nvs_set_u32(handle, "real_ts", data->real_ts));
  ESP_ERROR_CHECK(nvs_set_u32(handle, "timescale", data->timescale));
  nvs_close(handle);
}

void storage_read(storage_data_t *data)
{
  nvs_handle handle;
  esp_err_t err;
  ESP_ERROR_CHECK(nvs_open("storage", NVS_READWRITE, &handle));
  err = nvs_get_u32(handle, "model_ts", &data->model_ts);
  if (err == ESP_ERR_NVS_NOT_FOUND)
    data->model_ts = DEFAULT_UNIX_TS;

  err = nvs_get_u32(handle, "real_ts", &data->real_ts);
  if (err == ESP_ERR_NVS_NOT_FOUND)
    data->real_ts = DEFAULT_REAL_TS;

  err = nvs_get_u32(handle, "timescale", &data->timescale);
  if (err == ESP_ERR_NVS_NOT_FOUND)
    data->timescale = DEFAULT_TIMESCALE;

  nvs_close(handle);
}