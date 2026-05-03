package com.example.bleheadphones

import android.Manifest
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothManager
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.media.AudioFormat
import android.media.AudioRecord
import android.media.MediaRecorder
import android.media.projection.MediaProjectionManager
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.util.Log
import android.widget.Button
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import java.util.UUID

class MainActivity : AppCompatActivity() {

    private var logText: TextView? = null
    private var devicesContainer: LinearLayout? = null
    private var bluetoothAdapter: BluetoothAdapter? = null
    private var isScanning = false
    private val handler = Handler(Looper.getMainLooper())
    private val PERMISSION_REQUEST_CODE = 100
    private val REQUEST_MEDIA_PROJECTION = 101
    
    private var connectedDevice: BluetoothDevice? = null
    private var bluetoothGatt: BluetoothGatt? = null
    private var dataCharacteristic: BluetoothGattCharacteristic? = null
    
    private var audioRecord: AudioRecord? = null
    private var isStreamingAudio = false
    private var audioThread: Thread? = null
    private var mediaProjectionManager: MediaProjectionManager? = null
    private var mediaProjection: android.media.projection.MediaProjection? = null
    
    private val SERVICE_UUID = UUID.fromString("4fafc201-1fb5-459e-8fcc-c5c9c331914b")
    private val CHAR_UUID = UUID.fromString("beb5483e-36e1-4688-b7f5-ea07361b26a8")
    
    private val SAMPLE_RATE = 44100
    private val CHANNEL_CONFIG = AudioFormat.CHANNEL_IN_STEREO
    private val AUDIO_FORMAT = AudioFormat.ENCODING_PCM_16BIT

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main_new)
        supportActionBar?.hide()

        logText = findViewById(R.id.log_text)
        devicesContainer = findViewById(R.id.devices_container)
        findViewById<Button>(R.id.scan_button).setOnClickListener { onScanClicked() }
        findViewById<Button>(R.id.disconnect_button).setOnClickListener { disconnect() }

        log("════════════════════════════════════════")
        log("BLE Headphones - v15.0 DEBUG")
        log("════════════════════════════════════════")

        mediaProjectionManager = getSystemService(Context.MEDIA_PROJECTION_SERVICE) as MediaProjectionManager

        val bluetoothManager = getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
        bluetoothAdapter = bluetoothManager.adapter

        if (bluetoothAdapter == null) {
            log("❌ NO BLUETOOTH SUPPORT")
            return
        }

        log("✅ Bluetooth available")
        log("Android: ${Build.VERSION.SDK_INT}")

        // Enable Bluetooth if needed
        if (!bluetoothAdapter!!.isEnabled) {
            log("📡 Enabling Bluetooth...")
            try {
                bluetoothAdapter!!.enable()
            } catch (e: Exception) {
                log("${e.message}")
            }
        }

        ensureAllPermissionsGranted()
    }

    private fun ensureAllPermissionsGranted() {
        val requiredPermissions = mutableListOf<String>()

        requiredPermissions.add(Manifest.permission.BLUETOOTH)
        requiredPermissions.add(Manifest.permission.BLUETOOTH_ADMIN)

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            requiredPermissions.add(Manifest.permission.BLUETOOTH_SCAN)
            requiredPermissions.add(Manifest.permission.BLUETOOTH_CONNECT)
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            requiredPermissions.add(Manifest.permission.ACCESS_FINE_LOCATION)
            requiredPermissions.add(Manifest.permission.ACCESS_COARSE_LOCATION)
        }

        requiredPermissions.add(Manifest.permission.RECORD_AUDIO)

        val missingPermissions = mutableListOf<String>()
        for (permission in requiredPermissions) {
            if (ContextCompat.checkSelfPermission(this, permission) != PackageManager.PERMISSION_GRANTED) {
                missingPermissions.add(permission)
            }
        }

        if (missingPermissions.isNotEmpty()) {
            log("🔐 Requesting ${missingPermissions.size} permissions")
            ActivityCompat.requestPermissions(
                this,
                missingPermissions.toTypedArray(),
                PERMISSION_REQUEST_CODE
            )
        } else {
            log("✅ All permissions granted!")
        }
    }

    override fun onRequestPermissionsResult(
        requestCode: Int,
        permissions: Array<String>,
        grantResults: IntArray
    ) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)

        if (requestCode == PERMISSION_REQUEST_CODE) {
            log("\n📋 Permissions:")
            var allGranted = true

            for (i in permissions.indices) {
                val permission = permissions[i]
                val granted = grantResults[i] == PackageManager.PERMISSION_GRANTED
                val icon = if (granted) "✅" else "❌"
                val name = permission.split(".").last()
                log("$icon $name")
                if (!granted) allGranted = false
            }

            if (allGranted) {
                log("✅ All granted!")
            } else {
                log("⚠️ Some denied")
            }
        }
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        super.onActivityResult(requestCode, resultCode, data)
        if (requestCode == REQUEST_MEDIA_PROJECTION && resultCode == RESULT_OK && data != null) {
            log("✅ Screen capture authorized")
            mediaProjection = mediaProjectionManager?.getMediaProjection(resultCode, data)
            startAudioStream(mediaProjection)
        }
    }

    private fun onScanClicked() {
        if (isScanning) stopScan() else startScan()
    }

    private fun startScan() {
        log("\n🔍 SCAN START")

        if (!bluetoothAdapter!!.isEnabled) {
            log("❌ Bluetooth OFF")
            return
        }

        isScanning = true
        devicesContainer?.removeAllViews()

        val callback = object : android.bluetooth.BluetoothAdapter.LeScanCallback {
            override fun onLeScan(device: BluetoothDevice?, rssi: Int, scanRecord: ByteArray?) {
                if (device != null && device.address !in addedDevices) {
                    runOnUiThread {
                        addDeviceButton(device)
                    }
                }
            }
        }

        try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                if (ActivityCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_SCAN) != PackageManager.PERMISSION_GRANTED) {
                    log("❌ BLUETOOTH_SCAN denied")
                    isScanning = false
                    return
                }
            }

            bluetoothAdapter!!.startLeScan(callback)
            log("✅ Scanning...")

            handler.postDelayed({ stopScan() }, 10000)

        } catch (e: Exception) {
            log("❌ Scan: ${e.message}")
            isScanning = false
        }
    }

    private fun stopScan() {
        if (isScanning) {
            isScanning = false
            try {
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                    if (ActivityCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_SCAN) == PackageManager.PERMISSION_GRANTED) {
                        bluetoothAdapter!!.stopLeScan(null)
                    }
                } else {
                    bluetoothAdapter!!.stopLeScan(null)
                }
                log("🛑 Scan stopped")
            } catch (e: Exception) {
                log("Error: ${e.message}")
            }
        }
    }

    private var addedDevices = mutableSetOf<String>()

    private fun addDeviceButton(device: BluetoothDevice) {
        addedDevices.add(device.address)
        val button = Button(this).apply {
            text = "${device.name ?: "Unknown"}\n${device.address}"
            setOnClickListener { connectToDevice(device) }
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                100
            ).apply {
                setMargins(8, 8, 8, 8)
            }
        }
        devicesContainer?.addView(button)
        log("📱 Found: ${device.name ?: "Unknown"}")
    }

    private fun connectToDevice(device: BluetoothDevice) {
        log("\n→ Connecting to ${device.name}...")
        stopScan()
        connectedDevice = device

        try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                if (ActivityCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED) {
                    log("❌ BLUETOOTH_CONNECT denied")
                    return
                }
            }

            bluetoothGatt = device.connectGatt(this, false, gattCallback)
            log("✅ Connecting...")

        } catch (e: Exception) {
            log("❌ Connect: ${e.message}")
        }
    }

    private val gattCallback = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(gatt: BluetoothGatt?, status: Int, newState: Int) {
            when (newState) {
                2 -> {
                    log("✅ CONNECTED!")
                    try {
                        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                            if (ActivityCompat.checkSelfPermission(this@MainActivity, Manifest.permission.BLUETOOTH_CONNECT) == PackageManager.PERMISSION_GRANTED) {
                                gatt?.discoverServices()
                            }
                        } else {
                            gatt?.discoverServices()
                        }
                    } catch (e: Exception) {
                        log("Error: ${e.message}")
                    }
                }
                0 -> {
                    log("❌ DISCONNECTED")
                    stopAudioStream()
                    try {
                        gatt?.close()
                    } catch (e: Exception) {}
                    bluetoothGatt = null
                }
            }
        }

        override fun onServicesDiscovered(gatt: BluetoothGatt?, status: Int) {
            if (status == 0) {
                log("✅ Services found")

                val service = gatt?.getService(SERVICE_UUID)
                if (service != null) {
                    dataCharacteristic = service.getCharacteristic(CHAR_UUID)
                    if (dataCharacteristic != null) {
                        log("✅ Characteristic found")
                        log("🎬 Requesting screen capture...")
                        requestScreenCapture()
                    } else {
                        log("❌ Characteristic NOT found")
                    }
                } else {
                    log("❌ Service NOT found")
                }
            } else {
                log("❌ Discovery failed: $status")
            }
        }
    }

    private fun requestScreenCapture() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            try {
                val intent = mediaProjectionManager?.createScreenCaptureIntent()
                if (intent != null) {
                    startActivityForResult(intent, REQUEST_MEDIA_PROJECTION)
                    return
                }
            } catch (e: Exception) {
                log("❌ Screen capture: ${e.message}")
            }
        }
        log("⚠️ Fallback to microphone")
        startAudioStream(null)
    }

    private fun startAudioStream(mediaProjection: android.media.projection.MediaProjection? = null) {
        if (isStreamingAudio) {
            log("⚠️ Already streaming")
            return
        }

        if (ActivityCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO) != PackageManager.PERMISSION_GRANTED) {
            log("❌ RECORD_AUDIO permission required!")
            return
        }

        isStreamingAudio = true
        log("\n🎤 Starting audio stream...")
        log("MediaProjection: ${mediaProjection != null}")

        try {
            val bufferSize = AudioRecord.getMinBufferSize(SAMPLE_RATE, CHANNEL_CONFIG, AUDIO_FORMAT)
            log("Buffer size: $bufferSize bytes")

            audioRecord = if (Build.VERSION.SDK_INT >= 29 && mediaProjection != null) {
                log("🎬 Attempting system audio capture...")
                try {
                    val config = android.media.AudioPlaybackCaptureConfiguration.Builder(mediaProjection)
                        .addMatchingUsage(android.media.AudioAttributes.USAGE_MEDIA)
                        .build()

                    log("✅ Config built")

                    val record = AudioRecord.Builder()
                        .setAudioSource(MediaRecorder.AudioSource.DEFAULT)
                        .setAudioFormat(
                            AudioFormat.Builder()
                                .setEncoding(AUDIO_FORMAT)
                                .setSampleRate(SAMPLE_RATE)
                                .setChannelMask(CHANNEL_CONFIG)
                                .build()
                        )
                        .setBufferSizeInBytes(bufferSize * 2)
                        .setAudioPlaybackCaptureConfig(config)
                        .build()

                    log("✅ AudioRecord created")
                    record
                } catch (e: Exception) {
                    log("❌ System audio: ${e.message}")
                    log("⚠️ Using microphone instead")
                    AudioRecord(
                        MediaRecorder.AudioSource.MIC,
                        SAMPLE_RATE,
                        CHANNEL_CONFIG,
                        AUDIO_FORMAT,
                        bufferSize * 2
                    )
                }
            } else {
                log("🎤 Using microphone (API ${Build.VERSION.SDK_INT})")
                AudioRecord(
                    MediaRecorder.AudioSource.MIC,
                    SAMPLE_RATE,
                    CHANNEL_CONFIG,
                    AUDIO_FORMAT,
                    bufferSize * 2
                )
            }

            if (audioRecord?.state != 1) {
                log("❌ AudioRecord state: ${audioRecord?.state} (expected 1)")
                isStreamingAudio = false
                return
            }

            audioRecord?.startRecording()
            log("✅ Recording started")
            log("📱 Play YouTube now!")

            audioThread = Thread {
                val buffer = ByteArray(1024)
                var chunks = 0
                var zeros = 0

                while (isStreamingAudio && audioRecord != null) {
                    try {
                        val bytesRead = audioRecord!!.read(buffer, 0, buffer.size)
                        if (bytesRead > 0) {
                            sendAudioData(buffer.copyOf(bytesRead))
                            chunks++

                            // Check if we're getting silence
                            var isSilent = true
                            for (i in 0 until minOf(bytesRead, 100)) {
                                if (buffer[i].toInt() != 0) {
                                    isSilent = false
                                    break
                                }
                            }

                            if (isSilent) {
                                zeros++
                            } else {
                                zeros = 0
                            }

                            if (chunks % 50 == 0) {
                                runOnUiThread {
                                    val silenceStatus = if (zeros > 20) "🔇 SILENCE" else "🔊 OK"
                                    log("Chunks: $chunks ($silenceStatus)")
                                }
                            }
                        }
                    } catch (e: Exception) {
                        log("❌ Read: ${e.message}")
                        break
                    }
                }
                log("🛑 Audio thread ended ($chunks chunks)")
            }
            audioThread?.start()

        } catch (e: Exception) {
            log("❌ Setup: ${e.message}")
            isStreamingAudio = false
        }
    }

    private fun sendAudioData(data: ByteArray) {
        if (dataCharacteristic == null || bluetoothGatt == null) return

        try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                if (ActivityCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED) {
                    return
                }
            }

            var offset = 0
            while (offset < data.size) {
                val chunkSize = minOf(512, data.size - offset)
                val chunk = data.sliceArray(offset until offset + chunkSize)

                dataCharacteristic!!.value = chunk
                bluetoothGatt!!.writeCharacteristic(dataCharacteristic!!)

                offset += chunkSize
            }
        } catch (e: Exception) {
            // Silent
        }
    }

    private fun stopAudioStream() {
        if (!isStreamingAudio) return

        isStreamingAudio = false
        log("\n🛑 Stopping audio...")

        try {
            audioRecord?.stop()
            audioRecord?.release()
            audioRecord = null
            audioThread?.join(1000)
            mediaProjection?.stop()
            log("✅ Stopped")
        } catch (e: Exception) {
            log("Error: ${e.message}")
        }
    }

    private fun disconnect() {
        log("\n→ Disconnecting...")
        stopAudioStream()

        try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                if (ActivityCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_CONNECT) == PackageManager.PERMISSION_GRANTED) {
                    bluetoothGatt?.disconnect()
                    bluetoothGatt?.close()
                }
            } else {
                bluetoothGatt?.disconnect()
                bluetoothGatt?.close()
            }
            bluetoothGatt = null
            addedDevices.clear()
            log("✅ Disconnected")
        } catch (e: Exception) {
            log("Error: ${e.message}")
        }
    }

    private fun log(msg: String) {
        Log.d("BLE", msg)
        runOnUiThread {
            val current = logText?.text.toString()
            logText?.text = if (current.isEmpty()) msg else "$current\n$msg"
            (logText?.parent?.parent as? ScrollView)?.fullScroll(ScrollView.FOCUS_DOWN)
        }
    }
}
