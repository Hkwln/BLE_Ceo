#include "le_audio_iso.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "ISO";

typedef struct {
  uint16_t conn_handle;
  uint16_t length;
  uint16_t packet_seq;
  uint8_t data[ISO_MAX_SDU_SIZE];
} iso_frame_t;

typedef struct {
  uint16_t conn_handle;
  uint8_t cig_id;
  uint8_t cis_id;
  bool active;
  uint16_t tx_seq_num;
  uint32_t rx_count;
  uint32_t tx_count;
  uint32_t rx_lost;
} iso_conn_ctx_t;

static iso_conn_ctx_t iso_conn[4];
static SemaphoreHandle_t iso_mutex = NULL;
static QueueHandle_t tx_queue = NULL;
static QueueHandle_t rx_queue = NULL;
static TaskHandle_t tx_task_handle = NULL;

static le_audio_iso_rx_cb_t rx_user_cb = NULL;
static le_audio_iso_tx_done_cb_t tx_done_cb = NULL;

// ── Helper ──────────────────────────────────────────────
static iso_conn_ctx_t *find_conn(uint16_t handle) {
  for (int i = 0; i < 4; i++)
    if (iso_conn[i].conn_handle == handle && iso_conn[i].active)
      return &iso_conn[i];
  return NULL;
}

// ── Init ────────────────────────────────────────────────
void le_audio_iso_init(void) {
  iso_mutex = xSemaphoreCreateMutex();
  configASSERT(iso_mutex);
  tx_queue = xQueueCreate(ISO_QUEUE_DEPTH, sizeof(iso_frame_t));
  rx_queue = xQueueCreate(ISO_QUEUE_DEPTH, sizeof(iso_frame_t));
  configASSERT(tx_queue && rx_queue);
  for (int i = 0; i < 4; i++) {
    iso_conn[i].conn_handle = 0xFFFF;
    iso_conn[i].active = false;
    iso_conn[i].tx_seq_num = 0;
    iso_conn[i].rx_count = 0;
    iso_conn[i].tx_count = 0;
    iso_conn[i].rx_lost = 0;
  }
  ESP_LOGI(TAG, "ISO init (queue=%u frame=%u B)", ISO_QUEUE_DEPTH,
           ISO_MAX_SDU_SIZE);
}

// ── Data path ───────────────────────────────────────────
int le_audio_iso_setup_data_path(const le_audio_iso_config_t *cfg) {
  if (!cfg)
    return -1;
  xSemaphoreTake(iso_mutex, portMAX_DELAY);
  int slot = -1;
  for (int i = 0; i < 4; i++)
    if (!iso_conn[i].active) {
      slot = i;
      break;
    }
  if (slot < 0) {
    xSemaphoreGive(iso_mutex);
    ESP_LOGE(TAG, "No free slot");
    return -2;
  }
  iso_conn[slot].conn_handle = cfg->conn_handle;
  iso_conn[slot].cig_id = cfg->cig_id;
  iso_conn[slot].cis_id = cfg->cis_id;
  iso_conn[slot].active = true;
  iso_conn[slot].tx_seq_num = 0;
  xSemaphoreGive(iso_mutex);
  ESP_LOGI(TAG, "Data path: conn=0x%04X", cfg->conn_handle);
  return 0;
}

int le_audio_iso_remove_data_path(uint16_t conn_handle, uint8_t dir) {
  (void)dir;
  xSemaphoreTake(iso_mutex, portMAX_DELAY);
  for (int i = 0; i < 4; i++) {
    if (iso_conn[i].conn_handle == conn_handle) {
      ESP_LOGI(TAG, "Path removed 0x%04X tx=%lu rx=%lu lost=%lu", conn_handle,
               (unsigned long)iso_conn[i].tx_count,
               (unsigned long)iso_conn[i].rx_count,
               (unsigned long)iso_conn[i].rx_lost);
      iso_conn[i].active = false;
      iso_conn[i].conn_handle = 0xFFFF;
      xSemaphoreGive(iso_mutex);
      return 0;
    }
  }
  xSemaphoreGive(iso_mutex);
  return -1;
}

// ── TX ──────────────────────────────────────────────────
int le_audio_iso_send(uint16_t conn_handle, const uint8_t *data,
                      uint16_t length) {
  if (!data || length == 0 || length > ISO_MAX_SDU_SIZE)
    return -1;
  iso_conn_ctx_t *ctx = find_conn(conn_handle);
  if (!ctx)
    return -2;

  iso_frame_t frame;
  frame.conn_handle = conn_handle;
  frame.length = length;
  frame.packet_seq = ctx->tx_seq_num++;
  memcpy(frame.data, data, length);

  if (xQueueSend(tx_queue, &frame, pdMS_TO_TICKS(1)) != pdTRUE) {
    ESP_LOGW(TAG, "TX queue full, drop seq=%u", frame.packet_seq);
    return -3;
  }
  ctx->tx_count++;
  return 0;
}

// ── RX ──────────────────────────────────────────────────
void le_audio_iso_rx_handler(uint16_t conn_handle, const uint8_t *data,
                             uint16_t length) {
  if (!data || length == 0)
    return;
  iso_conn_ctx_t *ctx = find_conn(conn_handle);
  if (!ctx) {
    ESP_LOGW(TAG, "RX unknown 0x%04X", conn_handle);
    return;
  }

  iso_frame_t frame;
  frame.conn_handle = conn_handle;
  frame.length = (length > ISO_MAX_SDU_SIZE) ? ISO_MAX_SDU_SIZE : length;
  frame.packet_seq = (uint16_t)ctx->rx_count++;
  memcpy(frame.data, data, frame.length);

  if (rx_user_cb) {
    rx_user_cb(conn_handle, frame.data, frame.length);
    return;
  }
  if (xQueueSend(rx_queue, &frame, 0) != pdTRUE)
    ctx->rx_lost++;
}

bool le_audio_iso_receive(uint16_t conn_handle, uint8_t *out_buf,
                          uint16_t *out_len, uint32_t timeout_ticks) {
  if (!out_buf || !out_len)
    return false;
  iso_frame_t frame;
  if (xQueueReceive(rx_queue, &frame, timeout_ticks) != pdTRUE)
    return false;
  if (frame.conn_handle != conn_handle)
    return false;
  memcpy(out_buf, frame.data, frame.length);
  *out_len = frame.length;
  return true;
}

// ── Callbacks ───────────────────────────────────────────
void le_audio_iso_register_rx_cb(le_audio_iso_rx_cb_t cb) { rx_user_cb = cb; }
void le_audio_iso_register_tx_done_cb(le_audio_iso_tx_done_cb_t cb) {
  tx_done_cb = cb;
}

// ── TX task ─────────────────────────────────────────────
static void iso_tx_task(void *arg) {
  iso_frame_t frame;
  ESP_LOGI(TAG, "TX task @ core %d", xPortGetCoreID());
  while (1) {
    if (xQueueReceive(tx_queue, &frame, portMAX_DELAY) == pdTRUE) {
      // Forward to GATT layer
      extern void le_audio_gatt_send_audio_frame(uint16_t, const uint8_t *,
                                                 uint16_t);
      le_audio_gatt_send_audio_frame(frame.conn_handle, frame.data,
                                     frame.length);
      if (tx_done_cb)
        tx_done_cb(frame.conn_handle, 0);
    }
  }
}

void le_audio_iso_start_tasks(void) {
  if (tx_task_handle)
    return;

  BaseType_t ok =
      xTaskCreatePinnedToCore(iso_tx_task, "iso_tx", 4096, NULL,
                              configMAX_PRIORITIES - 2, &tx_task_handle, 1);
  if (ok != pdPASS) {
    tx_task_handle = NULL;
    ESP_LOGE(TAG, "Failed to start ISO TX task");
  }
}

// ── Stats ───────────────────────────────────────────────
int le_audio_iso_is_ready(uint16_t h) { return find_conn(h) ? 1 : 0; }

void le_audio_iso_print_stats(uint16_t conn_handle) {
  iso_conn_ctx_t *ctx = find_conn(conn_handle);
  if (!ctx) {
    ESP_LOGW(TAG, "Stats: 0x%04X not found", conn_handle);
    return;
  }
  ESP_LOGI(TAG, "Stats [0x%04X] TX=%lu RX=%lu Lost=%lu", conn_handle,
           (unsigned long)ctx->tx_count, (unsigned long)ctx->rx_count,
           (unsigned long)ctx->rx_lost);
}
