#include "i2c_driver.h"
#include "../err_check.h"
#include "../hardware.h"

#include "esp_err.h"

static i2c_master_bus_handle_t i2c_handle = NULL;
static bool i2c_initialized = false;

esp_err_t i2c_init(void) {
    if(i2c_initialized)
        return ESP_OK;

    i2c_master_bus_config_t i2c_bus_conf = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .sda_io_num = I2C_SDA,
        .scl_io_num = I2C_SCL,
        .i2c_port = I2C_NUM,
    };
    ERROR_CHECK_RETURN_ERR(i2c_new_master_bus(&i2c_bus_conf, &i2c_handle));

    i2c_initialized = true;

    return ESP_OK;
}

esp_err_t i2c_deinit(void) {
    ERROR_CHECK_RETURN_ERR(i2c_del_master_bus(i2c_handle));
    i2c_initialized = false;
    return ESP_OK;
}

i2c_master_bus_handle_t i2c_get_handle(void) {
    i2c_init();
    return i2c_handle;
}