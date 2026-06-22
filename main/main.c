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
#include "nvs_driver/nvs_driver.h"
#include "rtc_driver/rtc_driver.h"
#include "power_driver/power_driver.h"
#include "display_driver/display_driver.h"
#include "font_render/font_render.h"
#include "font_render/font_arial_16.h"
#include "font_render/font_arial_20.h"
#include "font_render/font_arial_48.h"
#include "font_render/font_arial_128.h"

static const char *TAG = "main";

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
    bool display_on = true;

    while(1) {
        bool btn_pressed = (gpio_get_level(BTN_BOOT) == 0);
        if(btn_pressed && !btn_prev_pressed) {
            display_on = false;
            display_fill(CLR_BLACK);
            display_flush();
            display_brightness_set(0);
        }
        btn_prev_pressed = btn_pressed;

        uint32_t notif = 0;
        xTaskNotifyWait(0, ULONG_MAX, &notif, pdMS_TO_TICKS(16));
        if(notif & NOTIFY_PWRKEY) {
            display_on = false;
            display_fill(CLR_BLACK);
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
            display_fill(CLR_BLACK);

            dtime_t dt = {0};
            rtc_get_time(&dt);

            char hh_str[4], mm_str[4], ss_str[4];
            uint8_t h12 = dt.hour % 12;
            if(h12 == 0) h12 = 12;
            const char *ampm_str = dt.hour >= 12 ? "PM" : "AM";
            snprintf(hh_str, sizeof(hh_str), "%02d", h12);
            snprintf(mm_str, sizeof(mm_str), "%02d", dt.minute);
            snprintf(ss_str, sizeof(ss_str), "%02d", dt.second);

            // Left column — time stacked vertically
            int time_x = 20;
            int ty = 20;
            font_draw_string(&font_arial_128, time_x, ty, hh_str, CLR_WHITE);
            ty += font_line_height(&font_arial_128);
            font_draw_string(&font_arial_128, time_x, ty, mm_str, CLR_WHITE);
            ty += font_line_height(&font_arial_128);
            font_draw_string(&font_arial_128, time_x, ty, ss_str, CLR_WHITE);
            ty += font_line_height(&font_arial_128);
            font_draw_string(&font_arial_128, time_x, ty, ampm_str, CLR_WHITE);

            // Right column — date block
            char date_str[64];
            // compute day-of-year for week number
            static const uint8_t dim[] = {0,31,28,31,30,31,30,31,31,30,31,30,31};
            bool is_leap = ((2000 + dt.year) % 4 == 0);
            int doy = dt.day;
            for (int m = 1; m < dt.month && m <= 11; m++) {
                doy += dim[m];
                if (m == 2 && is_leap) doy++;
            }
            int week_num  = (doy - 1) / 7 + 1;
            bool week_even = (week_num % 2 == 0);

            int date_right = LCD_W - 10;
            int dy = 20;

            // Row 1 — weekday name
            const char *wday = RTC_WEEKDAY_NAMES[dt.weekday % 7];
            int wdw = font_measure_string(&font_arial_20, wday);
            font_draw_string(&font_arial_20, date_right - wdw, dy, wday, CLR_YELLOW);
            dy += 28;

            // Row 2 — DD / MM / YYYY
            snprintf(date_str, sizeof(date_str), "%02d / %02d / %04d",
                     dt.day, dt.month, 2000 + dt.year);
            int datew = font_measure_string(&font_arial_20, date_str);
            font_draw_string(&font_arial_20, date_right - datew, dy, date_str, CLR_WHITE);
            dy += 28;

            // Row 3 — Month name  |  Week N (Even/Odd)
            char daymon_str[64];
            snprintf(daymon_str, sizeof(daymon_str), "%s  |  Week %d (%s)",
                     RTC_MONTH_NAMES[dt.month <= 12 ? dt.month : 0],
                     week_num, week_even ? "Even" : "Odd");
            int dmw = font_measure_string(&font_arial_16, daymon_str);
            font_draw_string(&font_arial_16, date_right - dmw, dy, daymon_str, CLR_YELLOW);

            display_flush();
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "AstraFW");

    ESP_ERROR_CHECK(display_new());
    ESP_ERROR_CHECK(display_brightness_set(80));

    ESP_ERROR_CHECK(nvs_drv_init());

    const power_config_t pwr_cfg = { .irq_gpio = GPIO_NUM_NC };
    ESP_ERROR_CHECK(power_init(&pwr_cfg));

    ESP_ERROR_CHECK(rtc_init());

    dtime_t dt = {
        .second  = 0,
        .minute  = 0,
        .hour    = 0,
        .day     = 22,
        .weekday = 1,
        .month   = 6,
        .year    = 26,
    };
    rtc_set_time(&dt);
    
    display_fill(CLR_BLACK);
    display_flush();
    
    esp_lcd_touch_handle_t tp = NULL;
    ESP_ERROR_CHECK(touch_new(&tp));

    xTaskCreate(draw_task, "draw", 4096, tp, 5, &s_draw_task);

    buttons_init();
}