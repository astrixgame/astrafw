#include "power_driver.h"
#include "../err_check.h"
#include "../hardware.h"
#include "../i2c_driver/i2c_driver.h"

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

static const char *TAG = "Power";

#define REG_PMU_STATUS_1        (0x00)
#define REG_PMU_STATUS_2        (0x01)
#define REG_CHIP_ID             (0x03)
#define REG_CHARGE_GAUGE_WDT    (0x18)
#define REG_PWRON_STATUS        (0x20)
#define REG_ADC_ENABLE          (0x30)
#define REG_VBAT_H              (0x34)
#define REG_VBAT_L              (0x35)
#define REG_VBUS_H              (0x38)
#define REG_VBUS_L              (0x39)
#define REG_VSYS_H              (0x3A)
#define REG_VSYS_L              (0x3B)
#define REG_IRQ_ENABLE_0        (0x40)
#define REG_IRQ_ENABLE_1        (0x41)
#define REG_IRQ_ENABLE_2        (0x42)
#define REG_IRQ_STATUS_0        (0x48)
#define REG_IRQ_STATUS_1        (0x49)
#define REG_IRQ_STATUS_2        (0x4A)
#define REG_BATTERY_PERCENT     (0xA4)

#define ADC_EN_VBUS             (1 << 6)
#define ADC_EN_VBAT             (1 << 5)
#define ADC_EN_VSYS             (1 << 4)

#define GAUGE_EN_FUEL_GAUGE     (1 << 7)
#define GAUGE_EN_BAT_VOLT_ADC   (1 << 4)

#define STATUS1_VBUS_GOOD       (1 << 5)
#define STATUS1_BAT_PRESENT     (1 << 3)
#define STATUS1_BAT_ACTIVE      (1 << 2)

#define STATUS2_CHARGE_TIMEOUT  (1 << 7)
#define STATUS2_CHARGE_TERM     (1 << 6)
#define STATUS2_CHARGING        (1 << 5)
#define STATUS2_DISCHARGING     (1 << 4)
#define STATUS2_CHARGE_SUSPEND  (1 << 3)

#define IRQ0_VBUS_REMOVED       (1 << 7)
#define IRQ0_VBUS_CONNECTED     (1 << 6)
#define IRQ0_BAT_INSERTED       (1 << 3)
#define IRQ0_BAT_REMOVED        (1 << 2)

#define IRQ1_CHARGE_DONE        (1 << 7)
#define IRQ1_CHARGE_STARTED     (1 << 6)
#define IRQ1_CHARGE_TIMEOUT     (1 << 5)
#define IRQ1_CHARGE_ERROR       (1 << 4)

#define IRQ2_PWRON_EVENT        (1 << 7)
#define IRQ2_PWROFF_EVENT       (1 << 6)
#define IRQ2_VBUS_OVERVOLT      (1 << 2)
#define IRQ2_TEMP_WARNING       (1 << 1)
#define IRQ2_LOW_BATTERY        (1 << 0)

static i2c_master_dev_handle_t dev_handle = NULL;
static power_irq_callback_t    irq_cb     = NULL;
static QueueHandle_t           irq_queue  = NULL;
static TaskHandle_t            irq_task   = NULL;

static esp_err_t reg_read(uint8_t reg, uint8_t *val) {
    return i2c_master_transmit_receive(dev_handle, &reg, 1, val, 1, pdMS_TO_TICKS(100));
}

static esp_err_t reg_write(uint8_t reg, uint8_t val) {
    uint8_t buf[2] = { reg, val };
    return i2c_master_transmit(dev_handle, buf, sizeof(buf), pdMS_TO_TICKS(100));
}

static int read_adc_mv(uint8_t reg_h, uint8_t reg_l) {
    uint8_t h = 0, l = 0;
    if (reg_read(reg_h, &h) != ESP_OK || reg_read(reg_l, &l) != ESP_OK)
        return -1;
    return (int)(((uint16_t)h << 4) | (l >> 4));
}

static void IRAM_ATTR pmic_gpio_isr(void *arg) {
    uint32_t v = 1;
    xQueueSendFromISR(irq_queue, &v, NULL);
}

static void pmic_irq_task(void *arg) {
    uint32_t v;
    while (1) {
        if (!xQueueReceive(irq_queue, &v, portMAX_DELAY))
            continue;

        uint8_t s0 = 0, s1 = 0, s2 = 0;
        reg_read(REG_IRQ_STATUS_0, &s0);
        reg_read(REG_IRQ_STATUS_1, &s1);
        reg_read(REG_IRQ_STATUS_2, &s2);

        if (s0) reg_write(REG_IRQ_STATUS_0, s0);
        if (s1) reg_write(REG_IRQ_STATUS_1, s1);
        if (s2) reg_write(REG_IRQ_STATUS_2, s2);

        if (!irq_cb || (!s0 && !s1 && !s2))
            continue;

        uint32_t events = 0;

        if (s0 & IRQ0_VBUS_REMOVED)   events |= POWER_IRQ_VBUS_REMOVED;
        if (s0 & IRQ0_VBUS_CONNECTED)  events |= POWER_IRQ_VBUS_CONNECTED;
        if (s0 & IRQ0_BAT_INSERTED)    events |= POWER_IRQ_BAT_INSERTED;
        if (s0 & IRQ0_BAT_REMOVED)     events |= POWER_IRQ_BAT_REMOVED;

        if (s1 & IRQ1_CHARGE_DONE)     events |= POWER_IRQ_CHARGE_DONE;
        if (s1 & IRQ1_CHARGE_STARTED)  events |= POWER_IRQ_CHARGE_STARTED;
        if (s1 & IRQ1_CHARGE_TIMEOUT)  events |= POWER_IRQ_CHARGE_TIMEOUT;
        if (s1 & IRQ1_CHARGE_ERROR)    events |= POWER_IRQ_CHARGE_ERROR;

        if (s2 & IRQ2_PWRON_EVENT)     events |= POWER_IRQ_PWRKEY_SHORT;
        if (s2 & IRQ2_PWROFF_EVENT)    events |= POWER_IRQ_PWRKEY_LONG;

        if (s2 & IRQ2_VBUS_OVERVOLT)   events |= POWER_IRQ_VBUS_OVERVOLT;
        if (s2 & IRQ2_TEMP_WARNING)    events |= POWER_IRQ_TEMP_WARNING;
        if (s2 & IRQ2_LOW_BATTERY)     events |= POWER_IRQ_LOW_BATTERY;

        if (events)
            irq_cb(events);
    }
}

esp_err_t power_init(const power_config_t *config) {
    assert(config != NULL);

    const i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = AXP2101_ADDR,
        .scl_speed_hz    = 400000,
    };
    ERROR_CHECK_RETURN_ERR(i2c_master_bus_add_device(i2c_get_handle(), &dev_cfg, &dev_handle));

    uint8_t chip_id = 0;
    ERROR_CHECK_RETURN_ERR(reg_read(REG_CHIP_ID, &chip_id));
    ESP_LOGI(TAG, "AXP2101 chip ID: 0x%02X", chip_id);

    ERROR_CHECK_RETURN_ERR(reg_write(REG_ADC_ENABLE, ADC_EN_VBUS | ADC_EN_VBAT | ADC_EN_VSYS));

    ERROR_CHECK_RETURN_ERR(reg_write(REG_CHARGE_GAUGE_WDT, GAUGE_EN_FUEL_GAUGE | GAUGE_EN_BAT_VOLT_ADC));

    ERROR_CHECK_RETURN_ERR(reg_write(REG_IRQ_ENABLE_0,
        IRQ0_VBUS_REMOVED | IRQ0_VBUS_CONNECTED | IRQ0_BAT_INSERTED | IRQ0_BAT_REMOVED));
    ERROR_CHECK_RETURN_ERR(reg_write(REG_IRQ_ENABLE_1,
        IRQ1_CHARGE_DONE | IRQ1_CHARGE_STARTED | IRQ1_CHARGE_TIMEOUT | IRQ1_CHARGE_ERROR));
    ERROR_CHECK_RETURN_ERR(reg_write(REG_IRQ_ENABLE_2,
        IRQ2_PWRON_EVENT | IRQ2_PWROFF_EVENT | IRQ2_VBUS_OVERVOLT | IRQ2_TEMP_WARNING | IRQ2_LOW_BATTERY));

    reg_write(REG_IRQ_STATUS_0, 0xFF);
    reg_write(REG_IRQ_STATUS_1, 0xFF);
    reg_write(REG_IRQ_STATUS_2, 0xFF);

    if (config->irq_gpio != GPIO_NUM_NC) {
        irq_queue = xQueueCreate(8, sizeof(uint32_t));
        xTaskCreate(pmic_irq_task, "pmic_irq", 2048, NULL, 10, &irq_task);

        const gpio_config_t io_cfg = {
            .pin_bit_mask = (1ULL << config->irq_gpio),
            .mode         = GPIO_MODE_INPUT,
            .pull_up_en   = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type    = GPIO_INTR_NEGEDGE,
        };
        ERROR_CHECK_RETURN_ERR(gpio_config(&io_cfg));

        esp_err_t ret = gpio_install_isr_service(0);
        if (ret != ESP_ERR_INVALID_STATE)
            ERROR_CHECK_RETURN_ERR(ret);

        ERROR_CHECK_RETURN_ERR(gpio_isr_handler_add(config->irq_gpio, pmic_gpio_isr, NULL));
    }

    ESP_LOGI(TAG, "Initialized");
    return ESP_OK;
}

esp_err_t power_deinit(void) {
    if (irq_task) {
        vTaskDelete(irq_task);
        irq_task = NULL;
    }
    if (irq_queue) {
        vQueueDelete(irq_queue);
        irq_queue = NULL;
    }
    if (dev_handle) {
        i2c_master_bus_rm_device(dev_handle);
        dev_handle = NULL;
    }
    irq_cb = NULL;
    return ESP_OK;
}

int power_get_battery_voltage_mv(void) {
    return read_adc_mv(REG_VBAT_H, REG_VBAT_L);
}

int power_get_system_voltage_mv(void) {
    return read_adc_mv(REG_VSYS_H, REG_VSYS_L);
}

int power_get_input_voltage_mv(void) {
    return read_adc_mv(REG_VBUS_H, REG_VBUS_L);
}

int power_get_battery_percent(void) {
    uint8_t val = 0;
    if (reg_read(REG_BATTERY_PERCENT, &val) != ESP_OK)
        return -1;
    return (int)(val & 0x7F);
}

power_charge_state_t power_get_charge_state(void) {
    uint8_t s2 = 0;
    if (reg_read(REG_PMU_STATUS_2, &s2) != ESP_OK)
        return POWER_CHARGE_NONE;

    if (s2 & STATUS2_CHARGE_TIMEOUT) return POWER_CHARGE_TIMEOUT;
    if (s2 & STATUS2_CHARGE_SUSPEND) return POWER_CHARGE_SUSPENDED;
    if (s2 & STATUS2_CHARGE_TERM)    return POWER_CHARGE_DONE;
    if (s2 & STATUS2_CHARGING)       return POWER_CHARGE_FAST;
    return POWER_CHARGE_NONE;
}

power_bat_state_t power_get_battery_state(void) {
    uint8_t s1 = 0;
    if (reg_read(REG_PMU_STATUS_1, &s1) != ESP_OK)
        return POWER_BAT_ABSENT;

    if (!(s1 & STATUS1_BAT_PRESENT)) return POWER_BAT_ABSENT;
    if (s1 & STATUS1_BAT_ACTIVE)     return POWER_BAT_ACTIVE;
    return POWER_BAT_PRESENT;
}

bool power_is_vbus_present(void) {
    uint8_t s1 = 0;
    reg_read(REG_PMU_STATUS_1, &s1);
    return (s1 & STATUS1_VBUS_GOOD) != 0;
}

esp_err_t power_register_irq_callback(power_irq_callback_t cb) {
    irq_cb = cb;
    return ESP_OK;
}
