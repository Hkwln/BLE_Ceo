#include "le_audio_audio_pipeline.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "le_audio_i2s.h"
#include "le_audio_lc3.h"
#include "le_audio_pacs_ascs_vcs.h" // le_audio_vcs_get_volume
#include <string.h>

static const char *TAG = "PIPELINE";

// ── Frame buffer ─────────────────────────────────────────
typedef struct {
  uint16_t conn_handle;
  uint16_t length;
  uint8_t lc3_data[LC3_BYTES_PER_FRAME];
} pframe_t;

#define PIPELINE_QUEUE_DEPTH 8 // 80 ms jitter buffer

static QueueHandle_t pipeline_q = NULL;
static le_audio_lc3_decoder_t *decoder = NULL;
static TaskHandle_t task_handle = NULL;
static volatile bool running = false;

// PCM buffer (stack-allocated in task, not global – saves heap)

// ── Pipeline task ────────────────────────────────────────
static void pipeline_task(void *arg) {
  pframe_t frame;
  int16_t pcm[LC3_PCM_SAMPLES];
  uint32_t frames = 0, errors = 0, underruns = 0;

  ESP_LOGI(TAG, "Task running @ core %d", xPortGetCoreID());

  while (running) {
    // Wait up to 15 ms (1.5× frame time) for a frame
    if (xQueueReceive(pipeline_q, &frame, pdMS_TO_TICKS(15)) != pdTRUE) {
      // Buffer underrun – output silence to keep I2S clock alive
      memset(pcm, 0, sizeof(pcm));
      size_t w;
      le_audio_i2s_write(pcm, LC3_PCM_SAMPLES, &w);
      underruns++;
      continue;
    }

    // Decode LC3 → PCM
    int res = le_audio_lc3_decode(decoder, frame.lc3_data, frame.length, pcm,
                                  LC3_PCM_SAMPLES);
    if (res < 0)
      errors++;

    // Volume scaling (0-255 linear)
    uint8_t vol = le_audio_vcs_get_volume();
    if (vol < 255) {
      int32_t s = (int32_t)vol;
      for (int i = 0; i < LC3_PCM_SAMPLES; i++)
        pcm[i] = (int16_t)(((int32_t)pcm[i] * s) >> 8);
    }

    // Write to I2S DAC
    size_t written = 0;
    le_audio_i2s_write(pcm, LC3_PCM_SAMPLES, &written);

    frames++;
    // Log every 5 s (500 frames @ 10 ms)
    if (frames % 500 == 0)
      ESP_LOGI(TAG, "frames=%lu err=%lu underrun=%lu vol=%u%%",
               (unsigned long)frames, (unsigned long)errors,
               (unsigned long)underruns, (unsigned)(vol * 100u / 255u));
  }

  ESP_LOGI(TAG, "Task stopped");
  task_handle = NULL;
  vTaskDelete(NULL);
}

// ── Public API ───────────────────────────────────────────
void le_audio_pipeline_init(void) {
  if (pipeline_q)
    return; // idempotent

  pipeline_q = xQueueCreate(PIPELINE_QUEUE_DEPTH, sizeof(pframe_t));
  configASSERT(pipeline_q);

  decoder = le_audio_lc3_decoder_create();
  if (!decoder) {
    ESP_LOGE(TAG, "LC3 decoder create FAILED");
    vQueueDelete(pipeline_q);
    pipeline_q = NULL;
    return;
  }

  if (le_audio_i2s_init() != ESP_OK) {
    ESP_LOGE(TAG, "I2S init FAILED");
    le_audio_lc3_decoder_destroy(decoder);
    decoder = NULL;
    vQueueDelete(pipeline_q);
    pipeline_q = NULL;
    return;
  }

  ESP_LOGI(TAG, "Pipeline init OK");
}

bool le_audio_pipeline_start(uint16_t conn_handle) {
  if (running)
    return true;

  running = true;
  BaseType_t ok = xTaskCreatePinnedToCore(
      pipeline_task, "audio", 8192, NULL, configMAX_PRIORITIES - 1,
      &task_handle, 1); // Core 1 (BLE uses Core 0)
  if (ok != pdPASS) {
    task_handle = NULL;
    running = false;
    ESP_LOGE(TAG, "Failed to start audio pipeline task");
    return false;
  }

  ESP_LOGI(TAG, "Pipeline started for conn=0x%04X", conn_handle);
  return true;
}

void le_audio_pipeline_stop(void) {
  if (!running)
    return;
  running = false;
  // Unblock the task if it's waiting on the queue
  pframe_t dummy = {0};
  xQueueSend(pipeline_q, &dummy, 0);
  // Wait for task to exit
  vTaskDelay(pdMS_TO_TICKS(50));
  // Flush queue
  xQueueReset(pipeline_q);
  ESP_LOGI(TAG, "Pipeline stopped");
}

// ── RX entry point (called from GATT write callback) ─────
// NOTE: GATT callbacks run in a normal FreeRTOS task context,
//       NOT in an ISR – use xQueueSend, never xQueueSendFromISR
void le_audio_pipeline_rx(uint16_t conn_handle, const uint8_t *lc3_data,
                          uint16_t length) {
  if (!running || !lc3_data)
    return;
  if (length > LC3_BYTES_PER_FRAME) {
    ESP_LOGW(TAG, "Frame too large %u > %u", length, LC3_BYTES_PER_FRAME);
    return;
  }

  pframe_t f;
  f.conn_handle = conn_handle;
  f.length = length;
  memcpy(f.lc3_data, lc3_data, length);

  // Non-blocking send; if full, drop the oldest frame
  if (xQueueSend(pipeline_q, &f, 0) != pdTRUE) {
    pframe_t discard;
    xQueueReceive(pipeline_q, &discard, 0); // drop oldest
    xQueueSend(pipeline_q, &f, 0);          // insert newest
  }
}
