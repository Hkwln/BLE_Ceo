#pragma once
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void le_audio_pipeline_init(void);
bool le_audio_pipeline_start(uint16_t conn_handle);
void le_audio_pipeline_stop(void);

// Called from GATT write callback (normal task context, NOT ISR)
void le_audio_pipeline_rx(uint16_t conn_handle, const uint8_t *lc3_data,
                          uint16_t length);

#ifdef __cplusplus
}
#endif
