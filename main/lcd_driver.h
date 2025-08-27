#ifndef LCD_DRIVER_H
#define LCD_DRIVER_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "esp_log.h"

// I2C configuration
#define I2C_MASTER_NUM        I2C_NUM_0
#define I2C_MASTER_SDA_IO     8
#define I2C_MASTER_SCL_IO     9
#define I2C_MASTER_FREQ_HZ    100000
#define LCD_I2C_ADDRESS       0x27

// LCD commands
#define WRITE_BIT           I2C_MASTER_WRITE

#define LCD_BACKLIGHT (1 << 3) // Backlight bit
#define LCD_ENABLE (1 << 2)   // Enable bit
#define LCD_ENABLE_OFF (0 << 2) // Enable off
#define LCD_RW (1 << 1)      // Read/Write bit
#define LCD_RW_WRITE (0 << 1) // Write mode
#define LCD_RW_READ (1 << 1)  // Read mode
#define LCD_RS (1 << 0)      // Register Select bit
#define LCD_RS_CMD (0 << 0)    // Command mode
#define LCD_RS_DATA (1 << 0)   // Data mode
#define LCD_DB7 (1 << 7) // Data bit 7
#define LCD_DB6 (1 << 6) // Data bit 6
#define LCD_DB5 (1 << 5) // Data bit 5
#define LCD_DB4 (1 << 4) // Data bit 4

#define LCD_FPS 2 // Frames per second
#define LCD_COLS 20
#define LCD_ROWS 4
#define LCD_ROW_OFFSET {0x00, 0x40, 0x14, 0x54} // Row offsets for 20x4 LCD
#define LCD_BUFFER_SIZE (LCD_COLS * LCD_ROWS)
#define LCD_BUFFER_DEPTH 2 // Double buffering

typedef enum {
    LCD_SCREEN_SPLASH = 0,
    LCD_SCREEN_RESTARTING,
    LCD_SCREEN_CLOCK,
    LCD_SCREEN_SETTINGS,
    LCD_SCREEN_MAX
} lcd_screen_state_t;
#define LCD_SCREEN_START_SCREEN LCD_SCREEN_CLOCK

void i2c_initialize(void);
void lcd_initialize(void);


#endif