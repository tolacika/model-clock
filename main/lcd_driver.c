#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <rom/ets_sys.h>
#include "lcd_driver.h"
#include "event_handler.h"
#include "timer.h" // for unix_ts
#include "state_machine.h"

static const char *TAG = "I2C_LCD";
static const uint8_t COMMAND_8BIT_MODE = 0b00110000;
static const uint8_t COMMAND_4BIT_MODE = 0b00100000;
static const uint8_t INIT_COMMANDS[] = {
    0b00101000, // Function set: 4-bit mode, 2 lines, 5x8 dots
    0b00001100, // Display control: display on, cursor off, blink off
    0b00000001, // Clear display
    0b00000110, // Entry mode set: increment cursor, no shift
    0b00000010, // Set cursor to home position
    0b10000000  // Set cursor to first line
};

static const char *SPLASH_SCREEN_CONTENT =
    "   Splash Screen    "
    "                    "
    "    Model Clock     "
    "    v0.1            ";
static const char *RESTART_SCREEN_CONTENT =
    "   Restarting...    "
    "                    "
    "    Model Clock     "
    "    v0.1            ";

static TaskHandle_t lcd_task_handle = NULL;
static i2c_master_dev_handle_t i2c_device_handle = NULL;
static i2c_master_bus_handle_t i2c_bus_handle = NULL;
static uint8_t lcd_backlight_status = LCD_BACKLIGHT;

static char lcd_buffer[LCD_BUFFER_DEPTH][LCD_BUFFER_SIZE]; // 2x80-byte buffer for the LCD
static uint8_t lcd_buffer_index_active = 0;
static uint8_t lcd_buffer_index_draw = 1;
static bool isRendering = false;
//static bool next_render_requested = false;

static uint8_t cursor_col = 0;
static uint8_t cursor_row = 0;

// Forward declarations

void lcd_set_cursor_position(uint8_t col, uint8_t row);
void lcd_set_cursor(uint8_t col, uint8_t row);
void lcd_clear_buffer(void);
void lcd_write_character(char c);
void lcd_write_text(const char *str);
void lcd_write_textf(const char *str, size_t size, ...);
void lcd_write_buffer(const char *buffer, size_t size);
void lcd_toggle_backlight(bool state);
void lcd_render(void);
void lcd_render_cycle();
void lcd_update_task(void *pvParameter);

void constant_screen(const char *content);
void screen_clock(void);
void screen_settings(void);
void screen_editing(void);
void screen_lcd_test(void);

static void lcd_init_cycle(void);
static esp_err_t i2c_send_with_toggle(uint8_t data);
static esp_err_t i2c_send_4bit_data(uint8_t data, uint8_t rs);
static bool compare_double_buffer(void);

static void lcd_event_handler(void *handler_arg, esp_event_base_t base, int32_t id, void *event_data);

void lcd_render_cycle()
{
  if (isRendering)
  {
    return;
  }
  isRendering = true;

  switch (get_top_state())
  {
  case STATE_INIT:
    constant_screen(SPLASH_SCREEN_CONTENT);
    break;
  case STATE_RESTART:
    constant_screen(RESTART_SCREEN_CONTENT);
    break;
  case STATE_CLOCK:
    screen_clock();
    break;
  case STATE_MENU:
    screen_settings();
    break;
  case STATE_EDIT:
    screen_editing();
    break;
  case STATE_LCD_TEST:
    screen_lcd_test();
    break;
  default:
    break;
  }

  lcd_render();
  isRendering = false;
}

// Render the buffer to the LCD
void lcd_render(void)
{
  // Compare the two buffers
  if (compare_double_buffer())
  {
    return;
  }

  // Swap the buffers
  lcd_buffer_index_active = lcd_buffer_index_draw;
  lcd_buffer_index_draw = (lcd_buffer_index_draw + 1) % LCD_BUFFER_DEPTH;

  // Send the buffer to the LCD
  for (uint8_t row = 0; row < LCD_ROWS; row++)
  {
    lcd_set_cursor_position(0, row);
    for (uint8_t col = 0; col < LCD_COLS; col++)
    {
      ESP_ERROR_CHECK(i2c_send_4bit_data(lcd_buffer[lcd_buffer_index_active][row * LCD_COLS + col], LCD_RS_DATA));
    }
  }
}

void lcd_update_task(void *pvParameter)
{
  lcd_task_handle = xTaskGetCurrentTaskHandle();

  // Wait for the current render to finish
  while (isRendering)
    vTaskDelay(pdMS_TO_TICKS(10));
  
  for (;;)
  {
    // Wait until either timeout (frame) or a notification triggers immediate render
    if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000 / LCD_FPS)) > 0)
    {
      // notification: render immediately
      lcd_render_cycle();
    }
    else
    {
      // timeout: render next frame
      lcd_render_cycle();
    }
  }
}

// -----------------
// LCD Screens
// -----------------

void constant_screen(const char *content)
{
  // Display the splash screen on the LCD
  lcd_clear_buffer();
  lcd_set_cursor(0, 0);
  lcd_write_buffer(content, strlen(content));
}

void screen_clock(void)
{
  lcd_clear_buffer();

  // Real time in the first line
  lcd_set_cursor(0, 0);
  char bufR[21];
  time_t now = time(NULL);
  format_time(now, bufR, sizeof(bufR));
  lcd_write_buffer(bufR, strlen(bufR));

  // Model time in the second line
  lcd_set_cursor(0, 1);
  char bufM[21];
  format_time(unix_ts, bufM, sizeof(bufM));
  lcd_write_buffer(bufM, strlen(bufM));

  // App state in the last line
  lcd_set_cursor(0, 3);
  if (timer_is_running())
  {
    lcd_write_text("RUNNING");
  }
  else
  {
    lcd_write_text("PAUSED");
  }
  lcd_set_cursor(16, 3);
  lcd_write_text("1:");
  uint32_t app_scale = timer_get_timescale();
  lcd_write_textf("%02d", 2, app_scale);
}

void screen_settings(void)
{
  lcd_clear_buffer();

  int count = get_menu_count();
  int start = get_menu_scroll_top();

  int max_start = count > LCD_ROWS ? count - LCD_ROWS : 0;

  // clamp start into valid range:
  if (start < 0)
    start = 0;
  if (start > max_start)
    start = max_start;

  int selected = get_menu_selected();

  for (int row = 0; row < LCD_ROWS; ++row)
  {
    int idx = start + row;
    if (idx >= count)
      break;

    // cursor caret
    if (selected == idx)
    {
      lcd_set_cursor(0, row);
      lcd_write_character((char)0x7E);
    }

    // menu item
    const char *item = get_menu_item(idx);
    if (item)
    {
      lcd_set_cursor(2, row);
      lcd_write_buffer(item, strnlen(item, LCD_COLS - 2)); // safe length
    }
  }

  if (start > 0)
  {
    lcd_set_cursor(LCD_COLS - 1, 0);
    lcd_write_text("^");
  }
  if (start + LCD_ROWS < count)
  {
    lcd_set_cursor(LCD_COLS - 1, LCD_ROWS - 1);
    lcd_write_text("v");
  }
}

void screen_editing(void)
{
  edit_mode_t mode = get_edit_mode();
  lcd_clear_buffer();
  
  // Headline
  lcd_set_cursor(0, 0);
  if (mode == EDIT_REALTIME)
  {
    lcd_write_text("Realtime:");
  }
  else if (mode == EDIT_MODELTIME)
  {
    lcd_write_text("Modeltime:");
  }
  else if (mode == EDIT_TIMESCALE)
  {
    lcd_write_text("Timescale:");
  }
  

  // Time
  if (mode == EDIT_REALTIME || mode == EDIT_MODELTIME)
  {
    lcd_set_cursor(0, 1);
    int32_t ts_val = get_edit_timestamp();
    struct tm tm;
    ts_to_tm(ts_val, &tm);
    lcd_write_textf("%04d-%02d-%02d  %02d:%02d:%02d", 20, tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);

    int cursor = get_edit_cursor();
    // positions: 0, 5, 8, 12, 15, 18
    if (cursor == 0)
      lcd_set_cursor(0, 2);
    else if ( cursor < 3)
      lcd_set_cursor(2 + cursor * 3, 2);
    else
      lcd_set_cursor(3 + cursor * 3, 2);
    
    lcd_write_text(cursor == 0 ? "^^^^" : "^^");
  }
  else if (mode == EDIT_TIMESCALE)
  {
    lcd_set_cursor(9, 1);
    uint32_t timescale = get_edit_timescale();
    lcd_write_textf("%02d", 2, timescale);
    lcd_set_cursor(9, 2);
    lcd_write_text("^^");
  }

  // buttons
  lcd_set_cursor(0, 3);
  lcd_write_text("BACK");
  lcd_set_cursor(16, 3);
  lcd_write_text("OK");
}

void screen_lcd_test(void)
{
  lcd_clear_buffer();

  lcd_set_cursor(0, 0);
  lcd_write_text("Y/X 0123456789ABCDEF");

  int i = get_lcd_test_iterator();

  for (int row = 0; row < LCD_ROWS - 1; row++)
  {
    int y = row + i;
    lcd_set_cursor(0, row + 1);
    lcd_write_textf("%1XX", 2, y & 0xF); // prints 0X, 1X, 2X, etc

    // characters
    lcd_set_cursor(4, row + 1);
    for (int x = 0; x < 16; x++)
    {
      unsigned char c = (y * 16) + x;
      lcd_write_character((char)c);
    }
  }
}

// -----------------
// Initialization
// -----------------

void i2c_initialize(void)
{
  // Initialize the I2C master
  i2c_master_bus_config_t i2c_bus_config = {
      .clk_source = I2C_CLK_SRC_DEFAULT,
      .i2c_port = I2C_MASTER_NUM,
      .scl_io_num = I2C_MASTER_SCL_IO,
      .sda_io_num = I2C_MASTER_SDA_IO,
      .glitch_ignore_cnt = 7,
      .flags.enable_internal_pullup = true};
  ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_config, &i2c_bus_handle));
  ESP_LOGI(TAG, "I2C bus initialized");

  i2c_device_config_t i2c_device_config = {
      .dev_addr_length = I2C_ADDR_BIT_LEN_7,
      .device_address = LCD_I2C_ADDRESS,
      .scl_speed_hz = I2C_MASTER_FREQ_HZ};
  ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus_handle, &i2c_device_config, &i2c_device_handle));
  ESP_LOGI(TAG, "I2C device added");
  vTaskDelay(pdMS_TO_TICKS(50)); // Wait for LCD to power up
  ESP_LOGI(TAG, "I2C device initialized");
}

static void lcd_init_cycle(void)
{
  // Initialize the LCD
  ESP_ERROR_CHECK(i2c_send_with_toggle(lcd_backlight_status | LCD_ENABLE_OFF | LCD_RW_WRITE | LCD_RS_CMD));
  ESP_ERROR_CHECK(i2c_send_with_toggle(COMMAND_8BIT_MODE | lcd_backlight_status | LCD_ENABLE_OFF | LCD_RW_WRITE | LCD_RS_CMD));
  ESP_ERROR_CHECK(i2c_send_with_toggle(COMMAND_8BIT_MODE | lcd_backlight_status | LCD_ENABLE_OFF | LCD_RW_WRITE | LCD_RS_CMD));
  ESP_ERROR_CHECK(i2c_send_with_toggle(COMMAND_8BIT_MODE | lcd_backlight_status | LCD_ENABLE_OFF | LCD_RW_WRITE | LCD_RS_CMD));
  ESP_ERROR_CHECK(i2c_send_with_toggle(COMMAND_4BIT_MODE | lcd_backlight_status | LCD_ENABLE_OFF | LCD_RW_WRITE | LCD_RS_CMD));

  for (uint8_t i = 0; i < sizeof(INIT_COMMANDS); i++)
  {
    ESP_ERROR_CHECK(i2c_send_4bit_data(INIT_COMMANDS[i], LCD_RS_CMD));
    ets_delay_us(1000);
  }

  lcd_toggle_backlight(true);
  lcd_clear_buffer();
}

void lcd_initialize(void)
{

  lcd_init_cycle();

  lcd_render();
  lcd_render_cycle();

  events_subscribe(EVENT_LCD_UPDATE, lcd_event_handler, NULL);

  // Create LCD update task
  xTaskCreatePinnedToCore(lcd_update_task, "lcd_update_task", 4096, NULL, 5, NULL, 1);
}

// -----------------
// Utility functions
// -----------------

static bool compare_double_buffer(void)
{
  // Compare the two double buffers
  for (uint8_t row = 0; row < LCD_ROWS; row++)
  {
    for (uint8_t col = 0; col < LCD_COLS; col++)
    {
      if (lcd_buffer[lcd_buffer_index_active][row * LCD_COLS + col] != lcd_buffer[lcd_buffer_index_draw][row * LCD_COLS + col])
        return false;
    }
  }
  return true;
}

static esp_err_t i2c_send_with_toggle(uint8_t data)
{
  // Helper function to toggle the enable bit
  uint8_t data_with_enable = data | LCD_ENABLE;
  ESP_ERROR_CHECK(i2c_master_transmit(i2c_device_handle, &data_with_enable, 1, -1));
  ets_delay_us(50);

  data_with_enable &= ~LCD_ENABLE;
  ESP_ERROR_CHECK(i2c_master_transmit(i2c_device_handle, &data_with_enable, 1, -1));
  ets_delay_us(50);

  return ESP_OK;
}

static esp_err_t i2c_send_4bit_data(uint8_t data, uint8_t rs)
{
  // Send a byte of data to the LCD in 4-bit mode
  uint8_t nibbles[2] = {
      (data & 0xF0) | rs | lcd_backlight_status | LCD_RW_WRITE,
      ((data << 4) & 0xF0) | rs | lcd_backlight_status | LCD_RW_WRITE};
  ESP_ERROR_CHECK(i2c_send_with_toggle(nibbles[0]));
  ESP_ERROR_CHECK(i2c_send_with_toggle(nibbles[1]));

  return ESP_OK;
}

void lcd_set_cursor_position(uint8_t col, uint8_t row)
{
  // Set the cursor position on the LCD
  if (col >= LCD_COLS)
    col = LCD_COLS - 1;
  if (row >= LCD_ROWS)
    row = LCD_ROWS - 1;

  static const uint8_t row_offsets[] = LCD_ROW_OFFSET;
  uint8_t data = 0x80 | (col + row_offsets[row]);
  ESP_ERROR_CHECK(i2c_send_4bit_data(data, LCD_RS_CMD));
}

void lcd_set_cursor(uint8_t col, uint8_t row)
{
  // Update the cursor position in the buffer
  if (col < LCD_COLS && row < LCD_ROWS)
  {
    cursor_col = col;
    cursor_row = row;
  }
}

void lcd_clear_buffer(void)
{
  memset(lcd_buffer[lcd_buffer_index_draw], ' ', LCD_BUFFER_SIZE);
  lcd_set_cursor(0, 0);
}

void lcd_write_character(char c)
{
  // ESP_LOGI(TAG, "idx: %d", lcd_buffer_index_draw);
  // Write a single character to the buffer
  if (cursor_col < LCD_COLS && cursor_row < LCD_ROWS)
  {
    lcd_buffer[lcd_buffer_index_draw][cursor_row * LCD_COLS + cursor_col] = c;
    cursor_col++;
    if (cursor_col >= LCD_COLS)
    {
      cursor_col = 0;
      cursor_row = (cursor_row + 1) % LCD_ROWS;
    }
  }
}

void lcd_write_textf(const char *str, size_t size, ...)
{
  char buffer[size + 1];
  va_list args;
  va_start(args, size);
  vsnprintf(buffer, size + 1, str, args);
  va_end(args);
  lcd_write_buffer(buffer, strlen(buffer));
}

void lcd_write_text(const char *str)
{
  while (*str)
  {
    lcd_write_character(*str);
    str++;
  }
}

void lcd_write_buffer(const char *buffer, size_t size)
{
  for (size_t i = 0; i < size; i++)
  {
    lcd_write_character(buffer[i]);
  }
}

void lcd_toggle_backlight(bool state)
{
  // Control the LCD backlight
  if (state)
  {
    lcd_backlight_status |= LCD_BACKLIGHT;
  }
  else
  {
    lcd_backlight_status &= ~LCD_BACKLIGHT;
  }
  ESP_ERROR_CHECK(i2c_master_transmit(i2c_device_handle, &lcd_backlight_status, 1, -1));
}

// -----------------
// LCD Event Handler
// -----------------

static void lcd_event_handler(void *handler_arg, esp_event_base_t base, int32_t id, void *event_data)
{
  if (base == CUSTOM_EVENTS && id == EVENT_LCD_UPDATE)
  {
    if (lcd_task_handle)
      xTaskNotifyGive(lcd_task_handle);
  }
}
