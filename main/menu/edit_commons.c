#include "menu.h"

void editor_general_cancel(void)
{
  state_ctx.edit_mode = EDIT_NONE;
  state_ctx.edit_cursor = 0;
  state_ctx.edit_timestamp = 0;
  state_ctx.edit_timescale = 0;
  
  enter_state_menu();
}

