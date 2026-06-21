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

static TaskHandle_t s_draw_task = NULL;

#define NOTIFY_PWRKEY (1 << 0)

static void IRAM_ATTR power_btn_isr(void *arg) {
    BaseType_t woken = pdFALSE;
    xTaskNotifyFromISR(s_draw_task, NOTIFY_PWRKEY, eSetBits, &woken);
    portYIELD_FROM_ISR(woken);
}

static void buttons_init(void) {
    const gpio_config_t boot_cfg = {
        .pin_bit_mask = 1ULL << BTN_BOOT,
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&boot_cfg));

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
        bool btn_pressed = (gpio_get_level(BTN_BOOT) == 0);
        if(btn_pressed && !btn_prev_pressed) {
            display_on = false;
            display_fill(COL_BLACK);
            display_flush();
            display_brightness_set(0);
        }
        btn_prev_pressed = btn_pressed;

        uint32_t notif = 0;
        xTaskNotifyWait(0, ULONG_MAX, &notif, pdMS_TO_TICKS(16));
        if(notif & NOTIFY_PWRKEY) {
            display_on = false;
            display_fill(COL_BLACK);
            display_flush();
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
            display_fill(COL_GREEN);
            display_flush();
        }
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "AstraFW");

    esp_lcd_panel_io_handle_t io_handle = NULL;

    ESP_ERROR_CHECK(display_new());
    ESP_ERROR_CHECK(display_brightness_set(80));

    display_fill(COL_BLACK);
    display_flush();

    esp_lcd_touch_handle_t tp = NULL;
    ESP_ERROR_CHECK(touch_new(&tp));

    xTaskCreate(draw_task, "draw", 4096, tp, 5, &s_draw_task);

    buttons_init();
}