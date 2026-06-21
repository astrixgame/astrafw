#include "nvs_driver.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "nvs";

esp_err_t nvs_drv_init(void) {
    esp_err_t err = nvs_flash_init();
    if(err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    return err;
}

esp_err_t nvs_drv_save_int(const char *key, int32_t value) {
    nvs_handle_t handle;
    esp_err_t err;

    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &handle);
    if(err != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_i32(handle, key, value);
    if(err != ESP_OK) {
        ESP_LOGE(TAG, "Error writing to NVS: %s", esp_err_to_name(err));
        nvs_close(handle);
        return err;
    }

    err = nvs_commit(handle);
    if(err != ESP_OK)
        ESP_LOGE(TAG, "Error committing to NVS: %s", esp_err_to_name(err));

    nvs_close(handle);
    ESP_LOGI(TAG, "Saved %s = %ld", key, (long)value);
    return err;
}

esp_err_t nvs_drv_save_string(const char *key, const char *value) {
    nvs_handle_t handle;
    esp_err_t err;

    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &handle);
    if(err != ESP_OK)
        return err;

    err = nvs_set_str(handle, key, value);
    if(err == ESP_OK)
        err = nvs_commit(handle);

    nvs_close(handle);
    ESP_LOGI(TAG, "Saved %s = %s", key, value);
    return err;
}

esp_err_t nvs_drv_save_uint8(const char *key, uint8_t value) {
    nvs_handle_t handle;
    esp_err_t err;

    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &handle);
    if(err != ESP_OK)
        return err;

    err = nvs_set_u8(handle, key, value);
    if(err == ESP_OK)
        err = nvs_commit(handle);

    nvs_close(handle);
    ESP_LOGI(TAG, "Saved %s = %d", key, value);
    return err;
}

esp_err_t nvs_drv_save_blob(const char *key, const void *data, size_t length) {
    nvs_handle_t handle;
    esp_err_t err;

    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &handle);
    if(err != ESP_OK)
        return err;

    err = nvs_set_blob(handle, key, data, length);
    if(err == ESP_OK)
        err = nvs_commit(handle);

    nvs_close(handle);
    ESP_LOGI(TAG, "Saved blob %s (%zu bytes)", key, length);
    return err;
}

int32_t nvs_drv_load_int(const char *key, int32_t default_value) {
    nvs_handle_t handle;
    esp_err_t err;
    int32_t value = default_value;

    err = nvs_open(STORAGE_NAMESPACE, NVS_READONLY, &handle);
    if(err != ESP_OK) {
        ESP_LOGW(TAG, "NVS not opened, using default for %s", key);
        return default_value;
    }

    err = nvs_get_i32(handle, key, &value);
    if(err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "Key %s not found, using default: %ld", key, (long)default_value);
        value = default_value;
    } else if(err != ESP_OK) {
        ESP_LOGE(TAG, "Error reading %s: %s", key, esp_err_to_name(err));
        value = default_value;
    } else
        ESP_LOGI(TAG, "Loaded %s = %ld", key, (long)value);

    nvs_close(handle);
    return value;
}

esp_err_t nvs_drv_load_string(const char *key, char *buffer, size_t buffer_size) {
    nvs_handle_t handle;
    esp_err_t err;
    size_t required_size = buffer_size;

    err = nvs_open(STORAGE_NAMESPACE, NVS_READONLY, &handle);
    if(err != ESP_OK)
        return err;

    err = nvs_get_str(handle, key, buffer, &required_size);
    if(err == ESP_OK)
        ESP_LOGI(TAG, "Loaded %s = %s", key, buffer);
    else if(err == ESP_ERR_NVS_NOT_FOUND)
        ESP_LOGW(TAG, "Key %s not found", key);

    nvs_close(handle);
    return err;
}

uint8_t nvs_drv_load_uint8(const char *key, uint8_t default_value) {
    nvs_handle_t handle;
    esp_err_t err;
    uint8_t value = default_value;

    err = nvs_open(STORAGE_NAMESPACE, NVS_READONLY, &handle);
    if(err != ESP_OK)
        return default_value;

    err = nvs_get_u8(handle, key, &value);
    if(err != ESP_OK) {
        value = default_value;
        ESP_LOGW(TAG, "Key %s not found or error, using default: %d", key, default_value);
    } else
        ESP_LOGI(TAG, "Loaded %s = %d", key, value);

    nvs_close(handle);
    return value;
}

esp_err_t nvs_drv_load_blob(const char *key, void *data, size_t *length) {
    nvs_handle_t handle;
    esp_err_t err;

    err = nvs_open(STORAGE_NAMESPACE, NVS_READONLY, &handle);
    if(err != ESP_OK)
        return err;

    size_t required_size = 0;
    err = nvs_get_blob(handle, key, NULL, &required_size);

    if(err == ESP_OK && required_size > 0) {
        if(required_size <= *length) {
            err = nvs_get_blob(handle, key, data, &required_size);
            *length = required_size;
            ESP_LOGI(TAG, "Loaded blob %s (%zu bytes)", key, required_size);
        } else
            err = ESP_ERR_NVS_INVALID_LENGTH;
    }

    nvs_close(handle);
    return err;
}

esp_err_t nvs_drv_delete_key(const char *key) {
    nvs_handle_t handle;
    esp_err_t err;

    err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &handle);
    if(err != ESP_OK)
        return err;

    err = nvs_erase_key(handle, key);
    if(err == ESP_OK) {
        err = nvs_commit(handle);
        ESP_LOGI(TAG, "Deleted key: %s", key);
    }

    nvs_close(handle);
    return err;
}
