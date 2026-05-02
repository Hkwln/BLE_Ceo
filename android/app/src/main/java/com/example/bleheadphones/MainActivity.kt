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
    private val REQUEST_PERMISSIONS = 100
    private val REQUEST_MEDIA_PROJECTION = 101
    
    private var connectedDevice: BluetoothDevice? = null
    private var bluetoothGatt: BluetoothGatt? = null
    private var dataCharacteristic: BluetoothGattCharacteristic? = null
    
    // Audio Capture
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
        log("BLE Headphones - v13.0")
        log("Simple Audio Streaming")
        log("════════════════════════════════════════")

        mediaProjectionManager = getSystemService(Context.MEDIA_PROJECTION_SERVICE) as MediaProjectionManager

        val bluetoothManager = getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
        bluetoothAdapter = bluetoothManager.adapter

        if (bluetoothAdapter == null) {
            log("❌ NO BLUETOOTH")
            return
        }

        log("✅ Bluetooth adapter found")
        
        if (!bluetoothAdapter!!.isEnabled) {
            log("Enabling Bluetooth...")
            try {
                bluetoothAdapter!!.enable()
            } catch (e: Exception) {
                log("${e.message}")
            }
        }

        requestMinimalPermissions()
    }

    private fun requestMinimalPermissions() {
        // Only ask for what we absolutely need
        val permsNeeded = mutableListOf<String>()

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            if (ContextCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_SCAN) != PackageManager.PERMISSION_GRANTED) {
                permsNeeded.add(Manifest.permission.BLUETOOTH_SCAN)
            }
            if (ContextCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED) {
                permsNeeded.add(Manifest.permission.BLUETOOTH_CONNECT)
            }
        }

        if (ContextCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO) != PackageManager.PERMISSION_GRANTED) {
            permsNeeded.add(Manifest.permission.RECORD_AUDIO)
        }

        if (permsNeeded.isNotEmpty()) {
            log("Requesting: ${permsNeeded.size} permissions...")
            ActivityCompat.requestPermissions(this, permsNeeded.toTypedArray(), REQUEST_PERMISSIONS)
        } else {
            log("✅ Permissions ready")
        }
    }

    override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<String>, grantResults: IntArray) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        
        if (requestCode == REQUEST_PERMISSIONS) {
            var allGranted = true
            for (i in permissions.indices) {
                val granted = grantResults[i] == PackageManager.PERMISSION_GRANTED
                if (!granted) allGranted = false
                log("${if(granted) "✅" else "❌"} ${permissions[i].split(".").last()}")
            }
            if (allGranted) {
                log("Ready to scan!")
            }
        }
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        super.onActivityResult(requestCode, resultCode, data)
        if (requestCode == REQUEST_MEDIA_PROJECTION && resultCode == RESULT_OK) {
            log("Screen capture OK")
        }
    }

    private fun onScanClicked() {
        if (isScanning) stopScan() else startScan()
    }

    private fun startScan() {
        log("\n🔍 SCANNING...")
        if (!bluetoothAdapter!!.isEnabled) {
            log("Bluetooth OFF")
            return
        }

        isScanning = true
        devicesContainer?.removeAllViews()

        val callback = object : android.bluetooth.BluetoothAdapter.LeScanCallback {
            override fun onLeScan(device: BluetoothDevice?, rssi: Int, scanRecord: ByteArray?) {
                if (device != null && device.address !in addedDevices) {
                    runOnUiThread { addDeviceButton(device) }
                }
            }
        }

        try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S && 
                ActivityCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_SCAN) == PackageManager.PERMISSION_GRANTED) {
                bluetoothAdapter!!.startLeScan(callback)
            } else if (Build.VERSION.SDK_INT < Build.VERSION_CODES.S) {
                bluetoothAdapter!!.startLeScan(callback)
            }
            log("Scanning...")

            handler.postDelayed({ stopScan() }, 10000)
        } catch (e: Exception) {
            log("Scan error: ${e.message}")
        }
    }

    private fun stopScan() {
        if (isScanning) {
            isScanning = false
            try {
                bluetoothAdapter?.stopLeScan(null)
                log("Scan stopped")
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
            layoutParams = LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, 100).apply {
                setMargins(8, 8, 8, 8)
            }
        }
        devicesContainer?.addView(button)
        log("Found: ${device.name}")
    }

    private fun connectToDevice(device: BluetoothDevice) {
        log("Connecting to ${device.name}...")
        stopScan()
        connectedDevice = device

        try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                if (ActivityCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED) {
                    log("Need BLUETOOTH_CONNECT permission")
                    return
                }
            }

            bluetoothGatt = device.connectGatt(this, false, gattCallback)
        } catch (e: Exception) {
            log("Connect error: ${e.message}")
        }
    }

    private val gattCallback = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(gatt: BluetoothGatt?, status: Int, newState: Int) {
            when (newState) {
                2 -> {
                    log("CONNECTED!")
                    try {
                        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                            if (ActivityCompat.checkSelfPermission(this@MainActivity, Manifest.permission.BLUETOOTH_CONNECT) == PackageManager.PERMISSION_GRANTED) {
                                gatt?.discoverServices()
                            }
                        } else {
                            gatt?.discoverServices()
                        }
                    } catch (e: Exception) {
                        log("Discover error: ${e.message}")
                    }
                }
                0 -> {
                    log("DISCONNECTED")
                    stopAudioStream()
                    gatt?.close()
                    bluetoothGatt = null
                }
            }
        }

        override fun onServicesDiscovered(gatt: BluetoothGatt?, status: Int) {
            if (status == 0) {
                log("Services found")
                val service = gatt?.getService(SERVICE_UUID)
                if (service != null) {
                    dataCharacteristic = service.getCharacteristic(CHAR_UUID)
                    if (dataCharacteristic != null) {
                        log("Ready for audio!")
                        log("Starting audio stream...")
                        requestScreenCapture()
                    } else {
                        log("Characteristic not found")
                    }
                } else {
                    log("Service not found")
                }
            } else {
                log("Discovery failed")
            }
        }
    }

    private fun requestScreenCapture() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            val intent = mediaProjectionManager?.createScreenCaptureIntent()
            if (intent != null) {
                startActivityForResult(intent, REQUEST_MEDIA_PROJECTION)
                return
            }
        }
        startAudioStream(null)
    }

    private fun startAudioStream(mediaProjection: android.media.projection.MediaProjection? = null) {
        if (isStreamingAudio) return
        
        if (ContextCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO) != PackageManager.PERMISSION_GRANTED) {
            log("Need RECORD_AUDIO permission")
            return
        }
        
        isStreamingAudio = true
        log("Starting audio capture...")

        try {
            val bufferSize = AudioRecord.getMinBufferSize(SAMPLE_RATE, CHANNEL_CONFIG, AUDIO_FORMAT)

            audioRecord = if (Build.VERSION.SDK_INT >= 29 && mediaProjection != null) {
                try {
                    val config = android.media.AudioPlaybackCaptureConfiguration.Builder(mediaProjection)
                        .addMatchingUsage(android.media.AudioAttributes.USAGE_MEDIA)
                        .build()
                    AudioRecord.Builder()
                        .setAudioSource(MediaRecorder.AudioSource.DEFAULT)
                        .setAudioFormat(AudioFormat.Builder().setEncoding(AUDIO_FORMAT).setSampleRate(SAMPLE_RATE).setChannelMask(CHANNEL_CONFIG).build())
                        .setBufferSizeInBytes(bufferSize * 2)
                        .setAudioPlaybackCaptureConfig(config)
                        .build()
                } catch (e: Exception) {
                    log("System audio failed: ${e.message}")
                    AudioRecord(MediaRecorder.AudioSource.MIC, SAMPLE_RATE, CHANNEL_CONFIG, AUDIO_FORMAT, bufferSize * 2)
                }
            } else {
                AudioRecord(MediaRecorder.AudioSource.MIC, SAMPLE_RATE, CHANNEL_CONFIG, AUDIO_FORMAT, bufferSize * 2)
            }

            if (audioRecord?.state != 1) {
                log("AudioRecord failed")
                isStreamingAudio = false
                return
            }

            audioRecord?.startRecording()
            log("Recording started!")
            log("Play YouTube now!")

            audioThread = Thread {
                val buffer = ByteArray(1024)
                var chunks = 0
                while (isStreamingAudio && audioRecord != null) {
                    try {
                        val bytesRead = audioRecord!!.read(buffer, 0, buffer.size)
                        if (bytesRead > 0) {
                            sendAudioData(buffer.copyOf(bytesRead))
                            chunks++
                            if (chunks % 100 == 0) {
                                runOnUiThread { log("Streamed: $chunks chunks") }
                            }
                        }
                    } catch (e: Exception) {
                        break
                    }
                }
            }
            audioThread?.start()

        } catch (e: Exception) {
            log("Audio error: ${e.message}")
            isStreamingAudio = false
        }
    }

    private fun sendAudioData(data: ByteArray) {
        if (dataCharacteristic == null || bluetoothGatt == null) return
        try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                if (ActivityCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED) return
            }

            var offset = 0
            while (offset < data.size) {
                val chunk = data.sliceArray(offset until minOf(offset + 512, data.size))
                dataCharacteristic!!.value = chunk
                bluetoothGatt!!.writeCharacteristic(dataCharacteristic!!)
                offset += 512
            }
        } catch (e: Exception) {
            // Silent
        }
    }

    private fun stopAudioStream() {
        if (!isStreamingAudio) return
        isStreamingAudio = false
        log("Stopping...")
        try {
            audioRecord?.stop()
            audioRecord?.release()
            audioRecord = null
            audioThread?.join(1000)
        } catch (e: Exception) {
            log("Error: ${e.message}")
        }
    }

    private fun disconnect() {
        log("Disconnecting...")
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
            log("Disconnected")
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
