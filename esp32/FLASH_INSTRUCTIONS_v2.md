# ESP32 Firmware Flash Instructions - v2.0 Audio Edition

## 📝 Was ist neu?

✅ **Audio Data Processing** — Empfängt BLE Audio-Daten vom Android Smartphone  
✅ **I2S Output** — Schreibt Daten direkt zu den MAX98357A Amplifiern  
✅ **Stereo Support** — Beide Kanäle (LEFT + RIGHT) gleichzeitig  
✅ **Performance Logging** — Zählt Chunks um sicherzustellen dass Audio läuft  

---

## 🔧 Hardware Check

**Deine Pin-Belegung:**
- ESP32 GND + 3V3 an MAX98357A
- PIN 1 (ESP32 GPIO9) → MAX98357A BCLK (LEFT)
- PIN 2 (ESP32 GPIO8) → MAX98357A LRCK (LEFT)
- PIN 3 (ESP32 GPIO10) → MAX98357A DIN (LEFT)
- PIN 4 (ESP32 GPIO6) → MAX98357A BCLK (RIGHT)
- PIN 5 (ESP32 GPIO7) → MAX98357A LRCK (RIGHT)
- PIN 6 (ESP32 GPIO5) → MAX98357A DIN (RIGHT)

✅ **Perfekt!** Das entspricht exakt der Firmware!

---

## 🚀 Firmware Flashen mit Arduino IDE

### 1️⃣ Arduino IDE mit Seeed XIAO Support installieren

```bash
# Macbook:
# Download Arduino IDE von arduino.cc

# Dann: Arduino IDE → Preferences → Additional Boards Manager URLs
# Paste: https://files.seeedstudio.com/arduino/package_seeeduino_index.json

# Tools → Board Manager
# Suche: "Seeed XIAO"
# Install: "Seeed XIAO ESP32-S3 by Seeed Studio"
```

### 2️⃣ Projekt öffnen

```bash
# File → Open
# Wähle: ble-headphones/esp32/src/main.cpp
```

### 3️⃣ Board + Port einstellen

```bash
# Tools → Board → "Seeed XIAO ESP32-S3"
# Tools → Port → (dein USB Port, z.B. /dev/ttyUSB0 oder COM3)
```

### 4️⃣ Sketch Upload

```bash
# Sketch → Upload
# oder: Ctrl+U (Macbook: Cmd+U)

# Warte bis "Done uploading" erscheint
```

### 5️⃣ Serial Monitor Check

```bash
# Tools → Serial Monitor (9600 baud)

# Du solltest sehen:
# ╔════════════════════════════════════════════════════════╗
# ║   BLE HEADPHONES AUDIO RECEIVER - v2.0                ║
# ║   Seeed XIAO ESP32-S3 + MAX98357A x2 (Stereo)         ║
# ╚════════════════════════════════════════════════════════╝
# [I2S] ✅ STEREO I2S fully initialized!
# [BLE] ✅ ADVERTISING STARTED!
```

---

## 🎯 After Firmware Upload

1. **Serial Monitor öffnen** (9600 baud)
2. **Android App v9.0 öffnen**
3. **SCAN drücken** → "BLE-Headphones" findest du
4. **Tap drauf** → connectet
5. **App startet Auto-Audio-Streaming!**
6. **In Serial Monitor siehst du:**
   ```
   [Status] ✅ CONNECTED - Received: 500 chunks, Played: 500 chunks
   ```
7. **Auf den MAX98357A sollte SOUND kommen!** 🎵

---

## 🔧 Wenn kein Sound kommt

### Check 1: I2S Initialization
```
[I2S] ✅ LEFT pins configured
[I2S] ✅ RIGHT pins configured
```
✅ Sollte erscheinen → Wenn nicht: **GPIO-Konflikt!**

### Check 2: BLE Connection
```
[BLE] ✅ CLIENT CONNECTED!
[BLE] 🎵 Ready to receive audio data!
```
✅ Sollte beim App-Connect erscheinen

### Check 3: Audio Chunks
```
[Status] ✅ CONNECTED - Received: 500 chunks, Played: 500 chunks
```
✅ Zahlen sollten steigen → Audio kommt an!

---

## 🎵 Audio Quality

- **Sample Rate:** 44.1 kHz (Smartphone Standard)
- **Bit Depth:** 16-bit (Smartphone Standard)
- **Channels:** Stereo (2x MAX98357A)
- **Format:** PCM Little-Endian (IPhone/Android Standard)

---

## 📱 Android App

Download v9.0 (mit Audio):
```
https://github.com/Hkwln/BLE_Ceo/raw/main/releases/BLE_Headphones_v9.0-AudioStreaming.apk
```

---

## 💡 Debugging Tips

Falls Audio nicht funktioniert:

1. **Volumen Check:**
   - MAX98357A hat keinen Volume-Control → Überprüf Android Lautstärke!
   - Android Volume + 50% oder höher stellen

2. **Lautsprecher Check:**
   - MAX98357A LED sollte grün leuchten wenn powered
   - Test mit Headphones statt Speakers

3. **Takt-Problem? (Wenn Sound gestört/verzerrt):**
   - ESP32 I2S Takt könnte mit MAX98357A nicht synchron sein
   - Das ist ein Hardware-Timing-Problem

---

## 🎉 Happy Audio Streaming!

Wenn Sound kommt → **VICTORY!** 🎵🎉

Schreib mir wie es funktioniert!
