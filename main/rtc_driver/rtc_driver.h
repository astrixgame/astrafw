#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#define PCF85063_ADDR 0x51

typedef struct {
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day;
    uint8_t weekday;
    uint8_t month;
    uint8_t year; // 2-digit offset from 2000 (e.g. 24 = 2024)
} dtime_t;

typedef enum {
    RTC_TIMER_CLK_4096HZ = 0x00, // ~244 us per tick
    RTC_TIMER_CLK_64HZ   = 0x01, // ~15.6 ms per tick
    RTC_TIMER_CLK_1HZ    = 0x02, // 1 s per tick  (max ~255 s)
    RTC_TIMER_CLK_1_60HZ = 0x03, // 1 min per tick (max ~255 min)
} rtc_timer_clk_t;

esp_err_t rtc_init(void);
void      rtc_deinit(void);

esp_err_t rtc_set_time(const dtime_t *time);
esp_err_t rtc_get_time(dtime_t *time);

esp_err_t rtc_start_timer(uint8_t value, rtc_timer_clk_t clk, bool interrupt_enable);
esp_err_t rtc_stop_timer(void);
bool      rtc_is_timer_expired(void);
esp_err_t rtc_clear_timer_flag(void);
