idf_component_register(SRCS "state_machine.c" "event_handler.c" "timer.c" "main.c" "lcd_driver.c" "output_driver.c" "button_driver.c" "storage.c"
                       REQUIRES driver esp_event nvs_flash
                       PRIV_REQUIRES spi_flash
                       INCLUDE_DIRS "")
