#include <Arduino.h>
#include <BluetoothA2DPSink.h>
#include <driver/i2s.h>
#include <USB.h>
#include <USBCDC.h>

// ============================================================================
// PIN DEFINITIONS - Seeed XIAO ESP32-S3 + MAX98357A (x2 for stereo)
// ============================================================================

// I2S - Left Channel (MAX98357A #1)
#define I2S_BCK_LEFT   GPIO_NUM_9   // Bit Clock
#define I2S_WS_LEFT    GPIO_NUM_8   // Word Select (LR Clock)
#define I2S_DIN_LEFT   GPIO_NUM_10  // Data In

// I2S - Right Channel (MAX98357A #2)
#define I2S_BCK_RIGHT  GPIO_NUM_6   // Bit Clock
#define I2S_WS_RIGHT   GPIO_NUM_7   // Word Select
#define I2S_DIN_RIGHT  GPIO_NUM_5   // Data In

// ============================================================================
// GLOBALS
// ============================================================================

BluetoothA2DPSink a2dp_sink;
USBCDC SerialUSB;

bool usb_phone_connected = false;
bool ble_active = true;
bool usb_audio_ready = false;
unsigned long usb_handshake_time = 0;
const unsigned long USB_HANDSHAKE_TIMEOUT = 2000; // 2 seconds to get ACK

// ============================================================================
// I2S SETUP
// ============================================================================

void setupI2S() {
  // I2S configuration for LEFT channel
  i2s_config_t i2s_config_left = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = 44100,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_STAND_I2S),
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 64,
    .use_apll = false,
  };

  i2s_driver_install(I2S_NUM_0, &i2s_config_left, 0, NULL);
  
  i2s_pin_config_t pin_config_left = {
    .bck_io_num = I2S_BCK_LEFT,
    .ws_io_num = I2S_WS_LEFT,
    .data_out_num = I2S_DIN_LEFT,
    .data_in_num = I2S_PIN_NO_CHANGE,
  };
  i2s_set_pin(I2S_NUM_0, &pin_config_left);

  // I2S configuration for RIGHT channel
  i2s_config_t i2s_config_right = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = 44100,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,
    .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_STAND_I2S),
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 64,
    .use_apll = false,
  };

  i2s_driver_install(I2S_NUM_1, &i2s_config_right, 0, NULL);
  
  i2s_pin_config_t pin_config_right = {
    .bck_io_num = I2S_BCK_RIGHT,
    .ws_io_num = I2S_WS_RIGHT,
    .data_out_num = I2S_DIN_RIGHT,
    .data_in_num = I2S_PIN_NO_CHANGE,
  };
  i2s_set_pin(I2S_NUM_1, &pin_config_right);

  Serial.println("[I2S] Initialized stereo (two MAX98357A boards)");
}

// ============================================================================
// BLE A2DP CALLBACKS
// ============================================================================

void audio_data_callback(const uint8_t *data, uint32_t len) {
  // Only process BLE audio if BLE is active and USB phone is NOT connected
  if (!ble_active || usb_phone_connected) return;

  size_t bytes_written = 0;

  // Write to both I2S ports simultaneously
  i2s_write(I2S_NUM_0, data, len, &bytes_written, portMAX_DELAY);
  i2s_write(I2S_NUM_1, data, len, &bytes_written, portMAX_DELAY);

  // Serial.printf("[BLE] Audio data: %d bytes\n", len);
}

void connection_state_changed(esp_a2d_connection_state_t state, void *ptr) {
  if (state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
    Serial.println("[BLE] Connected to Android device");
    ble_active = true;
  } else if (state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
    Serial.println("[BLE] Disconnected from Android device");
    ble_active = false;
  }
}

// ============================================================================
// USB SERIAL HANDLING
// ============================================================================

void handleUSBHandshake() {
  if (SerialUSB.available() > 0) {
    String incoming = SerialUSB.readStringUntil('\n');
    incoming.trim();

    if (incoming == "USB_AUDIO_MODE_ACK") {
      Serial.println("[USB] Handshake successful! Switching to USB audio mode");
      usb_phone_connected = true;
      usb_audio_ready = true;
      ble_active = false;
    }
  }

  // Timeout check
  if (!usb_audio_ready && (millis() - usb_handshake_time) > USB_HANDSHAKE_TIMEOUT) {
    Serial.println("[USB] Handshake timeout. Assuming power adapter, staying on BLE");
    usb_handshake_time = millis(); // Reset to avoid repeated messages
  }
}

void processUSBPCM() {
  // Read incoming USB PCM data and send to I2S
  if (SerialUSB.available() > 0) {
    uint8_t buffer[512];
    int bytes_read = SerialUSB.readBytes(buffer, sizeof(buffer));
    
    size_t bytes_written = 0;
    i2s_write(I2S_NUM_0, buffer, bytes_read, &bytes_written, portMAX_DELAY);
    i2s_write(I2S_NUM_1, buffer, bytes_read, &bytes_written, portMAX_DELAY);

    // Serial.printf("[USB] PCM data: %d bytes\n", bytes_read);
  }
}

void detectUSBConnection() {
  // Check if USB is enumerated (D+ pulled high)
  bool usb_detected = SerialUSB.availableForWrite() > 0;

  if (usb_detected && !usb_phone_connected && !usb_audio_ready) {
    // USB just connected, start handshake
    Serial.println("[USB] USB detected! Sending handshake...");
    SerialUSB.println("USB_AUDIO_MODE_READY");
    usb_handshake_time = millis();
  }

  if (!usb_detected && usb_phone_connected) {
    // USB disconnected, back to BLE
    Serial.println("[USB] USB disconnected! Switching back to BLE");
    usb_phone_connected = false;
    usb_audio_ready = false;
    ble_active = true;
  }
}

// ============================================================================
// SETUP
// ============================================================================

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n=== BLE Headphones Project ===");
  Serial.println("Seeed XIAO ESP32-S3 + MAX98357A (Stereo)");
  Serial.println("BLE A2DP + USB Serial Audio");

  // Setup I2S
  setupI2S();

  // Setup BLE A2DP
  a2dp_sink.set_on_data_received_callback(audio_data_callback);
  a2dp_sink.set_on_connection_state_changed_callback(connection_state_changed);
  a2dp_sink.start("BLE-Headphones");

  // Setup USB Serial
  SerialUSB.begin(115200);

  Serial.println("[SYSTEM] Ready. Waiting for BLE or USB connection...");
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
  detectUSBConnection();

  if (usb_audio_ready) {
    // USB phone is connected and ready for audio
    handleUSBHandshake();  // Still monitor for ACK retries or disconnection
    processUSBPCM();       // Read and process USB PCM data
  } else if (usb_phone_connected) {
    // Waiting for handshake acknowledgment
    handleUSBHandshake();
  }

  delay(10); // Small delay to prevent watchdog issues
}
