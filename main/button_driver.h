#ifndef BUTTON_DRIVER_H
#define BUTTON_DRIVER_H

#define BUTTON_COUNT 8
#define BUTTON_PINS {4, 5, 6, 7, 10, 11, 12, 13}

typedef enum {
  BUTTON_START_STOP = 0,
  BUTTON_MENU,
  BUTTON_LEFT,
  BUTTON_RIGHT,
  BUTTON_UP,
  BUTTON_DOWN,
  BUTTON_CANCEL,
  BUTTON_OK,
} button_t;


#define BUTTON_DEBOUNCE_MS 200


void button_init(void);

#endif