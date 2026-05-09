#include "le_audio_i2s.h"
#include "le_audio_lc3.h"
#include "esp_log.h"
#include "driver/i2s_std.h"
#include <string.h>

static const char *TAG = "I2S_DAC";
static i2s_chan_handle_t tx_channel = NULL;

esp_err_t le_audio_i2s_init(void) {
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true;
    esp_err_t ret = i2s_new_channel(&chan_cfg, &tx_channel, NULL);
    if (ret != ESP_OK) { ESP_LOGE(TAG, "i2s_new_channel fail"); return ret; }

    i2s_std_config_t std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(LC3_SAMPLE_RATE),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = I2S_BCLK_PIN,
            .ws   = I2S_LRCLK_PIN,
            .dout = I2S_DOUT_PIN,
            .din  = I2S_GPIO_UNUSED,
            .invert_flags = { .mclk_inv = false, .bclk_inv = false, .ws_inv = false },
        },
    };
    ret = i2s_channel_init_std_mode(tx_channel, &std_cfg);
    if (ret != ESP_OK) { i2s_del_channel(tx_channel); tx_channel = NULL; return ret; }
    ret = i2s_channel_enable(tx_channel);
    if (ret != ESP_OK) return ret;
    ESP_LOGI(TAG, "I2S init OK: %dHz Mono, BCLK=%d LRCLK=%d DOUT=%d", LC3_SAMPLE_RATE, I2S_BCLK_PIN, I2S_LRCLK_PIN, I2S_DOUT_PIN);
    return ESP_OK;
}

esp_err_t le_audio_i2s_write(const int16_t *pcm_data, size_t samples, size_t *written_samples) {
    if (!tx_channel || !pcm_data) return ESP_ERR_INVALID_STATE;
    size_t bytes_written = 0;
    size_t bytes_to_write = samples * sizeof(int16_t);
    esp_err_t ret = i2s_channel_write(tx_channel, pcm_data, bytes_to_write, &bytes_written, pdMS_TO_TICKS(20));
    if (written_samples) *written_samples = bytes_written / sizeof(int16_t);
    return ret;
}

void le_audio_i2s_deinit(void) {
    if (tx_channel) {
        i2s_channel_disable(tx_channel);
        i2s_del_channel(tx_channel);
        tx_channel = NULL;
    }
}
