#ifndef BUTTON_DRIVER_H
#define BUTTON_DRIVER_H

#define BUTTON_COUNT 8
#define BUTTON_PINS {4, 5, 6, 7, 10, 11, 12, 13}

#define BUTTON_DEBOUNCE_MS 100


void button_init(void);

#endif