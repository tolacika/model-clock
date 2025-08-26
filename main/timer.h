#ifndef TIMER_H
#define TIMER_H

#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// ----------------------
// Timescale settings
// ----------------------
#define TIMESCALE_1_1 1
#define TIMESCALE_1_2 2
#define TIMESCALE_1_6 6
#define TIMESCALE_1_12 12
#define TIMESCALE_1_20 20
#define TIMESCALE_1_30 30
#define TIMESCALE_1_60 60

#define TIMER_RES_HZ 1000000ULL

// Global model time counter (UNIX timestamp in seconds)
extern volatile uint32_t unix_ts;

// Queue handle for tick events
extern QueueHandle_t tick_queue;

// Initialize timer and start ticking
void timer_initialize(void);


// Get timescale
uint32_t timer_get_timescale(void);

// Utility: format unix_ts into "YYYY-MM-DD HH:MM:SS"
void format_time(time_t unix_ts, char *out, size_t out_sz);

// Check if timer is running
bool timer_is_running(void);

#endif