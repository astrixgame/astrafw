#pragma once

#include "esp_err.h"

#define SPIFFS_MOUNT_POINT "/spiffs"

esp_err_t bsp_spiffs_mount(void);

esp_err_t bsp_spiffs_unmount(void);