# ESP32 BLE Headphones Firmware - Fixed

## ✅ Problem Gelöst

**Issue:** Android App findet das ESP32 Gerät nicht
- **Ursache:** Gerätename war nicht "BLE-Headphones"
- **Lösung:** Kombiniert BLE Server + korrekter Gerätename

## 🎯 Was ist neu

### Hardware
- ✅ Seeed XIAO ESP32-S3
- ✅ 2x Adafruit MAX98357A (I2S stereo amplifiers)
- ✅ I2S Left Channel: GPIO 9, 8, 10
- ✅ I2S Right Channel: GPIO 6, 7, 5

### Software
- ✅ BLE Server (`BLEDevice`, `BLEServer`, `BLEService`)
- ✅ Gerätename: **"BLE-Headphones"** (wichtig!)
- ✅ I2S Audio Input (44.1kHz, 16-bit stereo)
- ✅ Characteristic für Datenempfang
- ✅ Notifications Support
- ✅ Multi-Connection Ready

## 📋 Features

```cpp
[Setup] Starting BLE Headphones System...
[I2S] ✅ I2S initialized for stereo (LEFT + RIGHT channels)
[BLE] ✅ BLE Device initialized as 'BLE-Headphones'
[BLE] ✅ BLE Server created
[BLE] ✅ BLE Service created
[BLE] ✅ BLE Characteristic created
[BLE] ✅ Advertising started

[Status] ✅ System ready!
[Status] Waiting for Android app to connect...
```

## 🔧 Build & Upload

### Lokal (PlatformIO CLI)
```bash
cd esp32
pio run -t upload
pio device monitor -b 115200
```

### VSCode + PlatformIO Extension
1. Öffne `esp32/platformio.ini`
2. Klick "Upload"
3. Öffne "Serial Monitor"

## 📱 Android App Connection

1. **App öffnet** → Scanned nach BLE Geräten
2. **Findet "BLE-Headphones"** (Gerätename!)
3. **Verbindet sich** 
4. **Serial Output zeigt:**
```
[BLE] ✅ Device connected!
```

## 🐛 Debug Tipps

### Serial Output nicht sichtbar?
```bash
# Richtige COM-Port finden
pio device list

# Dann in platformio.ini:
upload_port = /dev/ttyUSB0
monitor_port = /dev/ttyUSB0
```

### App findet Gerät noch nicht?
1. ESP32 **neu starten** (Reset button)
2. App **neu starten**
3. Serial Output checken: "Advertising started"?
4. Android Bluetooth Einstellungen: Erscheint "BLE-Headphones"?

### Gerätename falsch?
```cpp
// In setup():
BLEDevice::init("BLE-Headphones");  // ← WICHTIG: Genauer Name!
```

## 📊 Uart Output Format

```
[Component] Message Type

Components:
- [Setup]  = Initialization
- [I2S]    = Audio System
- [BLE]    = Bluetooth
- [Status] = System State
- [Audio]  = Audio Processing
```

## 📝 Nächste Schritte

1. ✅ Firmware flashen
2. ✅ Serial Monitor öffnen
3. ✅ Logs überprüfen
4. ✅ Android App starten
5. ⏳ Audio Streaming implementieren

## 🎵 Audio Streaming (TODO)

Sobald App connected:
- [ ] Receive audio data via BLE Characteristic
- [ ] Process PCM data
- [ ] Send to I2S amplifiers
- [ ] Test stereo output

---

**Status:** ✅ BLE Discovery Fixed  
**Date:** 2026-05-02  
**Version:** 2.0
