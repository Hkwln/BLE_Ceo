#pragma once
#include "esp_err.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  LE_AUDIO_CIS_IDLE = 0x00,
  LE_AUDIO_CIS_CONFIGURED = 0x01,
  LE_AUDIO_CIS_ESTABLISHED = 0x02,
  LE_AUDIO_CIS_STREAMING = 0x03,
} le_audio_cis_state_t;

typedef enum {
  LE_AUDIO_HCI_EVT_CIG_CREATED = 0x01,
  LE_AUDIO_HCI_EVT_CIS_ESTABLISHED = 0x02,
  LE_AUDIO_HCI_EVT_CIS_REQUEST = 0x03,
  LE_AUDIO_HCI_EVT_STREAM_READY = 0x04,
} le_audio_hci_event_type_t;

typedef struct {
  le_audio_hci_event_type_t type;
  uint16_t cis_handle;
  uint16_t acl_handle;
  uint8_t cig_id;
  uint8_t cis_id;
  uint8_t cis_count;
  uint16_t cis_handles[4];
  uint8_t status;
} le_audio_hci_event_t;

typedef void (*le_audio_hci_event_cb_t)(const le_audio_hci_event_t *evt);

void le_audio_hci_init(void);
void le_audio_hci_register_event_cb(le_audio_hci_event_cb_t cb);
void le_audio_hci_notify_stream_ready(uint16_t conn_handle);
le_audio_cis_state_t le_audio_hci_get_cis_state(uint16_t conn_handle);

#ifdef __cplusplus
}
#endif
