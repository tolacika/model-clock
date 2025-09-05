#ifndef MENU_TABLE_H
#define MENU_TABLE_H

#include "menu.h"

#define MENU_PAGE_ROWS 4

extern const menu_entry_t *menu_table[];
extern const uint8_t menu_table_count;

uint8_t get_menu_count(void);

#endif
