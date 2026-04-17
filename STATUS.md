# BLE Headphones Project — Status

## What's Built

### ESP32 Firmware (`esp32/src/main.cpp`)
✅ **BLE A2DP Sink** — receives stereo audio from Android phone
✅ **Dual I2S Output** — streams to two MAX98357A amplifiers (stereo)
✅ **USB Serial Handshake** — detects USB connection and mode switching
✅ **Protocol:**
   - ESP32 sends: `USB_AUDIO_MODE_READY\n`
   - Phone responds: `USB_AUDIO_MODE_ACK\n`
   - Both switch to raw PCM (44.1kHz, 16-bit stereo, little-endian)
✅ **Charging** — USB-C to power adapter = charge + stay on BLE (no data lines = no handshake)

### Android App (skeleton)
✅ `MainActivity.kt` — UI + BLE scanning
✅ `BLEManager.kt` — A2DP Bluetooth connection
✅ `USBSerialManager.kt` — USB Serial communication (using USB Serial library)
✅ `HandshakeProtocol.kt` — Mode switching logic
✅ `AudioManager.kt` — Audio capture from system

## Next Steps

### ESP32
1. **Test the I2S pinout** — verify wiring to MAX98357A boards
2. **Verify USB Serial** — confirm CDC enumeration works with Android
3. **Buffer tuning** — might need DMA buffer tweaks if audio stutters

### Android App
1. **Complete BLE A2DP connection** — use standard BluetoothAdapter profile APIs
2. **Implement audio capture** — use MediaProjection to capture system audio
3. **USB Serial integration** — handle incoming handshake from ESP32
4. **PCM streaming** — capture audio and send over USB Serial when in USB mode
5. **UI polish** — status indicators, error handling, reconnection logic

### Build & Test
1. Flash ESP32 firmware via PlatformIO
2. Build Android APK (Android Studio)
3. Pair phone + ESP32 via Bluetooth
4. Test BLE audio streaming
5. Connect USB-C cable and verify mode switch
6. Test charging with power adapter

## File Structure
```
ble-headphones/
├── esp32/
│   ├── platformio.ini
│   └── src/
│       └── main.cpp
└── android/
    ├── README.md
    └── src/
        ├── MainActivity.kt
        ├── BLEManager.kt
        ├── USBSerialManager.kt
        ├── HandshakeProtocol.kt
        └── AudioManager.kt
```

## Notes
- Using `USBCDC` for ESP32 native USB serial (no external chip needed)
- Android app uses `usb-serial-for-android` library for USB communication
- All audio is 44.1kHz, 16-bit stereo, little-endian
- Handshake ensures clean switching between BLE and USB modes
