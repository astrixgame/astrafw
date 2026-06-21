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

typedef struct {
    int max_transfer_sz;
} display_config_t;

esp_err_t display_new(const display_config_t *config, esp_lcd_panel_handle_t *ret_panel, esp_lcd_panel_io_handle_t *ret_io);

esp_err_t display_brightness_set(int brightness_percent);

int display_brightness_get(void);

esp_err_t touch_new(esp_lcd_touch_handle_t *ret_touch);