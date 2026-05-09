#ifndef LE_AUDIO_PACS_ASCS_VCS_H
#define LE_AUDIO_PACS_ASCS_VCS_H

#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gatt_defs.h"
#include "esp_gattc_api.h"
#include "esp_gatts_api.h"

#ifdef __cplusplus
extern "C" {
#endif

// Initialize LE Audio Services
void le_audio_pacs_init(void);
void le_audio_ascs_init(void);
void le_audio_vcs_init(void);

// Volume Control
void le_audio_vcs_update_volume(uint8_t volume);
uint8_t le_audio_vcs_get_volume(void);

// GATT Event Handler
void le_audio_gatts_register_handler(void);

#ifdef __cplusplus
}
#endif

#endif // LE_AUDIO_PACS_ASCS_VCS_H
