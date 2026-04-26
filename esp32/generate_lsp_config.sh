#!/bin/bash
# Generate compile_commands.json for ESP32 LSP support

WORKSPACE_DIR="/home/openclaw-worker/.openclaw/workspace/ble-headphones/esp32"
BUILD_DIR="${WORKSPACE_DIR}/.pio/build/seeeduino_xiao_esp32s3"
SRC_DIR="${WORKSPACE_DIR}/src"

# Find all ESP32 framework includes
ESP32_CORE="/home/openclaw-worker/.platformio/packages/framework-arduinoespressif32"
XIAO_VARIANT="${ESP32_CORE}/variants/seeed_xiao_esp32s3"

# Create compile_commands.json
cat > "${WORKSPACE_DIR}/compile_commands.json" << 'EOF'
[
  {
    "directory": "/home/openclaw-worker/.openclaw/workspace/ble-headphones/esp32",
    "command": "arm-none-eabi-g++ -DBOARD_HAS_PSRAM -mfix-esp32-psram-cache-issue -std=c++17 -fno-exceptions -I. -Isrc -I.pio/libdeps/seeeduino_xiao_esp32s3/ESP32-A2DP/src -I.pio/libdeps/seeeduino_xiao_esp32s3/Adafruit MAX98357 I2S Class D Mono Amp/src -I/home/openclaw-worker/.platformio/packages/framework-arduinoespressif32/cores/esp32 -I/home/openclaw-worker/.platformio/packages/framework-arduinoespressif32/variants/seeed_xiao_esp32s3 -c src/main.cpp -o .pio/build/seeeduino_xiao_esp32s3/src/main.cpp.o",
    "file": "/home/openclaw-worker/.openclaw/workspace/ble-headphones/esp32/src/main.cpp"
  }
]
EOF

echo "✅ compile_commands.json generated"
