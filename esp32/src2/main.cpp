#include <Arduino.h>
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_log.h"

#include "le_audio_hci.h"
#include "le_audio_iso.h"
#include "le_audio_pacs_ascs_vcs.h"
#include "le_audio_audio_pipeline.h"

static const char *TAG = "MAIN";

// Custom 128-bit service UUID (same as in GATT table)
static uint8_t custom_svc_uuid[16] = {
    0xBC,0x9A,0x78,0x56,0x34,0x12, 0x34,0x12,0x34,0x12, 0x12,0x34,0x56,0x78,0x9A,0xBC
};

static esp_ble_adv_data_t adv_data = {
    .set_scan_rsp        = false,
    .include_name        = true,
    .include_txpower     = true,
    .min_interval        = 0x0006,
    .max_interval        = 0x0010,
    .appearance          = 0x0941,   // Headphones
    .service_uuid_len    = 16,       // 128-bit UUID
    .p_service_uuid      = custom_svc_uuid,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

static esp_ble_adv_params_t adv_params = {
    .adv_int_min  = 0x0020,
    .adv_int_max  = 0x0040,
    .adv_type     = ADV_TYPE_IND,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .channel_map  = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

static uint16_t current_conn_handle = 0xFFFF;
static bool audio_pipeline_active = false;

static void on_hci_event(const le_audio_hci_event_t *evt) {
    if (evt->type == LE_AUDIO_HCI_EVT_STREAM_READY) {
        le_audio_iso_config_t cfg = {evt->cis_handle, 0, 0, 120};
        le_audio_iso_setup_data_path(&cfg);
        le_audio_iso_start_tasks();
        if (!audio_pipeline_active) {
            le_audio_pipeline_start(evt->cis_handle);
            audio_pipeline_active = true;
        }
    }
}

static void on_audio_rx(uint16_t conn, const uint8_t *data, uint16_t len) {
    if (!audio_pipeline_active) le_audio_hci_notify_stream_ready(conn);
    le_audio_pipeline_rx(conn, data, len);
}

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    if (event == ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT)
        esp_ble_gap_start_advertising(&adv_params);
    else if (event == ESP_GAP_BLE_ADV_START_COMPLETE_EVT && param->adv_start_cmpl.status == ESP_BT_STATUS_SUCCESS)
        ESP_LOGI(TAG, "Advertising started as 'XIAO-LE-Audio-HP'");
}

void setup() {
    Serial.begin(115200);
    delay(300);
    ESP_LOGI(TAG, "XIAO ESP32-S3 BLE Audio Headset");

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_bt_controller_init(&bt_cfg);
    esp_bt_controller_enable(ESP_BT_MODE_BLE);
    esp_bluedroid_init();
    esp_bluedroid_enable();

    esp_ble_gap_register_callback(gap_event_handler);
    le_audio_gatts_register_handler();

    le_audio_hci_init();
    le_audio_hci_register_event_cb(on_hci_event);
    le_audio_iso_init();
    le_audio_pipeline_init();
    le_audio_register_audio_rx_cb(on_audio_rx);
    le_audio_pacs_init();

    esp_ble_gap_set_device_name("XIAO-LE-Audio-HP");
    esp_ble_gap_config_adv_data(&adv_data);
}

void loop() {
    delay(1000);
}
