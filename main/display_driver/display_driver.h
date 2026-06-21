#pragma once

#include "esp_lcd_types.h"
#include "esp_lcd_touch.h"

#define ESP_LCD_COLOR_FORMAT_RGB565    (1)
#define ESP_LCD_COLOR_FORMAT_RGB888    (2)

#define LCD_COLOR_FORMAT        (ESP_LCD_COLOR_FORMAT_RGB565)
#define LCD_BIGENDIAN           (1)
#define LCD_BITS_PER_PIXEL      (16)
#define LCD_COLOR_SPACE         (ESP_LCD_COLOR_SPACE_RGB)

#define LCD_W               (410)
#define LCD_H               (502)

esp_err_t display_new();

esp_err_t display_brightness_set(int brightness_percent);

int display_brightness_get(void);

esp_err_t touch_new(esp_lcd_touch_handle_t *ret_touch);

void display_flush(void);

void display_set_pixel(int x, int y, uint16_t color);

void display_fill(uint16_t color);

void display_draw_dot(int x, int y, uint16_t color);

void display_draw_line(int x0, int y0, int x1, int y1, uint16_t color);

uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b);

#define CLR_RED    rgb565(0xFF, 0x00, 0x00)
#define CLR_GREEN  rgb565(0x00, 0xFF, 0x00)
#define CLR_BLUE   rgb565(0x00, 0x00, 0xFF)
#define CLR_YELLOW rgb565(0xFF, 0xFF, 0x00)
#define CLR_WHITE  rgb565(0xFF, 0xFF, 0xFF)
#define CLR_BLACK  rgb565(0x00, 0x00, 0x00)