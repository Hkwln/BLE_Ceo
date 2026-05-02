package com.example.bleheadphones

import android.Manifest
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
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
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView

class MainActivity : AppCompatActivity() {

    private lateinit var bleManager: BLEManager
    private lateinit var usbSerialManager: USBSerialManager
    private lateinit var audioCaptureManager: AudioCaptureManager
    private lateinit var handshakeProtocol: HandshakeProtocol

    private var statusText: TextView? = null
    private var scanButton: Button? = null
    private var disconnectButton: Button? = null
    private var devicesList: RecyclerView? = null
    private var deviceAdapter: BLEDeviceAdapter? = null
    
    private var bluetoothReceiver: BroadcastReceiver? = null
    private var isWaitingForBluetoothEnable = false
    private var bluetoothAdapter: BluetoothAdapter? = null

    private val REQUIRED_PERMISSIONS = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
        arrayOf(
            Manifest.permission.BLUETOOTH_SCAN,
            Manifest.permission.BLUETOOTH_CONNECT,
            Manifest.permission.RECORD_AUDIO,
            Manifest.permission.ACCESS_FINE_LOCATION  // Sometimes needed for BLE scanning
        )
    } else {
        arrayOf(
            Manifest.permission.BLUETOOTH,
            Manifest.permission.BLUETOOTH_ADMIN,
            Manifest.permission.RECORD_AUDIO,
            Manifest.permission.ACCESS_COARSE_LOCATION
        )
    }
    
    private val REQUEST_ENABLE_BT = 1
    private val REQUEST_PERMISSIONS = 100

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        try {
            supportActionBar?.hide()
            setContentView(R.layout.activity_main)

            statusText = findViewById(R.id.status_text)
            scanButton = findViewById(R.id.scan_button)
            disconnectButton = findViewById(R.id.disconnect_button)
            devicesList = findViewById(R.id.devices_list)

            Log.d("MainActivity", "🚀 App starting...")

            // Setup RecyclerView
            devicesList?.layoutManager = LinearLayoutManager(this)
            deviceAdapter = BLEDeviceAdapter(emptyList()) { device ->
                onDeviceSelected(device)
            }
            devicesList?.adapter = deviceAdapter

            // Request permissions FIRST
            requestAllPermissions()

            // Initialize Bluetooth
            val bluetoothManager = getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
            bluetoothAdapter = bluetoothManager.adapter

            if (bluetoothAdapter == null) {
                Log.e("MainActivity", "❌ Device does not support Bluetooth")
                updateStatus("❌ No Bluetooth support")
                return
            }

            bleManager = BLEManager(this, bluetoothAdapter)
            usbSerialManager = USBSerialManager(this)
            audioCaptureManager = AudioCaptureManager(this)
            handshakeProtocol = HandshakeProtocol(bleManager, usbSerialManager)

            // Check Bluetooth state
            if (!bleManager.isBluetoothEnabled()) {
                updateStatus("❌ Bluetooth is OFF")
            } else {
                updateStatus("✅ Bluetooth is ON")
            }

            // Button listeners
            scanButton?.setOnClickListener { scanForBLEDevices() }
            disconnectButton?.setOnClickListener { disconnect() }

            updateStatus("Ready - tap SCAN")
            Log.d("MainActivity", "✅ App initialized")
            
            // Register Bluetooth state receiver
            setupBluetoothReceiver()
        } catch (e: Exception) {
            Log.e("MainActivity", "❌ Fatal error during onCreate: ${e.message}", e)
            e.printStackTrace()
        }
    }

    private fun requestAllPermissions() {
        val missingPermissions = REQUIRED_PERMISSIONS.filter {
            ContextCompat.checkSelfPermission(this, it) != PackageManager.PERMISSION_GRANTED
        }.toTypedArray()

        if (missingPermissions.isNotEmpty()) {
            Log.d("MainActivity", "🔐 Requesting permissions: ${missingPermissions.joinToString()}")
            ActivityCompat.requestPermissions(this, missingPermissions, REQUEST_PERMISSIONS)
        } else {
            Log.d("MainActivity", "✅ All permissions already granted")
        }
    }

    override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<String>, grantResults: IntArray) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        
        if (requestCode == REQUEST_PERMISSIONS) {
            val denied = mutableListOf<String>()
            for (i in permissions.indices) {
                if (grantResults[i] != PackageManager.PERMISSION_GRANTED) {
                    denied.add(permissions[i])
                }
            }
            
            if (denied.isEmpty()) {
                Log.d("MainActivity", "✅ All permissions granted!")
                updateStatus("✅ Permissions OK - Ready to scan")
            } else {
                Log.e("MainActivity", "❌ Permissions denied: ${denied.joinToString()}")
                updateStatus("❌ Missing permissions: ${denied.joinToString()}")
            }
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
                            Log.d("MainActivity", "📱 Bluetooth turned ON")
                            updateStatus("✅ Bluetooth is ON")
                            if (isWaitingForBluetoothEnable) {
                                isWaitingForBluetoothEnable = false
                                android.os.Handler(android.os.Looper.getMainLooper()).postDelayed({
                                    if (bleManager.isBluetoothEnabled()) {
                                        startBLEScan()
                                    }
                                }, 500)
                            }
                        }
                        BluetoothAdapter.STATE_OFF -> {
                            Log.d("MainActivity", "📱 Bluetooth turned OFF")
                            updateStatus("❌ Bluetooth is OFF")
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

    private fun scanForBLEDevices() {
        Log.d("MainActivity", "🔍 Scan button pressed")
        
        // Check permissions again
        if (!bleManager.hasBluetoothPermission()) {
            Log.w("MainActivity", "⚠️ Permissions missing, requesting...")
            requestAllPermissions()
            return
        }

        // Check if Bluetooth is enabled
        if (!bleManager.isBluetoothEnabled()) {
            Log.d("MainActivity", "Bluetooth is OFF - showing dialog")
            showBluetoothDialog()
            return
        }

        startBLEScan()
    }

    private fun startBLEScan() {
        updateStatus("🔍 Scanning for BLE devices...")
        Log.d("MainActivity", "Starting BLE scan now")

        bleManager.startScan { devices ->
            Log.d("MainActivity", "📡 Scan callback: Found ${devices.size} device(s)")
            updateStatus("📡 Found ${devices.size} device(s)")
            deviceAdapter?.updateDevices(devices)
        }
    }

    private fun onDeviceSelected(device: BluetoothDevice) {
        bleManager.stopScan()
        updateStatus("✅ Connecting to ${device.name}...")
        Log.d("MainActivity", "Device selected: ${device.name} (${device.address})")

        bleManager.connectToDevice(device) { connected ->
            if (connected) {
                updateStatus("✅ BLE Connected!\n${device.name}")
                Log.d("MainActivity", "✅ Connected!")
                handshakeProtocol.startAudioStream()
            } else {
                updateStatus("❌ Connection failed")
                Log.e("MainActivity", "Connection failed")
            }
        }
    }

    private fun showBluetoothDialog() {
        AlertDialog.Builder(this)
            .setTitle("Bluetooth is OFF")
            .setMessage("Bluetooth must be enabled. Turn it on now?")
            .setPositiveButton("Yes") { _, _ ->
                updateStatus("Turning Bluetooth ON...")
                isWaitingForBluetoothEnable = true
                enableBluetoothDialog()
            }
            .setNegativeButton("No") { dialog, _ ->
                updateStatus("Scan cancelled")
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
                    Log.e("MainActivity", "❌ BLUETOOTH_CONNECT permission not granted")
                    updateStatus("❌ Permission denied")
                    isWaitingForBluetoothEnable = false
                }
            } else {
                startActivityForResult(enableBtIntent, REQUEST_ENABLE_BT)
            }
        } catch (e: Exception) {
            Log.e("MainActivity", "❌ Error: ${e.message}")
            updateStatus("❌ Could not enable Bluetooth")
            isWaitingForBluetoothEnable = false
        }
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        super.onActivityResult(requestCode, resultCode, data)
        if (requestCode == REQUEST_ENABLE_BT) {
            if (resultCode == RESULT_OK) {
                Log.d("MainActivity", "✅ Bluetooth enabled by user")
                updateStatus("✅ Bluetooth enabled")
                android.os.Handler(android.os.Looper.getMainLooper()).postDelayed({
                    startBLEScan()
                }, 1000)
            } else {
                Log.d("MainActivity", "❌ User rejected Bluetooth enable")
                updateStatus("❌ Bluetooth enable rejected")
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
            deviceAdapter?.updateDevices(emptyList())
            Log.d("MainActivity", "Disconnected")
        } catch (e: Exception) {
            Log.e("MainActivity", "Error during disconnect: ${e.message}")
        }
    }

    fun updateStatus(message: String) {
        try {
            runOnUiThread {
                statusText?.text = "Status: $message"
                Log.d("MainActivity", "UI Status: $message")
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
