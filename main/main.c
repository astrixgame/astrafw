#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_lcd_panel_ops.h"
#include "hardware.h"
#include "driver/gpio.h"

#include "err_check.h"
#include "i2c_driver/i2c_driver.h"
#include "i2s_driver/i2s_driver.h"
#include "display_driver/display_driver.h"

static const char *TAG = "astrafw";

static inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    uint16_t v = ((r & 0xF8u) << 8) | ((g & 0xFCu) << 3) | (b >> 3);
    return (v >> 8) | (v << 8);
}

#define COL_YELLOW rgb565(0xFF, 0xFF, 0x00)
#define COL_BLACK  rgb565(0x00, 0x00, 0x00)
#define COL_GREEN  rgb565(0x00, 0xFF, 0x00)

static uint16_t *fb = NULL;
static esp_lcd_panel_handle_t s_panel = NULL;
static TaskHandle_t s_draw_task = NULL;

#define NOTIFY_PWRKEY (1 << 0)

static void flush_all(void) {
    esp_lcd_panel_draw_bitmap(s_panel, 0, 0, LCD_W, LCD_H, fb);
}

static inline void set_pixel(int x, int y, uint16_t color) {
    fb[y * LCD_W + x] = color;
}

static void fill(uint16_t color) {
    for(int i = 0; i < LCD_W * LCD_H; i++) fb[i] = color;
}

static void draw_dot(int x, int y, uint16_t color) {
    for(int dy = -8; dy <= 8; dy++) {
        for(int dx = -8; dx <= 8; dx++) {
            int px = x + dx, py = y + dy;
            if(px >= 0 && px < LCD_W && py >= 0 && py < LCD_H)
                set_pixel(px, py, color);
        }
    }
}

static void draw_line(int x0, int y0, int x1, int y1, uint16_t color) {
    int dx = abs(x1 - x0), sx = (x0 < x1) ? 1 : -1;
    int dy = -abs(y1 - y0), sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy;
    while(1) {
        draw_dot(x0, y0, color);
        if(x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if(e2 >= dy) { err += dy; x0 += sx; }
        if(e2 <= dx) { err += dx; y0 += sy; }
    }
}

static void IRAM_ATTR power_btn_isr(void *arg) {
    BaseType_t woken = pdFALSE;
    xTaskNotifyFromISR(s_draw_task, NOTIFY_PWRKEY, eSetBits, &woken);
    portYIELD_FROM_ISR(woken);
}

static void buttons_init(void) {
    /* Boot button — polled, no interrupt */
    const gpio_config_t boot_cfg = {
        .pin_bit_mask = 1ULL << BTN_BOOT,
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&boot_cfg));

    /* Power button — falling-edge ISR for instant response */
    const gpio_config_t pwr_cfg = {
        .pin_bit_mask = 1ULL << BTN_POWER,
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_NEGEDGE,
    };
    ESP_ERROR_CHECK(gpio_config(&pwr_cfg));
    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    ESP_ERROR_CHECK(gpio_isr_handler_add(BTN_POWER, power_btn_isr, NULL));
}

static void draw_task(void *arg) {
    esp_lcd_touch_handle_t tp = (esp_lcd_touch_handle_t)arg;

    bool touching = false;
    bool btn_prev_pressed = false;
    bool display_on = false;

    while(1) {
        /* Boot button — polled, unchanged */
        bool btn_pressed = (gpio_get_level(BTN_BOOT) == 0);
        if(btn_pressed && !btn_prev_pressed) {
            display_on = false;
            fill(COL_BLACK);
            flush_all();
            display_brightness_set(0);
        }
        btn_prev_pressed = btn_pressed;

        /* Power button — ISR wakes us instantly, no polling delay */
        uint32_t notif = 0;
        xTaskNotifyWait(0, ULONG_MAX, &notif, pdMS_TO_TICKS(16));
        if(notif & NOTIFY_PWRKEY) {
            display_on = false;
            fill(COL_BLACK);
            flush_all();
            display_brightness_set(0);
        }

        esp_lcd_touch_read_data(tp);

        uint16_t tx[1], ty[1], ts[1];
        uint8_t  point_num = 0;
        bool pressed = esp_lcd_touch_get_coordinates(tp, tx, ty, ts, &point_num, 1);

        if(pressed && point_num > 0) {
            if(!touching)
                touching = true;
            display_on = true;
            display_brightness_set(80);
        } else {
            if(touching)
                touching = false;
        }

        if(display_on) {
            fill(COL_GREEN);
            flush_all();
        }
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "AstraFW");

    i2c_init();

    fb = heap_caps_malloc(LCD_W * LCD_H * sizeof(uint16_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if(!fb) {
        ESP_LOGE(TAG, "Failed to allocate framebuffer!");
        return;
    }

    esp_lcd_panel_io_handle_t io_handle = NULL;
    const display_config_t disp_cfg = {
        .max_transfer_sz = LCD_W * LCD_H * 2,
    };
    ESP_ERROR_CHECK(display_new(&disp_cfg, &s_panel, &io_handle));
    ESP_ERROR_CHECK(display_brightness_set(80));

    fill(COL_BLACK);
    flush_all();

    esp_lcd_touch_handle_t tp = NULL;
    ESP_ERROR_CHECK(touch_new(&tp));

    xTaskCreate(draw_task, "draw", 4096, tp, 5, &s_draw_task);

    /* Init buttons after task is created so the ISR has a valid task handle */
    buttons_init();
}