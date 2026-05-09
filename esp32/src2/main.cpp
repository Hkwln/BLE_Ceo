#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_log.h"
#include <Arduino.h>

#include "le_audio_audio_pipeline.h"
#include "le_audio_hci.h"
#include "le_audio_iso.h"
#include "le_audio_pacs_ascs_vcs.h"

static const char *TAG = "MAIN";

// ── Advertising ──────────────────────────────────────────
// Custom 128-bit UUID (same as GATT table)
static uint8_t adv_uuid128[16] = {0xBC, 0x9A, 0x78, 0x56, 0x34, 0x12,
                                  0x34, 0x12, 0x34, 0x12, 0x12, 0x34,
                                  0x56, 0x78, 0x9A, 0xBC};

static esp_ble_adv_data_t adv_data = {
    .set_scan_rsp = false,
    .include_name = true,
    .include_txpower = true,
    .min_interval = 0x0006,
    .max_interval = 0x0010,
    .appearance = 0x0941, // Headphones
    .manufacturer_len = 0,
    .p_manufacturer_data = nullptr,
    .service_data_len = 0,
    .p_service_data = nullptr,
    .service_uuid_len = 16,
    .p_service_uuid = adv_uuid128,
    .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT),
};

static esp_ble_adv_params_t adv_params = {
    .adv_int_min = 0x0020, // 20 ms
    .adv_int_max = 0x0040, // 40 ms
    .adv_type = ADV_TYPE_IND,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .channel_map = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

// ── State ────────────────────────────────────────────────
static volatile uint16_t active_conn = 0xFFFF;
static volatile bool pipeline_started = false;
static volatile bool adv_data_set = false;

// ── Restart advertising after disconnect ─────────────────
static void start_advertising(void) {
  esp_ble_gap_start_advertising(&adv_params);
}

// ── Connection callback (from GATT layer) ────────────────
static void on_connection(uint16_t conn_handle, bool is_connected) {
  if (is_connected) {
    active_conn = conn_handle;
    ESP_LOGI(TAG, "Connected: conn=0x%04X", conn_handle);
  } else {
    // Client disconnected – clean up and re-advertise
    ESP_LOGI(TAG, "Disconnected – restarting pipeline & advertising");

    if (pipeline_started) {
      le_audio_pipeline_stop();
      le_audio_iso_remove_data_path(active_conn, 0);
      pipeline_started = false;
    }
    active_conn = 0xFFFF;
    start_advertising();
  }
}

// ── HCI event callback ───────────────────────────────────
static void on_hci_event(const le_audio_hci_event_t *evt) {
  if (evt->type == LE_AUDIO_HCI_EVT_STREAM_READY) {
    ESP_LOGI(TAG, "Stream ready conn=0x%04X", evt->cis_handle);

    le_audio_iso_config_t cfg = {
        .conn_handle = evt->cis_handle,
        .cig_id = 0,
        .cis_id = 0,
        .max_sdu_size = 120,
    };
    le_audio_iso_setup_data_path(&cfg);
    le_audio_iso_start_tasks();

    if (!pipeline_started) {
      if (le_audio_pipeline_start(evt->cis_handle))
        pipeline_started = true;
      else
        ESP_LOGE(TAG, "Audio pipeline failed to start");
    }
    if (pipeline_started)
      ESP_LOGI(TAG, "✓ Audio pipeline active");
  }
}

// ── Audio RX (LC3 frame from sender over GATT Write) ─────
static void on_audio_rx(uint16_t conn, const uint8_t *data, uint16_t len) {
  // First frame triggers stream-ready notification
  if (!pipeline_started) {
    le_audio_hci_notify_stream_ready(conn);
    // on_hci_event will start the pipeline
  }
  le_audio_pipeline_rx(conn, data, len);
}

// ── GAP callback ─────────────────────────────────────────
static void gap_cb(esp_gap_ble_cb_event_t event,
                   esp_ble_gap_cb_param_t *param) {
  switch (event) {
  case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
    adv_data_set = true;
    start_advertising();
    break;

  case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
    if (param->adv_start_cmpl.status == ESP_BT_STATUS_SUCCESS)
      ESP_LOGI(TAG, "Advertising → 'XIAO-LE-Audio-HP'");
    else
      ESP_LOGE(TAG, "Advertising start failed %u",
               param->adv_start_cmpl.status);
    break;

  case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
    ESP_LOGI(TAG, "Conn params: interval=%u latency=%u timeout=%u",
             param->update_conn_params.conn_int,
             param->update_conn_params.latency,
             param->update_conn_params.timeout);
    break;

  default:
    break;
  }
}

// ── Setup ────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  vTaskDelay(pdMS_TO_TICKS(300));

  ESP_LOGI(TAG, "══════════════════════════════════════");
  ESP_LOGI(TAG, " XIAO ESP32-S3  BLE Audio Headphone  ");
  ESP_LOGI(TAG, " BLE 5.0 + LC3 (GATT transport)      ");
  ESP_LOGI(TAG, "══════════════════════════════════════");
  ESP_LOGW(TAG, "NOTE: ESP32-S3 = BLE 5.0, no ISO HW");
  ESP_LOGW(TAG, "GATT custom audio used as transport");
  ESP_LOGI(TAG, "I2S: BCLK=GPIO2  LRCLK=GPIO3  DOUT=GPIO4");

  // ── BLE stack ────────────────────────────────────────
  esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
  ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));
  ESP_ERROR_CHECK(esp_bluedroid_init());
  ESP_ERROR_CHECK(esp_bluedroid_enable());

  // ── Callbacks ────────────────────────────────────────
  ESP_ERROR_CHECK(esp_ble_gap_register_callback(gap_cb));
  le_audio_gatts_register_handler();

  // ── LE Audio layers ──────────────────────────────────
  le_audio_hci_init();
  le_audio_hci_register_event_cb(on_hci_event);

  le_audio_iso_init();

  le_audio_pipeline_init(); // LC3 decoder + I2S

  // ── GATT user callbacks ──────────────────────────────
  le_audio_register_audio_rx_cb(on_audio_rx);
  le_audio_register_conn_cb(on_connection);

  // ── Start GATT services ──────────────────────────────
  le_audio_pacs_init(); // registers PACS/ASCS/VCS/AUDIO

  // ── Advertising ──────────────────────────────────────
  ESP_ERROR_CHECK(esp_ble_gap_set_device_name("XIAO-LE-Audio-HP"));
  ESP_ERROR_CHECK(esp_ble_gap_config_adv_data(&adv_data));
  // Advertising starts automatically in gap_cb after data is set
}

// ── Loop ─────────────────────────────────────────────────
void loop() {
  static uint32_t last_log = 0;
  uint32_t now = millis();

  if (now - last_log > 5000) {
    last_log = now;
    ESP_LOGI(TAG, "Heap free=%lu min=%lu | conn=0x%04X stream=%s",
             (unsigned long)esp_get_free_heap_size(),
             (unsigned long)esp_get_minimum_free_heap_size(), active_conn,
             pipeline_started ? "ON" : "OFF");

    if (active_conn != 0xFFFF)
      le_audio_iso_print_stats(active_conn);
  }

  delay(100);
}
