#pragma once

#include "driver/gpio.h"
#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#define AXP2101_ADDR        (0x34)

typedef enum {
    POWER_IRQ_VBUS_REMOVED    = (1 << 0),
    POWER_IRQ_VBUS_CONNECTED  = (1 << 1),
    POWER_IRQ_BAT_INSERTED    = (1 << 2),
    POWER_IRQ_BAT_REMOVED     = (1 << 3),
    POWER_IRQ_CHARGE_DONE     = (1 << 4),
    POWER_IRQ_CHARGE_STARTED  = (1 << 5),
    POWER_IRQ_CHARGE_TIMEOUT  = (1 << 6),
    POWER_IRQ_CHARGE_ERROR    = (1 << 7),
    POWER_IRQ_LOW_BATTERY     = (1 << 8),
    POWER_IRQ_PWRKEY_SHORT    = (1 << 9),
    POWER_IRQ_PWRKEY_LONG     = (1 << 10),
    POWER_IRQ_TEMP_WARNING    = (1 << 11),
    POWER_IRQ_VBUS_OVERVOLT   = (1 << 12),
} power_irq_event_t;

typedef enum {
    POWER_CHARGE_NONE = 0,
    POWER_CHARGE_PRE,
    POWER_CHARGE_FAST,
    POWER_CHARGE_DONE,
    POWER_CHARGE_TIMEOUT,
    POWER_CHARGE_SUSPENDED,
    POWER_CHARGE_ERROR,
} power_charge_state_t;

typedef enum {
    POWER_BAT_ABSENT = 0,
    POWER_BAT_PRESENT,
    POWER_BAT_ACTIVE,
} power_bat_state_t;

typedef void (*power_irq_callback_t)(uint32_t events);

typedef struct {
    gpio_num_t irq_gpio;
} power_config_t;

esp_err_t power_init(const power_config_t *config);
esp_err_t power_deinit(void);

int  power_get_battery_voltage_mv(void);
int  power_get_system_voltage_mv(void);
int  power_get_input_voltage_mv(void);

int  power_get_battery_percent(void);

power_charge_state_t power_get_charge_state(void);
power_bat_state_t    power_get_battery_state(void);
bool                 power_is_vbus_present(void);

esp_err_t power_register_irq_callback(power_irq_callback_t cb);
