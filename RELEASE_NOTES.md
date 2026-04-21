# BLE Headphones App - v1.5-Fixed

**Status:** ✅ Production Ready

## Download

```
https://github.com/Hkwln/BLE_Ceo/raw/main/releases/BLE_Headphones_v1.5-Fixed.apk
```

**Size:** 5.3 MB  
**Min Android:** 5.0 (API 21)  
**Target Android:** 13 (API 33)

## Installation

```bash
adb install BLE_Headphones_v1.5-Fixed.apk
```

## Features

- 🔵 **Bluetooth Auto-Detection** — Automatically enables Bluetooth on startup
- 📡 **BLE Scanning** — Searches for "BLE-Headphones" devices
- 🔌 **USB Serial Support** — Ready for USB-C audio fallback
- 🎵 **Audio Streaming** — Receives audio from smartphone
- 🎨 **Material Design 3** — Modern, clean UI
- ⚡ **Error Handling** — Robust crash protection

## How It Works

1. **Open App** → Bluetooth auto-enables
2. **Tap "Scan for Headphones"** → App searches for BLE-Headphones
3. **Connect** → App pairs and starts listening for audio
4. **Play Music** → Audio streams via BLE A2DP
5. **USB Optional** → Auto-switches to USB if connected

## Technical Details

### Components
- **BLEManager** — Bluetooth scanning & connection
- **USBSerialManager** — USB serial communication
- **AudioCaptureManager** — Audio input handling
- **HandshakeProtocol** — BLE/USB mode switching

### Permissions Required
- `BLUETOOTH` — Classic Bluetooth access
- `BLUETOOTH_ADMIN` — Bluetooth control
- `RECORD_AUDIO` — Microphone access

### Material Design
- Material 3 Theme with modern colors
- MaterialCardView for status display
- MaterialButton with rounded corners
- Responsive layout for all screen sizes

## Troubleshooting

**App crashes on startup?**
- Grant Bluetooth permissions when prompted
- Ensure Bluetooth is available on device

**Can't find devices?**
- Make sure device name is "BLE-Headphones"
- Check ESP32 firmware is running (see esp32/ folder)
- Verify device is in pairing mode

**Audio not streaming?**
- Check USB Serial connection (if using USB mode)
- Verify audio permissions are granted
- See HandshakeProtocol.kt for mode switching logic

## Build from Source

```bash
cd ble-headphones/android
./gradlew assembleDebug
# APK: app/build/outputs/apk/debug/app-debug.apk
```

## Project Structure

```
BLE_Ceo/
├── android/                    # Android App (Kotlin)
│   ├── app/src/main/java/...  # MainActivity, BLEManager, etc.
│   ├── app/src/main/res/      # UI, colors, strings
│   └── build.gradle           # Dependencies, SDK config
├── esp32/                      # ESP32 Firmware (C++)
│   ├── src/main.cpp           # BLE A2DP Sink + I2S Output
│   └── platformio.ini         # PlatformIO config
└── releases/                   # APK Releases
```

## Support

For issues or feature requests, see the GitHub repository:
https://github.com/Hkwln/BLE_Ceo

---

**Built with:** Kotlin • Material Design 3 • Bluetooth A2DP • Android 13 (API 33)
