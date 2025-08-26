#include "event_handler.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "event_handler";

// Custom event loop handle
static esp_event_loop_handle_t custom_event_loop = NULL;

// Define the event base for custom events
ESP_EVENT_DEFINE_BASE(CUSTOM_EVENTS);

// Initialize the event system
void events_init(void)
{
    ESP_LOGI(TAG, "init Prio: %d, Core: %d", uxTaskPriorityGet(NULL), xPortGetCoreID());

    esp_event_loop_args_t loop_args = {
        .queue_size = 10,
        .task_name = "custom_evt_loop",
        .task_stack_size = 3072,
        .task_priority = 20,
        .task_core_id = 0,
    };

    esp_err_t err = esp_event_loop_create(&loop_args, &custom_event_loop);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to create custom event loop: %s", esp_err_to_name(err));
    }
}

// Post an event to the queue
void events_post(int32_t event_id, const void *event_data, size_t event_data_size)
{
    if (custom_event_loop == NULL)
    {
        ESP_LOGE(TAG, "Custom event loop not initialized");
        return;
    }

    esp_err_t err = esp_event_post_to(custom_event_loop, CUSTOM_EVENTS, event_id, event_data, event_data_size, 0);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to post event: %s", esp_err_to_name(err));
    }
}

void events_subscribe(int32_t event_id, esp_event_handler_t event_handler, void *event_handler_arg)
{
    if (custom_event_loop == NULL)
    {
        ESP_LOGE(TAG, "Custom event loop not initialized");
        return;
    }

    esp_err_t err = esp_event_handler_instance_register_with(custom_event_loop, CUSTOM_EVENTS, event_id,
                                                             event_handler, event_handler_arg, NULL);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to subscribe to event: %s", esp_err_to_name(err));
    }
}
