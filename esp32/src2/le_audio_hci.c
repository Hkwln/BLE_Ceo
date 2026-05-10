#include "le_audio_hci.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "HCI";

static le_audio_hci_event_cb_t user_event_cb = NULL;
static SemaphoreHandle_t hci_mutex = NULL;

typedef struct {
  le_audio_cis_state_t state;
  uint16_t conn_handle;
} cis_ctx_t;

static cis_ctx_t cis_ctx[4];

void le_audio_hci_init(void) {
  hci_mutex = xSemaphoreCreateMutex();
  configASSERT(hci_mutex);
  for (int i = 0; i < 4; i++) {
    cis_ctx[i].state = LE_AUDIO_CIS_IDLE;
    cis_ctx[i].conn_handle = 0xFFFF;
  }
  ESP_LOGI(TAG, "HCI init (BLE 5.0 GATT Audio mode)");
  ESP_LOGW(TAG, "ESP32-S3 = BLE 5.0: no ISO/CIS hardware");
  ESP_LOGW(TAG, "Real LE Audio ISO needs ESP32-C6 / nRF5340");
}

void le_audio_hci_register_event_cb(le_audio_hci_event_cb_t cb) {
  user_event_cb = cb;
}

void le_audio_hci_notify_stream_ready(uint16_t conn_handle) {
  bool assigned = false;

  xSemaphoreTake(hci_mutex, portMAX_DELAY);
  for (int i = 0; i < 4; i++) {
    if (cis_ctx[i].conn_handle == 0xFFFF) {
      cis_ctx[i].conn_handle = conn_handle;
      cis_ctx[i].state = LE_AUDIO_CIS_STREAMING;
      assigned = true;
      break;
    }
  }
  xSemaphoreGive(hci_mutex);

  if (!assigned) {
    ESP_LOGW(TAG, "No free CIS slot for conn=0x%04X", conn_handle);
    return;
  }

  if (user_event_cb) {
    le_audio_hci_event_t evt = {
        .type = LE_AUDIO_HCI_EVT_STREAM_READY,
        .cis_handle = conn_handle,
        .status = 0x00,
    };
    user_event_cb(&evt);
  }
  ESP_LOGI(TAG, "Stream ready: conn=0x%04X", conn_handle);
}

void le_audio_hci_release_conn(uint16_t conn_handle) {
  xSemaphoreTake(hci_mutex, portMAX_DELAY);
  for (int i = 0; i < 4; i++) {
    if (cis_ctx[i].conn_handle == conn_handle) {
      cis_ctx[i].state = LE_AUDIO_CIS_IDLE;
      cis_ctx[i].conn_handle = 0xFFFF;
      break;
    }
  }
  xSemaphoreGive(hci_mutex);
}

le_audio_cis_state_t le_audio_hci_get_cis_state(uint16_t conn_handle) {
  for (int i = 0; i < 4; i++) {
    if (cis_ctx[i].conn_handle == conn_handle)
      return cis_ctx[i].state;
  }
  return LE_AUDIO_CIS_IDLE;
}
