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
#define DEFAULT_TIMESCALE 2
#define MAX_TIMESCALE 60

#define DEFAULT_UNIX_TS 1735689600ULL
#define DEFAULT_REAL_TS 1735689600ULL

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
void format_datetime_lcd(time_t unix_ts, char *out, size_t out_sz);
void format_datetime(time_t unix_ts, char *out, size_t out_sz);

// Convert UNIX timestamp → tm
void ts_to_tm(uint32_t unix_ts, struct tm *out);

// Convert tm → UNIX timestamp
uint32_t tm_to_ts(struct tm *in);

// Check if timer is running
bool timer_is_running(void);

#endif