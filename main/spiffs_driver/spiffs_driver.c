#include "spiffs_driver.h"
#include "../err_check.h"

#include "esp_vfs_fat.h"
#include "esp_spiffs.h"

static const char *TAG = "spiffs";

esp_err_t bsp_spiffs_mount(void) {
    esp_vfs_spiffs_conf_t conf = {
        .base_path = SPIFFS_MOUNT_POINT,
        .partition_label = "storage",
        .max_files = 2,
        .format_if_mount_failed = false,
    };

    esp_err_t ret_val = esp_vfs_spiffs_register(&conf);

    ERROR_CHECK_RETURN_ERR(ret_val);

    size_t total = 0, used = 0;
    ret_val = esp_spiffs_info(conf.partition_label, &total, &used);

    if(ret_val != ESP_OK)
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret_val));
    else
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);

    return ret_val;
}

esp_err_t bsp_spiffs_unmount(void) {
    return esp_vfs_spiffs_unregister("storage");
}