#ifndef LE_AUDIO_ISO_H
#define LE_AUDIO_ISO_H

#include "esp_bt.h"
#include "esp_gatt_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

// ISO Data Path IDs (Bluetooth SIG)
#define ISO_DATA_PATH_ID_HCI 0x00
#define ISO_DATA_PATH_ID_I2S 0x01
#define ISO_DATA_PATH_ID_USB 0x02

// ISO Configuration
typedef struct {
    uint16_t conn_handle;
    uint8_t cig_id;
    uint8_t cis_id;
    uint16_t max_sdu_size;
    uint8_t phy;
    uint8_t framing;
    uint8_t block_len;
    uint8_t controller_delay;
} le_audio_iso_config_t;

// ISO RX/TX Buffer
typedef struct {
    uint8_t *rx_buffer;
    uint16_t rx_buffer_size;
    uint8_t *tx_buffer;
    uint16_t tx_buffer_size;
    uint16_t rx_offset;
    uint16_t tx_offset;
} le_audio_iso_buffers_t;

// Function prototypes
void le_audio_iso_init(void);

// ISO Data Path Setup
int le_audio_iso_setup_data_path(const le_audio_iso_config_t *config);
int le_audio_iso_remove_data_path(uint16_t conn_handle, uint8_t data_path_id);

// ISO RX/TX Handlers
void le_audio_iso_rx_handler(uint16_t conn_handle, const uint8_t *data, uint16_t length);
int le_audio_iso_tx_handler(uint16_t conn_handle, uint8_t *data, uint16_t length);

// Buffer management
void le_audio_iso_set_rx_buffer(uint8_t *buffer, uint16_t size);
void le_audio_iso_set_tx_buffer(uint8_t *buffer, uint16_t size);

// Status
int le_audio_iso_is_ready(uint16_t conn_handle);

#ifdef __cplusplus
}
#endif

#endif // LE_AUDIO_ISO_H
