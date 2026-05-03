#include <Arduino.h>
#include "BluetoothA2DPSink.h"
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
BluetoothA2DPSink a2dp_sink;

volatile uint32_t audioChunksReceived = 0;
volatile uint32_t audioChunksPlayed = 0;
volatile uint32_t audioErrors = 0;

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
  Serial.println("[I2S] LEFT driver installed");

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
  Serial.println("[I2S] LEFT pins configured");

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
  Serial.println("[I2S] RIGHT driver installed");

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
  Serial.println("[I2S] RIGHT pins configured");

  Serial.println("[I2S] ✅ Stereo I2S ready (44.1kHz, 16-bit, stereo)\n");
}

// ============================================================================
// A2DP AUDIO CALLBACK
// ============================================================================

void audio_data_callback(const uint8_t *data, uint32_t len) {
  if (data == nullptr || len == 0) {
    return;
  }

  audioChunksReceived++;
  
  size_t bytesWritten = 0;
  
  // Write to LEFT channel (I2S_NUM_0)
  esp_err_t err1 = i2s_write(I2S_NUM_0, (void*)data, len, &bytesWritten, 0);
  
  // Write to RIGHT channel (I2S_NUM_1) - duplicate for stereo
  esp_err_t err2 = i2s_write(I2S_NUM_1, (void*)data, len, &bytesWritten, 0);
  
  if (err1 == ESP_OK && err2 == ESP_OK) {
    audioChunksPlayed++;
  } else {
    audioErrors++;
    if (audioErrors % 10 == 0) {
      Serial.printf("[I2S] Errors: %lu (L:%d R:%d)\n", audioErrors, err1, err2);
    }
  }
  
  // Log every 100 chunks
  if (audioChunksReceived % 100 == 0) {
    Serial.printf("[A2DP] Recv: %lu, Played: %lu, Errors: %lu\n", 
                  audioChunksReceived, audioChunksPlayed, audioErrors);
  }
}

// ============================================================================
// A2DP CONNECTION CALLBACKS
// ============================================================================

void avrc_metadata_callback(uint8_t data1, const char *data2) {
  Serial.printf("[AVRC] %d: %s\n", data1, data2);
}

void connection_state_changed(esp_a2d_connection_state_t state, void *param) {
  if (state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
    Serial.println("[A2DP] ✅ CONNECTED!");
    audioChunksReceived = 0;
    audioChunksPlayed = 0;
    audioErrors = 0;
  } else if (state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
    Serial.println("[A2DP] ❌ DISCONNECTED");
  } else {
    Serial.printf("[A2DP] State changed: %d\n", state);
  }
}

void audio_state_changed(esp_a2d_audio_state_t state, void *param) {
  Serial.printf("[A2DP] Audio state: %d\n", state);
}

// ============================================================================
// SETUP
// ============================================================================

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("\n\n");
  Serial.println("╔════════════════════════════════════════╗");
  Serial.println("║  BLE HEADPHONES - A2DP SINK v4.1      ║");
  Serial.println("║  Seeed XIAO ESP32-S3 + MAX98357A x2   ║");
  Serial.println("╚════════════════════════════════════════╝\n");

  // Setup I2S first
  setupI2S();

  // Initialize A2DP Sink
  Serial.println("[A2DP] Initializing A2DP Sink...\n");
  
  // Configure A2DP
  a2dp_sink.set_auto_reconnect(true);
  a2dp_sink.set_avrc_controls(true);
  a2dp_sink.set_avrc_metadata_attribute_mask(ESP_AVRC_MD_ATTR_TITLE | 
                                              ESP_AVRC_MD_ATTR_ARTIST | 
                                              ESP_AVRC_MD_ATTR_ALBUM);
  a2dp_sink.set_on_data_received_callback(audio_data_callback);
  a2dp_sink.set_on_connection_state_changed(connection_state_changed);
  a2dp_sink.set_on_audio_state_changed(audio_state_changed);
  a2dp_sink.set_on_avrc_metadata_callback(avrc_metadata_callback);
  
  // Start with device name "BLE-Headphones"
  if (a2dp_sink.start("BLE-Headphones")) {
    Serial.println("[A2DP] ✅ A2DP Sink started successfully!");
  } else {
    Serial.println("[A2DP] ❌ Failed to start A2DP Sink!");
  }
  
  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║  📱 Search: 'BLE-Headphones'          ║");
  Serial.println("║  🔗 Connect                            ║");
  Serial.println("║  🎵 Play YouTube → Audio streams!     ║");
  Serial.println("╚════════════════════════════════════════╝\n");
}

// ============================================================================
// LOOP
// ============================================================================

void loop() {
  static unsigned long lastStatus = 0;
  
  if (millis() - lastStatus > 5000) {
    lastStatus = millis();
    
    if (a2dp_sink.is_connected()) {
      Serial.printf("[Status] ✅ Connected - Recv: %lu, Played: %lu\n", 
                    audioChunksReceived, audioChunksPlayed);
    } else {
      Serial.println("[Status] ⏳ Waiting for connection...");
    }
  }

  delay(100);
}
