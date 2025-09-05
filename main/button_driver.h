#ifndef BUTTON_DRIVER_H
#define BUTTON_DRIVER_H

#include <stdint.h>

typedef enum {
  BUTTON_START_STOP = 0,
  BUTTON_MENU,
  BUTTON_LEFT,
  BUTTON_RIGHT,
  BUTTON_UP,
  BUTTON_DOWN,
  BUTTON_CANCEL,
  BUTTON_OK,
  BUTTON__COUNT,
} button_t;


void button_init(void);

/**
 * Configure longpress/repeat behaviour (applies only to UP/DOWN).
 * longpress_ms: time required to trigger long press (default 1500)
 * repeat_ms: interval between repeat events while held (default 200)
 */
void button_set_longpress_params(uint32_t longpress_ms, uint32_t repeat_ms);

#endif