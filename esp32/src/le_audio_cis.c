#include "le_audio_hci.h"
#include "le_audio_iso.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_gatts_api.h"

static const char *TAG = "LE_AUDIO_CIS";

// CIS State Management
typedef struct {
    le_audio_cis_state_t state;
    uint16_t conn_handle;
    uint8_t cig_id;
    uint8_t cis_id;
    uint16_t max_sdu_size;
    uint8_t phy;
} le_audio_cis_context_t;

static le_audio_cis_context_t cis_context[4] = {0}; // Max 4 CIS

void le_audio_hci_init(void) {
    ESP_LOGI(TAG, "LE Audio HCI Layer initialized");
    
    // Initialize CIS contexts
    for (int i = 0; i < 4; i++) {
        cis_context[i].state = LE_AUDIO_CIS_IDLE;
        cis_context[i].conn_handle = 0xFFFF;
    }
}

void le_audio_hci_set_cig_parameters(const le_audio_cig_params_t *params) {
    if (!params) {
        ESP_LOGE(TAG, "Invalid CIG parameters");
        return;
    }
    
    ESP_LOGI(TAG, "Setting CIG Parameters: ID=%u, CIS Count=%u, SDU Interval=%uµs",
             params->cig_id, params->cis_count, params->sdu_interval);
    
    // Store CIG parameters in context
    for (int i = 0; i < params->cis_count; i++) {
        if (i < 4) {
            cis_context[i].cig_id = params->cig_id;
            cis_context[i].state = LE_AUDIO_CIS_CONFIGURED;
            cis_context[i].conn_handle = params->cis_conn_handle[i];
            ESP_LOGI(TAG, "CIS %u configured with conn_handle=0x%04X", i, params->cis_conn_handle[i]);
        }
    }
}

void le_audio_hci_create_cis(uint16_t *cis_conn_handles, uint8_t count) {
    if (!cis_conn_handles || count == 0 || count > 4) {
        ESP_LOGE(TAG, "Invalid CIS connection handles");
        return;
    }
    
    ESP_LOGI(TAG, "Creating %u CIS connections", count);
    
    for (int i = 0; i < count; i++) {
        uint16_t conn_handle = cis_conn_handles[i];
        
        // Find free CIS context
        for (int j = 0; j < 4; j++) {
            if (cis_context[j].conn_handle == 0xFFFF) {
                cis_context[j].conn_handle = conn_handle;
                cis_context[j].state = LE_AUDIO_CIS_ESTABLISHED;
                cis_context[j].cis_id = j;
                ESP_LOGI(TAG, "CIS %u established with conn_handle=0x%04X", j, conn_handle);
                break;
            }
        }
    }
}

void le_audio_hci_remove_cig(uint8_t cig_id) {
    ESP_LOGI(TAG, "Removing CIG ID=%u", cig_id);
    
    // Reset all CIS with this CIG ID
    for (int i = 0; i < 4; i++) {
        if (cis_context[i].cig_id == cig_id) {
            cis_context[i].state = LE_AUDIO_CIS_IDLE;
            cis_context[i].conn_handle = 0xFFFF;
            cis_context[i].cig_id = 0;
            ESP_LOGI(TAG, "CIG %u removed, CIS %u reset", cig_id, i);
        }
    }
}

void le_audio_hci_remove_cis(uint16_t conn_handle) {
    for (int i = 0; i < 4; i++) {
        if (cis_context[i].conn_handle == conn_handle) {
            ESP_LOGI(TAG, "Removing CIS conn_handle=0x%04X", conn_handle);
            cis_context[i].state = LE_AUDIO_CIS_DISCONNECTED;
            cis_context[i].conn_handle = 0xFFFF;
            break;
        }
    }
}

void le_audio_hci_setup_iso_data_path(const le_audio_iso_data_path_t *config) {
    if (!config) {
        ESP_LOGE(TAG, "Invalid ISO data path config");
        return;
    }
    
    ESP_LOGI(TAG, "Setting up ISO Data Path: conn=0x%04X, path_id=%u, dir=%u",
             config->conn_handle, config->data_path_id, config->direction);
    
    // Setup ISO data path based on direction
    if (config->direction == 0) { // Input (RX)
        le_audio_iso_set_rx_buffer(NULL, 0); // Placeholder for actual buffer
        ESP_LOGI(TAG, "ISO RX data path configured");
    } else { // Output (TX)
        le_audio_iso_set_tx_buffer(NULL, 0); // Placeholder for actual buffer
        ESP_LOGI(TAG, "ISO TX data path configured");
    }
}

void le_audio_hci_remove_iso_data_path(uint16_t conn_handle, uint8_t data_path_id) {
    ESP_LOGI(TAG, "Removing ISO Data Path: conn=0x%04X, path_id=%u", conn_handle, data_path_id);
    
    // Cleanup ISO data path
    if (data_path_id == ISO_DATA_PATH_ID_HCI) {
        ESP_LOGI(TAG, "HCI ISO data path removed");
    } else if (data_path_id == ISO_DATA_PATH_ID_I2S) {
        ESP_LOGI(TAG, "I2S ISO data path removed");
    }
}

le_audio_cis_state_t le_audio_hci_get_cis_state(uint16_t conn_handle) {
    for (int i = 0; i < 4; i++) {
        if (cis_context[i].conn_handle == conn_handle) {
            return cis_context[i].state;
        }
    }
    return LE_AUDIO_CIS_IDLE;
}

// ISO RX Callback (called from BLE ISO layer)
void le_audio_iso_rx_callback(const uint8_t *data, uint16_t length, uint16_t conn_handle) {
    if (!data || length == 0) {
        ESP_LOGE(TAG, "Invalid ISO RX data");
        return;
    }
    
    ESP_LOGI(TAG, "ISO RX: len=%u, conn=0x%04X", length, conn_handle);
    
    // Forward to ISO handler
    le_audio_iso_rx_handler(conn_handle, data, length);
}

// ISO TX Callback (called when TX completes)
void le_audio_iso_tx_callback(uint16_t conn_handle, int status) {
    if (status == 0) {
        ESP_LOGI(TAG, "ISO TX successful: conn=0x%04X", conn_handle);
    } else {
        ESP_LOGE(TAG, "ISO TX failed: conn=0x%04X, status=%d", conn_handle, status);
    }
}
