#pragma once
#include <stdbool.h>
#include "esp_gatts_api.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void le_audio_gatts_register_handler(void);
void le_audio_pacs_init(void);
void le_audio_ascs_init(void);
void le_audio_vcs_init(void);

void le_audio_gatt_send_audio_frame(uint16_t conn_handle, const uint8_t *data,
                                    uint16_t length);
void le_audio_vcs_update_volume(uint8_t vol);
uint8_t le_audio_vcs_get_volume(void);

typedef void (*le_audio_audio_rx_cb_t)(uint16_t conn_handle,
                                       const uint8_t *data, uint16_t length);
void le_audio_register_audio_rx_cb(le_audio_audio_rx_cb_t cb);

// Called by GATT layer when client connects/disconnects
// so main can restart advertising
typedef void (*le_audio_conn_cb_t)(uint16_t conn_handle, bool connected);
void le_audio_register_conn_cb(le_audio_conn_cb_t cb);

#ifdef __cplusplus
}
#endif
