#include "le_audio_lc3.h"
#include "esp_log.h"
#include "lc3.h" // google/liblc3
#include <stdlib.h>
#include <string.h>

static const char *TAG = "LC3";

// ── Opaque struct ────────────────────────────────────────
// liblc3 requires a memory block whose size is known only
// at runtime via lc3_decoder_size().  We heap-allocate it.
struct le_audio_lc3_decoder {
  lc3_decoder_t *handle; // points inside mem[]
  void *mem;             // raw allocation
};

le_audio_lc3_decoder_t *le_audio_lc3_decoder_create(void) {
  // 1. Query required memory size (runtime, not compile-time macro)
  unsigned mem_size = lc3_decoder_size(LC3_FRAME_DURATION, LC3_SAMPLE_RATE);
  if (mem_size == 0) {
    ESP_LOGE(TAG, "lc3_decoder_size returned 0 – bad params");
    return NULL;
  }

  // 2. Allocate wrapper + codec memory
  le_audio_lc3_decoder_t *dec = calloc(1, sizeof(*dec));
  if (!dec) {
    ESP_LOGE(TAG, "OOM (wrapper)");
    return NULL;
  }

  dec->mem = malloc(mem_size);
  if (!dec->mem) {
    ESP_LOGE(TAG, "OOM (codec mem %u bytes)", mem_size);
    free(dec);
    return NULL;
  }

  // 3. Initialise decoder
  // sr_pcm_hz = 0  →  same as sr_hz (no resampling)
  dec->handle =
      lc3_setup_decoder(LC3_FRAME_DURATION, LC3_SAMPLE_RATE, 0, dec->mem);
  if (!dec->handle) {
    ESP_LOGE(TAG, "lc3_setup_decoder failed");
    free(dec->mem);
    free(dec);
    return NULL;
  }

  ESP_LOGI(TAG, "LC3 decoder ready: %d Hz / %d ms / %d B/frame",
           LC3_SAMPLE_RATE, LC3_FRAME_DURATION / 1000, LC3_BYTES_PER_FRAME);
  return dec;
}

void le_audio_lc3_decoder_destroy(le_audio_lc3_decoder_t *dec) {
  if (!dec)
    return;
  free(dec->mem);
  free(dec);
}

int le_audio_lc3_decode(le_audio_lc3_decoder_t *dec, const uint8_t *lc3_data,
                        int lc3_len, int16_t *pcm_out, int pcm_samples) {
  if (!dec || !lc3_data || !pcm_out)
    return -1;

  // lc3_decode() returns:
  //   0  = OK
  //   1  = PLC applied (frame lost, concealment used)
  //  -1  = error
  int ret =
      lc3_decode(dec->handle, lc3_data, lc3_len, LC3_PCM_FORMAT_S16, pcm_out,
                 1); // stride = 1 (mono)
  if (ret < 0) {
    ESP_LOGW(TAG, "lc3_decode error %d", ret);
    // Zero-fill so we don't emit garbage to I2S
    memset(pcm_out, 0, (size_t)pcm_samples * sizeof(int16_t));
    return ret;
  }
  if (ret == 1) {
    ESP_LOGD(TAG, "PLC active");
  }
  return pcm_samples;
}
