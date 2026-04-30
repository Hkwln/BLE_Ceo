# BLE Device Discovery Debugging Guide

## Problem
App findet BLE-Headphones Gerät nicht beim Scan.

## Checklist

### 1. **ESP32 Seite (Hardware)**
- ✅ ESP32 Firmware lädt?
- ✅ LED blinkt / Serial Output da?
- ✅ Gerät ist in Pairing Mode? (Check: BLE-Headphones sichtbar in Bluetooth-Einstellungen)

### 2. **Android App Seite**

**a) Berechtigungen prüfen:**
```bash
adb shell pm grant com.example.bleheadphones android.permission.BLUETOOTH_SCAN
adb shell pm grant com.example.bleheadphones android.permission.BLUETOOTH_CONNECT
adb shell pm grant com.example.bleheadphones android.permission.RECORD_AUDIO
```

**b) Logs checken:**
```bash
adb logcat | grep BLEManager
adb logcat | grep MainActivity
```

Achte auf diese Outputs:

```
✅ Gutes Zeichen:
D/BLEManager: === Starting BLE Scan for 'BLE-Headphones' ===
D/BLEManager: 🔍 Scanning started... (will timeout in 30s)
D/BLEManager: 📡 Found device: MyDevice (AA:BB:CC:DD:EE:FF) RSSI: -65 dBm
D/BLEManager: ✅ Found TARGET device!

❌ Probleme:
E/BLEManager: ❌ Bluetooth is OFF!
E/BLEManager: ❌ BluetoothLeScanner is null
W/BLEManager: ⏰ Scan timeout - no device found in 30 seconds
E/BLEManager: ❌ Scan failed: Already scanning (code: 1)
```

### 3. **Häufige Probleme & Lösungen**

**Problem: "Scan timeout - no device found in 30 seconds"**
- ✅ ESP32 power on?
- ✅ ESP32 BLE advertising?
- ✅ Gerätename ist genau "BLE-Headphones"? (Case-sensitive!)
- ✅ Zu weit weg? (BLE Reichweite ~10m)
- ✅ Zu viele andere BLE Devices? (andere Geräte ausschalten)

**Problem: "Bluetooth is OFF"**
- ✅ Bluetooth erlauben im Dialog
- ✅ Warten, bis Bluetooth wirklich an ist (1-2 Sekunden)
- ✅ Erneut Scan versuchen

**Problem: "BluetoothLeScanner is null"**
- ✅ Gerät nicht BLE-fähig?
- ✅ Android Version < 5.0? (Min API 21)
- ✅ Bluetooth komplett broken?

### 4. **Debug Modus aktivieren**

**Serial Output vom ESP32 lesen:**
```bash
# USB-TTL anstecken an UART0 (RX/TX Pins)
# Dann:
screen /dev/ttyUSB0 115200
# oder:
miniterm.py /dev/ttyUSB0 115200
```

**ESP32 Output sollte zeigen:**
```
[Setup] I2S initialized...
[Setup] BLE initialized - Device Name: BLE-Headphones
[BLE] Advertising started
[Status] Waiting for A2DP connection...
```

### 5. **Devicename checken**

Der Name MUSS in `esp32/src/main.cpp` definiert sein:
```cpp
a2dp_sink.set_avrc_metadata_attribute_mask(ESP_AVRC_MD_ATTR_TITLE | ...);
// Der Gerätename wird von BluetoothA2DPSink gesetzt
// Standardname: "ESP32-BLE-Sink" oder "BLE-Headphones" wenn explizit gesetzt
```

**Geändert werden kann der Name in der init() Funktion:**
```cpp
void init_bluetooth() {
  // Hier den Namen setzen:
  // a2dp_sink.set_avrc_metadata_attribute_mask(...);
  // oder über BluetoothAdapter.setName()
}
```

### 6. **App neustarten & Frisch testen**

```bash
adb uninstall com.example.bleheadphones
# APK neu installieren
adb install releases/BLE_Headphones_v1.5-Fixed.apk
# App öffnen
adb shell am start -n com.example.bleheadphones/.MainActivity
# Logs live verfolgen
adb logcat -v threadtime | grep -E "(BLEManager|MainActivity)"
```

### 7. **Telemetrie - Was wird tatsächlich gefunden?**

Die neue APK zeigt ALLE gefundenen Geräte im Log:
```
D/BLEManager: 📡 Found device: Device1 (AA:BB:CC:DD:EE:FF) RSSI: -65 dBm
D/BLEManager: 📡 Found device: Device2 (11:22:33:44:55:66) RSSI: -72 dBm
D/BLEManager: 📡 Found device: BLE-Headphones (AA:11:BB:22:CC:33) RSSI: -45 dBm
D/BLEManager: ✅ Found TARGET device!
```

Falls du siehst dass andere Devices gefunden werden, aber nicht "BLE-Headphones":
- ✅ Gerätename auf ESP32 nicht korrekt
- ✅ ESP32 nicht aktiv / kein BLE advertising

---

## APK aktualisiert

**Neue Version mit besseren Logs:**
```
https://github.com/Hkwln/BLE_Ceo/raw/main/releases/BLE_Headphones_v1.5-Fixed.apk
```

**Neue Features:**
- 📊 Detailliertes Device Logging
- ⏰ 30-Sekunden Scan Timeout (vorher unbegrenzt)
- 🎯 Better error messages
- 📡 RSSI (Signal Strength) anzeige
- 🔍 Zeigt alle gefundenen Devices im Log

---

**Probier das und sag mir was in den Logs steht!** 🚀
