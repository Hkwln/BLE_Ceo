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
// BLE UUIDs - AUDIO STREAMING SERVICE
// ============================================================================
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define AUDIO_CHAR_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define DEVICE_NAME "BLE-Headphones"

// ============================================================================
// AUDIO CONFIGURATION (BLE Optimized!)
// ============================================================================
// 16kHz Mono 16-bit = 256 kbps (BLE can handle ~300kbps max)
#define SAMPLE_RATE 16000          // 16kHz instead of 44.1kHz
#define CHANNEL_CONFIG I2S_CHANNEL_FMT_ONLY_MONO  // Mono instead of Stereo
#define BITS_PER_SAMPLE 16

// ============================================================================
// GLOBALS
// ============================================================================
bool deviceConnected = false;
BLECharacteristic* pAudioCharacteristic = nullptr;
BLEServer* pServer = nullptr;

volatile uint32_t audioChunksReceived = 0;
volatile uint32_t audioChunksPlayed = 0;

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
        
        // For MONO audio: write same data to both channels for stereo output
        // This upsamples the mono to stereo on the MAX98357A amplifiers
        esp_err_t err1 = i2s_write(I2S_NUM_0, audioBuffer, QUEUE_ITEM_SIZE, &bytesWritten, 0);
        esp_err_t err2 = i2s_write(I2S_NUM_1, audioBuffer, QUEUE_ITEM_SIZE, &bytesWritten, 0);
        
        audioChunksPlayed++;
        
        if (err1 != ESP_OK || err2 != ESP_OK) {
          Serial.printf("[I2S] Write error: L=%d R=%d\n", err1, err2);
        }
      }
    }
  }
}

// ============================================================================
// BLE SERVER CALLBACKS
// ============================================================================

class MyServerCallbacks: public BLEServerCallbacks {
public:
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
    audioChunksReceived = 0;
    audioChunksPlayed = 0;
    Serial.println("\n[BLE] ✅ CLIENT CONNECTED!");
    Serial.println("[BLE] 🎵 Ready for audio data!\n");
  }

  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
    Serial.println("\n[BLE] ❌ CLIENT DISCONNECTED\n");
  }
};

// ============================================================================
// BLE CHARACTERISTIC CALLBACKS
// ============================================================================

class MyCharacteristicCallbacks: public BLECharacteristicCallbacks {
public:
  void onWrite(BLECharacteristic *pCharacteristic) {
    std::string value = pCharacteristic->getValue();
    
    if (value.length() > 0) {
      audioChunksReceived++;
      
      // Queue the audio data for async processing
      if (audioQueue != nullptr) {
        uint8_t audioBuffer[512] = {0};
        
        // Copy data (max 512 bytes)
        size_t copyLen = (value.length() < 512) ? value.length() : 512;
        memcpy(audioBuffer, value.data(), copyLen);
        
        // Send to queue
        xQueueSendToBackFromISR(audioQueue, audioBuffer, nullptr);
      }
      
      // Log every 50 chunks (less spam for 16kHz)
      if (audioChunksReceived % 50 == 0) {
        Serial.printf("[Audio] Recv: %lu, Play: %lu\n", 
                      audioChunksReceived, audioChunksPlayed);
      }
    }
  }
};

// ============================================================================
// I2S SETUP (16kHz Mono → Stereo Output)
// ============================================================================

void setupI2S() {
  Serial.println("[I2S] Initializing I2S for 16kHz Mono...");
  
  // LEFT CHANNEL
  i2s_config_t i2s_config_left = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = SAMPLE_RATE,  // 16kHz
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
  Serial.println("[I2S] LEFT configured");

  // RIGHT CHANNEL (same as LEFT for mono upsampling)
  i2s_config_t i2s_config_right = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = SAMPLE_RATE,
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
  Serial.println("[I2S] RIGHT configured");

  Serial.println("[I2S] ✅ I2S ready (16kHz Mono → Stereo output)\n");
}

// ============================================================================
// SETUP
// ============================================================================

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("\n\n");
  Serial.println("╔════════════════════════════════════════╗");
  Serial.println("║  BLE HEADPHONES - AUDIO SINK v6.0     ║");
  Serial.println("║  Pure BLE (NO Classic Bluetooth!)      ║");
  Serial.println("║  16kHz Mono, 256kbps (BLE optimized)  ║");
  Serial.println("║  Seeed XIAO ESP32-S3 + MAX98357A x2   ║");
  Serial.println("╚════════════════════════════════════════╝\n");

  // Setup I2S
  setupI2S();

  // Create audio processing queue
  audioQueue = xQueueCreate(10, 512);
  if (audioQueue == nullptr) {
    Serial.println("[ERROR] Queue creation failed!");
    while(1) delay(1000);
  }
  Serial.println("[Queue] ✅ Created");

  // Create audio processing task
  xTaskCreatePinnedToCore(
    audioProcessTask,
    "AudioTask",
    4096,
    nullptr,
    2,
    &audioTaskHandle,
    1
  );
  Serial.println("[Task] ✅ Running on Core 1\n");

  // Initialize BLE Device
  Serial.println("[BLE] Initializing...");
  BLEDevice::init(DEVICE_NAME);
  BLEDevice::setMTU(512);  // REQUEST MTU 512 - CRITICAL for BLE performance!

  // Create BLE Server
  pServer = BLEDevice::createServer();
  if (pServer == nullptr) {
    Serial.println("[ERROR] Failed to create BLE server!");
    while(1) delay(1000);
  }
  pServer->setCallbacks(new MyServerCallbacks());
  Serial.println("[BLE] Server created");

  // Create BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);
  if (pService == nullptr) {
    Serial.println("[ERROR] Failed to create service!");
    while(1) delay(1000);
  }
  Serial.println("[BLE] Service created");

  // Create BLE Characteristic (WRITE only - for audio data)
  pAudioCharacteristic = pService->createCharacteristic(
    AUDIO_CHAR_UUID,
    BLECharacteristic::PROPERTY_WRITE |
    BLECharacteristic::PROPERTY_WRITE_NR  // Write without response = CRITICAL!
  );
  
  if (pAudioCharacteristic == nullptr) {
    Serial.println("[ERROR] Failed to create characteristic!");
    while(1) delay(1000);
  }

  pAudioCharacteristic->addDescriptor(new BLE2902());
  pAudioCharacteristic->setCallbacks(new MyCharacteristicCallbacks());
  pAudioCharacteristic->setMaxValue(512);
  pAudioCharacteristic->setMinValue(1);
  Serial.println("[BLE] Characteristic created");

  // Start Service
  pService->start();
  Serial.println("[BLE] Service started");

  // Configure Advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMaxPreferred(0x12);
  Serial.println("[BLE] Advertising configured");

  // Start Advertising
  BLEDevice::startAdvertising();
  Serial.println("[BLE] ✅ Advertising started!\n");

  Serial.println("╔════════════════════════════════════════╗");
  Serial.println("║  📱 Android App:                       ║");
  Serial.println("║  1. requestMtu(512)                    ║");
  Serial.println("║  2. CONNECTION_PRIORITY_HIGH           ║");
  Serial.println("║  3. Send audio with WRITE_NO_RESPONSE  ║");
  Serial.println("║  4. Stream 16kHz Mono audio!          ║");
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
                    audioChunksReceived, audioChunksPlayed);
    } else {
      Serial.println("[Status] ⏳ Waiting for BLE connection...");
    }
  }

  delay(100);
}
