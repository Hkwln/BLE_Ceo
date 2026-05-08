#include "le_audio_pacs_ascs_vcs.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_gattc_api.h"
#include "esp_gatts_api.h"

static const char *TAG = "LE_AUDIO_SERVICES";

// GATT Service UUIDs (Bluetooth SIG)
#define PACS_SERVICE_UUID       0x1852
#define ASCS_SERVICE_UUID       0x1853
#define VCS_SERVICE_UUID        0x1845

// GATT Characteristic UUIDs
#define PAC_CHARACTERISTIC_UUID 0x2BCA
#define ASE_RX_CHARACTERISTIC_UUID 0x2BCB
#define ASE_TX_CHARACTERISTIC_UUID 0x2BCC
#define VOLUME_STATE_UUID       0x2B7D
#define VOLUME_CONTROL_POINT_UUID 0x2B7E

// LC3 Configuration (Bluetooth SIG)
#define LC3_CODEC_ID 0x06 // LC3
#define LC3_SAMPLE_RATE_48K 0x03 // 48000 Hz
#define LC3_FRAME_DURATION_10MS 0x01 // 10ms
#define LC3_CHANNELS_STEREO 0x03 // Front Left + Front Right

// PACS: Published Audio Capabilities
typedef struct {
    uint8_t codec_id;
    uint8_t sampling_rate;
    uint8_t frame_duration;
    uint8_t audio_locations;
    uint16_t max_sdu_size;
    uint8_t supported_contexts[2];
} pacs_capabilities_t;

// ASCS: Audio Stream Endpoint
typedef struct {
    uint8_t state;
    uint8_t direction; // 0=Sink, 1=Source
    uint8_t codec_id;
    uint8_t sampling_rate;
    uint8_t frame_duration;
    uint16_t sdu_interval;
    uint8_t framing;
    uint8_t phy;
    uint8_t max_trans_latency;
} ascs_endpoint_t;

// VCS: Volume Control
typedef struct {
    uint8_t volume_state;
    uint8_t volume_range_min;
    uint8_t volume_range_max;
} vcs_state_t;

// Service Handles
static uint16_t pacs_service_handle = 0;
static uint16_t ascs_service_handle = 0;
static uint16_t vcs_service_handle = 0;

// Characteristic Handles
static uint16_t pac_characteristic_handle = 0;
static uint16_t ase_rx_handle = 0;
static uint16_t ase_tx_handle = 0;
static uint16_t volume_state_handle = 0;
static uint16_t volume_control_point_handle = 0;

// State
static pacs_capabilities_t pacs_capabilities = {
    .codec_id = LC3_CODEC_ID,
    .sampling_rate = LC3_SAMPLE_RATE_48K,
    .frame_duration = LC3_FRAME_DURATION_10MS,
    .audio_locations = LC3_CHANNELS_STEREO,
    .max_sdu_size = 120, // LC3 48kHz 10ms
    .supported_contexts = {0x01, 0x00} // Sink ASE
};

static ascs_endpoint_t ascs_rx_endpoint = {
    .state = 0,
    .direction = 0, // Sink (RX)
    .codec_id = LC3_CODEC_ID,
    .sampling_rate = LC3_SAMPLE_RATE_48K,
    .frame_duration = LC3_FRAME_DURATION_10MS,
    .sdu_interval = 10000, // 10ms
    .framing = 0,
    .phy = ESP_BLE_GAP_PHY_2M,
    .max_trans_latency = 40
};

static ascs_endpoint_t ascs_tx_endpoint = {
    .state = 0,
    .direction = 1, // Source (TX)
    .codec_id = LC3_CODEC_ID,
    .sampling_rate = LC3_SAMPLE_RATE_48K,
    .frame_duration = LC3_FRAME_DURATION_10MS,
    .sdu_interval = 10000, // 10ms
    .framing = 0,
    .phy = ESP_BLE_GAP_PHY_2M,
    .max_trans_latency = 40
};

static vcs_state_t vcs_state = {
    .volume_state = 64, // 50% (Bluetooth SIG Standard)
    .volume_range_min = 0,
    .volume_range_max = 127
};

// GATT Event Handler
static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
    switch (event) {
        case ESP_GATTS_REG_EVT:
            ESP_LOGI(TAG, "GATT Service registered: app_id=%d", param->reg.app_id);
            break;
            
        case ESP_GATTS_READ_EVT:
            ESP_LOGI(TAG, "GATT Read: handle=%d", param->read.handle);
            break;
            
        case ESP_GATTS_WRITE_EVT:
            ESP_LOGI(TAG, "GATT Write: handle=%d, len=%d", param->write.handle, param->write.len);
            
            // Handle Volume Control Point
            if (param->write.handle == volume_control_point_handle && param->write.len == 1) {
                uint8_t new_volume = param->write.value[0];
                if (new_volume >= vcs_state.volume_range_min && new_volume <= vcs_state.volume_range_max) {
                    vcs_state.volume_state = new_volume;
                    ESP_LOGI(TAG, "Volume updated: %u", new_volume);
                }
            }
            break;
            
        case ESP_GATTS_MTU_EVT:
            ESP_LOGI(TAG, "MTU updated: %d", param->mtu.mtu);
            break;
            
        default:
            break;
    }
}

// Initialize PACS Service
void le_audio_pacs_init(void) {
    esp_ble_gatts_cb_param_t reg_param = {0};
    reg_param.reg.app_id = PACS_SERVICE_UUID;
    
    ESP_LOGI(TAG, "Initializing PACS Service...");
    
    // Register GATT Service
    esp_ble_gatts_app_register(PACS_SERVICE_UUID);
    
    // TODO: Implement actual PACS characteristic setup
    ESP_LOGI(TAG, "PACS Service initialized (stub)");
}

// Initialize ASCS Service
void le_audio_ascs_init(void) {
    ESP_LOGI(TAG, "Initializing ASCS Service...");
    
    // Register GATT Service
    esp_ble_gatts_app_register(ASCS_SERVICE_UUID);
    
    // TODO: Implement actual ASE RX/TX characteristic setup
    ESP_LOGI(TAG, "ASCS Service initialized (stub)");
}

// Initialize VCS Service
void le_audio_vcs_init(void) {
    ESP_LOGI(TAG, "Initializing VCS Service...");
    
    // Register GATT Service
    esp_ble_gatts_app_register(VCS_SERVICE_UUID);
    
    // TODO: Implement actual Volume State + Control Point characteristic setup
    ESP_LOGI(TAG, "VCS Service initialized (stub)");
}

// Update Volume State Characteristic
void le_audio_vcs_update_volume(uint8_t volume) {
    if (volume >= vcs_state.volume_range_min && volume <= vcs_state.volume_range_max) {
        vcs_state.volume_state = volume;
        ESP_LOGI(TAG, "Volume state updated: %u", volume);
        
        // TODO: Send notification to connected client
    }
}

// Get Current Volume
uint8_t le_audio_vcs_get_volume(void) {
    return vcs_state.volume_state;
}

// Register GATT Event Handler
void le_audio_gatts_register_handler(void) {
    esp_ble_gatts_register_callback(gatts_event_handler);
    ESP_LOGI(TAG, "GATT Event Handler registered");
}
