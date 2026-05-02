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

class MainActivity : AppCompatActivity() {

    private var logText: TextView? = null
    private var devicesContainer: LinearLayout? = null
    private var bluetoothAdapter: BluetoothAdapter? = null
    private var isScanning = false
    private val handler = Handler(Looper.getMainLooper())
    private val REQUEST_PERMISSIONS = 999
    
    private var connectedDevice: BluetoothDevice? = null
    private var bluetoothGatt: BluetoothGatt? = null
    private var dataCharacteristic: BluetoothGattCharacteristic? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main_new)
        supportActionBar?.hide()

        logText = findViewById(R.id.log_text)
        devicesContainer = findViewById(R.id.devices_container)
        findViewById<Button>(R.id.scan_button).setOnClickListener { onScanClicked() }
        findViewById<Button>(R.id.disconnect_button).setOnClickListener { disconnect() }

        log("════════════════════════════════════════")
        log("BLE Headphones App - v8.0")
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
        }

        val missingPermissions = requiredPermissions.filter {
            ContextCompat.checkSelfPermission(this, it) != PackageManager.PERMISSION_GRANTED
        }

        if (missingPermissions.isNotEmpty()) {
            log("🔐 Requesting permissions...")
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
            log("✅ Permissions granted")
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
                    bluetoothGatt?.close()
                    bluetoothGatt = null
                }
            }
        }

        override fun onServicesDiscovered(gatt: BluetoothGatt?, status: Int) {
            super.onServicesDiscovered(gatt, status)
            
            if (status == android.bluetooth.BluetoothGatt.GATT_SUCCESS) {
                log("✅ Services discovered")
                
                // Find our characteristic
                val service = gatt?.getService(java.util.UUID.fromString("4fafc201-1fb5-459e-8fcc-c5c9c331914b"))
                if (service != null) {
                    dataCharacteristic = service.getCharacteristic(java.util.UUID.fromString("beb5483e-36e1-4688-b7f5-ea07361b26a8"))
                    if (dataCharacteristic != null) {
                        log("✅ Found data characteristic!")
                        log("📊 Ready to stream audio!")
                        // Enable notifications
                        try {
                            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                                if (ActivityCompat.checkSelfPermission(
                                        this@MainActivity,
                                        Manifest.permission.BLUETOOTH_CONNECT
                                    ) == PackageManager.PERMISSION_GRANTED
                                ) {
                                    gatt?.setCharacteristicNotification(dataCharacteristic, true)
                                }
                            } else {
                                gatt?.setCharacteristicNotification(dataCharacteristic, true)
                            }
                        } catch (e: Exception) {
                            log("Error enabling notifications: ${e.message}")
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

        override fun onCharacteristicRead(
            gatt: BluetoothGatt?,
            characteristic: BluetoothGattCharacteristic?,
            status: Int
        ) {
            super.onCharacteristicRead(gatt, characteristic, status)
            log("📖 Characteristic read: ${characteristic?.value?.size} bytes")
        }

        override fun onCharacteristicChanged(
            gatt: BluetoothGatt?,
            characteristic: BluetoothGattCharacteristic?
        ) {
            super.onCharacteristicChanged(gatt, characteristic)
            log("📝 Data received: ${characteristic?.value?.size} bytes")
        }
    }

    private fun disconnect() {
        log("\n→ Disconnecting...")
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
