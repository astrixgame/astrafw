#include "display_driver.h"
#include "../err_check.h"
#include "../hardware.h"
#include "../i2c_driver/i2c_driver.h"

#include "esp_lcd_panel_ops.h"
#include "esp_lcd_sh8601.h"
#include "esp_lcd_touch_ft5x06.h"

static const char *TAG = "display";

static esp_lcd_panel_handle_t panel_handle = NULL;
static esp_lcd_panel_io_handle_t io_handle = NULL;
uint8_t brightness;
static uint16_t s_bg_color = 0;

static const sh8601_lcd_init_cmd_t lcd_init_cmds[] = {
    {0x11, (uint8_t[]){0x00}, 0, 120},
    {0xC4, (uint8_t[]){0x80}, 1, 0},
    {0x44, (uint8_t[]){0x01, 0xD1}, 2, 0},
    {0x35, (uint8_t[]){0x00}, 1, 0},
    {0x53, (uint8_t[]){0x20}, 1, 10},
    {0x63, (uint8_t[]){0xFF}, 1, 10},
    {0x51, (uint8_t[]){0x00}, 1, 10},
    {0x2A, (uint8_t[]){0x00, 0x16, 0x01, 0xAF}, 4, 0},
    {0x2B, (uint8_t[]){0x00, 0x00, 0x01, 0xF5}, 4, 0},
    {0x29, (uint8_t[]){0x00}, 0, 10},
    {0x51, (uint8_t[]){0xFF}, 1, 0},
};


#define LCD_CMD_BITS (8)
#define LCD_PARAM_BITS (8)
#define LCD_LEDC_CH (1)

esp_err_t display_brightness_set(int brightness_percent) {
    if(panel_handle == NULL) {
        ESP_LOGE(TAG, "Panel handle is not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if(brightness_percent < 0 || brightness_percent > 100) {
        ESP_LOGE(TAG, "Invalid brightness percentage. Should be between 0 and 100.");
        return ESP_ERR_INVALID_ARG;
    }

    brightness = (uint8_t)(brightness_percent * 255 / 100);

    uint32_t lcd_cmd = 0x51;
    lcd_cmd &= 0xff;
    lcd_cmd <<= 8;
    lcd_cmd |= 0x02 << 24;
    uint8_t param = brightness;
    esp_lcd_panel_io_tx_param(io_handle, lcd_cmd, &param, 1);

    return ESP_OK;
}

int display_brightness_get(void) {
    if(panel_handle == NULL) {
        ESP_LOGE(TAG, "Panel handle is not initialized");
        return -1;
    }

    return brightness * 100 / 255;
}

esp_err_t display_new() {
    esp_err_t ret = ESP_OK;

    int max_transfer_sz = LCD_W * (int)sizeof(uint16_t);

    ESP_LOGI(TAG, "Initialize SPI bus");
    const spi_bus_config_t buscfg = SH8601_PANEL_BUS_QSPI_CONFIG(LCD_PCLK, LCD_DATA0, LCD_DATA1, LCD_DATA2, LCD_DATA3, max_transfer_sz);
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_SPI_NUM, &buscfg, SPI_DMA_CH_AUTO));

    const esp_lcd_panel_io_spi_config_t io_config = SH8601_PANEL_IO_QSPI_CONFIG(LCD_CS, NULL, NULL);

    sh8601_vendor_config_t vendor_config = {
        .init_cmds = lcd_init_cmds,
        .init_cmds_size = sizeof(lcd_init_cmds) / sizeof(lcd_init_cmds[0]),
        .flags = {
            .use_qspi_interface = 1,
        },
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_SPI_NUM, &io_config, &io_handle));
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = LCD_BITS_PER_PIXEL,
        .vendor_config = &vendor_config,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_sh8601(io_handle, &panel_config, &panel_handle));
    esp_lcd_panel_reset(panel_handle);
    esp_lcd_panel_init(panel_handle);
    esp_lcd_panel_set_gap(panel_handle, 0x16, 0);
    esp_lcd_panel_disp_on_off(panel_handle, true);

    return ret;
}

esp_err_t touch_new(esp_lcd_touch_handle_t *ret_touch) {
    const esp_lcd_touch_config_t tp_cfg = {
        .x_max = LCD_W,
        .y_max = LCD_H,
        .rst_gpio_num = LCD_TOUCH_RST,
        .int_gpio_num = LCD_TOUCH_INT,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0,
        },
    };
    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_FT5x06_CONFIG();
    tp_io_config.scl_speed_hz = 400000;
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c(i2c_get_handle(), &tp_io_config, &tp_io_handle), TAG, "");
    return esp_lcd_touch_new_i2c_ft5x06(tp_io_handle, &tp_cfg, ret_touch);
}

void display_flush(void) {
    // Immediate-mode rendering: drawing APIs push data to panel directly.
    // Kept as a no-op for API compatibility.
}

void display_set_pixel(int x, int y, uint16_t color) {
    if (!panel_handle || x < 0 || x >= LCD_W || y < 0 || y >= LCD_H) return;
    esp_lcd_panel_draw_bitmap(panel_handle, x, y, x + 1, y + 1, &color);
}

void display_fill(uint16_t color) {
    if (!panel_handle) return;

    s_bg_color = color;

    uint16_t row[LCD_W];
    for (int i = 0; i < LCD_W; i++) row[i] = color;
    for (int y = 0; y < LCD_H; y++) {
        esp_lcd_panel_draw_bitmap(panel_handle, 0, y, LCD_W, y + 1, row);
    }
}

void display_draw_bitmap(int x, int y, int w, int h, const uint16_t *buf) {
    if (!panel_handle || !buf || w <= 0 || h <= 0) return;

    int src_x0 = 0;
    int src_y0 = 0;
    int dst_x = x;
    int dst_y = y;
    int draw_w = w;
    int draw_h = h;

    if (dst_x < 0) {
        src_x0 = -dst_x;
        draw_w -= src_x0;
        dst_x = 0;
    }
    if (dst_y < 0) {
        src_y0 = -dst_y;
        draw_h -= src_y0;
        dst_y = 0;
    }
    if (dst_x + draw_w > LCD_W) draw_w = LCD_W - dst_x;
    if (dst_y + draw_h > LCD_H) draw_h = LCD_H - dst_y;
    if (draw_w <= 0 || draw_h <= 0) return;

    for (int row = 0; row < draw_h; row++) {
        const uint16_t *src_row = buf + (src_y0 + row) * w + src_x0;
        int py = dst_y + row;
        esp_lcd_panel_draw_bitmap(panel_handle, dst_x, py, dst_x + draw_w, py + 1, src_row);
    }
}

uint16_t display_get_bg_color(void) {
    return s_bg_color;
}

void display_draw_dot(int x, int y, uint16_t color) {
    for(int dy = -8; dy <= 8; dy++) {
        for(int dx = -8; dx <= 8; dx++) {
            int px = x + dx, py = y + dy;
            if(px >= 0 && px < LCD_W && py >= 0 && py < LCD_H)
                display_set_pixel(px, py, color);
        }
    }
}

void display_draw_line(int x0, int y0, int x1, int y1, uint16_t color) {
    int dx = abs(x1 - x0), sx = (x0 < x1) ? 1 : -1;
    int dy = -abs(y1 - y0), sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy;
    while(1) {
        display_draw_dot(x0, y0, color);
        if(x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if(e2 >= dy) { err += dy; x0 += sx; }
        if(e2 <= dx) { err += dx; y0 += sy; }
    }
}

uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    uint16_t v = ((r & 0xF8u) << 8) | ((g & 0xFCu) << 3) | (b >> 3);
    return (v >> 8) | (v << 8);
}