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

        try {
            logText = findViewById(R.id.log_text)
            devicesContainer = findViewById(R.id.devices_container)
            findViewById<Button>(R.id.scan_button).setOnClickListener { onScanClicked() }
            findViewById<Button>(R.id.disconnect_button).setOnClickListener { disconnect() }

            log("════════════════════════════════════════")
            log("BLE Headphones - v16.0 CRASHFIX")
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

            if (!bluetoothAdapter!!.isEnabled) {
                log("📡 Enabling Bluetooth...")
                try {
                    bluetoothAdapter!!.enable()
                } catch (e: Exception) {
                    log("${e.message}")
                }
            }

            ensureAllPermissionsGranted()
        } catch (e: Exception) {
            log("CRASH in onCreate: ${e.message}")
            e.printStackTrace()
        }
    }

    private fun ensureAllPermissionsGranted() {
        try {
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
        } catch (e: Exception) {
            log("CRASH in ensureAllPermissionsGranted: ${e.message}")
        }
    }

    override fun onRequestPermissionsResult(
        requestCode: Int,
        permissions: Array<String>,
        grantResults: IntArray
    ) {
        try {
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
        } catch (e: Exception) {
            log("CRASH in onRequestPermissionsResult: ${e.message}")
        }
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        try {
            super.onActivityResult(requestCode, resultCode, data)
            log("onActivityResult: code=$requestCode result=$resultCode data=${data != null}")
            
            if (requestCode == REQUEST_MEDIA_PROJECTION) {
                if (resultCode == RESULT_OK && data != null) {
                    try {
                        log("✅ Screen capture dialog returned OK")
                        mediaProjection = mediaProjectionManager?.getMediaProjection(resultCode, data)
                        if (mediaProjection != null) {
                            log("✅ MediaProjection acquired successfully")
                            startAudioStream(mediaProjection)
                        } else {
                            log("❌ MediaProjection is NULL")
                            startAudioStreamMicrophone()
                        }
                    } catch (e: Exception) {
                        log("❌ MediaProjection error: ${e.message}")
                        startAudioStreamMicrophone()
                    }
                } else {
                    log("⚠️ User cancelled screen capture (result=$resultCode)")
                    startAudioStreamMicrophone()
                }
            }
        } catch (e: Exception) {
            log("CRASH in onActivityResult: ${e.message}")
            e.printStackTrace()
        }
    }

    private fun onScanClicked() {
        try {
            if (isScanning) stopScan() else startScan()
        } catch (e: Exception) {
            log("CRASH in onScanClicked: ${e.message}")
        }
    }

    private fun startScan() {
        try {
            log("\n🔍 SCAN START")

            if (!bluetoothAdapter!!.isEnabled) {
                log("❌ Bluetooth OFF")
                return
            }

            isScanning = true
            devicesContainer?.removeAllViews()

            val callback = object : android.bluetooth.BluetoothAdapter.LeScanCallback {
                override fun onLeScan(device: BluetoothDevice?, rssi: Int, scanRecord: ByteArray?) {
                    try {
                        if (device != null && device.address !in addedDevices) {
                            runOnUiThread {
                                addDeviceButton(device)
                            }
                        }
                    } catch (e: Exception) {
                        log("CRASH in onLeScan: ${e.message}")
                    }
                }
            }

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
            log("CRASH in startScan: ${e.message}")
            isScanning = false
        }
    }

    private fun stopScan() {
        try {
            if (isScanning) {
                isScanning = false
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                    if (ActivityCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_SCAN) == PackageManager.PERMISSION_GRANTED) {
                        bluetoothAdapter!!.stopLeScan(null)
                    }
                } else {
                    bluetoothAdapter!!.stopLeScan(null)
                }
                log("🛑 Scan stopped")
            }
        } catch (e: Exception) {
            log("Error in stopScan: ${e.message}")
        }
    }

    private var addedDevices = mutableSetOf<String>()

    private fun addDeviceButton(device: BluetoothDevice) {
        try {
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
        } catch (e: Exception) {
            log("CRASH in addDeviceButton: ${e.message}")
        }
    }

    private fun connectToDevice(device: BluetoothDevice) {
        try {
            log("\n→ Connecting to ${device.name}...")
            stopScan()
            connectedDevice = device

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                if (ActivityCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED) {
                    log("❌ BLUETOOTH_CONNECT denied")
                    return
                }
            }

            bluetoothGatt = device.connectGatt(this, false, gattCallback)
            log("✅ Connecting...")

        } catch (e: Exception) {
            log("CRASH in connectToDevice: ${e.message}")
        }
    }

    private val gattCallback = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(gatt: BluetoothGatt?, status: Int, newState: Int) {
            try {
                when (newState) {
                    2 -> {
                        log("✅ CONNECTED!")
                        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                            if (ActivityCompat.checkSelfPermission(this@MainActivity, Manifest.permission.BLUETOOTH_CONNECT) == PackageManager.PERMISSION_GRANTED) {
                                gatt?.discoverServices()
                            }
                        } else {
                            gatt?.discoverServices()
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
            } catch (e: Exception) {
                log("CRASH in onConnectionStateChange: ${e.message}")
            }
        }

        override fun onServicesDiscovered(gatt: BluetoothGatt?, status: Int) {
            try {
                if (status == 0) {
                    log("✅ Services found")

                    val service = gatt?.getService(SERVICE_UUID)
                    if (service != null) {
                        dataCharacteristic = service.getCharacteristic(CHAR_UUID)
                        if (dataCharacteristic != null) {
                            log("✅ Characteristic found")
                            log("🎤 Starting audio stream (microphone only)")
                            startAudioStreamMicrophone()
                        } else {
                            log("❌ Characteristic NOT found")
                        }
                    } else {
                        log("❌ Service NOT found")
                    }
                } else {
                    log("❌ Discovery failed: $status")
                }
            } catch (e: Exception) {
                log("CRASH in onServicesDiscovered: ${e.message}")
            }
        }
    }

    private fun startAudioStreamMicrophone() {
        try {
            startAudioStream(null)
        } catch (e: Exception) {
            log("CRASH in startAudioStreamMicrophone: ${e.message}")
        }
    }

    private fun startAudioStream(mediaProjection: android.media.projection.MediaProjection? = null) {
        try {
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
            log("Using: ${if (mediaProjection != null) "System Audio" else "Microphone"}")

            val bufferSize = AudioRecord.getMinBufferSize(SAMPLE_RATE, CHANNEL_CONFIG, AUDIO_FORMAT)
            log("Buffer size: $bufferSize")

            audioRecord = try {
                log("🎵 Trying VOICE_DOWNLINK (YouTube capture)...")
                AudioRecord(
                    MediaRecorder.AudioSource.VOICE_DOWNLINK,
                    SAMPLE_RATE,
                    CHANNEL_CONFIG,
                    AUDIO_FORMAT,
                    bufferSize * 2
                )
            } catch (e: Exception) {
                log("VOICE_DOWNLINK not available: ${e.message}")
                AudioRecord(
                    MediaRecorder.AudioSource.MIC,
                    SAMPLE_RATE,
                    CHANNEL_CONFIG,
                    AUDIO_FORMAT,
                    bufferSize * 2
                )
            }

            if (audioRecord?.state != 1) {
                log("❌ AudioRecord state: ${audioRecord?.state}")
                isStreamingAudio = false
                return
            }

            audioRecord?.startRecording()
            log("✅ Recording started")
            log("🎵 Play YouTube now!")

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
                                    log("Sent: $chunks chunks")
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
            log("CRASH in startAudioStream: ${e.message}")
            e.printStackTrace()
            isStreamingAudio = false
        }
    }

    private fun sendAudioData(data: ByteArray) {
        try {
            if (dataCharacteristic == null || bluetoothGatt == null) return

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
        try {
            if (!isStreamingAudio) return

            isStreamingAudio = false
            log("\n🛑 Stopping audio...")

            audioRecord?.stop()
            audioRecord?.release()
            audioRecord = null
            audioThread?.join(1000)
            mediaProjection?.stop()
            log("✅ Stopped")
        } catch (e: Exception) {
            log("Error in stopAudioStream: ${e.message}")
        }
    }

    private fun disconnect() {
        try {
            log("\n→ Disconnecting...")
            stopAudioStream()

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
            log("Error in disconnect: ${e.message}")
        }
    }

    private fun log(msg: String) {
        Log.d("BLE", msg)
        try {
            runOnUiThread {
                val current = logText?.text.toString()
                logText?.text = if (current.isEmpty()) msg else "$current\n$msg"
                (logText?.parent?.parent as? ScrollView)?.fullScroll(ScrollView.FOCUS_DOWN)
            }
        } catch (e: Exception) {
            Log.e("BLE", "Error logging: ${e.message}")
        }
    }
}
