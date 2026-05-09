#include "le_audio_iso.h"
#include "le_audio_hci.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"

static const char *TAG = "LE_AUDIO_ISO";

// ISO Configuration Storage
static le_audio_iso_config_t iso_config[4] = {0};
static le_audio_iso_buffers_t iso_buffers = {0};

void le_audio_iso_init(void) {
    ESP_LOGI(TAG, "LE Audio ISO Layer initialized");
    
    // Initialize ISO configurations
    for (int i = 0; i < 4; i++) {
        iso_config[i].conn_handle = 0xFFFF;
    }
    
    // Initialize buffers
    iso_buffers.rx_buffer = NULL;
    iso_buffers.rx_buffer_size = 0;
    iso_buffers.tx_buffer = NULL;
    iso_buffers.tx_buffer_size = 0;
    iso_buffers.rx_offset = 0;
    iso_buffers.tx_offset = 0;
}

int le_audio_iso_setup_data_path(const le_audio_iso_config_t *config) {
    if (!config) {
        ESP_LOGE(TAG, "Invalid ISO data path configuration");
        return -1;
    }
    
    ESP_LOGI(TAG, "Setting up ISO Data Path: conn=0x%04X, cig=%u, cis=%u",
             config->conn_handle, config->cig_id, config->cis_id);
    
    // Find free slot
    int slot = -1;
    for (int i = 0; i < 4; i++) {
        if (iso_config[i].conn_handle == 0xFFFF) {
            slot = i;
            break;
        }
    }
    
    if (slot == -1) {
        ESP_LOGE(TAG, "No free ISO data path slots");
        return -2;
    }
    
    // Store configuration
    iso_config[slot] = *config;
    iso_config[slot].conn_handle = config->conn_handle;
    
    ESP_LOGI(TAG, "ISO Data Path setup successful for conn=0x%04X", config->conn_handle);
    return 0;
}

int le_audio_iso_remove_data_path(uint16_t conn_handle, uint8_t data_path_id) {
    for (int i = 0; i < 4; i++) {
        if (iso_config[i].conn_handle == conn_handle) {
            ESP_LOGI(TAG, "Removing ISO Data Path: conn=0x%04X, path_id=%u", conn_handle, data_path_id);
            iso_config[i].conn_handle = 0xFFFF;
            return 0;
        }
    }
    
    ESP_LOGE(TAG, "ISO Data Path not found for conn=0x%04X", conn_handle);
    return -1;
}

void le_audio_iso_rx_handler(uint16_t conn_handle, const uint8_t *data, uint16_t length) {
    if (!data || length == 0) {
        ESP_LOGE(TAG, "Invalid ISO RX data for conn=0x%04X", conn_handle);
        return;
    }
    
    ESP_LOGI(TAG, "Processing ISO RX: len=%u, conn=0x%04X", length, conn_handle);
    
    // TODO: Implement actual audio processing here
    // For now, just log the data
    
    // Forward to HCI layer callback
    le_audio_iso_rx_callback(data, length, conn_handle);
}

int le_audio_iso_tx_handler(uint16_t conn_handle, uint8_t *data, uint16_t length) {
    if (!data || length == 0) {
        ESP_LOGE(TAG, "Invalid ISO TX data for conn=0x%04X", conn_handle);
        return -1;
    }
    
    // Find the ISO config
    int config_index = -1;
    for (int i = 0; i < 4; i++) {
        if (iso_config[i].conn_handle == conn_handle) {
            config_index = i;
            break;
        }
    }
    
    if (config_index == -1) {
        ESP_LOGE(TAG, "No ISO config found for conn=0x%04X", conn_handle);
        return -2;
    }
    
    ESP_LOGI(TAG, "Processing ISO TX: len=%u, conn=0x%04X", length, conn_handle);
    
    // TODO: Implement actual audio transmission here
    // For now, simulate success
    
    // Call TX callback
    le_audio_iso_tx_callback(conn_handle, 0);
    
    return 0;
}

void le_audio_iso_set_rx_buffer(uint8_t *buffer, uint16_t size) {
    iso_buffers.rx_buffer = buffer;
    iso_buffers.rx_buffer_size = size;
    iso_buffers.rx_offset = 0;
    ESP_LOGI(TAG, "RX buffer set: size=%u", size);
}

void le_audio_iso_set_tx_buffer(uint8_t *buffer, uint16_t size) {
    iso_buffers.tx_buffer = buffer;
    iso_buffers.tx_buffer_size = size;
    iso_buffers.tx_offset = 0;
    ESP_LOGI(TAG, "TX buffer set: size=%u", size);
}

int le_audio_iso_is_ready(uint16_t conn_handle) {
    for (int i = 0; i < 4; i++) {
        if (iso_config[i].conn_handle == conn_handle) {
            return 1;
        }
    }
    return 0;
}
