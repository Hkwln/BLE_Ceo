#include "le_audio_iso.h"
#include "le_audio_hci.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

static const char *TAG = "LE_AUDIO_ISO";

// ─────────────────────────────────────────────
// ISO Frame Struktur
// ─────────────────────────────────────────────
#define ISO_MAX_SDU_SIZE 120 // LC3 @ 48kHz / 10ms
#define ISO_QUEUE_DEPTH 8 // 8 Frames Buffer = 80ms

typedef struct {
    uint16_t conn_handle;
    uint16_t length;
    uint16_t packet_seq;
    uint8_t data[ISO_MAX_SDU_SIZE];
} iso_frame_t;

// ─────────────────────────────────────────────
// ISO Verbindungs-Kontext
// ─────────────────────────────────────────────
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

// ─────────────────────────────────────────────
// TX / RX Queues
// ─────────────────────────────────────────────
static QueueHandle_t tx_queue = NULL;
static QueueHandle_t rx_queue = NULL;

// ─────────────────────────────────────────────
// User Callbacks
// ─────────────────────────────────────────────
static le_audio_iso_rx_cb_t rx_user_cb = NULL;
static le_audio_iso_tx_done_cb_t tx_done_cb = NULL;

// ─────────────────────────────────────────────
// Interne Funktionen
// ─────────────────────────────────────────────
static iso_conn_ctx_t* find_conn(uint16_t handle)
{
    for (int i = 0; i < 4; i++) {
        if (iso_conn[i].conn_handle == handle && iso_conn[i].active) {
            return &iso_conn[i];
        }
    }
    return NULL;
}

// ─────────────────────────────────────────────
// Initialisierung
// ─────────────────────────────────────────────
void le_audio_iso_init(void)
{
    iso_mutex = xSemaphoreCreateMutex();
    configASSERT(iso_mutex);

    // Queues erstellen
    tx_queue = xQueueCreate(ISO_QUEUE_DEPTH, sizeof(iso_frame_t));
    rx_queue = xQueueCreate(ISO_QUEUE_DEPTH, sizeof(iso_frame_t));
    configASSERT(tx_queue);
    configASSERT(rx_queue);

    // Kontexte initialisieren
    for (int i = 0; i < 4; i++) {
        iso_conn[i].conn_handle = 0xFFFF;
        iso_conn[i].active = false;
        iso_conn[i].tx_seq_num = 0;
        iso_conn[i].rx_count = 0;
        iso_conn[i].tx_count = 0;
        iso_conn[i].rx_lost = 0;
    }

    ESP_LOGI(TAG, "ISO Layer initialized (queue depth=%u)", ISO_QUEUE_DEPTH);
}

// ─────────────────────────────────────────────
// Data Path konfigurieren
// ─────────────────────────────────────────────
int le_audio_iso_setup_data_path(const le_audio_iso_config_t *config)
{
    if (!config) {
        ESP_LOGE(TAG, "Config NULL");
        return -1;
    }

    xSemaphoreTake(iso_mutex, portMAX_DELAY);

    // Freien Slot suchen
    int slot = -1;
    for (int i = 0; i < 4; i++) {
        if (!iso_conn[i].active) {
            slot = i;
            break;
        }
    }

    if (slot == -1) {
        xSemaphoreGive(iso_mutex);
        ESP_LOGE(TAG, "Kein freier ISO Slot");
        return -2;
    }

    iso_conn[slot].conn_handle = config->conn_handle;
    iso_conn[slot].cig_id = config->cig_id;
    iso_conn[slot].cis_id = config->cis_id;
    iso_conn[slot].active = true;
    iso_conn[slot].tx_seq_num = 0;
    iso_conn[slot].rx_count = 0;
    iso_conn[slot].tx_count = 0;
    iso_conn[slot].rx_lost = 0;

    xSemaphoreGive(iso_mutex);

    ESP_LOGI(TAG, "ISO Data Path aktiv: conn=0x%04X cig=%u cis=%u",
             config->conn_handle, config->cig_id, config->cis_id);
    return 0;
}

// ─────────────────────────────────────────────
// TX - LC3 Frame senden
// ─────────────────────────────────────────────
int le_audio_iso_send(uint16_t conn_handle,
                      const uint8_t *lc3_data,
                      uint16_t length)
{
    if (!lc3_data || length == 0 || length > ISO_MAX_SDU_SIZE) {
        ESP_LOGE(TAG, "TX: ungültige Daten len=%u", length);
        return -1;
    }

    iso_conn_ctx_t *ctx = find_conn(conn_handle);
    if (!ctx) {
        ESP_LOGE(TAG, "TX: keine ISO Verbindung für 0x%04X", conn_handle);
        return -2;
    }

    // Frame zusammenbauen
    iso_frame_t frame;
    frame.conn_handle = conn_handle;
    frame.length = length;
    frame.packet_seq = ctx->tx_seq_num++;
    memcpy(frame.data, lc3_data, length);

    // In Queue einreihen (max 1ms warten)
    if (xQueueSend(tx_queue, &frame, pdMS_TO_TICKS(1)) != pdTRUE) {
        ESP_LOGW(TAG, "TX Queue voll! Frame verworfen");
        return -3;
    }

    ctx->tx_count++;
    return 0;
}

// ─────────────────────────────────────────────
// TX Worker Task
// Liest aus tx_queue und sendet via BLE ISO
// ─────────────────────────────────────────────
static void iso_tx_task(void *arg)
{
    iso_frame_t frame;

    ESP_LOGI(TAG, "ISO TX Task gestartet");

    while (1) {
        if (xQueueReceive(tx_queue, &frame, portMAX_DELAY) == pdTRUE) {

            // ────────────────────────────────────────
            // HCI ISO Data Packet senden
            // Format: [Handle+Flags 2B][Data_Total_Length 2B]
            // [Time_Stamp 4B][Packet_Seq_Num 2B]
            // [ISO_SDU_Length 2B][ISO_SDU_Data]
            // ────────────────────────────────────────

            // Wir nutzen esp_ble_gap_vendor_command_send nicht für Daten
            // Stattdessen: esp_ble_hci_acl_data_send Äquivalent für ISO
            //
            // HINWEIS: In ESP-IDF 5.x gibt es noch keine direkte
            // esp_ble_iso_send() API. Workaround:

#if CONFIG_BT_BLE_42_FEATURES_SUPPORTED
            // Für Chips mit BLE 5.2: Nutze Bluedroid internal
            // Diese Funktion muss ggf. über IDF Patch freigeschaltet werden
            uint8_t hci_buf[12 + ISO_MAX_SDU_SIZE];
            uint8_t off = 0;

            // Connection Handle + PB_Flag + TS_Flag
            // Bits: [15:14]=TS_Flag, [13:12]=PB_Flag, [11:0]=Handle
            // PB_Flag=0b10 (Complete SDU), TS_Flag=0 (kein Timestamp)
            uint16_t handle_flags = (frame.conn_handle & 0x0FFF) | (0x02 << 12);
            hci_buf[off++] = (handle_flags >> 0) & 0xFF;
            hci_buf[off++] = (handle_flags >> 8) & 0xFF;

            // Data_Total_Length (SDU + 4 Byte Header)
            uint16_t total_len = frame.length + 4;
            hci_buf[off++] = (total_len >> 0) & 0xFF;
            hci_buf[off++] = (total_len >> 8) & 0xFF;

            // Packet_Sequence_Number
            hci_buf[off++] = (frame.packet_seq >> 0) & 0xFF;
            hci_buf[off++] = (frame.packet_seq >> 8) & 0xFF;

            // ISO_SDU_Length + Packet_Status_Flag
            hci_buf[off++] = (frame.length >> 0) & 0xFF;
            hci_buf[off++] = (frame.length >> 8) & 0xFF; // upper 2 bits = PS_Flags

            // Payload
            memcpy(&hci_buf[off], frame.data, frame.length);
            off += frame.length;

            // Senden via Bluedroid HCI Transport
            esp_err_t ret = esp_ble_gap_vendor_command_send(
                0xFC00, // Vendor-spezifisch als Workaround
                off, hci_buf
            );

            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "ISO TX failed: %s", esp_err_to_name(ret));
            }
#endif

            // TX Done Callback
            if (tx_done_cb) {
                tx_done_cb(frame.conn_handle, 0);
            }
        }
    }
}

// ─────────────────────────────────────────────
// RX - Empfangene ISO Frames verarbeiten
// (wird aus BLE Stack Callback aufgerufen)
// ─────────────────────────────────────────────
void le_audio_iso_rx_handler(uint16_t conn_handle,
                              const uint8_t *data,
                              uint16_t length)
{
    if (!data || length == 0) {
        ESP_LOGE(TAG, "RX: leere Daten");
        return;
    }

    iso_conn_ctx_t *ctx = find_conn(conn_handle);
    if (!ctx) {
        ESP_LOGW(TAG, "RX: unbekannte Verbindung 0x%04X", conn_handle);
        return;
    }

    // Frame in RX Queue
    iso_frame_t frame;
    frame.conn_handle = conn_handle;
    frame.length = (length > ISO_MAX_SDU_SIZE) ? ISO_MAX_SDU_SIZE : length;
    frame.packet_seq = ctx->rx_count;
    memcpy(frame.data, data, frame.length);

    ctx->rx_count++;

    // Direkt Callback aufrufen (aus ISR-sicherem Kontext)
    if (rx_user_cb) {
        rx_user_cb(conn_handle, frame.data, frame.length);
        return;
    }

    // Oder in Queue für RX Task
    if (xQueueSend(rx_queue, &frame, 0) != pdTRUE) {
        ctx->rx_lost++;
        ESP_LOGW(TAG, "RX Queue voll! Verloren=%lu", ctx->rx_lost);
    }
}

// ─────────────────────────────────────────────
// RX - Blockierendes Lesen für RX Task
// ─────────────────────────────────────────────
bool le_audio_iso_receive(uint16_t conn_handle,
                           uint8_t *out_buf,
                           uint16_t *out_len,
                           uint32_t timeout_ticks)
{
    if (!out_buf || !out_len) return false;

    iso_frame_t frame;
    if (xQueueReceive(rx_queue, &frame, timeout_ticks) != pdTRUE) {
        return false;
    }

    if (frame.conn_handle != conn_handle) {
        // Falscher Handle → zurücklegen nicht möglich, verwerfen
        ESP_LOGW(TAG, "RX: falscher Handle 0x%04X", frame.conn_handle);
        return false;
    }

    memcpy(out_buf, frame.data, frame.length);
    *out_len = frame.length;
    return true;
}

// ─────────────────────────────────────────────
// Callbacks registrieren
// ─────────────────────────────────────────────
void le_audio_iso_register_rx_cb(le_audio_iso_rx_cb_t cb)
{
    rx_user_cb = cb;
    ESP_LOGI(TAG, "ISO RX Callback registriert");
}

void le_audio_iso_register_tx_done_cb(le_audio_iso_tx_done_cb_t cb)
{
    tx_done_cb = cb;
    ESP_LOGI(TAG, "ISO TX-Done Callback registriert");
}

// ─────────────────────────────────────────────
// TX Task starten
// ─────────────────────────────────────────────
void le_audio_iso_start_tasks(void)
{
    xTaskCreatePinnedToCore(
        iso_tx_task,
        "iso_tx",
        4096,
        NULL,
        configMAX_PRIORITIES - 2, // Hohe Priorität
        NULL,
        1 // Core 1
    );
    ESP_LOGI(TAG, "ISO TX Task gestartet");
}

// ─────────────────────────────────────────────
// Data Path entfernen
// ─────────────────────────────────────────────
int le_audio_iso_remove_data_path(uint16_t conn_handle, uint8_t data_path_id)
{
    xSemaphoreTake(iso_mutex, portMAX_DELAY);

    for (int i = 0; i < 4; i++) {
        if (iso_conn[i].conn_handle == conn_handle) {
            ESP_LOGI(TAG, "ISO Data Path entfernt: conn=0x%04X "
                     "(tx=%lu rx=%lu lost=%lu)",
                     conn_handle,
                     iso_conn[i].tx_count,
                     iso_conn[i].rx_count,
                     iso_conn[i].rx_lost);

            iso_conn[i].active = false;
            iso_conn[i].conn_handle = 0xFFFF;
            xSemaphoreGive(iso_mutex);
            return 0;
        }
    }

    xSemaphoreGive(iso_mutex);
    ESP_LOGE(TAG, "ISO conn 0x%04X nicht gefunden", conn_handle);
    return -1;
}

// ─────────────────────────────────────────────
// Status / Debug
// ─────────────────────────────────────────────
int le_audio_iso_is_ready(uint16_t conn_handle)
{
    return find_conn(conn_handle) != NULL ? 1 : 0;
}

void le_audio_iso_print_stats(uint16_t conn_handle)
{
    iso_conn_ctx_t *ctx = find_conn(conn_handle);
    if (!ctx) {
        ESP_LOGW(TAG, "Stats: conn 0x%04X nicht gefunden", conn_handle);
        return;
    }
    ESP_LOGI(TAG, "ISO Stats [0x%04X]: TX=%lu RX=%lu Lost=%lu",
             conn_handle, ctx->tx_count, ctx->rx_count, ctx->rx_lost);
}