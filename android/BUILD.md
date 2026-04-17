# BLE Headphones Android App - Build Instructions

## Prerequisites

- Android Studio (latest)
- Android SDK 34 or higher
- Gradle 8.0+
- Kotlin plugin

## Build Steps

### Option 1: Using Android Studio

1. Open Android Studio
2. Click `File → Open` and select the `android/` directory
3. Wait for Gradle to sync
4. Click `Build → Build Bundle(s) / APK(s) → Build APK(s)`
5. APK will be generated at: `android/app/build/outputs/apk/debug/app-debug.apk`

### Option 2: Using Gradle (Command Line)

```bash
cd /home/openclaw-worker/.openclaw/workspace/ble-headphones/android
./gradlew assembleDebug
```

APK will be at: `app/build/outputs/apk/debug/app-debug.apk`

## Install on Device

```bash
adb install app/build/outputs/apk/debug/app-debug.apk
```

Or use Android Studio's "Run" button to build and install automatically.

## Permissions

The app requires:
- Bluetooth (scan, connect)
- Record Audio
- USB access
- Phone state read

These will be requested on first launch (Android 6+).

## Testing

1. Ensure ESP32 is flashed and powered on
2. Launch the app
3. Click "Connect to BLE Headphones"
4. Select the ESP32 device when it appears
5. Once connected, the app monitors for USB connection
6. Plug in USB-C cable to trigger USB audio mode

## Troubleshooting

- **Permissions not granted?** Go to Settings → Apps → BLE Headphones → Permissions and enable manually
- **Bluetooth not scanning?** Ensure Bluetooth is enabled on the phone
- **USB not detected?** Check USB driver on phone (may need to enable USB debugging)
