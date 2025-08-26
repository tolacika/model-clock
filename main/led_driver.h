#ifndef LED_DRIVER_H
#define LED_DRIVER_H

#define LED_GREEN_PIN 35
#define LED_AMBER_PIN 36
#define LED_RED_PIN 37
#define LED_NEOPIXEL_PIN 48

#define LED_GREEN 0
#define LED_AMBER 1
#define LED_RED 2

#define LED_COUNT 3


void led_driver_init(void);

#endif