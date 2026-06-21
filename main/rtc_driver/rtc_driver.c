#include "rtc_driver.h"
#include "../i2c_driver/i2c_driver.h"
#include "esp_log.h"
#include "driver/i2c_master.h"

static const char *TAG = "PCF85063";

#define PCF85063_REG_CTRL1      0x00
#define PCF85063_REG_CTRL2      0x01
#define PCF85063_REG_SECONDS    0x04
#define PCF85063_REG_MINUTES    0x05
#define PCF85063_REG_HOURS      0x06
#define PCF85063_REG_DAYS       0x07
#define PCF85063_REG_WEEKDAYS   0x08
#define PCF85063_REG_MONTHS     0x09
#define PCF85063_REG_YEARS      0x0A
#define PCF85063_REG_TMR_VAL    0x10
#define PCF85063_REG_TMR_MODE   0x11

// CTRL2 bits
#define CTRL2_TF        (1 << 3)  // Timer Flag
// Timer mode bits
#define TMR_TE          (1 << 4)  // Timer Enable
#define TMR_TIE         (1 << 5)  // Timer Interrupt Enable
#define TMR_TCF_SHIFT   2         // Clock frequency field offset

static i2c_master_dev_handle_t rtc_dev_handle = NULL;

static uint8_t bcd_to_dec(uint8_t val) {
    return (val / 16 * 10) + (val % 16);
}

static uint8_t dec_to_bcd(uint8_t val) {
    return (val / 10 * 16) + (val % 10);
}

static esp_err_t reg_write(uint8_t reg, uint8_t value) {
    uint8_t buf[2] = {reg, value};
    return i2c_master_transmit(rtc_dev_handle, buf, 2, -1);
}

static esp_err_t reg_read(uint8_t reg, uint8_t *value) {
    return i2c_master_transmit_receive(rtc_dev_handle, &reg, 1, value, 1, -1);
}

esp_err_t rtc_init(void) {
    i2c_master_bus_handle_t bus = i2c_get_handle();
    if (!bus) {
        ESP_LOGE(TAG, "Invalid I2C bus handle");
        return ESP_ERR_INVALID_ARG;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = PCF85063_ADDR,
        .scl_speed_hz    = 100000,
    };

    esp_err_t ret = i2c_master_bus_add_device(bus, &dev_cfg, &rtc_dev_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add RTC device");
        return ret;
    }

    // Clear STOP bit via software reset
    ret = reg_write(PCF85063_REG_CTRL1, 0x20);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize RTC");
        i2c_master_bus_rm_device(rtc_dev_handle);
        rtc_dev_handle = NULL;
        return ret;
    }

    ret = reg_write(PCF85063_REG_CTRL1, 0x00);
    if (ret != ESP_OK) {
        i2c_master_bus_rm_device(rtc_dev_handle);
        rtc_dev_handle = NULL;
        return ret;
    }

    ESP_LOGI(TAG, "PCF85063 initialized");
    return ESP_OK;
}

void rtc_deinit(void) {
    if (rtc_dev_handle) {
        i2c_master_bus_rm_device(rtc_dev_handle);
        rtc_dev_handle = NULL;
    }
}

esp_err_t rtc_set_time(const dtime_t *time) {
    if (!rtc_dev_handle) return ESP_ERR_INVALID_STATE;
    if (!time) return ESP_ERR_INVALID_ARG;

    uint8_t data[8];
    data[0] = PCF85063_REG_SECONDS;
    data[1] = dec_to_bcd(time->second);
    data[2] = dec_to_bcd(time->minute);
    data[3] = dec_to_bcd(time->hour);
    data[4] = dec_to_bcd(time->day);
    data[5] = time->weekday;
    data[6] = dec_to_bcd(time->month);
    data[7] = dec_to_bcd(time->year);

    esp_err_t ret = i2c_master_transmit(rtc_dev_handle, data, 8, -1);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Time set: %02d:%02d:%02d %02d/%02d/20%02d",
                 time->hour, time->minute, time->second,
                 time->day, time->month, time->year);
    }
    return ret;
}

esp_err_t rtc_get_time(dtime_t *time) {
    if (!rtc_dev_handle) return ESP_ERR_INVALID_STATE;
    if (!time) return ESP_ERR_INVALID_ARG;

    uint8_t reg = PCF85063_REG_SECONDS;
    uint8_t data[7];

    esp_err_t ret = i2c_master_transmit_receive(rtc_dev_handle, &reg, 1, data, 7, -1);
    if (ret == ESP_OK) {
        time->second  = bcd_to_dec(data[0] & 0x7F);
        time->minute  = bcd_to_dec(data[1] & 0x7F);
        time->hour    = bcd_to_dec(data[2] & 0x3F);
        time->day     = bcd_to_dec(data[3] & 0x3F);
        time->weekday = data[4] & 0x07;
        time->month   = bcd_to_dec(data[5] & 0x1F);
        time->year    = bcd_to_dec(data[6]);
    }
    return ret;
}

esp_err_t rtc_start_timer(uint8_t value, rtc_timer_clk_t clk, bool interrupt_enable) {
    if (!rtc_dev_handle) return ESP_ERR_INVALID_STATE;

    esp_err_t ret = reg_write(PCF85063_REG_TMR_VAL, value);
    if (ret != ESP_OK) return ret;

    uint8_t mode = TMR_TE | ((clk & 0x03) << TMR_TCF_SHIFT);
    if (interrupt_enable)
        mode |= TMR_TIE;

    ret = reg_write(PCF85063_REG_TMR_MODE, mode);
    if (ret == ESP_OK)
        ESP_LOGI(TAG, "Timer started: value=%d clk=%d irq=%d", value, (int)clk, interrupt_enable);
    return ret;
}

esp_err_t rtc_stop_timer(void) {
    if (!rtc_dev_handle) return ESP_ERR_INVALID_STATE;

    esp_err_t ret = reg_write(PCF85063_REG_TMR_MODE, 0x00);
    if (ret == ESP_OK)
        ESP_LOGI(TAG, "Timer stopped");
    return ret;
}

bool rtc_is_timer_expired(void) {
    if (!rtc_dev_handle) return false;

    uint8_t ctrl2 = 0;
    if (reg_read(PCF85063_REG_CTRL2, &ctrl2) != ESP_OK)
        return false;
    return (ctrl2 & CTRL2_TF) != 0;
}

esp_err_t rtc_clear_timer_flag(void) {
    if (!rtc_dev_handle) return ESP_ERR_INVALID_STATE;

    uint8_t ctrl2 = 0;
    esp_err_t ret = reg_read(PCF85063_REG_CTRL2, &ctrl2);
    if (ret != ESP_OK) return ret;

    return reg_write(PCF85063_REG_CTRL2, ctrl2 & ~CTRL2_TF);
}
