idf_component_register(
    SRCS "main.c" "err_check.h" "i2c_driver/i2c_driver.c" "i2s_driver/i2s_driver.c" "power_driver/power_driver.c" "display_driver/display_driver.c" "spiffs_driver/spiffs_driver.c"
    INCLUDE_DIRS "." "i2c_driver" "i2s_driver" "power_driver" "display_driver" "spiffs_driver"
    REQUIRES esp_lcd_touch freertos esp_driver_i2c driver esp_driver_gpio esp_lcd esp_driver_i2s esp_lcd_sh8601 esp_lcd_touch_ft5x06 esp_lcd_panel_io_additions esp_codec_dev
    PRIV_REQUIRES esp_timer spiffs esp_psram fatfs esp_driver_ledc
)