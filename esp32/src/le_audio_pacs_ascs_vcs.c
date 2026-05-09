#include "le_audio_pacs_ascs_vcs.h"
#include "esp_log.h"
#include "esp_gatts_api.h"
#include "esp_bt_defs.h"
#include "esp_gatt_common_api.h"

static const char *TAG = "LE_AUDIO_SVC";

// ─────────────────────────────────────────────
// Bluetooth SIG UUIDs (16-Bit)
// ─────────────────────────────────────────────
#define UUID_PACS_SERVICE 0x1850
#define UUID_ASCS_SERVICE 0x184E
#define UUID_VCS_SERVICE 0x1844

#define UUID_SINK_PAC 0x2BC9
#define UUID_SOURCE_PAC 0x2BCB
#define UUID_AVAILABLE_AUDIO_CTX 0x2BCD
#define UUID_SUPPORTED_AUDIO_CTX 0x2BCE
#define UUID_SINK_ASE 0x2BC4
#define UUID_SOURCE_ASE 0x2BC5
#define UUID_ASE_CONTROL_POINT 0x2BC6
#define UUID_VOLUME_STATE 0x2B7D
#define UUID_VOLUME_CONTROL_POINT 0x2B7E
#define UUID_VOLUME_FLAGS 0x2B7F
#define UUID_CCCD 0x2902 // Client Characteristic Config

// ─────────────────────────────────────────────
// LC3 Codec Capabilities Record
// Format: Bluetooth Assigned Numbers §6.12.4
// ─────────────────────────────────────────────
// LTV Struktur:
// [Length][Type][Value...]
// Type 0x01 = Supported_Sampling_Frequencies
// Type 0x02 = Supported_Frame_Durations
// Type 0x03 = Supported_Audio_Channel_Counts
// Type 0x04 = Supported_Octets_per_Codec_Frame
// Type 0x05 = Supported_Max_Codec_Frames_per_SDU

static const uint8_t lc3_codec_capabilities[] = {
    // Sampling Frequencies (Type 0x01)
    0x03, 0x01,
    0x80, 0x00, // 48000 Hz = Bit 7

    // Frame Durations (Type 0x02)
    0x02, 0x02,
    0x02, // 10ms = Bit 1

    // Audio Channel Counts (Type 0x03)
    0x02, 0x03,
    0x03, // Mono + Stereo

    // Octets per Codec Frame (Type 0x04)
    0x05, 0x04,
    0x78, 0x00, // Min = 120
    0x78, 0x00, // Max = 120

    // Max Codec Frames per SDU (Type 0x05)
    0x02, 0x05,
    0x01 // Max 1 Frame pro SDU
};

// Sink PAC Record
static const uint8_t sink_pac_record[] = {
    0x01, // Coding_Format = LC3 (0x06 im echten BT)
    // HINWEIS: Bei echtem LC3 muss hier 0x06 stehen
    // für Tests 0x01 (transparent)
    0x06, // Coding_Format = LC3
    0x00, 0x00, // Company_ID
    0x00, 0x00, // Vendor_Defined_Codec_ID
    sizeof(lc3_codec_capabilities), // Codec_Specific_Capabilities_Length
    // Danach: lc3_codec_capabilities (wird im Handler zusammengesetzt)
};

// ─────────────────────────────────────────────
// GATT Handle Tabelle
// ─────────────────────────────────────────────
typedef enum {
    // PACS
    PACS_SVC_IDX,
    PACS_SINK_PAC_IDX,
    PACS_SINK_PAC_VAL_IDX,
    PACS_SINK_PAC_CCCD_IDX,
    PACS_AVAIL_CTX_IDX,
    PACS_AVAIL_CTX_VAL_IDX,
    PACS_AVAIL_CTX_CCCD_IDX,
    PACS_SUPP_CTX_IDX,
    PACS_SUPP_CTX_VAL_IDX,

    // ASCS
    ASCS_SVC_IDX,
    ASCS_SINK_ASE_IDX,
    ASCS_SINK_ASE_VAL_IDX,
    ASCS_SINK_ASE_CCCD_IDX,
    ASCS_SRC_ASE_IDX,
    ASCS_SRC_ASE_VAL_IDX,
    ASCS_SRC_ASE_CCCD_IDX,
    ASCS_CTRL_PT_IDX,
    ASCS_CTRL_PT_VAL_IDX,

    // VCS
    VCS_SVC_IDX,
    VCS_VOL_STATE_IDX,
    VCS_VOL_STATE_VAL_IDX,
    VCS_VOL_STATE_CCCD_IDX,
    VCS_VOL_CTRL_PT_IDX,
    VCS_VOL_CTRL_PT_VAL_IDX,
    VCS_VOL_FLAGS_IDX,
    VCS_VOL_FLAGS_VAL_IDX,

    HANDLE_TABLE_SIZE
} gatt_handle_idx_t;

static uint16_t handle_table[HANDLE_TABLE_SIZE];

// ─────────────────────────────────────────────
// Service State
// ─────────────────────────────────────────────
static uint8_t volume_state[3] = {64, 0, 0};
 // [volume, mute, change_counter]
static uint8_t volume_flags = 0x00;
static uint16_t avail_audio_ctx = 0x0006; // Conversational + Media (Sink)
static uint16_t supp_audio_ctx = 0x0006;

// ASE State (Bluetooth SIG ASCS §5)
static uint8_t sink_ase_state = 0x00; // Idle
static uint8_t src_ase_state = 0x00; // Idle

// Connected client
static uint16_t connected_gatts_if = 0;
static uint16_t connected_conn_id = 0;

// ─────────────────────────────────────────────
// Notification senden
// ─────────────────────────────────────────────
static void send_notification(uint16_t attr_handle,
                              uint8_t *data,
                              uint16_t len)
{
    if (connected_conn_id == 0) return;

    esp_ble_gatts_send_indicate(
        connected_gatts_if,
        connected_conn_id,
        attr_handle,
        len,
        data,
        false // false = Notification, true = Indication
    );
}

// ─────────────────────────────────────────────
// GATT Attribut Tabelle
// ─────────────────────────────────────────────
static const esp_gatts_attr_db_t gatt_db[HANDLE_TABLE_SIZE] = {

    // ══════════════════════════════════════════
    // PACS Service
    // ══════════════════════════════════════════
    [PACS_SVC_IDX] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t*)&(uint16_t){0x2800},
         ESP_GATT_PERM_READ, sizeof(uint16_t), sizeof(uint16_t),
         (uint8_t*)&(uint16_t){UUID_PACS_SERVICE}}
    },

    // Sink PAC Characteristic Declaration
    [PACS_SINK_PAC_IDX] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t*)&(uint16_t){0x2803},
         ESP_GATT_PERM_READ, sizeof(uint8_t) + sizeof(uint16_t) + ESP_UUID_LEN_16,
         0, NULL}
    },

    // Sink PAC Value
    [PACS_SINK_PAC_VAL_IDX] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t*)&(uint16_t){UUID_SINK_PAC},
         ESP_GATT_PERM_READ,
         sizeof(sink_pac_record) + sizeof(lc3_codec_capabilities),
         sizeof(sink_pac_record),
         (uint8_t*)sink_pac_record}
    },

    // Sink PAC CCCD (Notifications)
    [PACS_SINK_PAC_CCCD_IDX] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t*)&(uint16_t){UUID_CCCD},
         ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
         sizeof(uint16_t), 0, NULL}
    },

    // Available Audio Contexts
    [PACS_AVAIL_CTX_IDX] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t*)&(uint16_t){0x2803},
         ESP_GATT_PERM_READ, 0, 0, NULL}
    },

    [PACS_AVAIL_CTX_VAL_IDX] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t*)&(uint16_t){UUID_AVAILABLE_AUDIO_CTX},
         ESP_GATT_PERM_READ, sizeof(uint32_t), sizeof(uint32_t),
         (uint8_t*)&avail_audio_ctx}
    },

    [PACS_AVAIL_CTX_CCCD_IDX] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t*)&(uint16_t){UUID_CCCD},
         ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
         sizeof(uint16_t), 0, NULL}
    },

    // Supported Audio Contexts (read-only)
    [PACS_SUPP_CTX_IDX] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t*)&(uint16_t){0x2803},
         ESP_GATT_PERM_READ, 0, 0, NULL}
    },

    [PACS_SUPP_CTX_VAL_IDX] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t*)&(uint16_t){UUID_SUPPORTED_AUDIO_CTX},
         ESP_GATT_PERM_READ, sizeof(uint32_t), sizeof(uint32_t),
         (uint8_t*)&supp_audio_ctx}
    },

    // ══════════════════════════════════════════
    // ASCS Service
    // ══════════════════════════════════════════
    [ASCS_SVC_IDX] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t*)&(uint16_t){0x2800},
         ESP_GATT_PERM_READ, sizeof(uint16_t), sizeof(uint16_t),
         (uint8_t*)&(uint16_t){UUID_ASCS_SERVICE}}
    },

    // Sink ASE
    [ASCS_SINK_ASE_IDX] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t*)&(uint16_t){0x2803},
         ESP_GATT_PERM_READ, 0, 0, NULL}
    },

    [ASCS_SINK_ASE_VAL_IDX] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t*)&(uint16_t){UUID_SINK_ASE},
         ESP_GATT_PERM_READ,
         3, 2,
         (uint8_t[]){0x01, 0x00}} // [ASE_ID=1, State=Idle]
    },

    [ASCS_SINK_ASE_CCCD_IDX] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t*)&(uint16_t){UUID_CCCD},
         ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
         sizeof(uint16_t), 0, NULL}
    },

    // Source ASE
    [ASCS_SRC_ASE_IDX] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t*)&(uint16_t){0x2803},
         ESP_GATT_PERM_READ, 0, 0, NULL}
    },

    [ASCS_SRC_ASE_VAL_IDX] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t*)&(uint16_t){UUID_SOURCE_ASE},
         ESP_GATT_PERM_READ,
         3, 2,
         (uint8_t[]){0x02, 0x00}} // [ASE_ID=2, State=Idle]
    },

    [ASCS_SRC_ASE_CCCD_IDX] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t*)&(uint16_t){UUID_CCCD},
         ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
         sizeof(uint16_t), 0, NULL}
    },

    // ASE Control Point (Writable, Indicatable)
    [ASCS_CTRL_PT_IDX] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t*)&(uint16_t){0x2803},
         ESP_GATT_PERM_READ, 0, 0, NULL}
    },

    [ASCS_CTRL_PT_VAL_IDX] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t*)&(uint16_t){UUID_ASE_CONTROL_POINT},
         ESP_GATT_PERM_WRITE,
         64, 0, NULL} // Writable, max 64 bytes
    },

    // ══════════════════════════════════════════
    // VCS Service
    // ══════════════════════════════════════════
    [VCS_SVC_IDX] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t*)&(uint16_t){0x2800},
         ESP_GATT_PERM_READ, sizeof(uint16_t), sizeof(uint16_t),
         (uint8_t*)&(uint16_t){UUID_VCS_SERVICE}}
    },

    // Volume State
    [VCS_VOL_STATE_IDX] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t*)&(uint16_t){0x2803},
         ESP_GATT_PERM_READ, 0, 0, NULL}
    },

    [VCS_VOL_STATE_VAL_IDX] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t*)&(uint16_t){UUID_VOLUME_STATE},
         ESP_GATT_PERM_READ,
         sizeof(volume_state), sizeof(volume_state),
         volume_state}
    },

    [VCS_VOL_STATE_CCCD_IDX] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t*)&(uint16_t){UUID_CCCD},
         ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
         sizeof(uint16_t), 0, NULL}
    },

    // Volume Control Point
    [VCS_VOL_CTRL_PT_IDX] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t*)&(uint16_t){0x2803},
         ESP_GATT_PERM_READ, 0, 0, NULL}
    },

    [VCS_VOL_CTRL_PT_VAL_IDX] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t*)&(uint16_t){UUID_VOLUME_CONTROL_POINT},
         ESP_GATT_PERM_WRITE,
         3, 0, NULL}
    },

    // Volume Flags
    [VCS_VOL_FLAGS_IDX] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t*)&(uint16_t){0x2803},
         ESP_GATT_PERM_READ, 0, 0, NULL}
    },

    [VCS_VOL_FLAGS_VAL_IDX] = {
        {ESP_GATT_AUTO_RSP},
        {ESP_UUID_LEN_16, (uint8_t*)&(uint16_t){UUID_VOLUME_FLAGS},
         ESP_GATT_PERM_READ,
         sizeof(volume_flags), sizeof(volume_flags),
         &volume_flags}
    },
};

// ─────────────────────────────────────────────
// ASE Control Point Handler
// Opcodes: 0x01=Config Codec, 0x02=Config QoS,
// 0x03=Enable, 0x04=Receiver Start Ready
// 0x05=Disable, 0x06=Receiver Stop Ready
// 0x07=Update Metadata, 0x08=Release
// ─────────────────────────────────────────────
static void handle_ase_control_point(const uint8_t *data, uint16_t len)
{
    if (!data || len < 2) return;

    uint8_t opcode = data[0];
    uint8_t ase_count = data[1];

    ESP_LOGI(TAG, "ASE Control Point: opcode=0x%02X, ase_count=%u",
             opcode, ase_count);

    switch (opcode) {
        case 0x01: // Config Codec
            ESP_LOGI(TAG, "ASE: Config Codec");
            sink_ase_state = 0x01; // Codec Configured
            // Notification senden
            uint8_t notify[2] = {0x01, sink_ase_state};
            send_notification(handle_table[ASCS_SINK_ASE_VAL_IDX],
                              notify, sizeof(notify));
            break;

        case 0x02: // Config QoS
            ESP_LOGI(TAG, "ASE: Config QoS");
            sink_ase_state = 0x02; // QoS Configured
            break;

        case 0x03: // Enable
            ESP_LOGI(TAG, "ASE: Enable");
            sink_ase_state = 0x03; // Enabling
            break;

        case 0x04: // Receiver Start Ready
            ESP_LOGI(TAG, "ASE: Receiver Start Ready → Streaming");
            sink_ase_state = 0x04; // Streaming
            break;

        case 0x08: // Release
            ESP_LOGI(TAG, "ASE: Release");
            sink_ase_state = 0x00; // Idle
            break;

        default:
            ESP_LOGW(TAG, "ASE: unbekannter Opcode 0x%02X", opcode);
            break;
    }
}

// ─────────────────────────────────────────────
// Volume Control Point Handler
// ─────────────────────────────────────────────
static void handle_volume_control_point(const uint8_t *data, uint16_t len)
{
    if (!data || len < 2) return;

    uint8_t opcode = data[0];
    uint8_t change_counter = data[1];

    // Change counter verifizieren
    if (change_counter != volume_state[2]) {
        ESP_LOGW(TAG, "VCS: Invalid change counter %u (expected %u)",
                 change_counter, volume_state[2]);
        return;
    }

    switch (opcode) {
        case 0x01: // Set Absolute Volume
            if (len >= 3) {
                volume_state[0] = data[2];
                volume_state[2]++; // Increment change counter
                ESP_LOGI(TAG, "VCS: Volume = %u", volume_state[0]);
            }
            break;

        case 0x02: // Unmute
            volume_state[1] = 0;
            volume_state[2]++;
            ESP_LOGI(TAG, "VCS: Unmuted");
            break;

        case 0x03: // Mute
            volume_state[1] = 1;
            volume_state[2]++;
            ESP_LOGI(TAG, "VCS: Muted");
            break;

        default:
            ESP_LOGW(TAG, "VCS: unbekannter Opcode 0x%02X", opcode);
            return;
    }

    // Volume State Notification senden
    send_notification(handle_table[VCS_VOL_STATE_VAL_IDX],
                      volume_state, sizeof(volume_state));
}

// ─────────────────────────────────────────────
// GATT Event Handler
// ─────────────────────────────────────────────
static void gatts_event_handler(esp_gatts_cb_event_t event,
                                 esp_gatt_if_t gatts_if,
                                 esp_ble_gatts_cb_param_t *param)
{
    switch (event) {

        case ESP_GATTS_REG_EVT:
            if (param->reg.status == ESP_GATT_OK) {
                ESP_LOGI(TAG, "GATT App registriert: if=%d", gatts_if);

                // Alle Services in einer Tabelle anlegen
                esp_ble_gatts_create_attr_tab(
                    gatt_db,
                    gatts_if,
                    HANDLE_TABLE_SIZE,
                    0 // Service Instance ID
                );
            }
            break;

        case ESP_GATTS_CREAT_ATTR_TAB_EVT:
            if (param->add_attr_tab.status == ESP_GATT_OK) {
                if (param->add_attr_tab.num_handle == HANDLE_TABLE_SIZE) {
                    // Handle Tabelle kopieren
                    memcpy(handle_table,
                           param->add_attr_tab.handles,
                           HANDLE_TABLE_SIZE * sizeof(uint16_t));

                    // Services starten
                    esp_ble_gatts_start_service(handle_table[PACS_SVC_IDX]);
                    esp_ble_gatts_start_service(handle_table[ASCS_SVC_IDX]);
                    esp_ble_gatts_start_service(handle_table[VCS_SVC_IDX]);

                    ESP_LOGI(TAG, "✓ PACS/ASCS/VCS Services gestartet");
                    ESP_LOGI(TAG, " PACS handle: 0x%04X",
                             handle_table[PACS_SVC_IDX]);
                    ESP_LOGI(TAG, " ASCS handle: 0x%04X",
                             handle_table[ASCS_SVC_IDX]);
                    ESP_LOGI(TAG, " VCS handle: 0x%04X",
                             handle_table[VCS_SVC_IDX]);
                } else {
                    ESP_LOGE(TAG, "Handle Tabelle falsche Größe: %u (erwartet %u)",
                             param->add_attr_tab.num_handle, HANDLE_TABLE_SIZE);
                }
            }
            break;

        case ESP_GATTS_CONNECT_EVT:
            connected_gatts_if = gatts_if;
            connected_conn_id = param->connect.conn_id;
            ESP_LOGI(TAG, "Client verbunden: conn_id=%u", param->connect.conn_id);

            // MTU auf Maximum setzen
            esp_ble_gatt_set_local_mtu(517);
            break;

        case ESP_GATTS_DISCONNECT_EVT:
            connected_conn_id = 0;
            ESP_LOGI(TAG, "Client getrennt: conn_id=%u", param->disconnect.conn_id);

            // ASE Status zurücksetzen
            sink_ase_state = 0x00;
            src_ase_state = 0x00;
            break;

        case ESP_GATTS_WRITE_EVT: {
            uint16_t handle = param->write.handle;
            uint8_t *data = param->write.value;
            uint16_t len = param->write.len;

            ESP_LOGI(TAG, "GATT Write: handle=0x%04X len=%u", handle, len);

            // ASE Control Point
            if (handle == handle_table[ASCS_CTRL_PT_VAL_IDX]) {
                handle_ase_control_point(data, len);
            }
            // Volume Control Point
            else if (handle == handle_table[VCS_VOL_CTRL_PT_VAL_IDX]) {
                handle_volume_control_point(data, len);
            }

            // Response senden falls nötig
            if (param->write.need_rsp) {
                esp_ble_gatts_send_response(
                    gatts_if,
                    param->write.conn_id,
                    param->write.trans_id,
                    ESP_GATT_OK,
                    NULL
                );
            }
            break;
        }

        case ESP_GATTS_MTU_EVT:
            ESP_LOGI(TAG, "MTU: %u bytes", param->mtu.mtu);
            break;

        default:
            break;
    }
}

// ─────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────
void le_audio_gatts_register_handler(void)
{
    esp_ble_gatts_register_callback(gatts_event_handler);
    ESP_LOGI(TAG, "GATT Handler registriert");
}

void le_audio_pacs_init(void)
{
    esp_ble_gatts_app_register(0x0001); // App ID für PACS/ASCS/VCS
    ESP_LOGI(TAG, "Services werden registriert...");
}

// ASCS und VCS werden zusammen mit PACS in einer App registriert
void le_audio_ascs_init(void) { /* Handled in PACS init */ }
void le_audio_vcs_init(void) { /* Handled in PACS init */ }

void le_audio_vcs_update_volume(uint8_t vol)
{
    volume_state[0] = vol;
    volume_state[2]++;
    send_notification(handle_table[VCS_VOL_STATE_VAL_IDX],
                      volume_state, sizeof(volume_state));
    ESP_LOGI(TAG, "Volume: %u", vol);
}

uint8_t le_audio_vcs_get_volume(void)
{
    return volume_state[0];
}