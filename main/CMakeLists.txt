idf_component_register(SRCS "state_machine.c" "event_handler.c" "timer.c" "main.c" "lcd_driver.c" "led_driver.c" "button_driver.c"
                       REQUIRES driver esp_event
                       PRIV_REQUIRES spi_flash
                       INCLUDE_DIRS "")
