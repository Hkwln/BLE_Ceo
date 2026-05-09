#include "le_audio_lc3.h"
#include "lc3.h"                // from liblc3
#include "esp_log.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "LC3";

struct le_audio_lc3_decoder_t {
    lc3_decoder_t *handle;
    uint8_t scratch[LC3_DECODER_BUFFER_SIZE(LC3_FRAME_DURATION, LC3_SAMPLE_RATE)];
};

le_audio_lc3_decoder_t *le_audio_lc3_decoder_create(void) {
    le_audio_lc3_decoder_t *dec = (le_audio_lc3_decoder_t *)calloc(1, sizeof(*dec));
    if (!dec) {
        ESP_LOGE(TAG, "OOM");
        return NULL;
    }
    dec->handle = lc3_setup_decoder(LC3_FRAME_DURATION, LC3_SAMPLE_RATE, 0,
                                    dec->scratch, sizeof(dec->scratch));
    if (!dec->handle) {
        ESP_LOGE(TAG, "LC3 decoder setup failed");
        free(dec);
        return NULL;
    }
    ESP_LOGI(TAG, "LC3 Decoder ready: %dHz/%dms/%dB", LC3_SAMPLE_RATE,
             LC3_FRAME_DURATION/1000, LC3_BYTES_PER_FRAME);
    return dec;
}

void le_audio_lc3_decoder_destroy(le_audio_lc3_decoder_t *dec) {
    if (dec) free(dec);
}

int le_audio_lc3_decode(le_audio_lc3_decoder_t *dec,
                        const uint8_t *lc3_data, int lc3_len,
                        int16_t *pcm_out, int pcm_samples) {
    if (!dec || !lc3_data || !pcm_out) return -1;
    int ret = lc3_decode(dec->handle, lc3_data, lc3_len,
                         LC3_PCM_FORMAT_S16, pcm_out, 1);
    if (ret < 0) {
        ESP_LOGW(TAG, "LC3 decode error %d (PLC active)", ret);
    }
    return (ret == 0) ? pcm_samples : ret;
}
