#include "menu.h"

extern const menu_entry_t menu_item_realtime;
extern const menu_entry_t menu_item_modeltime;
extern const menu_entry_t menu_item_timescale;

const menu_entry_t *menu_table[] = {
    &menu_item_realtime,
    &menu_item_modeltime,
    &menu_item_timescale,
};

const uint8_t menu_table_count = sizeof(menu_table) / sizeof(menu_table[0]);

uint8_t get_menu_count(void) { return menu_table_count; }
