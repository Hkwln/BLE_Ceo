#pragma once
#include <stdint.h>

void le_audio_pipeline_init(void);
void le_audio_pipeline_start(uint16_t conn_handle);
void le_audio_pipeline_stop(void);
void le_audio_pipeline_rx(uint16_t conn_handle, const uint8_t *lc3_data, uint16_t length);
