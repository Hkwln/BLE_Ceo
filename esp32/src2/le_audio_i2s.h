#pragma once
#include "esp_err.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ── XIAO ESP32-S3 → MAX98357A wiring ──────────────────
// GPIO 2  →  BCLK
// GPIO 3  →  LRC  (WS)
// GPIO 4  →  DIN
// 3V3     →  VDD
// GND     →  GND
// Speaker (4-8 Ω) between OUT+ / OUT-
// ───────────────────────────────────────────────────────
#define I2S_BCLK_PIN 2
#define I2S_LRCLK_PIN 3
#define I2S_DOUT_PIN 4

esp_err_t le_audio_i2s_init(void);
esp_err_t le_audio_i2s_write(const int16_t *pcm, size_t samples,
                             size_t *written_samples);
void le_audio_i2s_deinit(void);

#ifdef __cplusplus
}
#endif
