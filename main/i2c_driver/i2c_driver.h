#pragma once

#include "driver/i2c_master.h"
#include "esp_err.h"

esp_err_t i2c_init(void);

esp_err_t i2c_deinit(void);

i2c_master_bus_handle_t i2c_get_handle(void);