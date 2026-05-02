#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <driver/i2s.h>

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
// BLE UUIDs
// ============================================================================
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// ============================================================================
// GLOBALS
// ============================================================================
bool deviceConnected = false;
bool oldDeviceConnected = false;
BLECharacteristic* pCharacteristic = nullptr;

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

  // Install LEFT channel
  esp_err_t err = i2s_driver_install(I2S_NUM_0, &i2s_config_left, 0, NULL);
  if (err != ESP_OK) {
    Serial.printf("[I2S] Failed to install LEFT driver: %s\n", esp_err_to_name(err));
  }

  // Pin configuration for LEFT
  i2s_pin_config_t pin_config_left = {
    .bck_io_num = I2S_BCK_LEFT,
    .ws_io_num = I2S_WS_LEFT,
    .data_out_num = I2S_DIN_LEFT,
    .data_in_num = -1,
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

  // Install RIGHT channel
  err = i2s_driver_install(I2S_NUM_1, &i2s_config_right, 0, NULL);
  if (err != ESP_OK) {
    Serial.printf("[I2S] Failed to install RIGHT driver: %s\n", esp_err_to_name(err));
  }

  // Pin configuration for RIGHT
  i2s_pin_config_t pin_config_right = {
    .bck_io_num = I2S_BCK_RIGHT,
    .ws_io_num = I2S_WS_RIGHT,
    .data_out_num = I2S_DIN_RIGHT,
    .data_in_num = -1,
  };

  i2s_set_pin(I2S_NUM_1, &pin_config_right);

  Serial.println("[I2S] ✅ I2S initialized for stereo (LEFT + RIGHT channels)");
}

// ============================================================================
// BLE CALLBACKS
// ============================================================================

class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
    Serial.println("[BLE] ✅ Device connected!");
    BLEDevice::startAdvertising(); // Re-enable advertising for multiple connections
  }

  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
    Serial.println("[BLE] ❌ Device disconnected. Restarting advertising...");
    BLEDevice::startAdvertising();
  }
};

class MyCharacteristicCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    std::string value = pCharacteristic->getValue();
    
    if (value.length() > 0) {
      Serial.print("[BLE] Received: ");
      for (int i = 0; i < value.length(); i++) {
        Serial.print(value[i]);
      }
      Serial.println();
    }
  }

  void onRead(BLECharacteristic *pCharacteristic) {
    Serial.println("[BLE] Characteristic read");
  }
};

// ============================================================================
// SETUP
// ============================================================================

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n===========================================");
  Serial.println("[Setup] Starting BLE Headphones System...");
  Serial.println("===========================================\n");

  // Initialize I2S
  setupI2S();

  // 1. Initialize BLE Device
  BLEDevice::init("BLE-Headphones");
  Serial.println("[BLE] ✅ BLE Device initialized as 'BLE-Headphones'");

  // 2. Create BLE Server
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  Serial.println("[BLE] ✅ BLE Server created");

  // 3. Create BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);
  Serial.println("[BLE] ✅ BLE Service created");

  // 4. Create BLE Characteristic
  pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ |
    BLECharacteristic::PROPERTY_WRITE |
    BLECharacteristic::PROPERTY_NOTIFY
  );

  pCharacteristic->addDescriptor(new BLE2902());
  pCharacteristic->setCallbacks(new MyCharacteristicCallbacks());
  pCharacteristic->setValue("BLE-Headphones Ready");
  Serial.println("[BLE] ✅ BLE Characteristic created");

  // 5. Start Service
  pService->start();
  Serial.println("[BLE] ✅ BLE Service started");

  // 6. Configure Advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMaxPreferred(0x12);
  Serial.println("[BLE] ✅ Advertising configured");

  // 7. Start Advertising
  BLEDevice::startAdvertising();
  Serial.println("[BLE] ✅ Advertising started\n");

  Serial.println("===========================================");
  Serial.println("[Status] ✅ System ready!");
  Serial.println("[Status] Waiting for Android app to connect...");
  Serial.println("===========================================\n");
}

// ============================================================================
// LOOP
// ============================================================================

void loop() {
  // Notify connected clients if needed
  if (deviceConnected && !oldDeviceConnected) {
    Serial.println("[BLE] Device just connected - updating advertising");
    oldDeviceConnected = deviceConnected;
  }

  if (!deviceConnected && oldDeviceConnected) {
    Serial.println("[BLE] Device just disconnected");
    oldDeviceConnected = deviceConnected;
  }

  // Send test data every 2 seconds if connected
  if (deviceConnected) {
    static unsigned long lastSend = 0;
    if (millis() - lastSend > 2000) {
      lastSend = millis();
      pCharacteristic->setValue("Heartbeat: " + String(millis()));
      pCharacteristic->notify();
      Serial.println("[BLE] Sent heartbeat notification");
    }
  }

  delay(1000);
}
