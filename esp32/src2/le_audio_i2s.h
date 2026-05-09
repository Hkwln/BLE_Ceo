#pragma once
#include <stdint.h>
#include "esp_err.h"

// XIAO ESP32-S3 pins for MAX98357A
#define I2S_BCLK_PIN    2   // D0
#define I2S_LRCLK_PIN   3   // D1
#define I2S_DOUT_PIN    4   // D2

esp_err_t le_audio_i2s_init(void);
esp_err_t le_audio_i2s_write(const int16_t *pcm_data, size_t samples, size_t *written_samples);
void le_audio_i2s_deinit(void);
