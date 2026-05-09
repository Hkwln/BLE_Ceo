#include "le_audio_audio_pipeline.h"
#include "le_audio_lc3.h"
#include "le_audio_i2s.h"
#include "le_audio_pacs_ascs_vcs.h"  // for volume getter
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "PIPELINE";

typedef struct {
    uint16_t conn_handle;
    uint16_t length;
    uint8_t  lc3_data[LC3_BYTES_PER_FRAME];
} pipeline_frame_t;

static QueueHandle_t pipeline_queue = NULL;
static le_audio_lc3_decoder_t *lc3_decoder = NULL;
static bool pipeline_running = false;
static int16_t pcm_buffer[LC3_PCM_SAMPLES];

static void pipeline_task(void *arg) {
    pipeline_frame_t frame;
    uint32_t frame_count = 0;
    while (pipeline_running) {
        if (xQueueReceive(pipeline_queue, &frame, pdMS_TO_TICKS(15)) != pdTRUE) {
            memset(pcm_buffer, 0, sizeof(pcm_buffer));
            size_t w;
            le_audio_i2s_write(pcm_buffer, LC3_PCM_SAMPLES, &w);
            continue;
        }
        int res = le_audio_lc3_decode(lc3_decoder, frame.lc3_data, frame.length, pcm_buffer, LC3_PCM_SAMPLES);
        uint8_t vol = le_audio_vcs_get_volume();
        if (vol < 255) {
            int32_t scale = (int32_t)vol;
            for (int i = 0; i < LC3_PCM_SAMPLES; i++)
                pcm_buffer[i] = (int16_t)(((int32_t)pcm_buffer[i] * scale) >> 8);
        }
        size_t written = 0;
        le_audio_i2s_write(pcm_buffer, LC3_PCM_SAMPLES, &written);
        frame_count++;
        if (frame_count % 100 == 0)
            ESP_LOGI(TAG, "Pipeline frames=%lu vol=%u%%", (unsigned long)frame_count, (unsigned)(vol*100/255));
    }
    vTaskDelete(NULL);
}

void le_audio_pipeline_init(void) {
    pipeline_queue = xQueueCreate(8, sizeof(pipeline_frame_t));
    configASSERT(pipeline_queue);
    lc3_decoder = le_audio_lc3_decoder_create();
    if (!lc3_decoder) { ESP_LOGE(TAG, "LC3 decoder create failed"); return; }
    le_audio_i2s_init();
    ESP_LOGI(TAG, "Audio Pipeline initialized");
}

void le_audio_pipeline_start(uint16_t conn_handle) {
    if (pipeline_running) return;
    pipeline_running = true;
    xTaskCreatePinnedToCore(pipeline_task, "pipeline", 8192, NULL, configMAX_PRIORITIES-1, NULL, 1);
    ESP_LOGI(TAG, "Pipeline started for conn 0x%04X", conn_handle);
}

void le_audio_pipeline_stop(void) { pipeline_running = false; }

void le_audio_pipeline_rx(uint16_t conn_handle, const uint8_t *lc3_data, uint16_t length) {
    if (!pipeline_running) return;
    if (length > LC3_BYTES_PER_FRAME) return;
    pipeline_frame_t frame;
    frame.conn_handle = conn_handle;
    frame.length = length;
    memcpy(frame.lc3_data, lc3_data, length);
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (xQueueSendFromISR(pipeline_queue, &frame, &xHigherPriorityTaskWoken) != pdTRUE) {
        pipeline_frame_t discard;
        xQueueReceiveFromISR(pipeline_queue, &discard, &xHigherPriorityTaskWoken);
        xQueueSendFromISR(pipeline_queue, &frame, &xHigherPriorityTaskWoken);
    }
    if (xHigherPriorityTaskWoken) portYIELD_FROM_ISR();
}
