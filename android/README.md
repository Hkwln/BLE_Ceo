# Android BLE Audio App

Custom Android app for BLE Headphones system.

## Architecture

- **BLE Scanner** — finds "BLE-Headphones" device
- **A2DP Sink Connection** — connects to ESP32 via Bluetooth A2DP
- **USB Serial Handler** — detects USB connection and switches to serial mode
- **Audio Capture** — captures device audio and streams via BLE or USB Serial
- **Handshake Protocol** — communicates with ESP32 for mode switching

## Components

- `MainActivity.kt` — Main UI + BLE control
- `BLEManager.kt` — BLE A2DP connection logic
- `USBSerialManager.kt` — USB Serial communication
- `AudioManager.kt` — System audio capture
- `HandshakeProtocol.kt` — USB/BLE mode switching

## Build

```bash
# Android Studio or:
./gradlew build
./gradlew installDebug
```

