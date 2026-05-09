#include "le_audio_hci.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "LE_AUDIO_HCI";

// ─────────────────────────────────────────────
// HCI Opcodes (Bluetooth Core 5.3, Vol 4)
// ─────────────────────────────────────────────
#define HCI_LE_SET_CIG_PARAMETERS 0x2062
#define HCI_LE_CREATE_CIS 0x2064
#define HCI_LE_REMOVE_CIG 0x2065
#define HCI_LE_ACCEPT_CIS_REQUEST 0x2066
#define HCI_LE_REJECT_CIS_REQUEST 0x2067
#define HCI_LE_SETUP_ISO_DATA_PATH 0x206E
#define HCI_LE_REMOVE_ISO_DATA_PATH 0x206F

// ─────────────────────────────────────────────
// HCI Event Subcodes
// ─────────────────────────────────────────────
#define HCI_EVT_LE_META 0x3E
#define HCI_EVT_CMD_COMPLETE 0x0E
#define HCI_SUBEVENT_CIS_ESTABLISHED 0x19
#define HCI_SUBEVENT_CIS_REQUEST 0x1A

// ─────────────────────────────────────────────
// CIS State Management
typedef struct {
    le_audio_cis_state_t state;
    uint16_t conn_handle;
    uint8_t cig_id;
    uint8_t cis_id;
    uint16_t max_sdu_size;
    uint8_t phy;
} le_audio_cis_context_t;

static le_audio_cis_context_t cis_context[4];
static SemaphoreHandle_t hci_mutex = NULL;

// Callback Pointer
static le_audio_hci_event_cb_t user_event_cb = NULL;

// ─────────────────────────────────────────────
// Interne Hilfsfunktion: HCI Command senden
// ─────────────────────────────────────────────
static esp_err_t hci_send_command(uint16_t opcode,
                                   uint8_t *params,
                                   uint8_t param_len)
{
    // Baue kompletten HCI Command Packet
    // Format: [OCF/OGF 2B][Param_Len 1B][Params]
    uint8_t cmd_buf[258];

    if (param_len > 255) {
        ESP_LOGE(TAG, "HCI param too long: %u", param_len);
        return ESP_ERR_INVALID_ARG;
    }

    cmd_buf[0] = (uint8_t)(opcode & 0xFF); // OCF low
    cmd_buf[1] = (uint8_t)((opcode >> 8) & 0xFF); // OGF+OCF high
    cmd_buf[2] = param_len;

    if (params && param_len > 0) {
        memcpy(&cmd_buf[3], params, param_len);
    }

    // ESP-IDF 5.x: Vendor HCI Interface
    esp_err_t ret = esp_ble_gap_vendor_command_send(
        opcode,
        param_len,
        params
    );

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "HCI cmd 0x%04X failed: %s",
                 opcode, esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "HCI cmd 0x%04X sent (%u bytes)", opcode, param_len);
    }

    return ret;
}

// ─────────────────────────────────────────────
// Initialisierung
// ─────────────────────────────────────────────
void le_audio_hci_init(void)
{
    hci_mutex = xSemaphoreCreateMutex();
    configASSERT(hci_mutex);

    for (int i = 0; i < 4; i++) {
        cis_context[i].state = LE_AUDIO_CIS_IDLE;
        cis_context[i].conn_handle = 0xFFFF;
        cis_context[i].cig_id = 0xFF;
        cis_context[i].cis_id = 0xFF;
    }

    ESP_LOGI(TAG, "HCI Layer initialized");
}

// ─────────────────────────────────────────────
// Callback registrieren
// ─────────────────────────────────────────────
void le_audio_hci_register_event_cb(le_audio_hci_event_cb_t cb)
{
    user_event_cb = cb;
}

// ─────────────────────────────────────────────
// HCI LE Set CIG Parameters (Opcode 0x2062)
// Bluetooth Core 5.3 Vol 4 Part E §7.8.97
// ─────────────────────────────────────────────
esp_err_t le_audio_hci_set_cig_parameters(const le_audio_cig_params_t *params)
{
    if (!params) {
        ESP_LOGE(TAG, "CIG params NULL");
        return ESP_ERR_INVALID_ARG;
    }
    if (params->cis_count == 0 || params->cis_count > 4) {
        ESP_LOGE(TAG, "Invalid CIS count: %u", params->cis_count);
        return ESP_ERR_INVALID_ARG;
    }

    // Parameterlänge berechnen
    // Fixed part: 15 Bytes
    // Per-CIS: 9 Bytes
    uint8_t param_len = 15 + (params->cis_count * 9);
    uint8_t buf[15 + 4 * 9]; // Max 4 CIS
    uint8_t offset = 0;

    // CIG_ID
    buf[offset++] = params->cig_id;

    // SDU_Interval_C_To_P (3 Bytes LE)
    buf[offset++] = (params->sdu_interval >> 0) & 0xFF;
    buf[offset++] = (params->sdu_interval >> 8) & 0xFF;
    buf[offset++] = (params->sdu_interval >> 16) & 0xFF;

    // SDU_Interval_P_To_C (3 Bytes LE) - gleich wie C_To_P
    buf[offset++] = (params->sdu_interval >> 0) & 0xFF;
    buf[offset++] = (params->sdu_interval >> 8) & 0xFF;
    buf[offset++] = (params->sdu_interval >> 16) & 0xFF;

    // Worst_Case_SCA (0 = 251-500ppm)
    buf[offset++] = 0x00;

    // Packing (0 = Sequential)
    buf[offset++] = 0x00;

    // Framing
    buf[offset++] = params->framing;

    // Max_Transport_Latency_C_To_P (2 Bytes LE)
    buf[offset++] = (params->max_trans_latency >> 0) & 0xFF;
    buf[offset++] = (params->max_trans_latency >> 8) & 0xFF;

    // Max_Transport_Latency_P_To_C (2 Bytes LE)
    buf[offset++] = (params->max_trans_latency >> 0) & 0xFF;
    buf[offset++] = (params->max_trans_latency >> 8) & 0xFF;

    // CIS_Count
    buf[offset++] = params->cis_count;

    // Pro CIS Parameter
    for (int i = 0; i < params->cis_count; i++) {
        buf[offset++] = (uint8_t)i; // CIS_ID
        buf[offset++] = 120; // Max_SDU_C_To_P LSB (LC3 120B)
        buf[offset++] = 0; // Max_SDU_C_To_P MSB
        buf[offset++] = 120; // Max_SDU_P_To_C LSB
        buf[offset++] = 0; // Max_SDU_P_To_C MSB
        buf[offset++] = 0x02; // PHY_C_To_P (2M PHY)
        buf[offset++] = 0x02; // PHY_P_To_C (2M PHY)
        buf[offset++] = params->rtn; // RTN_C_To_P
        buf[offset++] = params->rtn; // RTN_P_To_C

        // CIS Context vormerken
        if (i < 4) {
            xSemaphoreTake(hci_mutex, portMAX_DELAY);
            cis_context[i].cig_id = params->cig_id;
            cis_context[i].cis_id = (uint8_t)i;
            cis_context[i].state = LE_AUDIO_CIS_CONFIGURED;
            xSemaphoreGive(hci_mutex);
        }
    }

    ESP_LOGI(TAG, "→ HCI LE Set CIG Params: cig=%u, cis_count=%u",
             params->cig_id, params->cis_count);

    return hci_send_command(HCI_LE_SET_CIG_PARAMETERS, buf, offset);
}

// ─────────────────────────────────────────────
// HCI LE Create CIS (Opcode 0x2064)
// Bluetooth Core 5.3 Vol 4 Part E §7.8.99
// Wird erst nach ACL-Verbindung aufgerufen!
// ─────────────────────────────────────────────
esp_err_t le_audio_hci_create_cis(uint16_t *cis_handles,
                                    uint16_t *acl_handles,
                                    uint8_t count)
{
    if (!cis_handles || !acl_handles || count == 0 || count > 4) {
        ESP_LOGE(TAG, "Invalid create CIS params");
        return ESP_ERR_INVALID_ARG;
    }

    // Parameterlänge: 1 + count*4
    uint8_t param_len = 1 + (count * 4);
    uint8_t buf[1 + 4 * 4];
    uint8_t offset = 0;

    // CIS_Count
    buf[offset++] = count;

    for (int i = 0; i < count; i++) {
        // CIS_Connection_Handle (2 Bytes LE)
        buf[offset++] = (cis_handles[i] >> 0) & 0xFF;
        buf[offset++] = (cis_handles[i] >> 8) & 0xFF;

        // ACL_Connection_Handle (2 Bytes LE)
        buf[offset++] = (acl_handles[i] >> 0) & 0xFF;
        buf[offset++] = (acl_handles[i] >> 8) & 0xFF;

        ESP_LOGI(TAG, " CIS[%d]: cis_handle=0x%04X, acl_handle=0x%04X",
                 i, cis_handles[i], acl_handles[i]);
    }

    ESP_LOGI(TAG, "→ HCI LE Create CIS: count=%u", count);
    return hci_send_command(HCI_LE_CREATE_CIS, buf, param_len);
}

// ─────────────────────────────────────────────
// HCI LE Accept CIS Request (Opcode 0x2066)
// Wird vom Peripheral aufgerufen wenn CIS Request kommt
// ─────────────────────────────────────────────
esp_err_t le_audio_hci_accept_cis_request(uint16_t cis_handle)
{
    uint8_t buf[2];
    buf[0] = (cis_handle >> 0) & 0xFF;
    buf[1] = (cis_handle >> 8) & 0xFF;

    ESP_LOGI(TAG, "→ HCI LE Accept CIS Request: handle=0x%04X", cis_handle);
    return hci_send_command(HCI_LE_ACCEPT_CIS_REQUEST, buf, 2);
}

// ─────────────────────────────────────────────
// HCI LE Setup ISO Data Path (Opcode 0x206E)
// Bluetooth Core 5.3 Vol 4 Part E §7.8.109
// ─────────────────────────────────────────────
esp_err_t le_audio_hci_setup_iso_data_path(const le_audio_iso_data_path_t *cfg)
{
    if (!cfg) {
        ESP_LOGE(TAG, "ISO data path config NULL");
        return ESP_ERR_INVALID_ARG;
    }

    // Codec_Configuration_Length = 0 (kein Controller-Codec)
    // da wir LC3 im Host machen
    uint8_t codec_config_len = 0;
    uint8_t param_len = 13 + codec_config_len;
    uint8_t buf[13];
    uint8_t offset = 0;

    // Connection_Handle (2 Bytes)
    buf[offset++] = (cfg->conn_handle >> 0) & 0xFF;
    buf[offset++] = (cfg->conn_handle >> 8) & 0xFF;

    // Data_Path_Direction
    // 0x00 = Input (Host→Controller, TX)
    // 0x01 = Output (Controller→Host, RX)
    buf[offset++] = cfg->direction;

    // Data_Path_ID
    // 0x00 = HCI (Software LC3)
    buf[offset++] = cfg->data_path_id;

    // Codec_ID (5 Bytes)
    // [Codec_Format][Company_ID 2B][Vendor_Codec_ID 2B]
    buf[offset++] = cfg->codec_format; // 0x06 = LC3
    buf[offset++] = 0x00; // Company ID LSB (N/A für Standard)
    buf[offset++] = 0x00; // Company ID MSB
    buf[offset++] = 0x00; // Vendor Codec ID LSB
    buf[offset++] = 0x00; // Vendor Codec ID MSB

    // Controller_Delay (3 Bytes LE, in Mikrosekunden)
    uint32_t delay_us = (uint32_t)cfg->controller_delay * 1000;
    buf[offset++] = (delay_us >> 0) & 0xFF;
    buf[offset++] = (delay_us >> 8) & 0xFF;
    buf[offset++] = (delay_us >> 16) & 0xFF;

    // Codec_Configuration_Length
    buf[offset++] = codec_config_len;

    ESP_LOGI(TAG, "→ HCI LE Setup ISO Data Path: conn=0x%04X dir=%u path=%u",
             cfg->conn_handle, cfg->direction, cfg->data_path_id);

    return hci_send_command(HCI_LE_SETUP_ISO_DATA_PATH, buf, offset);
}

// ─────────────────────────────────────────────
// HCI LE Remove ISO Data Path (Opcode 0x206F)
// ─────────────────────────────────────────────
esp_err_t le_audio_hci_remove_iso_data_path(uint16_t conn_handle,
                                             uint8_t direction_bits)
{
    uint8_t buf[3];
    buf[0] = (conn_handle >> 0) & 0xFF;
    buf[1] = (conn_handle >> 8) & 0xFF;
    // Bit 0 = Input, Bit 1 = Output
    buf[2] = direction_bits;

    ESP_LOGI(TAG, "→ HCI LE Remove ISO Data Path: conn=0x%04X", conn_handle);
    return hci_send_command(HCI_LE_REMOVE_ISO_DATA_PATH, buf, 3);
}

// ─────────────────────────────────────────────
// HCI LE Remove CIG (Opcode 0x2065)
// ─────────────────────────────────────────────
esp_err_t le_audio_hci_remove_cig(uint8_t cig_id)
{
    uint8_t buf[1] = { cig_id };

    // Lokale Kontexte bereinigen
    xSemaphoreTake(hci_mutex, portMAX_DELAY);
    for (int i = 0; i < 4; i++) {
        if (cis_context[i].cig_id == cig_id) {
            cis_context[i].state = LE_AUDIO_CIS_IDLE;
            cis_context[i].conn_handle = 0xFFFF;
            cis_context[i].cig_id = 0xFF;
        }
    }
    xSemaphoreGive(hci_mutex);

    ESP_LOGI(TAG, "→ HCI LE Remove CIG: id=%u", cig_id);
    return hci_send_command(HCI_LE_REMOVE_CIG, buf, 1);
}

// ─────────────────────────────────────────────
// HCI Event Parser (aus GAP Vendor Callback)
// ─────────────────────────────────────────────
void le_audio_hci_process_event(uint8_t *data, uint16_t len)
{
    if (!data || len < 2) return;

    uint8_t event_code = data[0];
    // data[1] = Parameter Total Length

    if (event_code == HCI_EVT_LE_META && len >= 3) {
        uint8_t subevent = data[2];

        switch (subevent) {

            // ── CIS Established (Peripheral: nach Accept CIS Request)
            case HCI_SUBEVENT_CIS_ESTABLISHED: {
                if (len < 29) break;
                uint8_t status = data[3];
                uint16_t cis_handle = data[4] | (data[5] << 8);

                if (status == 0x00) {
                    ESP_LOGI(TAG, "✓ CIS Established: handle=0x%04X", cis_handle);

                    // State aktualisieren
                    xSemaphoreTake(hci_mutex, portMAX_DELAY);
                    for (int i = 0; i < 4; i++) {
                        if (cis_context[i].conn_handle == cis_handle ||
                            cis_context[i].conn_handle == 0xFFFF) {
                            cis_context[i].conn_handle = cis_handle;
                            cis_context[i].state = LE_AUDIO_CIS_ESTABLISHED;
                            break;
                        }
                    }
                    xSemaphoreGive(hci_mutex);

                    if (user_event_cb) {
                        le_audio_hci_event_t evt = {
                            .type = LE_AUDIO_HCI_EVT_CIS_ESTABLISHED,
                            .cis_handle = cis_handle,
                            .status = status
                        };
                        user_event_cb(&evt);
                    }
                } else {
                    ESP_LOGE(TAG, "✗ CIS Establish failed: status=0x%02X", status);
                }
                break;
            }

            // ── CIS Request (Peripheral bekommt diesen Event)
            case HCI_SUBEVENT_CIS_REQUEST: {
                if (len < 9) break;
                uint16_t acl_handle = data[3] | (data[4] << 8);
                uint16_t cis_handle = data[5] | (data[6] << 8);
                uint8_t cig_id = data[7];
                uint8_t cis_id = data[8];

                ESP_LOGI(TAG, "CIS Request: acl=0x%04X cis=0x%04X cig=%u cis_id=%u",
                         acl_handle, cis_handle, cig_id, cis_id);

                if (user_event_cb) {
                    le_audio_hci_event_t evt = {
                        .type = LE_AUDIO_HCI_EVT_CIS_REQUEST,
                        .cis_handle = cis_handle,
                        .acl_handle = acl_handle,
                        .cig_id = cig_id,
                        .cis_id = cis_id
                    };
                    user_event_cb(&evt);
                } else {
                    // Auto-Accept wenn kein Callback gesetzt
                    le_audio_hci_accept_cis_request(cis_handle);
                }
                break;
            }

            default:
                break;
        }

    } else if (event_code == HCI_EVT_CMD_COMPLETE && len >= 5) {
        // data[2] = Num_HCI_Command_Packets
        uint16_t opcode = data[3] | (data[4] << 8);
        uint8_t status = (len >= 6) ? data[5] : 0xFF;

        switch (opcode) {
            case HCI_LE_SET_CIG_PARAMETERS:
                if (status == 0) {
                    // Response enthält CIS Handles
                    // data[6] = CIG_ID, data[7] = CIS_Count
                    // data[8..] = CIS Connection Handles (2B each)
                    if (len >= 8) {
                        uint8_t cis_count = data[7];
                        ESP_LOGI(TAG, "✓ CIG Parameters set: %u CIS handles", cis_count);

                        if (user_event_cb) {
                            le_audio_hci_event_t evt = {
                                .type = LE_AUDIO_HCI_EVT_CIG_CREATED,
                                .cis_count = cis_count,
                                .status = status
                            };
                            // CIS Handles kopieren
                            for (int i = 0; i < cis_count && i < 4; i++) {
                                evt.cis_handles[i] = data[8 + i*2] |
                                                     (data[9 + i*2] << 8);
                            }
                            user_event_cb(&evt);
                        }
                    }
                } else {
                    ESP_LOGE(TAG, "✗ CIG Parameters failed: 0x%02X", status);
                }
                break;

            case HCI_LE_SETUP_ISO_DATA_PATH:
                if (status == 0) {
                    ESP_LOGI(TAG, "✓ ISO Data Path setup OK");
                } else {
                    ESP_LOGE(TAG, "✗ ISO Data Path failed: 0x%02X", status);
                }
                break;

            case HCI_LE_REMOVE_CIG:
                ESP_LOGI(TAG, "CIG removed: status=0x%02X", status);
                break;

            default:
                break;
        }
    }
}

// ─────────────────────────────────────────────
// State Getter
// ─────────────────────────────────────────────
le_audio_cis_state_t le_audio_hci_get_cis_state(uint16_t conn_handle)
{
    for (int i = 0; i < 4; i++) {
        if (cis_context[i].conn_handle == conn_handle) {
            return cis_context[i].state;
        }
    }
    return LE_AUDIO_CIS_IDLE;
}