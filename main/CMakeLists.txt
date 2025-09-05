set(srcs
    "main.c"
    "timer.c"
    "storage.c"
    "lcd_driver.c"
    "state_machine.c"
    "event_handler.c"
    "output_driver.c"
    "button_driver.c"
    # ... other main sources ...
    "menu/menu.c"
    "menu/menu_table.c"
    "menu/edit_commons.c"
    "menu/edit_datetime.c"
    #"menu/modeltime.c"
    "menu/edit_timescale.c"
    #"menu/wifi.c"
    #"menu/test_lcd.c"
    #"menu/other2.c"
)

idf_component_register(SRCS "${srcs}"
                       REQUIRES driver esp_event nvs_flash
                       PRIV_REQUIRES spi_flash
                       INCLUDE_DIRS "")
