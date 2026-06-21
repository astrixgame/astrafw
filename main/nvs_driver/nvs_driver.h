#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifndef STORAGE_NAMESPACE
#define STORAGE_NAMESPACE "default"
#endif

esp_err_t nvs_drv_init(void);

esp_err_t nvs_drv_save_int(const char *key, int32_t value);
int32_t   nvs_drv_load_int(const char *key, int32_t default_value);

esp_err_t nvs_drv_save_string(const char *key, const char *value);
esp_err_t nvs_drv_load_string(const char *key, char *buffer, size_t buffer_size);

esp_err_t nvs_drv_save_uint8(const char *key, uint8_t value);
uint8_t   nvs_drv_load_uint8(const char *key, uint8_t default_value);

esp_err_t nvs_drv_save_blob(const char *key, const void *data, size_t length);
esp_err_t nvs_drv_load_blob(const char *key, void *data, size_t *length);

esp_err_t nvs_drv_delete_key(const char *key);
