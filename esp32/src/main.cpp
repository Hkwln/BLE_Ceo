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

// ============================================================================
// I2S SETUP
// ============================================================================

void setupI2S() {
  Serial.println("\n[I2S] Initializing...");
  
  // LEFT CHANNEL
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
    Serial.printf("[I2S] LEFT failed: %d\n", err);
    return;
  }

  i2s_pin_config_t pin_config_left = {
    .bck_io_num = I2S_BCK_LEFT,
    .ws_io_num = I2S_WS_LEFT,
    .data_out_num = I2S_DIN_LEFT,
    .data_in_num = -1,
  };

  err = i2s_set_pin(I2S_NUM_0, &pin_config_left);
  if (err == ESP_OK) {
    Serial.println("[I2S] LEFT OK");
  }

  // RIGHT CHANNEL
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
    Serial.printf("[I2S] RIGHT failed: %d\n", err);
    return;
  }

  i2s_pin_config_t pin_config_right = {
    .bck_io_num = I2S_BCK_RIGHT,
    .ws_io_num = I2S_WS_RIGHT,
    .data_out_num = I2S_DIN_RIGHT,
    .data_in_num = -1,
  };

  err = i2s_set_pin(I2S_NUM_1, &pin_config_right);
  if (err == ESP_OK) {
    Serial.println("[I2S] RIGHT OK");
  }

  Serial.println("[I2S] Ready!\n");
}

// ============================================================================
// A2DP AUDIO CALLBACKS
// ============================================================================

void audio_data_callback(const uint8_t *data, uint32_t len) {
  audioChunksReceived++;
  
  if (len > 0) {
    size_t bytesWritten = 0;
    
    // Write to LEFT channel
    i2s_write(I2S_NUM_0, data, len, &bytesWritten, 0);
    
    // Write to RIGHT channel
    i2s_write(I2S_NUM_1, data, len, &bytesWritten, 0);
    
    audioChunksPlayed++;
    
    if (audioChunksReceived % 100 == 0) {
      Serial.printf("[A2DP] Recv: %lu, Play: %lu\n", 
                    audioChunksReceived, audioChunksPlayed);
    }
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
  Serial.println("║  BLE HEADPHONES - A2DP SINK v1.0      ║");
  Serial.println("║  Seeed XIAO ESP32-S3 + MAX98357A x2   ║");
  Serial.println("╚════════════════════════════════════════╝\n");

  // Setup I2S
  setupI2S();

  // Initialize A2DP Sink
  Serial.println("[A2DP] Initializing A2DP Sink...");
  
  a2dp_sink.set_auto_reconnect(true);
  a2dp_sink.set_avrc_controls(true);
  a2dp_sink.set_on_data_received_callback(audio_data_callback);
  
  // Start with device name "BLE-Headphones"
  a2dp_sink.start("BLE-Headphones");
  
  Serial.println("[A2DP] ✅ A2DP Sink started!");
  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║  Ready for Bluetooth Audio!            ║");
  Serial.println("║  Search for 'BLE-Headphones' on phone ║");
  Serial.println("║  Connect + Play YouTube!              ║");
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
      Serial.printf("[Status] Connected - Recv: %lu, Play: %lu\n", 
                    audioChunksReceived, audioChunksPlayed);
    } else {
      Serial.println("[Status] Waiting for connection...");
    }
  }

  delay(100);
}
