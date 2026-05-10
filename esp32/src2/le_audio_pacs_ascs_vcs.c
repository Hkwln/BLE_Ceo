#include "le_audio_pacs_ascs_vcs.h"
#include "esp_gap_ble_api.h"
#include "esp_gatt_common_api.h"
#include "esp_gatts_api.h"
#include "esp_log.h"
#include "le_audio_iso.h" // ISO_MAX_SDU_SIZE
#include <stdbool.h>
#include <string.h>

static const char *TAG = "GATT";

// ── SIG UUIDs ───────────────────────────────────────────
#define UUID16(x) ((uint16_t)(x))

#define SVC_PRIMARY 0x2800
#define CHAR_DECL 0x2803
#define CCCD 0x2902

#define UUID_PACS 0x1850
#define UUID_ASCS 0x184E
#define UUID_VCS 0x1844

#define UUID_SINK_PAC 0x2BC9
#define UUID_AVAIL_CTX 0x2BCD
#define UUID_SUPP_CTX 0x2BCE
#define UUID_SINK_ASE 0x2BC4
#define UUID_ASE_CP 0x2BC6
#define UUID_VOL_STATE 0x2B7D
#define UUID_VOL_CP 0x2B7E
#define UUID_VOL_FLAGS 0x2B7F

// ── 128-bit Custom Audio UUIDs ──────────────────────────
static uint8_t AUDIO_SVC_UUID[16] = {0xBC, 0x9A, 0x78, 0x56, 0x34, 0x12,
                                     0x34, 0x12, 0x34, 0x12, 0x12, 0x34,
                                     0x56, 0x78, 0x9A, 0xBC};
static uint8_t AUDIO_CHAR_UUID[16] = {0xBD, 0x9A, 0x78, 0x56, 0x34, 0x12,
                                      0x34, 0x12, 0x34, 0x12, 0x12, 0x34,
                                      0x56, 0x78, 0x9A, 0xBC};

// ── Sink PAC value (fully pre-built, no sizeof references) ─
// Format: [Num_Records][Coding_Format][Company_ID 2B]
//         [Vendor_ID 2B][Caps_Len][Caps...][Meta_Len]
static const uint8_t sink_pac_val[] = {
    0x01, // Num_PAC_Records = 1
    0x06, // LC3
    0x00,
    0x00, // Company_ID (N/A)
    0x00,
    0x00, // Vendor_Codec_ID (N/A)
    // Codec_Specific_Capabilities_Length = 16 bytes
    0x10,
    // LTV: Sampling Frequencies  (type=0x01, len=3)
    0x03,
    0x01,
    0x80,
    0x00, // bit7 = 48 kHz
    // LTV: Frame Durations        (type=0x02, len=2)
    0x02,
    0x02,
    0x02, // bit1 = 10 ms
    // LTV: Audio Channel Counts   (type=0x03, len=2)
    0x02,
    0x03,
    0x01, // bit0 = 1 channel
    // LTV: Octets/Frame           (type=0x04, len=5)
    0x05,
    0x04,
    0x78,
    0x00,
    0x78,
    0x00, // min=120 max=120
    // Metadata_Length = 0
    0x00,
};

// ── ASE initial value: [ASE_ID=1, State=Idle=0x00] ──────
static const uint8_t ase_init_val[2] = {0x01, 0x00};

// ── Service state ────────────────────────────────────────
static uint8_t vol_state[3] = {64, 0, 0}; // vol, mute, counter
static uint8_t vol_flags = 0x00;
static uint16_t avail_ctx = 0x0006; // Conversational|Media
static uint16_t supp_ctx = 0x0006;
static uint8_t sink_ase_state = 0x00;

// ── GATT connection ──────────────────────────────────────
static esp_gatt_if_t gatt_if = ESP_GATT_IF_NONE;
static uint16_t conn_id = 0;
static bool connected = false;

// ── User callbacks ───────────────────────────────────────
static le_audio_audio_rx_cb_t audio_rx_cb = NULL;
static le_audio_conn_cb_t conn_cb = NULL;

// ── Attribute index enum ─────────────────────────────────
enum {
  /* PACS */
  IDX_PACS_SVC,
  IDX_SINK_PAC_DECL,
  IDX_SINK_PAC_VAL,
  IDX_SINK_PAC_CCCD,
  IDX_AVAIL_CTX_DECL,
  IDX_AVAIL_CTX_VAL,
  IDX_AVAIL_CTX_CCCD,
  IDX_SUPP_CTX_DECL,
  IDX_SUPP_CTX_VAL,
  /* ASCS */
  IDX_ASCS_SVC,
  IDX_SINK_ASE_DECL,
  IDX_SINK_ASE_VAL,
  IDX_SINK_ASE_CCCD,
  IDX_ASE_CP_DECL,
  IDX_ASE_CP_VAL,
  /* VCS */
  IDX_VCS_SVC,
  IDX_VOL_STATE_DECL,
  IDX_VOL_STATE_VAL,
  IDX_VOL_STATE_CCCD,
  IDX_VOL_CP_DECL,
  IDX_VOL_CP_VAL,
  IDX_VOL_FLAGS_DECL,
  IDX_VOL_FLAGS_VAL,
  /* Custom Audio */
  IDX_AUDIO_SVC,
  IDX_AUDIO_CHAR_DECL,
  IDX_AUDIO_CHAR_VAL,
  IDX_AUDIO_CHAR_CCCD,
  HANDLE_TABLE_SIZE
};

static uint16_t htable[HANDLE_TABLE_SIZE];

// ── Convenience macro ────────────────────────────────────
#define U16PTR(v) ((uint8_t *)&(uint16_t){(v)})

// ── GATT DB ──────────────────────────────────────────────
static const esp_gatts_attr_db_t gatt_db[HANDLE_TABLE_SIZE] = {

    /* ── PACS ───────────────────────────────────────── */
    [IDX_PACS_SVC] = {{ESP_GATT_AUTO_RSP},
                      {ESP_UUID_LEN_16, U16PTR(SVC_PRIMARY), ESP_GATT_PERM_READ,
                       2, 2, U16PTR(UUID_PACS)}},
    [IDX_SINK_PAC_DECL] = {{ESP_GATT_AUTO_RSP},
                           {ESP_UUID_LEN_16, U16PTR(CHAR_DECL),
                            ESP_GATT_PERM_READ, 0, 0, NULL}},
    [IDX_SINK_PAC_VAL] = {{ESP_GATT_AUTO_RSP},
                          {ESP_UUID_LEN_16, U16PTR(UUID_SINK_PAC),
                           ESP_GATT_PERM_READ, sizeof(sink_pac_val),
                           sizeof(sink_pac_val), (uint8_t *)sink_pac_val}},
    [IDX_SINK_PAC_CCCD] = {{ESP_GATT_AUTO_RSP},
                           {ESP_UUID_LEN_16, U16PTR(CCCD),
                            ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, 2, 0,
                            NULL}},
    [IDX_AVAIL_CTX_DECL] = {{ESP_GATT_AUTO_RSP},
                            {ESP_UUID_LEN_16, U16PTR(CHAR_DECL),
                             ESP_GATT_PERM_READ, 0, 0, NULL}},
    [IDX_AVAIL_CTX_VAL] = {{ESP_GATT_AUTO_RSP},
                           {ESP_UUID_LEN_16, U16PTR(UUID_AVAIL_CTX),
                            ESP_GATT_PERM_READ, 4, 4, (uint8_t *)&avail_ctx}},
    [IDX_AVAIL_CTX_CCCD] = {{ESP_GATT_AUTO_RSP},
                            {ESP_UUID_LEN_16, U16PTR(CCCD),
                             ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, 2, 0,
                             NULL}},
    [IDX_SUPP_CTX_DECL] = {{ESP_GATT_AUTO_RSP},
                           {ESP_UUID_LEN_16, U16PTR(CHAR_DECL),
                            ESP_GATT_PERM_READ, 0, 0, NULL}},
    [IDX_SUPP_CTX_VAL] = {{ESP_GATT_AUTO_RSP},
                          {ESP_UUID_LEN_16, U16PTR(UUID_SUPP_CTX),
                           ESP_GATT_PERM_READ, 4, 4, (uint8_t *)&supp_ctx}},

    /* ── ASCS ───────────────────────────────────────── */
    [IDX_ASCS_SVC] = {{ESP_GATT_AUTO_RSP},
                      {ESP_UUID_LEN_16, U16PTR(SVC_PRIMARY), ESP_GATT_PERM_READ,
                       2, 2, U16PTR(UUID_ASCS)}},
    [IDX_SINK_ASE_DECL] = {{ESP_GATT_AUTO_RSP},
                           {ESP_UUID_LEN_16, U16PTR(CHAR_DECL),
                            ESP_GATT_PERM_READ, 0, 0, NULL}},
    [IDX_SINK_ASE_VAL] = {{ESP_GATT_AUTO_RSP},
                          {ESP_UUID_LEN_16, U16PTR(UUID_SINK_ASE),
                           ESP_GATT_PERM_READ, 2, 2, (uint8_t *)ase_init_val}},
    [IDX_SINK_ASE_CCCD] = {{ESP_GATT_AUTO_RSP},
                           {ESP_UUID_LEN_16, U16PTR(CCCD),
                            ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, 2, 0,
                            NULL}},
    [IDX_ASE_CP_DECL] = {{ESP_GATT_AUTO_RSP},
                         {ESP_UUID_LEN_16, U16PTR(CHAR_DECL),
                          ESP_GATT_PERM_READ, 0, 0, NULL}},
    [IDX_ASE_CP_VAL] = {{ESP_GATT_AUTO_RSP},
                        {ESP_UUID_LEN_16, U16PTR(UUID_ASE_CP),
                         ESP_GATT_PERM_WRITE, 64, 0, NULL}},

    /* ── VCS ────────────────────────────────────────── */
    [IDX_VCS_SVC] = {{ESP_GATT_AUTO_RSP},
                     {ESP_UUID_LEN_16, U16PTR(SVC_PRIMARY), ESP_GATT_PERM_READ,
                      2, 2, U16PTR(UUID_VCS)}},
    [IDX_VOL_STATE_DECL] = {{ESP_GATT_AUTO_RSP},
                            {ESP_UUID_LEN_16, U16PTR(CHAR_DECL),
                             ESP_GATT_PERM_READ, 0, 0, NULL}},
    [IDX_VOL_STATE_VAL] = {{ESP_GATT_AUTO_RSP},
                           {ESP_UUID_LEN_16, U16PTR(UUID_VOL_STATE),
                            ESP_GATT_PERM_READ, 3, 3, vol_state}},
    [IDX_VOL_STATE_CCCD] = {{ESP_GATT_AUTO_RSP},
                            {ESP_UUID_LEN_16, U16PTR(CCCD),
                             ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, 2, 0,
                             NULL}},
    [IDX_VOL_CP_DECL] = {{ESP_GATT_AUTO_RSP},
                         {ESP_UUID_LEN_16, U16PTR(CHAR_DECL),
                          ESP_GATT_PERM_READ, 0, 0, NULL}},
    [IDX_VOL_CP_VAL] = {{ESP_GATT_AUTO_RSP},
                        {ESP_UUID_LEN_16, U16PTR(UUID_VOL_CP),
                         ESP_GATT_PERM_WRITE, 3, 0, NULL}},
    [IDX_VOL_FLAGS_DECL] = {{ESP_GATT_AUTO_RSP},
                            {ESP_UUID_LEN_16, U16PTR(CHAR_DECL),
                             ESP_GATT_PERM_READ, 0, 0, NULL}},
    [IDX_VOL_FLAGS_VAL] = {{ESP_GATT_AUTO_RSP},
                           {ESP_UUID_LEN_16, U16PTR(UUID_VOL_FLAGS),
                            ESP_GATT_PERM_READ, 1, 1, &vol_flags}},

    /* ── Custom Audio Transport ─────────────────────── */
    [IDX_AUDIO_SVC] = {{ESP_GATT_AUTO_RSP},
                       {ESP_UUID_LEN_128, AUDIO_SVC_UUID, ESP_GATT_PERM_READ, 2,
                        2, U16PTR(SVC_PRIMARY)}},
    [IDX_AUDIO_CHAR_DECL] = {{ESP_GATT_AUTO_RSP},
                             {ESP_UUID_LEN_16, U16PTR(CHAR_DECL),
                              ESP_GATT_PERM_READ, 0, 0, NULL}},
    [IDX_AUDIO_CHAR_VAL] = {{ESP_GATT_AUTO_RSP},
                            {ESP_UUID_LEN_128, AUDIO_CHAR_UUID,
                             ESP_GATT_PERM_WRITE | ESP_GATT_PERM_READ,
                             ISO_MAX_SDU_SIZE, 0, NULL}},
    [IDX_AUDIO_CHAR_CCCD] = {{ESP_GATT_AUTO_RSP},
                             {ESP_UUID_LEN_16, U16PTR(CCCD),
                              ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, 2, 0,
                              NULL}},
};

// ── Notification helper ──────────────────────────────────
static void notify(uint16_t handle, uint8_t *data, uint16_t len) {
  if (!connected)
    return;
  esp_ble_gatts_send_indicate(gatt_if, conn_id, handle, len, data, false);
}

// ── ASE Control Point ────────────────────────────────────
static void on_ase_cp(const uint8_t *data, uint16_t len) {
  if (!data || len < 2)
    return;
  uint8_t op = data[0];
  ESP_LOGI(TAG, "ASE CP opcode=0x%02X", op);
  switch (op) {
  case 0x01:
    sink_ase_state = 0x01;
    break; // Codec Configured
  case 0x02:
    sink_ase_state = 0x02;
    break; // QoS Configured
  case 0x03:
    sink_ase_state = 0x03;
    break;     // Enabling
  case 0x04: { // Receiver Start Ready
    sink_ase_state = 0x04;
    uint8_t n[2] = {0x01, 0x04};
    notify(htable[IDX_SINK_ASE_VAL], n, 2);
    break;
  }
  case 0x08:
    sink_ase_state = 0x00;
    break; // Release
  default:
    ESP_LOGW(TAG, "Unknown ASE opcode 0x%02X", op);
  }
}

// ── Volume Control Point ─────────────────────────────────
static void on_vol_cp(const uint8_t *data, uint16_t len) {
  if (!data || len < 2)
    return;
  uint8_t expected = (uint8_t)(vol_state[2] + 1);
  if (data[1] != expected) {
    ESP_LOGW(TAG, "VCS bad counter %u expected %u", data[1], expected);
    return;
  }
  switch (data[0]) {
  case 0x01:
    if (len >= 3) {
      vol_state[0] = data[2];
      vol_state[2]++;
    }
    break;
  case 0x02:
    vol_state[1] = 0;
    vol_state[2]++;
    break;
  case 0x03:
    vol_state[1] = 1;
    vol_state[2]++;
    break;
  default:
    return;
  }
  notify(htable[IDX_VOL_STATE_VAL], vol_state, 3);
  ESP_LOGI(TAG, "VCS: vol=%u mute=%u", vol_state[0], vol_state[1]);
}

// ── GATT event handler ───────────────────────────────────
static void gatts_cb(esp_gatts_cb_event_t event, esp_gatt_if_t gi,
                     esp_ble_gatts_cb_param_t *p) {
  switch (event) {

  case ESP_GATTS_REG_EVT:
    if (p->reg.status == ESP_GATT_OK) {
      gatt_if = gi;
      ESP_LOGI(TAG, "GATT app registered if=%d", gi);
      esp_ble_gatts_create_attr_tab(gatt_db, gi, HANDLE_TABLE_SIZE, 0);
    } else {
      ESP_LOGE(TAG, "GATT reg failed %u", p->reg.status);
    }
    break;

  case ESP_GATTS_CREAT_ATTR_TAB_EVT:
    if (p->add_attr_tab.status == ESP_GATT_OK) {
      if (p->add_attr_tab.num_handle == HANDLE_TABLE_SIZE) {
        memcpy(htable, p->add_attr_tab.handles,
               HANDLE_TABLE_SIZE * sizeof(uint16_t));
        esp_ble_gatts_start_service(htable[IDX_PACS_SVC]);
        esp_ble_gatts_start_service(htable[IDX_ASCS_SVC]);
        esp_ble_gatts_start_service(htable[IDX_VCS_SVC]);
        esp_ble_gatts_start_service(htable[IDX_AUDIO_SVC]);
        ESP_LOGI(TAG, "✓ All services started");
        ESP_LOGI(TAG,
                 "  PACS=0x%04X ASCS=0x%04X "
                 "VCS=0x%04X AUDIO=0x%04X",
                 htable[IDX_PACS_SVC], htable[IDX_ASCS_SVC],
                 htable[IDX_VCS_SVC], htable[IDX_AUDIO_SVC]);
      } else {
        ESP_LOGE(TAG, "Handle count wrong: got %u want %u",
                 p->add_attr_tab.num_handle, HANDLE_TABLE_SIZE);
      }
    }
    break;

  case ESP_GATTS_CONNECT_EVT:
    conn_id = p->connect.conn_id;
    connected = true;
    ESP_LOGI(TAG, "Client connected conn=%u", conn_id);
    // Request 251-byte MTU → 247-byte payload > 120-byte LC3 frame
    esp_ble_gatt_set_local_mtu(251);
    // Tighten connection interval for low-latency audio
    {
      esp_ble_conn_update_params_t cp;
      memcpy(cp.bda, p->connect.remote_bda, 6);
      cp.min_int = 6; // 7.5 ms
      cp.max_int = 8; // 10 ms
      cp.latency = 0;
      cp.timeout = 200; // 2 s supervision
      esp_ble_gap_update_conn_params(&cp);
    }
    if (conn_cb)
      conn_cb(conn_id, true);
    break;

  case ESP_GATTS_DISCONNECT_EVT:
    connected = false;
    sink_ase_state = 0x00;
    ESP_LOGI(TAG, "Client disconnected");
    if (conn_cb)
      conn_cb(0, false);
    break;

  case ESP_GATTS_MTU_EVT:
    ESP_LOGI(TAG, "MTU=%u (payload=%u)", p->mtu.mtu, p->mtu.mtu - 3);
    break;

  case ESP_GATTS_WRITE_EVT: {
    uint16_t h = p->write.handle;
    if (h == htable[IDX_ASE_CP_VAL])
      on_ase_cp(p->write.value, p->write.len);
    else if (h == htable[IDX_VOL_CP_VAL])
      on_vol_cp(p->write.value, p->write.len);
    else if (h == htable[IDX_AUDIO_CHAR_VAL] && audio_rx_cb)
      audio_rx_cb(conn_id, p->write.value, p->write.len);

    if (p->write.need_rsp)
      esp_ble_gatts_send_response(gi, p->write.conn_id, p->write.trans_id,
                                  ESP_GATT_OK, NULL);
    break;
  }

  default:
    break;
  }
}

// ── Public API ───────────────────────────────────────────
void le_audio_gatts_register_handler(void) {
  esp_ble_gatts_register_callback(gatts_cb);
  ESP_LOGI(TAG, "GATT handler registered");
}
void le_audio_pacs_init(void) {
  esp_ble_gatts_app_register(0x0001);
  ESP_LOGI(TAG, "Registering PACS/ASCS/VCS/AUDIO...");
}
void le_audio_ascs_init(void) {}
void le_audio_vcs_init(void) {}

void le_audio_register_audio_rx_cb(le_audio_audio_rx_cb_t cb) {
  audio_rx_cb = cb;
}
void le_audio_register_conn_cb(le_audio_conn_cb_t cb) { conn_cb = cb; }
void le_audio_gatt_send_audio_frame(uint16_t h, const uint8_t *data,
                                    uint16_t length) {
  (void)h; // single-connection: use static conn_id
  if (!connected || !data || length == 0)
    return;
  esp_ble_gatts_send_indicate(gatt_if, conn_id, htable[IDX_AUDIO_CHAR_VAL],
                              length, (uint8_t *)data, false);
}
void le_audio_vcs_update_volume(uint8_t v) {
  vol_state[0] = v;
  vol_state[2]++;
  notify(htable[IDX_VOL_STATE_VAL], vol_state, 3);
}
uint8_t le_audio_vcs_get_volume(void) { return vol_state[0]; }
