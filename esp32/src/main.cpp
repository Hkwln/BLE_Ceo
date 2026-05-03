#include <Arduino.h>
#include <esp_a2dp_api.h>
#include <esp_avrc_api.h>
#include <esp_gap_bt_api.h>
#include <driver/i2s.h>

// ============================================================================
// PIN DEFINITIONS - Seeed XIAO ESP32-S3 + MAX98357A (x2 for stereo)
// ============================================================================

#define I2S_BCK_LEFT   GPIO_NUM_9
#define I2S_WS_LEFT    GPIO_NUM_8
#define I2S_DIN_LEFT   GPIO_NUM_10

#define I2S_BCK_RIGHT  GPIO_NUM_6
#define I2S_WS_RIGHT   GPIO_NUM_7
#define I2S_DIN_RIGHT  GPIO_NUM_5

// ============================================================================
// GLOBALS
// ============================================================================
volatile uint32_t audioChunksReceived = 0;
volatile uint32_t audioChunksPlayed = 0;
volatile bool btConnected = false;

// ============================================================================
// I2S SETUP
// ============================================================================

void setupI2S() {
  Serial.println("\n[I2S] Initializing stereo I2S...");
  
  // LEFT CHANNEL (I2S_NUM_0)
  i2s_config_t i2s_config_left = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = 44100,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_STAND_I2S),
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = 512,
    .use_apll = false,
  };

  esp_err_t err = i2s_driver_install(I2S_NUM_0, &i2s_config_left, 0, NULL);
  if (err != ESP_OK) {
    Serial.printf("[I2S] LEFT driver failed: %d\n", err);
    return;
  }

  i2s_pin_config_t pin_config_left = {
    .bck_io_num = I2S_BCK_LEFT,
    .ws_io_num = I2S_WS_LEFT,
    .data_out_num = I2S_DIN_LEFT,
    .data_in_num = -1,
  };

  err = i2s_set_pin(I2S_NUM_0, &pin_config_left);
  if (err != ESP_OK) {
    Serial.printf("[I2S] LEFT pins failed: %d\n", err);
    return;
  }

  // RIGHT CHANNEL (I2S_NUM_1)
  i2s_config_t i2s_config_right = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = 44100,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,
    .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_STAND_I2S),
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = 512,
    .use_apll = false,
  };

  err = i2s_driver_install(I2S_NUM_1, &i2s_config_right, 0, NULL);
  if (err != ESP_OK) {
    Serial.printf("[I2S] RIGHT driver failed: %d\n", err);
    return;
  }

  i2s_pin_config_t pin_config_right = {
    .bck_io_num = I2S_BCK_RIGHT,
    .ws_io_num = I2S_WS_RIGHT,
    .data_out_num = I2S_DIN_RIGHT,
    .data_in_num = -1,
  };

  err = i2s_set_pin(I2S_NUM_1, &pin_config_right);
  if (err != ESP_OK) {
    Serial.printf("[I2S] RIGHT pins failed: %d\n", err);
    return;
  }

  Serial.println("[I2S] ✅ Stereo I2S ready\n");
}

// ============================================================================
// BLUETOOTH A2DP CALLBACKS
// ============================================================================

void bt_app_a2d_cb(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param) {
  switch (event) {
    case ESP_A2D_CONNECTION_STATE_EVT:
      if (param->conn_stat.state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
        Serial.println("[BT] ✅ A2DP CONNECTED!");
        btConnected = true;
        audioChunksReceived = 0;
        audioChunksPlayed = 0;
      } else if (param->conn_stat.state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
        Serial.println("[BT] ❌ A2DP DISCONNECTED");
        btConnected = false;
      }
      break;

    case ESP_A2D_AUDIO_STATE_EVT:
      Serial.printf("[BT] Audio state: %d\n", param->audio_stat.state);
      break;

    case ESP_A2D_AUDIO_CFG_EVT:
      Serial.printf("[BT] Audio config: sr=%d, ch=%d\n", 
                    param->audio_cfg.mcc.cie.samp_freq,
                    param->audio_cfg.mcc.cie.ch_mode);
      break;

    default:
      break;
  }
}

void bt_app_a2d_data_cb(const uint8_t *data, uint32_t len) {
  if (data == nullptr || len == 0) return;

  audioChunksReceived++;

  if (btConnected) {
    size_t bytesWritten = 0;
    i2s_write(I2S_NUM_0, (void*)data, len, &bytesWritten, 0);
    i2s_write(I2S_NUM_1, (void*)data, len, &bytesWritten, 0);
    audioChunksPlayed++;

    if (audioChunksReceived % 100 == 0) {
      Serial.printf("[Audio] Recv: %lu, Played: %lu\n", 
                    audioChunksReceived, audioChunksPlayed);
    }
  }
}

// ============================================================================
// BLUETOOTH GAP CALLBACKS
// ============================================================================

void bt_app_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param) {
  switch (event) {
    case ESP_BT_GAP_AUTH_CMPL_EVT:
      if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS) {
        Serial.println("[GAP] ✅ Pairing complete");
      } else {
        Serial.printf("[GAP] Pairing failed: %d\n", param->auth_cmpl.stat);
      }
      break;

    case ESP_BT_GAP_PIN_REQ_EVT:
      Serial.println("[GAP] PIN request received");
      esp_bt_pin_code_t pin_code = {0, 0, 0, 0};
      esp_bt_gap_pin_reply(param->pin_req.bda, true, 4, pin_code);
      break;

    case ESP_BT_GAP_MODE_CHG_EVT:
      Serial.printf("[GAP] Mode changed: %d\n", param->mode_chg.mode);
      break;

    default:
      break;
  }
}

// ============================================================================
// SETUP
// ============================================================================

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("\n\n");
  Serial.println("╔════════════════════════════════════════╗");
  Serial.println("║  BLE HEADPHONES - A2DP SINK v5.1      ║");
  Serial.println("║  Bluetooth Audio Device                ║");
  Serial.println("║  Seeed XIAO ESP32-S3 + MAX98357A x2   ║");
  Serial.println("╚════════════════════════════════════════╝\n");

  // Setup I2S
  setupI2S();

  // Initialize Bluetooth
  Serial.println("[BT] Initializing Classic Bluetooth A2DP...");
  
  // Initialize Bluetooth Controller
  esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
  esp_bt_controller_init(&bt_cfg);
  esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT);
  
  // Initialize Bluedroid
  esp_bluedroid_init();
  esp_bluedroid_enable();
  
  // Register GAP callback
  esp_bt_gap_register_callback(bt_app_gap_cb);
  
  // Register A2DP callback
  esp_a2d_register_callback(bt_app_a2d_cb);
  esp_a2d_sink_register_data_callback(bt_app_a2d_data_cb);
  
  // Initialize A2DP Sink
  esp_a2d_sink_init();
  
  // Set local Bluetooth name (shows in paired devices)
  esp_bt_dev_set_device_name("BLE-Headphones");
  
  // Make device discoverable and connectable
  esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
  
  // Set Pin (for pairing) - usually 0000
  esp_bt_pin_code_t pin_code = {0x30, 0x30, 0x30, 0x30};  // "0000"
  esp_bt_gap_set_pin(ESP_BT_PIN_TYPE_FIXED, 4, pin_code);
  
  Serial.println("[BT] ✅ A2DP Sink initialized!");
  Serial.println("[BT] ✅ Device discoverable and connectable!");
  
  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║  📱 On your phone:                     ║");
  Serial.println("║  1. Bluetooth Settings                 ║");
  Serial.println("║  2. Search for 'BLE-Headphones'       ║");
  Serial.println("║  3. Pair (PIN: 0000)                  ║");
  Serial.println("║  4. Play YouTube → Audio streams!     ║");
  Serial.println("╚════════════════════════════════════════╝\n");
}

// ============================================================================
// LOOP
// ============================================================================

void loop() {
  static unsigned long lastStatus = 0;
  
  if (millis() - lastStatus > 5000) {
    lastStatus = millis();
    
    if (btConnected) {
      Serial.printf("[Status] ✅ CONNECTED - Recv: %lu, Played: %lu\n", 
                    audioChunksReceived, audioChunksPlayed);
    } else {
      Serial.println("[Status] ⏳ Waiting for connection... (Search 'BLE-Headphones')");
    }
  }

  delay(100);
}
