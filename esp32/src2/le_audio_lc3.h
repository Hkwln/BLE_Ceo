#pragma once
#include <stdint.h>

#define LC3_SAMPLE_RATE      48000
#define LC3_FRAME_DURATION   10000   // µs
#define LC3_CHANNELS         1
#define LC3_BYTES_PER_FRAME  120
#define LC3_PCM_SAMPLES      480     // 48000 * 0.01s

typedef struct le_audio_lc3_decoder_t le_audio_lc3_decoder_t;

le_audio_lc3_decoder_t *le_audio_lc3_decoder_create(void);
void le_audio_lc3_decoder_destroy(le_audio_lc3_decoder_t *dec);
int le_audio_lc3_decode(le_audio_lc3_decoder_t *dec,
                        const uint8_t *lc3_data, int lc3_len,
                        int16_t *pcm_out, int pcm_samples);
