#include <Arduino.h>

#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_log.h"

#if __has_include("hci/ble_hci_iso.h")
#include "hci/ble_hci_iso.h"
#define HAS_BLE_HCI_ISO 1
#else
#define HAS_BLE_HCI_ISO 0
#endif

static const char *TAG = "LE_ISO_POC";

#if HAS_BLE_HCI_ISO
extern "C" void ble_hci_register_rx_iso_data_cb(void *cb);
#endif

static void iso_rx_cb(uint8_t *data, uint16_t len) {
    ESP_LOGI(TAG, "ISO RX: len=%u", static_cast<unsigned>(len));
    // TODO: Push into jitter buffer + LC3 decode
    (void)data;
}

static void iso_dummy_tx(uint16_t conn_handle) {
#if HAS_BLE_HCI_ISO
    uint8_t sdu[16] = {0};
    uint32_t ts = 0;
    uint16_t seq = 0;
    int rc = esp_ble_hci_iso_tx(conn_handle, sdu, sizeof(sdu), ts, seq);
    ESP_LOGI(TAG, "ISO TX dummy rc=%d (handle=0x%04X)", rc, conn_handle);
#else
    ESP_LOGW(TAG, "ISO TX unavailable: ble_hci_iso.h not present");
    (void)conn_handle;
#endif
}

static void bt_init_ble_only() {
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));
    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());
}

void setup() {
    Serial.begin(115200);
    delay(100);

    ESP_LOGI(TAG, "LE Audio ISO POC boot");
    bt_init_ble_only();

#if HAS_BLE_HCI_ISO
    ble_hci_register_rx_iso_data_cb((void *)iso_rx_cb);
    ESP_LOGI(TAG, "ISO RX callback registered");
#else
    ESP_LOGW(TAG, "ISO RX callback unavailable: missing ble_hci_iso.h");
#endif

    // NOTE: This is a POC. A CIS must be established before ISO TX will work.
    // TODO: add CIG/CIS setup via esp_ble_iso_* APIs and data path setup.
    iso_dummy_tx(0x0000);
}

void loop() {
    delay(1000);
}
