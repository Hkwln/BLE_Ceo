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
        log("BLE Headphones - v14.0")
        log("nRF Connect Permission Model")
        log("════════════════════════════════════════")

        mediaProjectionManager = getSystemService(Context.MEDIA_PROJECTION_SERVICE) as MediaProjectionManager

        val bluetoothManager = getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
        bluetoothAdapter = bluetoothManager.adapter

        if (bluetoothAdapter == null) {
            log("❌ NO BLUETOOTH SUPPORT")
            return
        }

        log("✅ Bluetooth available")

        // Enable Bluetooth if needed
        if (!bluetoothAdapter!!.isEnabled) {
            log("📡 Enabling Bluetooth...")
            try {
                bluetoothAdapter!!.enable()
            } catch (e: Exception) {
                log("${e.message}")
            }
        }

        // Request ALL permissions upfront (nRF Connect style)
        ensureAllPermissionsGranted()
    }

    /**
     * nRF Connect approach: Request ALL permissions needed upfront
     * This prevents "permission denied" errors later during scanning/connecting
     */
    private fun ensureAllPermissionsGranted() {
        val requiredPermissions = mutableListOf<String>()

        // Bluetooth permissions - ALWAYS needed
        requiredPermissions.add(Manifest.permission.BLUETOOTH)
        requiredPermissions.add(Manifest.permission.BLUETOOTH_ADMIN)

        // Android 12+ specific permissions
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            requiredPermissions.add(Manifest.permission.BLUETOOTH_SCAN)
            requiredPermissions.add(Manifest.permission.BLUETOOTH_CONNECT)
        }

        // Location - REQUIRED for BLE scanning (even though we don't use it)
        // This is Android's security model - can't scan BLE without location permission
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            requiredPermissions.add(Manifest.permission.ACCESS_FINE_LOCATION)
            requiredPermissions.add(Manifest.permission.ACCESS_COARSE_LOCATION)
        }

        // Audio capture
        requiredPermissions.add(Manifest.permission.RECORD_AUDIO)

        // Check which permissions are missing
        val missingPermissions = mutableListOf<String>()
        for (permission in requiredPermissions) {
            if (ContextCompat.checkSelfPermission(this, permission) != PackageManager.PERMISSION_GRANTED) {
                missingPermissions.add(permission)
            }
        }

        if (missingPermissions.isNotEmpty()) {
            log("🔐 Requesting ${missingPermissions.size} permissions:")
            for (perm in missingPermissions) {
                log("   ✓ ${formatPermissionName(perm)}")
            }
            // Request ALL missing permissions at once (like nRF Connect)
            ActivityCompat.requestPermissions(
                this,
                missingPermissions.toTypedArray(),
                PERMISSION_REQUEST_CODE
            )
        } else {
            log("✅ All permissions already granted!")
        }
    }

    private fun formatPermissionName(permission: String): String {
        return permission.split(".").last().replace("_", " ").lowercase()
    }

    override fun onRequestPermissionsResult(
        requestCode: Int,
        permissions: Array<String>,
        grantResults: IntArray
    ) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)

        if (requestCode == PERMISSION_REQUEST_CODE) {
            log("\n📋 Permission Results:")
            var allGranted = true

            for (i in permissions.indices) {
                val permission = permissions[i]
                val granted = grantResults[i] == PackageManager.PERMISSION_GRANTED
                val icon = if (granted) "✅" else "❌"
                log("$icon ${formatPermissionName(permission)}")

                if (!granted) {
                    allGranted = false
                }
            }

            if (allGranted) {
                log("\n🎉 All permissions granted!")
            } else {
                log("\n⚠️ Some permissions were denied")
                log("App may not work correctly")
            }
        }
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        super.onActivityResult(requestCode, resultCode, data)
        if (requestCode == REQUEST_MEDIA_PROJECTION && resultCode == RESULT_OK) {
            log("✅ Screen capture authorized")
        }
    }

    private fun onScanClicked() {
        if (isScanning) {
            stopScan()
        } else {
            startScan()
        }
    }

    private fun startScan() {
        log("\n🔍 SCAN START")

        if (!bluetoothAdapter!!.isEnabled) {
            log("❌ Bluetooth is OFF")
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
            // Check permission before scanning (like nRF Connect)
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                if (ActivityCompat.checkSelfPermission(
                        this,
                        Manifest.permission.BLUETOOTH_SCAN
                    ) != PackageManager.PERMISSION_GRANTED
                ) {
                    log("❌ BLUETOOTH_SCAN permission required!")
                    isScanning = false
                    return
                }
            }

            bluetoothAdapter!!.startLeScan(callback)
            log("✅ Scanning for devices...")

            handler.postDelayed({ stopScan() }, 10000)

        } catch (e: SecurityException) {
            log("❌ Security error: ${e.message}")
            isScanning = false
        } catch (e: Exception) {
            log("❌ Scan error: ${e.message}")
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
        log("📱 Found: ${device.name}")
    }

    private fun connectToDevice(device: BluetoothDevice) {
        log("\n→ Connecting to ${device.name}...")
        stopScan()
        connectedDevice = device

        try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                if (ActivityCompat.checkSelfPermission(
                        this,
                        Manifest.permission.BLUETOOTH_CONNECT
                    ) != PackageManager.PERMISSION_GRANTED
                ) {
                    log("❌ BLUETOOTH_CONNECT permission required!")
                    return
                }
            }

            bluetoothGatt = device.connectGatt(this, false, gattCallback)
            log("✅ Connecting...")

        } catch (e: SecurityException) {
            log("❌ Security error: ${e.message}")
        } catch (e: Exception) {
            log("❌ Connection error: ${e.message}")
        }
    }

    private val gattCallback = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(gatt: BluetoothGatt?, status: Int, newState: Int) {
            when (newState) {
                2 -> {
                    log("✅ CONNECTED!")
                    try {
                        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                            if (ActivityCompat.checkSelfPermission(
                                    this@MainActivity,
                                    Manifest.permission.BLUETOOTH_CONNECT
                                ) == PackageManager.PERMISSION_GRANTED
                            ) {
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
                    } catch (e: Exception) {
                        // Silent
                    }
                    bluetoothGatt = null
                }
            }
        }

        override fun onServicesDiscovered(gatt: BluetoothGatt?, status: Int) {
            if (status == 0) {
                log("✅ Services discovered")

                val service = gatt?.getService(SERVICE_UUID)
                if (service != null) {
                    dataCharacteristic = service.getCharacteristic(CHAR_UUID)
                    if (dataCharacteristic != null) {
                        log("✅ Found data characteristic")
                        log("🎵 Starting audio stream...")
                        requestScreenCapture()
                    } else {
                        log("❌ Characteristic not found")
                    }
                } else {
                    log("❌ Service not found")
                }
            } else {
                log("❌ Service discovery failed")
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
                log("Screen capture not available")
            }
        }
        startAudioStream(null)
    }

    private fun startAudioStream(mediaProjection: android.media.projection.MediaProjection? = null) {
        if (isStreamingAudio) return

        // Check RECORD_AUDIO permission
        if (ActivityCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO) != PackageManager.PERMISSION_GRANTED) {
            log("❌ RECORD_AUDIO permission required!")
            return
        }

        isStreamingAudio = true
        log("🎤 Initializing audio...")

        try {
            val bufferSize = AudioRecord.getMinBufferSize(SAMPLE_RATE, CHANNEL_CONFIG, AUDIO_FORMAT)

            audioRecord = if (Build.VERSION.SDK_INT >= 29 && mediaProjection != null) {
                try {
                    val config = android.media.AudioPlaybackCaptureConfiguration.Builder(mediaProjection)
                        .addMatchingUsage(android.media.AudioAttributes.USAGE_MEDIA)
                        .build()

                    AudioRecord.Builder()
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
                } catch (e: Exception) {
                    log("System audio not available, using microphone")
                    AudioRecord(
                        MediaRecorder.AudioSource.MIC,
                        SAMPLE_RATE,
                        CHANNEL_CONFIG,
                        AUDIO_FORMAT,
                        bufferSize * 2
                    )
                }
            } else {
                AudioRecord(
                    MediaRecorder.AudioSource.MIC,
                    SAMPLE_RATE,
                    CHANNEL_CONFIG,
                    AUDIO_FORMAT,
                    bufferSize * 2
                )
            }

            if (audioRecord?.state != 1) {
                log("❌ AudioRecord initialization failed")
                isStreamingAudio = false
                return
            }

            audioRecord?.startRecording()
            log("✅ Recording started")
            log("📱 Play YouTube now!")

            audioThread = Thread {
                val buffer = ByteArray(1024)
                var chunks = 0

                while (isStreamingAudio && audioRecord != null) {
                    try {
                        val bytesRead = audioRecord!!.read(buffer, 0, buffer.size)
                        if (bytesRead > 0) {
                            sendAudioData(buffer.copyOf(bytesRead))
                            chunks++
                            if (chunks % 50 == 0) {
                                runOnUiThread {
                                    log("🔊 ${chunks} chunks sent")
                                }
                            }
                        }
                    } catch (e: Exception) {
                        break
                    }
                }
            }
            audioThread?.start()

        } catch (e: Exception) {
            log("❌ Audio error: ${e.message}")
            isStreamingAudio = false
        }
    }

    private fun sendAudioData(data: ByteArray) {
        if (dataCharacteristic == null || bluetoothGatt == null) return

        try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                if (ActivityCompat.checkSelfPermission(
                        this,
                        Manifest.permission.BLUETOOTH_CONNECT
                    ) != PackageManager.PERMISSION_GRANTED
                ) {
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
        log("🛑 Stopping audio...")

        try {
            audioRecord?.stop()
            audioRecord?.release()
            audioRecord = null
            audioThread?.join(1000)
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
                if (ActivityCompat.checkSelfPermission(
                        this,
                        Manifest.permission.BLUETOOTH_CONNECT
                    ) == PackageManager.PERMISSION_GRANTED
                ) {
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
