#ifndef OUTPUT_DRIVER_H
#define OUTPUT_DRIVER_H

typedef enum {
    OUTPUT_ROLE_LED_GREEN = 0,
    OUTPUT_ROLE_LED_RED,
    OUTPUT_ROLE_NEOPIXEL,
    OUTPUT_ROLE_CLOCK_CH0,
    OUTPUT_ROLE_CLOCK_CH1,
    OUTPUT_ROLE_CLOCK_CH2,
    OUTPUT_ROLE__COUNT
} output_role_t;

void output_driver_init(void);

#endif