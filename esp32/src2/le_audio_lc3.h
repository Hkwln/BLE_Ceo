#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// liblc3 parameters
#define LC3_SAMPLE_RATE 48000    // Hz
#define LC3_FRAME_DURATION 10000 // µs  (10 ms)
#define LC3_BYTES_PER_FRAME 120  // encoded bytes per frame
#define LC3_PCM_SAMPLES 480      // 48000 * 0.010

// Opaque decoder handle
typedef struct le_audio_lc3_decoder le_audio_lc3_decoder_t;

le_audio_lc3_decoder_t *le_audio_lc3_decoder_create(void);
void le_audio_lc3_decoder_destroy(le_audio_lc3_decoder_t *dec);

// Returns LC3_PCM_SAMPLES on success, negative on error (PLC applied)
int le_audio_lc3_decode(le_audio_lc3_decoder_t *dec, const uint8_t *lc3_data,
                        int lc3_len, int16_t *pcm_out, int pcm_samples);

#ifdef __cplusplus
}
#endif
