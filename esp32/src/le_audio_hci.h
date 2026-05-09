#ifndef LE_AUDIO_HCI_H
#define LE_AUDIO_HCI_H

#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gatt_defs.h"
#include "esp_gattc_api.h"
#include "esp_gatts_api.h"
#include "esp_gap_ble_api.h"

#ifdef __cplusplus
extern "C" {
#endif

// LE Audio HCI Command Definitions
typedef struct {
    uint8_t cig_id;
    uint8_t cis_count;
    uint16_t sdu_interval;
    uint8_t framing;
    uint8_t max_trans_latency;
    uint8_t rtn;
    uint16_t cis_conn_handle[4]; // Max 4 CIS connections
} le_audio_cig_params_t;

// LE Audio CIS State
typedef enum {
    LE_AUDIO_CIS_IDLE,
    LE_AUDIO_CIS_CONFIGURED,
    LE_AUDIO_CIS_ESTABLISHED,
    LE_AUDIO_CIS_DISCONNECTED
} le_audio_cis_state_t;

// ISO Data Path Configuration
typedef struct {
    uint16_t conn_handle;
    uint8_t data_path_id;
    uint8_t direction; // 0=Input, 1=Output
    uint8_t codec_format;
    uint8_t controller_delay;
} le_audio_iso_data_path_t;

// Function prototypes
void le_audio_hci_init(void);

// CIG/CIS Commands
void le_audio_hci_set_cig_parameters(const le_audio_cig_params_t *params);
void le_audio_hci_create_cis(uint16_t *cis_conn_handles, uint8_t count);
void le_audio_hci_remove_cig(uint8_t cig_id);
void le_audio_hci_remove_cis(uint16_t conn_handle);

// ISO Data Path Commands
void le_audio_hci_setup_iso_data_path(const le_audio_iso_data_path_t *config);
void le_audio_hci_remove_iso_data_path(uint16_t conn_handle, uint8_t data_path_id);

// Callback for ISO RX data
void le_audio_iso_rx_callback(const uint8_t *data, uint16_t length, uint16_t conn_handle);

// Callback for ISO TX completion
void le_audio_iso_tx_callback(uint16_t conn_handle, int status);

// State management
le_audio_cis_state_t le_audio_hci_get_cis_state(uint16_t conn_handle);

#ifdef __cplusplus
}
#endif

#endif // LE_AUDIO_HCI_H
