#pragma once

#include "driver/i2s_std.h"
#include "esp_codec_dev.h"
#include "../hardware.h"

esp_err_t audio_init(const i2s_std_config_t *i2s_config);

esp_codec_dev_handle_t audio_codec_speaker_init(void);

esp_codec_dev_handle_t audio_codec_microphone_init(void);