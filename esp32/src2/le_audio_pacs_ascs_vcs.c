#include "le_audio_pacs_ascs_vcs.h"
#include "le_audio_iso.h"          // for ISO_MAX_SDU_SIZE
#include "esp_log.h"
#include "esp_gatts_api.h"
#include "esp_bt_defs.h"
#include "esp_gatt_common_api.h"
#include <string.h>

static const char *TAG = "LE_AUDIO_SVC";

// Standard SIG UUIDs
#define UUID_PACS_SERVICE    0x1850
#define UUID_ASCS_SERVICE    0x184E
#define UUID_VCS_SERVICE     0x1844
#define UUID_SINK_PAC        0x2BC9
#define UUID_AVAILABLE_AUDIO_CTX 0x2BCD
#define UUID_SUPPORTED_AUDIO_CTX 0x2BCE
#define UUID_SINK_ASE        0x2BC4
#define UUID_ASE_CONTROL_POINT 0x2BC6
#define UUID_VOLUME_STATE    0x2B7D
#define UUID_VOLUME_CONTROL_PT 0x2B7E
#define UUID_VOLUME_FLAGS    0x2B7F

// Custom audio transport 128-bit UUID
static uint8_t AUDIO_SERVICE_UUID[16] = {
    0xBC,0x9A,0x78,0x56,0x34,0x12, 0x34,0x12,0x34,0x12, 0x12,0x34,0x56,0x78,0x9A,0xBC
};
static uint8_t AUDIO_CHAR_UUID[16] = {
    0xBD,0x9A,0x78,0x56,0x34,0x12, 0x34,0x12,0x34,0x12, 0x12,0x34,0x56,0x78,0x9A,0xBC
};

// Pre-built full Sink PAC record (LC3 codec capabilities baked in)
static const uint8_t sink_pac_full[] = {
    0x01,                           // Num_PAC_Records = 1
    0x06,                           // Coding_Format = LC3
    0x00,0x00,                     // Company_ID
    0x00,0x00,                     // Vendor_Codec_ID
    sizeof(lc3_codec_caps),         // Codec_Caps_Length
    // lc3_codec_caps inline
    0x03,0x01, 0x80,0x00,          // 48kHz
    0x02,0x02, 0x02,               // 10ms
    0x02,0x03, 0x01,               // Mono
    0x05,0x04, 0x78,0x00, 0x78,0x00, // 120 octets
    0x02,0x05, 0x01,               // 1 frame/SDU
    0x00                            // Metadata_Length = 0
};

// State
static uint8_t  volume_state[3] = {64,0,0};
static uint8_t  volume_flags = 0x00;
static uint16_t avail_audio_ctx = 0x0006;
static uint16_t supp_audio_ctx = 0x0006;
static uint8_t  sink_ase_state = 0x00;

static esp_gatt_if_t gatts_if_saved = ESP_GATT_IF_NONE;
static uint16_t conn_id_saved = 0;
static bool client_connected = false;
static le_audio_audio_rx_cb_t audio_rx_cb = NULL;

// Attribute table
enum {
    IDX_PACS_SVC, IDX_SINK_PAC_DECL, IDX_SINK_PAC_VAL, IDX_SINK_PAC_CCCD,
    IDX_AVAIL_CTX_DECL, IDX_AVAIL_CTX_VAL, IDX_AVAIL_CTX_CCCD,
    IDX_SUPP_CTX_DECL, IDX_SUPP_CTX_VAL,
    IDX_ASCS_SVC, IDX_SINK_ASE_DECL, IDX_SINK_ASE_VAL, IDX_SINK_ASE_CCCD,
    IDX_ASE_CTRL_DECL, IDX_ASE_CTRL_VAL,
    IDX_VCS_SVC, IDX_VOL_STATE_DECL, IDX_VOL_STATE_VAL, IDX_VOL_STATE_CCCD,
    IDX_VOL_CTRL_DECL, IDX_VOL_CTRL_VAL, IDX_VOL_FLAGS_DECL, IDX_VOL_FLAGS_VAL,
    IDX_AUDIO_SVC, IDX_AUDIO_CHAR_DECL, IDX_AUDIO_CHAR_VAL, IDX_AUDIO_CHAR_CCCD,
    HANDLE_TABLE_SIZE
};

static uint16_t handle_table[HANDLE_TABLE_SIZE];

static const uint8_t ase_init_val[2] = {0x01, 0x00};

static const esp_gatts_attr_db_t gatt_db[HANDLE_TABLE_SIZE] = {
    [IDX_PACS_SVC] = {{ESP_GATT_AUTO_RSP},{ESP_UUID_LEN_16,(uint8_t*)&(uint16_t){0x2800},ESP_GATT_PERM_READ,sizeof(uint16_t),sizeof(uint16_t),(uint8_t*)&(uint16_t){UUID_PACS_SERVICE}}},
    [IDX_SINK_PAC_DECL] = {{ESP_GATT_AUTO_RSP},{ESP_UUID_LEN_16,(uint8_t*)&(uint16_t){0x2803},ESP_GATT_PERM_READ,0,0,NULL}},
    [IDX_SINK_PAC_VAL] = {{ESP_GATT_AUTO_RSP},{ESP_UUID_LEN_16,(uint8_t*)&(uint16_t){UUID_SINK_PAC},ESP_GATT_PERM_READ,sizeof(sink_pac_full),sizeof(sink_pac_full),(uint8_t*)sink_pac_full}},
    [IDX_SINK_PAC_CCCD] = {{ESP_GATT_AUTO_RSP},{ESP_UUID_LEN_16,(uint8_t*)&(uint16_t){0x2902},ESP_GATT_PERM_READ|ESP_GATT_PERM_WRITE,sizeof(uint16_t),0,NULL}},
    [IDX_AVAIL_CTX_DECL] = {{ESP_GATT_AUTO_RSP},{ESP_UUID_LEN_16,(uint8_t*)&(uint16_t){0x2803},ESP_GATT_PERM_READ,0,0,NULL}},
    [IDX_AVAIL_CTX_VAL] = {{ESP_GATT_AUTO_RSP},{ESP_UUID_LEN_16,(uint8_t*)&(uint16_t){UUID_AVAILABLE_AUDIO_CTX},ESP_GATT_PERM_READ,sizeof(uint32_t),sizeof(uint32_t),(uint8_t*)&avail_audio_ctx}},
    [IDX_AVAIL_CTX_CCCD] = {{ESP_GATT_AUTO_RSP},{ESP_UUID_LEN_16,(uint8_t*)&(uint16_t){0x2902},ESP_GATT_PERM_READ|ESP_GATT_PERM_WRITE,sizeof(uint16_t),0,NULL}},
    [IDX_SUPP_CTX_DECL] = {{ESP_GATT_AUTO_RSP},{ESP_UUID_LEN_16,(uint8_t*)&(uint16_t){0x2803},ESP_GATT_PERM_READ,0,0,NULL}},
    [IDX_SUPP_CTX_VAL] = {{ESP_GATT_AUTO_RSP},{ESP_UUID_LEN_16,(uint8_t*)&(uint16_t){UUID_SUPPORTED_AUDIO_CTX},ESP_GATT_PERM_READ,sizeof(uint32_t),sizeof(uint32_t),(uint8_t*)&supp_audio_ctx}},
    [IDX_ASCS_SVC] = {{ESP_GATT_AUTO_RSP},{ESP_UUID_LEN_16,(uint8_t*)&(uint16_t){0x2800},ESP_GATT_PERM_READ,sizeof(uint16_t),sizeof(uint16_t),(uint8_t*)&(uint16_t){UUID_ASCS_SERVICE}}},
    [IDX_SINK_ASE_DECL] = {{ESP_GATT_AUTO_RSP},{ESP_UUID_LEN_16,(uint8_t*)&(uint16_t){0x2803},ESP_GATT_PERM_READ,0,0,NULL}},
    [IDX_SINK_ASE_VAL] = {{ESP_GATT_AUTO_RSP},{ESP_UUID_LEN_16,(uint8_t*)&(uint16_t){UUID_SINK_ASE},ESP_GATT_PERM_READ,2,2,(uint8_t*)ase_init_val}},
    [IDX_SINK_ASE_CCCD] = {{ESP_GATT_AUTO_RSP},{ESP_UUID_LEN_16,(uint8_t*)&(uint16_t){0x2902},ESP_GATT_PERM_READ|ESP_GATT_PERM_WRITE,sizeof(uint16_t),0,NULL}},
    [IDX_ASE_CTRL_DECL] = {{ESP_GATT_AUTO_RSP},{ESP_UUID_LEN_16,(uint8_t*)&(uint16_t){0x2803},ESP_GATT_PERM_READ,0,0,NULL}},
    [IDX_ASE_CTRL_VAL] = {{ESP_GATT_AUTO_RSP},{ESP_UUID_LEN_16,(uint8_t*)&(uint16_t){UUID_ASE_CONTROL_POINT},ESP_GATT_PERM_WRITE,64,0,NULL}},
    [IDX_VCS_SVC] = {{ESP_GATT_AUTO_RSP},{ESP_UUID_LEN_16,(uint8_t*)&(uint16_t){0x2800},ESP_GATT_PERM_READ,sizeof(uint16_t),sizeof(uint16_t),(uint8_t*)&(uint16_t){UUID_VCS_SERVICE}}},
    [IDX_VOL_STATE_DECL] = {{ESP_GATT_AUTO_RSP},{ESP_UUID_LEN_16,(uint8_t*)&(uint16_t){0x2803},ESP_GATT_PERM_READ,0,0,NULL}},
    [IDX_VOL_STATE_VAL] = {{ESP_GATT_AUTO_RSP},{ESP_UUID_LEN_16,(uint8_t*)&(uint16_t){UUID_VOLUME_STATE},ESP_GATT_PERM_READ,sizeof(volume_state),sizeof(volume_state),volume_state}},
    [IDX_VOL_STATE_CCCD] = {{ESP_GATT_AUTO_RSP},{ESP_UUID_LEN_16,(uint8_t*)&(uint16_t){0x2902},ESP_GATT_PERM_READ|ESP_GATT_PERM_WRITE,sizeof(uint16_t),0,NULL}},
    [IDX_VOL_CTRL_DECL] = {{ESP_GATT_AUTO_RSP},{ESP_UUID_LEN_16,(uint8_t*)&(uint16_t){0x2803},ESP_GATT_PERM_READ,0,0,NULL}},
    [IDX_VOL_CTRL_VAL] = {{ESP_GATT_AUTO_RSP},{ESP_UUID_LEN_16,(uint8_t*)&(uint16_t){UUID_VOLUME_CONTROL_PT},ESP_GATT_PERM_WRITE,3,0,NULL}},
    [IDX_VOL_FLAGS_DECL] = {{ESP_GATT_AUTO_RSP},{ESP_UUID_LEN_16,(uint8_t*)&(uint16_t){0x2803},ESP_GATT_PERM_READ,0,0,NULL}},
    [IDX_VOL_FLAGS_VAL] = {{ESP_GATT_AUTO_RSP},{ESP_UUID_LEN_16,(uint8_t*)&(uint16_t){UUID_VOLUME_FLAGS},ESP_GATT_PERM_READ,sizeof(volume_flags),sizeof(volume_flags),&volume_flags}},
    [IDX_AUDIO_SVC] = {{ESP_GATT_AUTO_RSP},{ESP_UUID_LEN_128,AUDIO_SERVICE_UUID,ESP_GATT_PERM_READ,sizeof(uint16_t),sizeof(uint16_t),(uint8_t*)&(uint16_t){0x2800}}},
    [IDX_AUDIO_CHAR_DECL] = {{ESP_GATT_AUTO_RSP},{ESP_UUID_LEN_16,(uint8_t*)&(uint16_t){0x2803},ESP_GATT_PERM_READ,0,0,NULL}},
    [IDX_AUDIO_CHAR_VAL] = {{ESP_GATT_AUTO_RSP},{ESP_UUID_LEN_128,AUDIO_CHAR_UUID,ESP_GATT_PERM_WRITE|ESP_GATT_PERM_READ,ISO_MAX_SDU_SIZE,0,NULL}},
    [IDX_AUDIO_CHAR_CCCD] = {{ESP_GATT_AUTO_RSP},{ESP_UUID_LEN_16,(uint8_t*)&(uint16_t){0x2902},ESP_GATT_PERM_READ|ESP_GATT_PERM_WRITE,sizeof(uint16_t),0,NULL}},
};

static void send_notification(uint16_t attr_handle, uint8_t *data, uint16_t len) {
    if (!client_connected) return;
    esp_ble_gatts_send_indicate(gatts_if_saved, conn_id_saved, attr_handle, len, data, false);
}

static void handle_ase_control(const uint8_t *data, uint16_t len) {
    if (!data || len<2) return;
    uint8_t op = data[0];
    ESP_LOGI(TAG, "ASE opcode 0x%02X", op);
    switch (op) {
        case 0x01: sink_ase_state=0x01; break;
        case 0x02: sink_ase_state=0x02; break;
        case 0x03: sink_ase_state=0x03; break;
        case 0x04: {
            sink_ase_state=0x04;
            uint8_t noti[2]={0x01,0x04};
            send_notification(handle_table[IDX_SINK_ASE_VAL], noti, 2);
            break;
        }
        case 0x08: sink_ase_state=0x00; break;
        default: ESP_LOGW(TAG, "Unknown ASE opcode");
    }
}

static void handle_vcs_write(const uint8_t *data, uint16_t len) {
    if (!data || len<2 || data[1]!=volume_state[2]) return;
    switch (data[0]) {
        case 0x01: if(len>=3){ volume_state[0]=data[2]; volume_state[2]++; } break;
        case 0x02: volume_state[1]=0; volume_state[2]++; break;
        case 0x03: volume_state[1]=1; volume_state[2]++; break;
    }
    send_notification(handle_table[IDX_VOL_STATE_VAL], volume_state, sizeof(volume_state));
}

static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
    switch (event) {
        case ESP_GATTS_REG_EVT:
            if (param->reg.status == ESP_GATT_OK) {
                gatts_if_saved = gatts_if;
                esp_ble_gatts_create_attr_tab(gatt_db, gatts_if, HANDLE_TABLE_SIZE, 0);
            }
            break;
        case ESP_GATTS_CREAT_ATTR_TAB_EVT:
            if (param->add_attr_tab.status == ESP_GATT_OK && param->add_attr_tab.num_handle == HANDLE_TABLE_SIZE) {
                memcpy(handle_table, param->add_attr_tab.handles, HANDLE_TABLE_SIZE*sizeof(uint16_t));
                esp_ble_gatts_start_service(handle_table[IDX_PACS_SVC]);
                esp_ble_gatts_start_service(handle_table[IDX_ASCS_SVC]);
                esp_ble_gatts_start_service(handle_table[IDX_VCS_SVC]);
                esp_ble_gatts_start_service(handle_table[IDX_AUDIO_SVC]);
                ESP_LOGI(TAG, "All services started");
            }
            break;
        case ESP_GATTS_CONNECT_EVT:
            conn_id_saved = param->connect.conn_id;
            client_connected = true;
            esp_ble_gatt_set_local_mtu(251);
            break;
        case ESP_GATTS_DISCONNECT_EVT:
            client_connected = false;
            sink_ase_state = 0;
            break;
        case ESP_GATTS_WRITE_EVT: {
            uint16_t h = param->write.handle;
            if (h == handle_table[IDX_ASE_CTRL_VAL]) handle_ase_control(param->write.value, param->write.len);
            else if (h == handle_table[IDX_VOL_CTRL_VAL]) handle_vcs_write(param->write.value, param->write.len);
            else if (h == handle_table[IDX_AUDIO_CHAR_VAL] && audio_rx_cb)
                audio_rx_cb(conn_id_saved, param->write.value, param->write.len);
            if (param->write.need_rsp) esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
            break;
        }
        default: break;
    }
}

void le_audio_gatts_register_handler(void) {
    esp_ble_gatts_register_callback(gatts_event_handler);
}
void le_audio_pacs_init(void) { esp_ble_gatts_app_register(0x0001); }
void le_audio_ascs_init(void) {}
void le_audio_vcs_init(void) {}

void le_audio_register_audio_rx_cb(le_audio_audio_rx_cb_t cb) { audio_rx_cb = cb; }

void le_audio_gatt_send_audio_frame(uint16_t conn_handle, const uint8_t *data, uint16_t length) {
    if (!client_connected) return;
    esp_ble_gatts_send_indicate(gatts_if_saved, conn_id_saved, handle_table[IDX_AUDIO_CHAR_VAL], length, (uint8_t*)data, false);
}
void le_audio_vcs_update_volume(uint8_t vol) { volume_state[0]=vol; volume_state[2]++; send_notification(handle_table[IDX_VOL_STATE_VAL],volume_state,sizeof(volume_state)); }
uint8_t le_audio_vcs_get_volume(void) { return volume_state[0]; }#include "le_audio_pacs_ascs_vcs.h"
#include "le_audio_iso.h"          // for ISO_MAX_SDU_SIZE
#include "esp_log.h"
#include "esp_gatts_api.h"
#include "esp_bt_defs.h"
#include "esp_gatt_common_api.h"
#include <string.h>

static const char *TAG = "LE_AUDIO_SVC";

// Standard SIG UUIDs
#define UUID_PACS_SERVICE    0x1850
#define UUID_ASCS_SERVICE    0x184E
#define UUID_VCS_SERVICE     0x1844
#define UUID_SINK_PAC        0x2BC9
#define UUID_AVAILABLE_AUDIO_CTX 0x2BCD
#define UUID_SUPPORTED_AUDIO_CTX 0x2BCE
#define UUID_SINK_ASE        0x2BC4
#define UUID_ASE_CONTROL_POINT 0x2BC6
#define UUID_VOLUME_STATE    0x2B7D
#define UUID_VOLUME_CONTROL_PT 0x2B7E
#define UUID_VOLUME_FLAGS    0x2B7F

// Custom audio transport 128-bit UUID
static uint8_t AUDIO_SERVICE_UUID[16] = {
        0xBC,0x9A,0x78,0x56,0x34,0x12, 0x34,0x12,0x34,0x12, 0x12,0x34,0x56,0x78,0x9A,0xBC
};
static uint8_t AUDIO_CHAR_UUID[16] = {
        0xBD,0x9A,0x78,0x56,0x34,0x12, 0x34,0x12,0x34,0x12, 0x12,0x34,0x56,0x78,0x9A,0xBC
};

// Pre-built full Sink PAC record (LC3 codec capabilities baked in)
static const uint8_t sink_pac_full[] = {
        0x01,                           // Num_PAC_Records = 1
            0x06,                           // Coding_Format = LC3
                0x00,0x00,                     // Company_ID
                    0x00,0x00,                     // Vendor_Codec_ID
                        sizeof(lc3_codec_caps),         // Codec_Caps_Length
                            // lc3_codec_caps inline
                            0x03,0x01, 0x80,0x00,          // 48kHz
                                0x02,0x02, 0x02,               // 10ms
                                    0x02,0x03, 0x01,               // Mono
                                        0x05,0x04, 0x78,0x00, 0x78,0x00, // 120 octets
                                            0x02,0x05, 0x01,               // 1 frame/SDU
                                                0x00                            // Metadata_Length = 0
};

// State
static uint8_t  volume_state[3] = {64,0,0};
static uint8_t  volume_flags = 0x00;
static uint16_t avail_audio_ctx = 0x0006;
static uint16_t supp_audio_ctx = 0x0006;
static uint8_t  sink_ase_state = 0x00;

static esp_gatt_if_t gatts_if_saved = ESP_GATT_IF_NONE;
static uint16_t conn_id_saved = 0;
static bool client_connected = false;
static le_audio_audio_rx_cb_t audio_rx_cb = NULL;

// Attribute table
enum {
        IDX_PACS_SVC, IDX_SINK_PAC_DECL, IDX_SINK_PAC_VAL, IDX_SINK_PAC_CCCD,
            IDX_AVAIL_CTX_DECL, IDX_AVAIL_CTX_VAL, IDX_AVAIL_CTX_CCCD,
                IDX_SUPP_CTX_DECL, IDX_SUPP_CTX_VAL,
                    IDX_ASCS_SVC, IDX_SINK_ASE_DECL, IDX_SINK_ASE_VAL, IDX_SINK_ASE_CCCD,
                        IDX_ASE_CTRL_DECL, IDX_ASE_CTRL_VAL,
                            IDX_VCS_SVC, IDX_VOL_STATE_DECL, IDX_VOL_STATE_VAL, IDX_VOL_STATE_CCCD,
                                IDX_VOL_CTRL_DECL, IDX_VOL_CTRL_VAL, IDX_VOL_FLAGS_DECL, IDX_VOL_FLAGS_VAL,
                                    IDX_AUDIO_SVC, IDX_AUDIO_CHAR_DECL, IDX_AUDIO_CHAR_VAL, IDX_AUDIO_CHAR_CCCD,
                                        HANDLE_TABLE_SIZE
};

static uint16_t handle_table[HANDLE_TABLE_SIZE];

static const uint8_t ase_init_val[2] = {0x01, 0x00};

static const esp_gatts_attr_db_t gatt_db[HANDLE_TABLE_SIZE] = {
        [IDX_PACS_SVC] = {{ESP_GATT_AUTO_RSP},{ESP_UUID_LEN_16,(uint8_t*)&(uint16_t){0x2800},ESP_GATT_PERM_READ,sizeof(uint16_t),sizeof(uint16_t),(uint8_t*)&(uint16_t){UUID_PACS_SERVICE}}},
            [IDX_SINK_PAC_DECL] = {{ESP_GATT_AUTO_RSP},{ESP_UUID_LEN_16,(uint8_t*)&(uint16_t){0x2803},ESP_GATT_PERM_READ,0,0,NULL}},
                [IDX_SINK_PAC_VAL] = {{ESP_GATT_AUTO_RSP},{ESP_UUID_LEN_16,(uint8_t*)&(uint16_t){UUID_SINK_PAC},ESP_GATT_PERM_READ,sizeof(sink_pac_full),sizeof(sink_pac_full),(uint8_t*)sink_pac_full}},
                    [IDX_SINK_PAC_CCCD] = {{ESP_GATT_AUTO_RSP},{ESP_UUID_LEN_16,(uint8_t*)&(uint16_t){0x2902},ESP_GATT_PERM_READ|ESP_GATT_PERM_WRITE,sizeof(uint16_t),0,NULL}},
                        [IDX_AVAIL_CTX_DECL] = {{ESP_GATT_AUTO_RSP},{ESP_UUID_LEN_16,(uint8_t*)&(uint16_t){0x2803},ESP_GATT_PERM_READ,0,0,NULL}},
                            [IDX_AVAIL_CTX_VAL] = {{ESP_GATT_AUTO_RSP},{ESP_UUID_LEN_16,(uint8_t*)&(uint16_t){UUID_AVAILABLE_AUDIO_CTX},ESP_GATT_PERM_READ,sizeof(uint32_t),sizeof(uint32_t),(uint8_t*)&avail_audio_ctx}},
                                [IDX_AVAIL_CTX_CCCD] = {{ESP_GATT_AUTO_RSP},{ESP_UUID_LEN_16,(uint8_t*)&(uint16_t){0x2902},ESP_GATT_PERM_READ|ESP_GATT_PERM_WRITE,sizeof(uint16_t),0,NULL}},
                                    [IDX_SUPP_CTX_DECL] = {{ESP_GATT_AUTO_RSP},{ESP_UUID_LEN_16,(uint8_t*)&(uint16_t){0x2803},ESP_GATT_PERM_READ,0,0,NULL}},
                                        [IDX_SUPP_CTX_VAL] = {{ESP_GATT_AUTO_RSP},{ESP_UUID_LEN_16,(uint8_t*)&(uint16_t){UUID_SUPPORTED_AUDIO_CTX},ESP_GATT_PERM_READ,sizeof(uint32_t),sizeof(uint32_t),(uint8_t*)&supp_audio_ctx}},
                                            [IDX_ASCS_SVC] = {{ESP_GATT_AUTO_RSP},{ESP_UUID_LEN_16,(uint8_t*)&(uint16_t){0x2800},ESP_GATT_PERM_READ,sizeof(uint16_t),sizeof(uint16_t),(uint8_t*)&(uint16_t){UUID_ASCS_SERVICE}}},
                                                [IDX_SINK_ASE_DECL] = {{ESP_GATT_AUTO_RSP},{ESP_UUID_LEN_16,(uint8_t*)&(uint16_t){0x2803},ESP_GATT_PERM_READ,0,0,NULL}},
                                                    [IDX_SINK_ASE_VAL] = {{ESP_GATT_AUTO_RSP},{ESP_UUID_LEN_16,(uint8_t*)&(uint16_t){UUID_SINK_ASE},ESP_GATT_PERM_READ,2,2,(uint8_t*)ase_init_val}},
                                                        [IDX_SINK_ASE_CCCD] = {{ESP_GATT_AUTO_RSP},{ESP_UUID_LEN_16,(uint8_t*)&(uint16_t){0x2902},ESP_GATT_PERM_READ|ESP_GATT_PERM_WRITE,sizeof(uint16_t),0,NULL}},
                                                            [IDX_ASE_CTRL_DECL] = {{ESP_GATT_AUTO_RSP},{ESP_UUID_LEN_16,(uint8_t*)&(uint16_t){0x2803},ESP_GATT_PERM_READ,0,0,NULL}},
                                                                [IDX_ASE_CTRL_VAL] = {{ESP_GATT_AUTO_RSP},{ESP_UUID_LEN_16,(uint8_t*)&(uint16_t){UUID_ASE_CONTROL_POINT},ESP_GATT_PERM_WRITE,64,0,NULL}},
                                                                    [IDX_VCS_SVC] = {{ESP_GATT_AUTO_RSP},{ESP_UUID_LEN_16,(uint8_t*)&(uint16_t){0x2800},ESP_GATT_PERM_READ,sizeof(uint16_t),sizeof(uint16_t),(uint8_t*)&(uint16_t){UUID_VCS_SERVICE}}},
                                                                        [IDX_VOL_STATE_DECL] = {{ESP_GATT_AUTO_RSP},{ESP_UUID_LEN_16,(uint8_t*)&(uint16_t){0x2803},ESP_GATT_PERM_READ,0,0,NULL}},
                                                                            [IDX_VOL_STATE_VAL] = {{ESP_GATT_AUTO_RSP},{ESP_UUID_LEN_16,(uint8_t*)&(uint16_t){UUID_VOLUME_STATE},ESP_GATT_PERM_READ,sizeof(volume_state),sizeof(volume_state),volume_state}},
                                                                                [IDX_VOL_STATE_CCCD] = {{ESP_GATT_AUTO_RSP},{ESP_UUID_LEN_16,(uint8_t*)&(uint16_t){0x2902},ESP_GATT_PERM_READ|ESP_GATT_PERM_WRITE,sizeof(uint16_t),0,NULL}},
                                                                                    [IDX_VOL_CTRL_DECL] = {{ESP_GATT_AUTO_RSP},{ESP_UUID_LEN_16,(uint8_t*)&(uint16_t){0x2803},ESP_GATT_PERM_READ,0,0,NULL}},
                                                                                        [IDX_VOL_CTRL_VAL] = {{ESP_GATT_AUTO_RSP},{ESP_UUID_LEN_16,(uint8_t*)&(uint16_t){UUID_VOLUME_CONTROL_PT},ESP_GATT_PERM_WRITE,3,0,NULL}},
                                                                                            [IDX_VOL_FLAGS_DECL] = {{ESP_GATT_AUTO_RSP},{ESP_UUID_LEN_16,(uint8_t*)&(uint16_t){0x2803},ESP_GATT_PERM_READ,0,0,NULL}},
                                                                                                [IDX_VOL_FLAGS_VAL] = {{ESP_GATT_AUTO_RSP},{ESP_UUID_LEN_16,(uint8_t*)&(uint16_t){UUID_VOLUME_FLAGS},ESP_GATT_PERM_READ,sizeof(volume_flags),sizeof(volume_flags),&volume_flags}},
                                                                                                    [IDX_AUDIO_SVC] = {{ESP_GATT_AUTO_RSP},{ESP_UUID_LEN_128,AUDIO_SERVICE_UUID,ESP_GATT_PERM_READ,sizeof(uint16_t),sizeof(uint16_t),(uint8_t*)&(uint16_t){0x2800}}},
                                                                                                        [IDX_AUDIO_CHAR_DECL] = {{ESP_GATT_AUTO_RSP},{ESP_UUID_LEN_16,(uint8_t*)&(uint16_t){0x2803},ESP_GATT_PERM_READ,0,0,NULL}},
                                                                                                            [IDX_AUDIO_CHAR_VAL] = {{ESP_GATT_AUTO_RSP},{ESP_UUID_LEN_128,AUDIO_CHAR_UUID,ESP_GATT_PERM_WRITE|ESP_GATT_PERM_READ,ISO_MAX_SDU_SIZE,0,NULL}},
                                                                                                                [IDX_AUDIO_CHAR_CCCD] = {{ESP_GATT_AUTO_RSP},{ESP_UUID_LEN_16,(uint8_t*)&(uint16_t){0x2902},ESP_GATT_PERM_READ|ESP_GATT_PERM_WRITE,sizeof(uint16_t),0,NULL}},
};

static void send_notification(uint16_t attr_handle, uint8_t *data, uint16_t len) {
        if (!client_connected) return;
            esp_ble_gatts_send_indicate(gatts_if_saved, conn_id_saved, attr_handle, len, data, false);
}

static void handle_ase_control(const uint8_t *data, uint16_t len) {
        if (!data || len<2) return;
            uint8_t op = data[0];
                ESP_LOGI(TAG, "ASE opcode 0x%02X", op);
                    switch (op) {
                                case 0x01: sink_ase_state=0x01; break;
                                                   case 0x02: sink_ase_state=0x02; break;
                                                                      case 0x03: sink_ase_state=0x03; break;
                                                                                         case 0x04: {
                                                                                                                    sink_ase_state=0x04;
                                                                                                                                uint8_t noti[2]={0x01,0x04};
                                                                                                                                            send_notification(handle_table[IDX_SINK_ASE_VAL], noti, 2);
                                                                                                                                                        break;
                                                                                                                                                                }
                                                                                         case 0x08: sink_ase_state=0x00; break;
                                                                                                            default: ESP_LOGW(TAG, "Unknown ASE opcode");
                                                                                                                         }
}

static void handle_vcs_write(const uint8_t *data, uint16_t len) {
        if (!data || len<2 || data[1]!=volume_state[2]) return;
            switch (data[0]) {
                        case 0x01: if(len>=3){ volume_state[0]=data[2]; volume_state[2]++; } break;
                                               case 0x02: volume_state[1]=0; volume_state[2]++; break;
                                                                  case 0x03: volume_state[1]=1; volume_state[2]++; break;
                                                                                 }
                send_notification(handle_table[IDX_VOL_STATE_VAL], volume_state, sizeof(volume_state));
}

static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
        switch (event) {
                    case ESP_GATTS_REG_EVT:
                                    if (param->reg.status == ESP_GATT_OK) {
                                                        gatts_if_saved = gatts_if;
                                                                        esp_ble_gatts_create_attr_tab(gatt_db, gatts_if, HANDLE_TABLE_SIZE, 0);
                                                                                    }
                                                break;
                                                        case ESP_GATTS_CREAT_ATTR_TAB_EVT:
                                                            if (param->add_attr_tab.status == ESP_GATT_OK && param->add_attr_tab.num_handle == HANDLE_TABLE_SIZE) {
                                                                                memcpy(handle_table, param->add_attr_tab.handles, HANDLE_TABLE_SIZE*sizeof(uint16_t));
                                                                                                esp_ble_gatts_start_service(handle_table[IDX_PACS_SVC]);
                                                                                                                esp_ble_gatts_start_service(handle_table[IDX_ASCS_SVC]);
                                                                                                                                esp_ble_gatts_start_service(handle_table[IDX_VCS_SVC]);
                                                                                                                                                esp_ble_gatts_start_service(handle_table[IDX_AUDIO_SVC]);
                                                                                                                                                                ESP_LOGI(TAG, "All services started");
                                                                                                                                                                            }
                                                                        break;
                                                                                case ESP_GATTS_CONNECT_EVT:
                                                                                    conn_id_saved = param->connect.conn_id;
                                                                                                client_connected = true;
                                                                                                            esp_ble_gatt_set_local_mtu(251);
                                                                                                                        break;
                                                                                                                                case ESP_GATTS_DISCONNECT_EVT:
                                                                                                                                    client_connected = false;
                                                                                                                                                sink_ase_state = 0;
                                                                                                                                                            break;
                                                                                                                                                                    case ESP_GATTS_WRITE_EVT: {
                                                                                                                                                                                                              uint16_t h = param->write.handle;
                                                                                                                                                                                                                          if (h == handle_table[IDX_ASE_CTRL_VAL]) handle_ase_control(param->write.value, param->write.len);
                                                                                                                                                                                                                                      else if (h == handle_table[IDX_VOL_CTRL_VAL]) handle_vcs_write(param->write.value, param->write.len);
                                                                                                                                                                                                                                                  else if (h == handle_table[IDX_AUDIO_CHAR_VAL] && audio_rx_cb)
                                                                                                                                                                                                                                                                      audio_rx_cb(conn_id_saved, param->write.value, param->write.len);
                                                                                                                                                                                                                                                              if (param->write.need_rsp) esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
                                                                                                                                                                                                                                                                          break;
                                                                                                                                                                                                                                                                                  }
                                                                                                                                                                                                      default: break;
                                                                                                                                                                                                                   }
}

void le_audio_gatts_register_handler(void) {
        esp_ble_gatts_register_callback(gatts_event_handler);
}
void le_audio_pacs_init(void) { esp_ble_gatts_app_register(0x0001); }
void le_audio_ascs_init(void) {}
void le_audio_vcs_init(void) {}

void le_audio_register_audio_rx_cb(le_audio_audio_rx_cb_t cb) { audio_rx_cb = cb; }

void le_audio_gatt_send_audio_frame(uint16_t conn_handle, const uint8_t *data, uint16_t length) {
        if (!client_connected) return;
            esp_ble_gatts_send_indicate(gatts_if_saved, conn_id_saved, handle_table[IDX_AUDIO_CHAR_VAL], length, (uint8_t*)data, false);
}
void le_audio_vcs_update_volume(uint8_t vol) { volume_state[0]=vol; volume_state[2]++; send_notification(handle_table[IDX_VOL_STATE_VAL],volume_state,sizeof(volume_state)); }
uint8_t le_audio_vcs_get_volume(void) { return volume_state[0]; 
