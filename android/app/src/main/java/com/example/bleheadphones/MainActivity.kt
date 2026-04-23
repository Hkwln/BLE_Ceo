package com.example.bleheadphones

import android.Manifest
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothManager
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.util.Log
import android.widget.Button
import android.widget.TextView
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat

class MainActivity : AppCompatActivity() {

    private lateinit var bleManager: BLEManager
    private lateinit var usbSerialManager: USBSerialManager
    private lateinit var audioCaptureManager: AudioCaptureManager
    private lateinit var handshakeProtocol: HandshakeProtocol

    private var statusText: TextView? = null
    private var connectButton: Button? = null
    private var disconnectButton: Button? = null
    
    private var bluetoothReceiver: BroadcastReceiver? = null
    private var isWaitingForBluetoothEnable = false
    private var bluetoothAdapter: BluetoothAdapter? = null

    private val PERMISSIONS = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
        arrayOf(
            Manifest.permission.BLUETOOTH_SCAN,
            Manifest.permission.BLUETOOTH_CONNECT,
            Manifest.permission.RECORD_AUDIO
        )
    } else {
        arrayOf(
            Manifest.permission.BLUETOOTH,
            Manifest.permission.BLUETOOTH_ADMIN,
            Manifest.permission.RECORD_AUDIO
        )
    }
    
    private val REQUEST_ENABLE_BT = 1

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        try {
            // Hide ActionBar completely
            supportActionBar?.hide()
            
            setContentView(R.layout.activity_main)

            statusText = findViewById(R.id.status_text)
            connectButton = findViewById(R.id.connect_button)
            disconnectButton = findViewById(R.id.disconnect_button)

            // Request permissions
            requestPermissions()

            // Initialize managers
            val bluetoothManager = getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
            bluetoothAdapter = bluetoothManager.adapter

            bleManager = BLEManager(this, bluetoothAdapter)
            usbSerialManager = USBSerialManager(this)
            audioCaptureManager = AudioCaptureManager(this)
            handshakeProtocol = HandshakeProtocol(bleManager, usbSerialManager)

            // Check if Bluetooth is enabled
            if (!bleManager.isBluetoothEnabled()) {
                updateStatus("Bluetooth is OFF")
            } else {
                updateStatus("Bluetooth is ON")
            }

            // Button listeners
            connectButton?.setOnClickListener { scanForBLEDevice() }
            disconnectButton?.setOnClickListener { disconnect() }

            updateStatus("Ready")
            Log.d("MainActivity", "App initialized successfully")
            
            // Register Bluetooth state receiver
            setupBluetoothReceiver()
        } catch (e: Exception) {
            Log.e("MainActivity", "Fatal error during onCreate: ${e.message}", e)
            e.printStackTrace()
        }
    }

    private fun setupBluetoothReceiver() {
        bluetoothReceiver = object : BroadcastReceiver() {
            override fun onReceive(context: Context?, intent: Intent?) {
                val action = intent?.action
                if (action == BluetoothAdapter.ACTION_STATE_CHANGED) {
                    val state = intent?.getIntExtra(BluetoothAdapter.EXTRA_STATE, BluetoothAdapter.ERROR)
                    when (state) {
                        BluetoothAdapter.STATE_ON -> {
                            Log.d("MainActivity", "Bluetooth turned ON")
                            updateStatus("Bluetooth is ON")
                            if (isWaitingForBluetoothEnable) {
                                isWaitingForBluetoothEnable = false
                                // Auto-start scan after Bluetooth is on
                                android.os.Handler(android.os.Looper.getMainLooper()).postDelayed({
                                    startBLEScan()
                                }, 500)
                            }
                        }
                        BluetoothAdapter.STATE_OFF -> {
                            Log.d("MainActivity", "Bluetooth turned OFF")
                            updateStatus("Bluetooth is OFF")
                        }
                    }
                }
            }
        }
        
        val filter = IntentFilter(BluetoothAdapter.ACTION_STATE_CHANGED)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            registerReceiver(bluetoothReceiver, filter, Context.RECEIVER_EXPORTED)
        } else {
            registerReceiver(bluetoothReceiver, filter)
        }
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
        // Check if Bluetooth is enabled
        if (!bleManager.isBluetoothEnabled()) {
            showBluetoothDialog()
            return
        }

        startBLEScan()
    }

    private fun startBLEScan() {
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

    private fun showBluetoothDialog() {
        AlertDialog.Builder(this)
            .setTitle("Bluetooth ist aus")
            .setMessage("Bluetooth muss aktiviert sein, um Geräte zu scannen. Jetzt aktivieren?")
            .setPositiveButton("Ja") { _, _ ->
                updateStatus("Bluetooth wird aktiviert...")
                isWaitingForBluetoothEnable = true
                enableBluetoothDialog()
            }
            .setNegativeButton("Nein") { dialog, _ ->
                updateStatus("Scan abgebrochen")
                dialog.dismiss()
            }
            .show()
    }

    private fun enableBluetoothDialog() {
        val enableBtIntent = Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE)
        try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                if (ActivityCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_CONNECT) == PackageManager.PERMISSION_GRANTED) {
                    startActivityForResult(enableBtIntent, REQUEST_ENABLE_BT)
                } else {
                    Log.e("MainActivity", "BLUETOOTH_CONNECT permission not granted")
                    updateStatus("Bluetooth Permission denied")
                }
            } else {
                startActivityForResult(enableBtIntent, REQUEST_ENABLE_BT)
            }
        } catch (e: Exception) {
            Log.e("MainActivity", "Error enabling Bluetooth: ${e.message}", e)
            updateStatus("Could not enable Bluetooth")
        }
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        super.onActivityResult(requestCode, resultCode, data)
        if (requestCode == REQUEST_ENABLE_BT) {
            if (resultCode == RESULT_OK) {
                Log.d("MainActivity", "Bluetooth enabled by user")
                updateStatus("Bluetooth enabled")
            } else {
                Log.d("MainActivity", "User rejected Bluetooth enable")
                updateStatus("Bluetooth enable rejected")
                isWaitingForBluetoothEnable = false
            }
        }
    }

    private fun disconnect() {
        try {
            bleManager.disconnect()
            usbSerialManager.close()
            handshakeProtocol.stop()
            updateStatus("Disconnected")
            Log.d("MainActivity", "Disconnected")
        } catch (e: Exception) {
            Log.e("MainActivity", "Error during disconnect: ${e.message}")
        }
    }

    fun updateStatus(message: String) {
        try {
            runOnUiThread {
                statusText?.text = "Status: $message"
                Log.d("MainActivity", "Status: $message")
            }
        } catch (e: Exception) {
            Log.e("MainActivity", "Error updating status: ${e.message}")
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        try {
            disconnect()
            if (bluetoothReceiver != null) {
                unregisterReceiver(bluetoothReceiver)
            }
        } catch (e: Exception) {
            Log.e("MainActivity", "Error in onDestroy: ${e.message}")
        }
    }
}
