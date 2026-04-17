package com.example.bleheadphones

import android.Manifest
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothManager
import android.content.Context
import android.content.pm.PackageManager
import android.media.projection.MediaProjectionManager
import android.os.Build
import android.os.Bundle
import android.util.Log
import android.widget.Button
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat

class MainActivity : AppCompatActivity() {

    private lateinit var bleManager: BLEManager
    private lateinit var usbSerialManager: USBSerialManager
    private lateinit var audioCaptureManager: AudioCaptureManager
    private lateinit var handshakeProtocol: HandshakeProtocol

    private lateinit var statusText: TextView
    private lateinit var connectButton: Button
    private lateinit var disconnectButton: Button

    private val PERMISSIONS = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
        arrayOf(
            Manifest.permission.BLUETOOTH_SCAN,
            Manifest.permission.BLUETOOTH_CONNECT,
            Manifest.permission.RECORD_AUDIO,
            Manifest.permission.READ_PHONE_STATE
        )
    } else {
        arrayOf(
            Manifest.permission.BLUETOOTH,
            Manifest.permission.BLUETOOTH_ADMIN,
            Manifest.permission.RECORD_AUDIO,
            Manifest.permission.READ_PHONE_STATE
        )
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        statusText = findViewById(R.id.status_text)
        connectButton = findViewById(R.id.connect_button)
        disconnectButton = findViewById(R.id.disconnect_button)

        // Request permissions
        requestPermissions()

        // Initialize managers
        val bluetoothManager = getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
        val bluetoothAdapter = bluetoothManager.adapter

        bleManager = BLEManager(this, bluetoothAdapter)
        usbSerialManager = USBSerialManager(this)
        audioCaptureManager = AudioCaptureManager(this)
        handshakeProtocol = HandshakeProtocol(bleManager, usbSerialManager)

        // Button listeners
        connectButton.setOnClickListener { scanForBLEDevice() }
        disconnectButton.setOnClickListener { disconnect() }

        updateStatus("Idle")
        Log.d("MainActivity", "App initialized")
    }

    private fun requestPermissions() {
        val missingPermissions = PERMISSIONS.filter {
            ContextCompat.checkSelfPermission(this, it) != PackageManager.PERMISSION_GRANTED
        }.toTypedArray()

        if (missingPermissions.isNotEmpty()) {
            Log.d("MainActivity", "Requesting permissions: ${missingPermissions.joinToString()}")
            ActivityCompat.requestPermissions(this, missingPermissions, 100)
        }
    }

    private fun scanForBLEDevice() {
        updateStatus("Scanning for BLE-Headphones...")
        Log.d("MainActivity", "Starting BLE scan")

        bleManager.startScan("BLE-Headphones") { device ->
            updateStatus("Found device! Connecting...")
            Log.d("MainActivity", "Device found: ${device.name}")

            bleManager.connectToDevice(device) { connected ->
                if (connected) {
                    updateStatus("BLE Connected!")
                    Log.d("MainActivity", "BLE Connected!")
                    handshakeProtocol.startAudioStream()
                } else {
                    updateStatus("Connection failed")
                    Log.e("MainActivity", "Connection failed")
                }
            }
        }
    }

    private fun disconnect() {
        bleManager.disconnect()
        usbSerialManager.close()
        handshakeProtocol.stop()
        updateStatus("Disconnected")
        Log.d("MainActivity", "Disconnected")
    }

    fun updateStatus(message: String) {
        runOnUiThread {
            statusText.text = "Status: $message"
            Log.d("MainActivity", "Status: $message")
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        disconnect()
    }
}
