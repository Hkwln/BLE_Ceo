#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
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
// BLE UUIDs
// ============================================================================
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define DEVICE_NAME "BLE-Headphones"

// ============================================================================
// GLOBALS
// ============================================================================
bool deviceConnected = false;
bool oldDeviceConnected = false;
BLECharacteristic* pCharacteristic = nullptr;
BLEServer* pServer = nullptr;

volatile uint32_t audioChunksReceived = 0;
volatile uint32_t audioChunksPlayed = 0;

// ============================================================================
// BLE CALLBACKS
// ============================================================================

class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
    Serial.println("\n[BLE] ✅ CLIENT CONNECTED!");
    Serial.println("[BLE] 🎵 Ready to receive audio data!\n");
  }

  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
    Serial.println("\n[BLE] ❌ CLIENT DISCONNECTED");
    Serial.println("[BLE] Waiting for reconnection...\n");
  }
};

class MyCharacteristicCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    std::string value = pCharacteristic->getValue();
    
    if (value.length() > 0) {
      audioChunksReceived++;
      
      // Convert to byte array
      const uint8_t* audioData = (const uint8_t*)value.data();
      size_t audioLen = value.length();
      
      // Write stereo audio to I2S
      // Split mono data into stereo (duplicate for both channels)
      size_t bytesWritten = 0;
      
      // Send to LEFT channel (I2S_NUM_0)
      i2s_write(I2S_NUM_0, audioData, audioLen, &bytesWritten, portMAX_DELAY);
      
      // Send to RIGHT channel (I2S_NUM_1) - same data for stereo
      i2s_write(I2S_NUM_1, audioData, audioLen, &bytesWritten, portMAX_DELAY);
      
      audioChunksPlayed++;
      
      // Log every 100 chunks to avoid spam
      if (audioChunksReceived % 100 == 0) {
        Serial.printf("[Audio] 🔊 Chunks: Received=%lu, Played=%lu\n", 
                      audioChunksReceived, audioChunksPlayed);
      }
    }
  }

  void onRead(BLECharacteristic *pCharacteristic) {
    Serial.println("[BLE] 📖 Characteristic read");
  }
};

// ============================================================================
// I2S SETUP
// ============================================================================

void setupI2S() {
  Serial.println("\n[I2S] 🔧 Starting I2S initialization...");
  
  // LEFT CHANNEL (I2S_NUM_0)
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

  esp_err_t err = i2s_driver_install(I2S_NUM_0, &i2s_config_left, 0, NULL);
  if (err != ESP_OK) {
    Serial.printf("[I2S] ❌ Failed to install LEFT driver: %s\n", esp_err_to_name(err));
    return;
  }
  Serial.println("[I2S] ✅ LEFT driver installed");

  i2s_pin_config_t pin_config_left = {
    .bck_io_num = I2S_BCK_LEFT,
    .ws_io_num = I2S_WS_LEFT,
    .data_out_num = I2S_DIN_LEFT,
    .data_in_num = -1,
  };

  err = i2s_set_pin(I2S_NUM_0, &pin_config_left);
  if (err != ESP_OK) {
    Serial.printf("[I2S] ❌ Failed to set LEFT pins: %s\n", esp_err_to_name(err));
  } else {
    Serial.println("[I2S] ✅ LEFT pins configured");
    Serial.printf("[I2S]    BCK: GPIO%d, WS: GPIO%d, DIN: GPIO%d\n", 
                  I2S_BCK_LEFT, I2S_WS_LEFT, I2S_DIN_LEFT);
  }

  // RIGHT CHANNEL (I2S_NUM_1)
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

  err = i2s_driver_install(I2S_NUM_1, &i2s_config_right, 0, NULL);
  if (err != ESP_OK) {
    Serial.printf("[I2S] ❌ Failed to install RIGHT driver: %s\n", esp_err_to_name(err));
    return;
  }
  Serial.println("[I2S] ✅ RIGHT driver installed");

  i2s_pin_config_t pin_config_right = {
    .bck_io_num = I2S_BCK_RIGHT,
    .ws_io_num = I2S_WS_RIGHT,
    .data_out_num = I2S_DIN_RIGHT,
    .data_in_num = -1,
  };

  err = i2s_set_pin(I2S_NUM_1, &pin_config_right);
  if (err != ESP_OK) {
    Serial.printf("[I2S] ❌ Failed to set RIGHT pins: %s\n", esp_err_to_name(err));
  } else {
    Serial.println("[I2S] ✅ RIGHT pins configured");
    Serial.printf("[I2S]    BCK: GPIO%d, WS: GPIO%d, DIN: GPIO%d\n", 
                  I2S_BCK_RIGHT, I2S_WS_RIGHT, I2S_DIN_RIGHT);
  }

  Serial.println("\n[I2S] ✅ STEREO I2S fully initialized!");
  Serial.println("[I2S]    Ready to stream audio to MAX98357A amplifiers\n");
}

// ============================================================================
// SETUP
// ============================================================================

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("\n\n");
  Serial.println("╔════════════════════════════════════════════════════════╗");
  Serial.println("║   BLE HEADPHONES AUDIO RECEIVER - v2.0                ║");
  Serial.println("║   Seeed XIAO ESP32-S3 + MAX98357A x2 (Stereo)         ║");
  Serial.println("╚════════════════════════════════════════════════════════╝");
  Serial.printf("[System] Starting up... (Time: %lu ms)\n\n", millis());

  // Initialize I2S first (for audio output)
  setupI2S();

  // Initialize BLE Device
  Serial.println("[BLE] 🔧 Initializing BLE as '" DEVICE_NAME "'...");
  BLEDevice::init(DEVICE_NAME);
  Serial.println("[BLE] ✅ BLE Device initialized\n");

  // Create BLE Server
  Serial.println("[BLE] 🔧 Creating BLE Server...");
  pServer = BLEDevice::createServer();
  if (pServer == nullptr) {
    Serial.println("[BLE] ❌ Failed to create server!");
    while(1) delay(1000);
  }
  pServer->setCallbacks(new MyServerCallbacks());
  Serial.println("[BLE] ✅ BLE Server created\n");

  // Create BLE Service
  Serial.println("[BLE] 🔧 Creating BLE Service...");
  BLEService *pService = pServer->createService(SERVICE_UUID);
  if (pService == nullptr) {
    Serial.println("[BLE] ❌ Failed to create service!");
    while(1) delay(1000);
  }
  Serial.printf("[BLE] ✅ Service created\n\n");

  // Create BLE Characteristic
  Serial.println("[BLE] 🔧 Creating BLE Characteristic (WRITE)...");
  pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ |
    BLECharacteristic::PROPERTY_WRITE |
    BLECharacteristic::PROPERTY_NOTIFY
  );
  
  if (pCharacteristic == nullptr) {
    Serial.println("[BLE] ❌ Failed to create characteristic!");
    while(1) delay(1000);
  }

  pCharacteristic->addDescriptor(new BLE2902());
  pCharacteristic->setCallbacks(new MyCharacteristicCallbacks());
  pCharacteristic->setValue("Ready for audio");
  Serial.println("[BLE] ✅ Characteristic created (ready to receive audio)\n");

  // Start Service
  Serial.println("[BLE] 🔧 Starting BLE Service...");
  pService->start();
  Serial.println("[BLE] ✅ Service started\n");

  // Configure Advertising
  Serial.println("[BLE] 🔧 Configuring BLE Advertising...");
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMaxPreferred(0x12);
  Serial.println("[BLE] ✅ Advertising configured\n");

  // Start Advertising
  Serial.println("[BLE] 🚀 Starting BLE Advertising...");
  BLEDevice::startAdvertising();
  
  Serial.println("[BLE] ✅ ADVERTISING STARTED!\n");
  Serial.println("╔════════════════════════════════════════════════════════╗");
  Serial.println("║  🎯 ESP32 Ready! Waiting for Android app...            ║");
  Serial.println("║  📱 Open Android app → Scan → Connect                  ║");
  Serial.println("║  🎵 Audio will stream to MAX98357A speakers!           ║");
  Serial.println("╚════════════════════════════════════════════════════════╝\n");

  Serial.printf("[System] Setup complete! Ready to receive audio.\n\n");
}

// ============================================================================
// LOOP
// ============================================================================

void loop() {
  static unsigned long lastStatus = 0;
  
  // Print status every 5 seconds
  if (millis() - lastStatus > 5000) {
    lastStatus = millis();
    
    if (deviceConnected) {
      Serial.printf("[Status] ✅ CONNECTED - Received: %lu chunks, Played: %lu chunks\n", 
                    audioChunksReceived, audioChunksPlayed);
    } else {
      Serial.printf("[Status] ⏳ Waiting for connection... (Uptime: %lu ms)\n", millis());
    }
  }

  delay(100);
}
