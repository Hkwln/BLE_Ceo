#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <driver/i2s.h>
#include <freertos/queue.h>

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
BLECharacteristic* pCharacteristic = nullptr;
BLEServer* pServer = nullptr;

volatile uint32_t audioChunksReceived = 0;
volatile uint32_t audioChunksPlayed = 0;

// Audio queue for async processing
QueueHandle_t audioQueue = nullptr;
TaskHandle_t audioTaskHandle = nullptr;

// ============================================================================
// AUDIO TASK - Process audio data asynchronously
// ============================================================================

void audioProcessTask(void* pvParameters) {
  const size_t QUEUE_ITEM_SIZE = 512;
  uint8_t audioBuffer[QUEUE_ITEM_SIZE];
  
  while (true) {
    if (xQueueReceive(audioQueue, audioBuffer, portMAX_DELAY)) {
      if (deviceConnected) {
        size_t bytesWritten = 0;
        
        // Write to LEFT channel
        esp_err_t err1 = i2s_write(I2S_NUM_0, audioBuffer, QUEUE_ITEM_SIZE, &bytesWritten, 0);
        
        // Write to RIGHT channel (duplicate for stereo)
        esp_err_t err2 = i2s_write(I2S_NUM_1, audioBuffer, QUEUE_ITEM_SIZE, &bytesWritten, 0);
        
        audioChunksPlayed++;
        
        if (err1 != ESP_OK || err2 != ESP_OK) {
          Serial.printf("[I2S] Error writing: L=%d, R=%d\n", err1, err2);
        }
      }
    }
  }
}

// ============================================================================
// BLE CALLBACKS
// ============================================================================

class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
    audioChunksReceived = 0;
    audioChunksPlayed = 0;
    Serial.println("\n[BLE] ✅ CLIENT CONNECTED!");
    Serial.println("[BLE] 🎵 Ready to receive audio!\n");
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
      
      // Queue the audio data for async processing (non-blocking)
      if (audioQueue != nullptr) {
        uint8_t audioBuffer[512] = {0};
        memcpy(audioBuffer, value.data(), minOf((size_t)512, value.length()));
        
        // Send to queue (don't wait if full)
        xQueueSendToBackFromISR(audioQueue, audioBuffer, nullptr);
      }
      
      // Log every 100 chunks
      if (audioChunksReceived % 100 == 0) {
        Serial.printf("[Audio] 🔊 Recv: %lu, Play: %lu\n", 
                      audioChunksReceived, audioChunksPlayed);
      }
    }
  }
};

// ============================================================================
// I2S SETUP
// ============================================================================

void setupI2S() {
  Serial.println("\n[I2S] 🔧 Starting I2S...");
  
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
    Serial.printf("[I2S] ❌ LEFT driver failed: %s\n", esp_err_to_name(err));
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
    Serial.println("[I2S] ✅ LEFT configured");
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
    Serial.printf("[I2S] ❌ RIGHT driver failed: %s\n", esp_err_to_name(err));
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
    Serial.println("[I2S] ✅ RIGHT configured");
  }

  Serial.println("[I2S] ✅ Stereo I2S ready!\n");
}

// ============================================================================
// SETUP
// ============================================================================

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("\n\n");
  Serial.println("╔════════════════════════════════════════════════════════╗");
  Serial.println("║   BLE HEADPHONES AUDIO RECEIVER - v3.0 (ASYNC)        ║");
  Serial.println("║   Seeed XIAO ESP32-S3 + MAX98357A x2 (Stereo)         ║");
  Serial.println("╚════════════════════════════════════════════════════════╝");

  // Setup I2S
  setupI2S();

  // Create audio processing queue
  audioQueue = xQueueCreate(10, 512);
  if (audioQueue == nullptr) {
    Serial.println("[ERROR] Failed to create audio queue!");
    while(1) delay(1000);
  }
  Serial.println("[Queue] ✅ Audio queue created");

  // Create audio processing task
  xTaskCreatePinnedToCore(
    audioProcessTask,      // Task function
    "AudioTask",           // Task name
    4096,                  // Stack size
    nullptr,               // Parameters
    2,                     // Priority
    &audioTaskHandle,      // Task handle
    1                      // Core 1
  );
  Serial.println("[Task] ✅ Audio task running\n");

  // Initialize BLE Device
  Serial.println("[BLE] 🔧 Initializing BLE...");
  BLEDevice::init(DEVICE_NAME);

  // Create BLE Server
  pServer = BLEDevice::createServer();
  if (pServer == nullptr) {
    Serial.println("[BLE] ❌ Failed to create server!");
    while(1) delay(1000);
  }
  pServer->setCallbacks(new MyServerCallbacks());

  // Create BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);
  if (pService == nullptr) {
    Serial.println("[BLE] ❌ Failed to create service!");
    while(1) delay(1000);
  }

  // Create BLE Characteristic
  pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_WRITE
  );
  
  if (pCharacteristic == nullptr) {
    Serial.println("[BLE] ❌ Failed to create characteristic!");
    while(1) delay(1000);
  }

  pCharacteristic->setCallbacks(new MyCharacteristicCallbacks());
  pService->start();

  // Configure Advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMaxPreferred(0x12);

  // Start Advertising
  BLEDevice::startAdvertising();
  
  Serial.println("[BLE] ✅ BLE ADVERTISING STARTED!\n");
  Serial.println("╔════════════════════════════════════════════════════════╗");
  Serial.println("║  🎯 Ready! Open Android app and connect                ║");
  Serial.println("║  📱 Scan → BLE-Headphones → Connect                    ║");
  Serial.println("║  🎵 Play YouTube → Audio streams to speakers!          ║");
  Serial.println("╚════════════════════════════════════════════════════════╝\n");
}

// ============================================================================
// LOOP
// ============================================================================

void loop() {
  static unsigned long lastStatus = 0;
  
  if (millis() - lastStatus > 5000) {
    lastStatus = millis();
    
    if (deviceConnected) {
      Serial.printf("[Status] ✅ Connected - Chunks: Recv=%lu, Play=%lu\n", 
                    audioChunksReceived, audioChunksPlayed);
    } else {
      Serial.printf("[Status] ⏳ Waiting for connection...\n");
    }
  }

  delay(100);
}
