#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <driver/i2s.h>
#include <freertos/queue.h>

// ============================================================================
// CONFIGURATION (MUST match Android App)
// ============================================================================
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// XIAO ESP32-S3 Pins für MAX98357A (CORRECTED!)
// I2S_NUM_0: LEFT channel
#define I2S_BCK_LEFT   GPIO_NUM_9   // D8 XIAO S3
#define I2S_WS_LEFT    GPIO_NUM_8   // D9 XIAO S3
#define I2S_DIN_LEFT   GPIO_NUM_10  // D10 XIAO S3

// I2S_NUM_1: RIGHT channel
#define I2S_BCK_RIGHT  GPIO_NUM_6   // D6 XIAO S3
#define I2S_WS_RIGHT   GPIO_NUM_7   // D7 XIAO S3
#define I2S_DIN_RIGHT  GPIO_NUM_5   // D5 XIAO S3

// Audio: 16kHz Mono (256 kbps - perfect for BLE!)
#define SAMPLE_RATE 16000
#define AUDIO_FORMAT I2S_BITS_PER_SAMPLE_16BIT

// ============================================================================
// GLOBALS
// ============================================================================
bool deviceConnected = false;
BLECharacteristic *pAudioCharacteristic = nullptr;

volatile uint32_t chunksReceived = 0;
volatile uint32_t chunksPlayed = 0;

// Audio queue for non-blocking processing
QueueHandle_t audioQueue = nullptr;
TaskHandle_t audioTaskHandle = nullptr;

// ============================================================================
// I2S SETUP (Dual channel for stereo output from mono input)
// ============================================================================

void setupI2S() {
  Serial.println("[I2S] Initializing stereo I2S (16kHz Mono → Stereo)...");
  
  // LEFT CHANNEL (I2S_NUM_0)
  i2s_config_t i2s_config_left = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = AUDIO_FORMAT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = 256,
    .use_apll = false
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
    .data_in_num = -1
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
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = AUDIO_FORMAT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = 256,
    .use_apll = false
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
    .data_in_num = -1
  };

  err = i2s_set_pin(I2S_NUM_1, &pin_config_right);
  if (err != ESP_OK) {
    Serial.printf("[I2S] RIGHT pins failed: %d\n", err);
    return;
  }
  Serial.println("[I2S] RIGHT pins configured");

  Serial.println("[I2S] ✅ Stereo I2S ready (16kHz Mono)\n");
}

// ============================================================================
// AUDIO PROCESSING TASK (Non-blocking!)
// ============================================================================

void audioProcessTask(void *pvParameters) {
  uint8_t audioBuffer[512];
  
  while (true) {
    // Wait for audio data from queue (blocking, but in separate task)
    if (xQueueReceive(audioQueue, audioBuffer, portMAX_DELAY)) {
      if (deviceConnected) {
        size_t bytesWritten = 0;
        
        // Write mono data to both channels for stereo output
        esp_err_t err1 = i2s_write(I2S_NUM_0, audioBuffer, 512, &bytesWritten, 0);
        esp_err_t err2 = i2s_write(I2S_NUM_1, audioBuffer, 512, &bytesWritten, 0);
        
        if (err1 == ESP_OK && err2 == ESP_OK) {
          chunksPlayed++;
        }
      }
    }
  }
}

// ============================================================================
// BLE CALLBACKS
// ============================================================================

class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer *pServer) {
    deviceConnected = true;
    chunksReceived = 0;
    chunksPlayed = 0;
    Serial.println("\n[BLE] ✅ CONNECTED!");
    Serial.println("[BLE] Ready for audio\n");
    
    // Request MTU 512 for better throughput
    pServer->updateConnParams(BLE_CONN_HANDLE_ALL, 0x10, 0x20, 0, 200);
  }

  void onDisconnect(BLEServer *pServer) {
    deviceConnected = false;
    Serial.println("\n[BLE] ❌ DISCONNECTED");
    Serial.println("[BLE] Restarting advertising...\n");
    BLEDevice::startAdvertising();
  }
};

class MyCharacteristicCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    std::string value = pCharacteristic->getValue();
    
    if (value.length() > 0) {
      chunksReceived++;
      
      // Queue data for async processing (NON-BLOCKING!)
      if (audioQueue != nullptr) {
        uint8_t buffer[512] = {0};
        size_t len = (value.length() < 512) ? value.length() : 512;
        memcpy(buffer, value.data(), len);
        
        // Send to queue WITHOUT waiting (portMAX_DELAY would block BLE callback!)
        xQueueSendToBackFromISR(audioQueue, buffer, nullptr);
      }
      
      // Log progress
      if (chunksReceived % 100 == 0) {
        Serial.printf("[Audio] Recv: %lu, Played: %lu\n", chunksReceived, chunksPlayed);
      }
    }
  }
};

// ============================================================================
// SETUP
// ============================================================================

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("\n\n");
  Serial.println("╔════════════════════════════════════════╗");
  Serial.println("║  BLE HEADPHONES - v6.0 FINAL         ║");
  Serial.println("║  XIAO ESP32-S3 + MAX98357A x2         ║");
  Serial.println("║  Pure BLE • 16kHz Mono • 256kbps      ║");
  Serial.println("╚════════════════════════════════════════╝\n");

  // Setup I2S
  setupI2S();

  // Create audio queue (512 bytes per chunk, max 10 chunks buffered)
  audioQueue = xQueueCreate(10, 512);
  if (audioQueue == nullptr) {
    Serial.println("[ERROR] Queue creation failed!");
    while (1) delay(1000);
  }
  Serial.println("[Queue] ✅ Created");

  // Create audio processing task on Core 1
  xTaskCreatePinnedToCore(
    audioProcessTask,
    "AudioTask",
    4096,
    nullptr,
    2,
    &audioTaskHandle,
    1
  );
  Serial.println("[Task] ✅ Running\n");

  // Initialize BLE
  Serial.println("[BLE] Initializing...");
  BLEDevice::init("BLE-Headphones");
  BLEDevice::setMTU(512);  // CRITICAL: Request MTU 512
  
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  Serial.println("[BLE] Server created");

  BLEService *pService = pServer->createService(SERVICE_UUID);
  Serial.println("[BLE] Service created");

  pAudioCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_WRITE |
    BLECharacteristic::PROPERTY_WRITE_NR  // NO RESPONSE = CRITICAL for speed!
  );

  pAudioCharacteristic->addDescriptor(new BLE2902());
  pAudioCharacteristic->setCallbacks(new MyCharacteristicCallbacks());
  pAudioCharacteristic->setMaxValue(512);
  Serial.println("[BLE] Characteristic created");

  pService->start();
  Serial.println("[BLE] Service started");

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMaxPreferred(0x12);
  BLEDevice::startAdvertising();
  
  Serial.println("[BLE] ✅ Advertising started!\n");
  Serial.println("╔════════════════════════════════════════╗");
  Serial.println("║  📱 Android App:                       ║");
  Serial.println("║  • requestMtu(512)                     ║");
  Serial.println("║  • CONNECTION_PRIORITY_HIGH            ║");
  Serial.println("║  • WRITE_NO_RESPONSE                   ║");
  Serial.println("║  • 16kHz Mono PCM                      ║");
  Serial.println("╚════════════════════════════════════════╝\n");
}

// ============================================================================
// LOOP
// ============================================================================

void loop() {
  static unsigned long lastStatus = 0;
  
  if (millis() - lastStatus > 5000) {
    lastStatus = millis();
    
    if (deviceConnected) {
      Serial.printf("[Status] ✅ Connected - Recv: %lu, Play: %lu\n", 
                    chunksReceived, chunksPlayed);
    } else {
      Serial.println("[Status] ⏳ Waiting for connection...");
    }
  }

  delay(100);
}
