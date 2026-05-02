package com.example.bleheadphones

import android.Manifest
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothManager
import android.content.Context
import android.content.pm.PackageManager
import android.media.AudioAttributes
import android.media.AudioFormat
import android.media.AudioRecord
import android.media.MediaRecorder
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
    private val REQUEST_PERMISSIONS = 999
    private val REQUEST_RECORD_AUDIO = 1000
    
    private var connectedDevice: BluetoothDevice? = null
    private var bluetoothGatt: BluetoothGatt? = null
    private var dataCharacteristic: BluetoothGattCharacteristic? = null
    
    // Audio
    private var audioRecord: AudioRecord? = null
    private var isStreamingAudio = false
    private var audioThread: Thread? = null
    
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
        log("BLE Headphones App - v10.0 AUDIO FIX")
        log("════════════════════════════════════════")

        val bluetoothManager = getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
        bluetoothAdapter = bluetoothManager.adapter

        if (bluetoothAdapter == null) {
            log("❌ NO BLUETOOTH")
            return
        }

        log("✅ Bluetooth adapter found")
        
        if (!bluetoothAdapter!!.isEnabled) {
            log("⚠️  Bluetooth OFF - enabling...")
            try {
                bluetoothAdapter!!.enable()
                Thread.sleep(1000)
            } catch (e: Exception) {
                log("Could not enable: ${e.message}")
            }
        }
        
        if (bluetoothAdapter!!.isEnabled) {
            log("✅ Bluetooth is ON")
        } else {
            log("❌ Bluetooth is OFF")
        }

        checkAndRequestPermissions()
    }

    private fun checkAndRequestPermissions() {
        val requiredPermissions = buildList {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                add(Manifest.permission.BLUETOOTH_SCAN)
                add(Manifest.permission.BLUETOOTH_CONNECT)
            }
            add(Manifest.permission.ACCESS_FINE_LOCATION)
            add(Manifest.permission.ACCESS_COARSE_LOCATION)
            add(Manifest.permission.RECORD_AUDIO)  // CRITICAL!
        }

        val missingPermissions = requiredPermissions.filter {
            ContextCompat.checkSelfPermission(this, it) != PackageManager.PERMISSION_GRANTED
        }

        if (missingPermissions.isNotEmpty()) {
            log("🔐 Requesting permissions:")
            missingPermissions.forEach { 
                log("   - ${it.split(".").last()}")
            }
            ActivityCompat.requestPermissions(
                this,
                missingPermissions.toTypedArray(),
                REQUEST_PERMISSIONS
            )
        } else {
            log("✅ All permissions OK")
        }
    }

    override fun onRequestPermissionsResult(
        requestCode: Int,
        permissions: Array<String>,
        grantResults: IntArray
    ) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        
        if (requestCode == REQUEST_PERMISSIONS) {
            log("📋 Permission results:")
            for (i in permissions.indices) {
                val perm = permissions[i].split(".").last()
                val granted = grantResults.getOrNull(i) == PackageManager.PERMISSION_GRANTED
                log("  ${if(granted) "✅" else "❌"} $perm")
            }
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
        log("\n🔍 SCAN START (10 seconds)")

        if (!bluetoothAdapter!!.isEnabled) {
            log("❌ Bluetooth OFF")
            return
        }

        isScanning = true
        devicesContainer?.removeAllViews()

        val callback = object : android.bluetooth.BluetoothAdapter.LeScanCallback {
            override fun onLeScan(device: BluetoothDevice?, rssi: Int, scanRecord: ByteArray?) {
                if (device != null && !isDeviceAlreadyAdded(device.address)) {
                    runOnUiThread {
                        addDeviceButton(device)
                    }
                }
            }
        }

        try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                if (ActivityCompat.checkSelfPermission(
                        this,
                        Manifest.permission.BLUETOOTH_SCAN
                    ) == PackageManager.PERMISSION_GRANTED
                ) {
                    bluetoothAdapter!!.startLeScan(callback)
                    log("✅ Scan running...")
                }
            } else {
                bluetoothAdapter!!.startLeScan(callback)
                log("✅ Scan running...")
            }

            handler.postDelayed({
                stopScan()
            }, 10000)

        } catch (e: Exception) {
            log("❌ Scan error: ${e.message}")
        }
    }

    private fun stopScan() {
        if (isScanning) {
            isScanning = false
            try {
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                    if (ActivityCompat.checkSelfPermission(
                            this,
                            Manifest.permission.BLUETOOTH_SCAN
                        ) == PackageManager.PERMISSION_GRANTED
                    ) {
                        bluetoothAdapter!!.stopLeScan(null)
                    }
                } else {
                    bluetoothAdapter!!.stopLeScan(null)
                }
                log("🛑 Scan stopped")
            } catch (e: Exception) {
                log("Error stopping scan: ${e.message}")
            }
        }
    }

    private var addedDevices = mutableSetOf<String>()

    private fun isDeviceAlreadyAdded(address: String): Boolean {
        return addedDevices.contains(address)
    }

    private fun addDeviceButton(device: BluetoothDevice) {
        addedDevices.add(device.address)
        
        val button = Button(this).apply {
            text = "${device.name ?: "Unknown"}\n${device.address}"
            setOnClickListener { connectToDevice(device) }
            layoutParams = LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                120
            ).apply {
                setMargins(8, 8, 8, 8)
            }
        }
        devicesContainer?.addView(button)
        log("📱 Found: ${device.name} (${device.address})")
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
                    log("❌ BLUETOOTH_CONNECT permission denied")
                    return
                }
            }

            bluetoothGatt = device.connectGatt(this, false, gattCallback)
            log("✅ Connection started...")

        } catch (e: Exception) {
            log("❌ Connection error: ${e.message}")
        }
    }

    private val gattCallback = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(gatt: BluetoothGatt?, status: Int, newState: Int) {
            super.onConnectionStateChange(gatt, status, newState)
            
            when (newState) {
                android.bluetooth.BluetoothProfile.STATE_CONNECTED -> {
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
                        log("Error discovering services: ${e.message}")
                    }
                }
                android.bluetooth.BluetoothProfile.STATE_DISCONNECTED -> {
                    log("❌ DISCONNECTED")
                    stopAudioStream()
                    bluetoothGatt?.close()
                    bluetoothGatt = null
                }
            }
        }

        override fun onServicesDiscovered(gatt: BluetoothGatt?, status: Int) {
            super.onServicesDiscovered(gatt, status)
            
            if (status == android.bluetooth.BluetoothGatt.GATT_SUCCESS) {
                log("✅ Services discovered")
                
                val service = gatt?.getService(SERVICE_UUID)
                if (service != null) {
                    dataCharacteristic = service.getCharacteristic(CHAR_UUID)
                    if (dataCharacteristic != null) {
                        log("✅ Found data characteristic!")
                        log("📊 Ready for audio streaming!")
                        
                        // Check RECORD_AUDIO permission before starting
                        if (ContextCompat.checkSelfPermission(
                                this@MainActivity,
                                Manifest.permission.RECORD_AUDIO
                            ) == PackageManager.PERMISSION_GRANTED
                        ) {
                            log("\n🎵 Starting audio capture...")
                            startAudioStream()
                        } else {
                            log("❌ RECORD_AUDIO permission NOT granted!")
                            log("📱 Go to Settings → Apps → Permissions → Microphone")
                            log("   and enable for this app")
                        }
                    } else {
                        log("❌ Characteristic not found")
                    }
                } else {
                    log("❌ Service not found")
                }
            } else {
                log("❌ Service discovery failed: $status")
            }
        }
    }

    private fun startAudioStream() {
        if (isStreamingAudio) return
        
        // Double-check permission
        if (ContextCompat.checkSelfPermission(
                this,
                Manifest.permission.RECORD_AUDIO
            ) != PackageManager.PERMISSION_GRANTED
        ) {
            log("❌ RECORD_AUDIO permission denied - cannot start!")
            return
        }
        
        isStreamingAudio = true
        log("🎤 Initializing audio capture...")

        try {
            val bufferSize = AudioRecord.getMinBufferSize(SAMPLE_RATE, CHANNEL_CONFIG, AUDIO_FORMAT)
            log("📊 Buffer size: $bufferSize bytes")

            audioRecord = AudioRecord(
                MediaRecorder.AudioSource.MIC,
                SAMPLE_RATE,
                CHANNEL_CONFIG,
                AUDIO_FORMAT,
                bufferSize * 4  // Increase buffer
            )

            if (audioRecord?.state != AudioRecord.STATE_INITIALIZED) {
                log("❌ AudioRecord initialization FAILED!")
                isStreamingAudio = false
                return
            }

            audioRecord?.startRecording()
            log("✅ Audio recording started!")
            log("🔊 Streaming audio to BLE device...")

            // Start streaming in background thread
            audioThread = Thread {
                val buffer = ByteArray(1024)  // Bigger chunks for better streaming
                var chunksLogged = 0
                
                while (isStreamingAudio && audioRecord != null) {
                    try {
                        val bytesRead = audioRecord!!.read(buffer, 0, buffer.size)
                        
                        if (bytesRead > 0) {
                            // Send to BLE device
                            sendAudioData(buffer.copyOf(bytesRead))
                            
                            chunksLogged++
                            if (chunksLogged % 50 == 0) {
                                runOnUiThread {
                                    log("🔊 Streamed $chunksLogged audio chunks (~${chunksLogged * 1024 / 1024}MB)")
                                }
                            }
                        }
                    } catch (e: Exception) {
                        log("❌ Audio read error: ${e.message}")
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

            // Split into BLE-safe chunks (max 512 bytes per write)
            var offset = 0
            while (offset < data.size) {
                val chunkSize = minOf(512, data.size - offset)
                val chunk = data.sliceArray(offset until offset + chunkSize)
                
                dataCharacteristic!!.value = chunk
                bluetoothGatt!!.writeCharacteristic(dataCharacteristic!!)
                
                offset += chunkSize
            }
            
        } catch (e: Exception) {
            // Silent - too many log messages
        }
    }

    private fun stopAudioStream() {
        if (!isStreamingAudio) return
        
        isStreamingAudio = false
        log("🛑 Stopping audio stream...")

        try {
            audioRecord?.stop()
            audioRecord?.release()
            audioRecord = null
            audioThread?.join(1000)
            log("✅ Audio stream stopped")
        } catch (e: Exception) {
            log("Error stopping audio: ${e.message}")
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
            connectedDevice = null
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
