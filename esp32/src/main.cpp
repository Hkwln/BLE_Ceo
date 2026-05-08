#include <Arduino.h>

#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_log.h"

#include "le_audio_hci.h"
#include "le_audio_iso.h"
#include "le_audio_pacs_ascs_vcs.h"

#define HAS_BLE_HCI_ISO 1

static const char *TAG = "LE_ISO_POC";

// LE Audio ISO Layer Integration
static le_audio_cig_params_t cig_params = {
    .cig_id = 1,
    .cis_count = 2,
    .sdu_interval = 10000, // 10ms
    .framing = 0, // Unframed
    .max_trans_latency = 40, // 40ms
    .rtn = 2, // Retransmissions
    .cis_conn_handle = {0x0040, 0x0041} // Example connection handles
};

static le_audio_iso_data_path_t iso_data_path_config = {
    .conn_handle = 0x0040,
    .data_path_id = ISO_DATA_PATH_ID_I2S,
    .direction = 1, // Output (TX to I2S)
    .codec_format = 0x06, // LC3
    .controller_delay = 2 // 2ms
};

void setup() {
    Serial.begin(115200);
    delay(100);

    ESP_LOGI(TAG, "LE Audio Services Integration starting...");
    
    // Initialize GATT Event Handler
    le_audio_gatts_register_handler();
    
    // Initialize LE Audio Services (PACS/ASCS/VCS)
    le_audio_pacs_init();
    le_audio_ascs_init();
    le_audio_vcs_init();
    
    // Initialize LE Audio HCI Layer
    le_audio_hci_init();
    
    // Initialize LE Audio ISO Layer
    le_audio_iso_init();
    
    // Set CIG parameters
    le_audio_hci_set_cig_parameters(&cig_params);
    
    // Create CIS connections
    le_audio_hci_create_cis(cig_params.cis_conn_handle, cig_params.cis_count);
    
    // Setup ISO data path for CIS
    le_audio_hci_setup_iso_data_path(&iso_data_path_config);
    
    // Setup ISO data path in ISO layer
    le_audio_iso_config_t iso_config = {
        .conn_handle = iso_data_path_config.conn_handle,
        .cig_id = cig_params.cig_id,
        .cis_id = 0,
        .max_sdu_size = 120, // LC3 48kHz 10ms
        .phy = ESP_BLE_GAP_PHY_2M,
        .framing = 0,
        .block_len = 1,
        .controller_delay = iso_data_path_config.controller_delay
    };
    
    le_audio_iso_setup_data_path(&iso_config);
    
    ESP_LOGI(TAG, "LE Audio Services + ISO Layer fully initialized and configured");
}

void loop() {
    delay(1000);
    ESP_LOGI(TAG, "LE Audio running...");
}
