package com.example.bleheadphones

import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothManager
import android.bluetooth.le.BluetoothLeScanner
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanResult
import android.content.Context
import android.util.Log

class BLEManager(val context: Context, val bluetoothAdapter: BluetoothAdapter?) {

    private var scanCallback: ((BluetoothDevice) -> Unit)? = null
    private var bluetoothStateCallback: ((Boolean) -> Unit)? = null
    private var connectCallback: ((Boolean) -> Unit)? = null
    private var connectedDevice: BluetoothDevice? = null
    private var isScanning = false
    private var leScanner: BluetoothLeScanner? = null

    private val leScanCallback = object : ScanCallback() {
        override fun onScanResult(callbackType: Int, result: ScanResult) {
            super.onScanResult(callbackType, result)
            val device = result.device
            val deviceName = device.name ?: "(no name)"
            val rssi = result.rssi
            Log.d("BLEManager", "📡 Found device: $deviceName (${device.address}) RSSI: $rssi dBm")

            if (device.name == "BLE-Headphones") {
                Log.d("BLEManager", "✅ Found TARGET device!")
                stopScan()
                scanCallback?.invoke(device)
            }
        }

        override fun onScanFailed(errorCode: Int) {
            super.onScanFailed(errorCode)
            val errorMsg = when(errorCode) {
                ScanCallback.SCAN_FAILED_ALREADY_STARTED -> "Already scanning"
                ScanCallback.SCAN_FAILED_APPLICATION_REGISTRATION_FAILED -> "App registration failed"
                ScanCallback.SCAN_FAILED_INTERNAL_ERROR -> "Internal error"
                ScanCallback.SCAN_FAILED_FEATURE_UNSUPPORTED -> "Feature unsupported"
                ScanCallback.SCAN_FAILED_OUT_OF_HARDWARE_RESOURCES -> "Out of hardware resources"
                else -> "Unknown error"
            }
            Log.e("BLEManager", "❌ Scan failed: $errorMsg (code: $errorCode)")
            connectCallback?.invoke(false)
        }
    }

    fun startScan(targetName: String, onDeviceFound: (BluetoothDevice) -> Unit) {
        if (isScanning) {
            Log.w("BLEManager", "Already scanning, ignoring request")
            return
        }

        scanCallback = onDeviceFound
        Log.d("BLEManager", "=== Starting BLE Scan for '$targetName' ===")

        // First check if device is already paired
        val pairedDevices = bluetoothAdapter?.bondedDevices ?: emptySet()
        val targetDevice = pairedDevices.firstOrNull { it.name == targetName }

        if (targetDevice != null) {
            Log.d("BLEManager", "🔐 Device already paired: $targetName (${targetDevice.address})")
            onDeviceFound(targetDevice)
            return
        }

        Log.d("BLEManager", "Paired devices: ${pairedDevices.size}")
        pairedDevices.forEach {
            Log.d("BLEManager", "  - ${it.name} (${it.address})")
        }

        // Start BLE scanning
        try {
            if (!isBluetoothEnabled()) {
                Log.e("BLEManager", "❌ Bluetooth is OFF!")
                return
            }

            leScanner = bluetoothAdapter?.bluetoothLeScanner
            if (leScanner == null) {
                Log.e("BLEManager", "❌ BluetoothLeScanner is null")
                return
            }

            leScanner?.startScan(leScanCallback)
            isScanning = true
            Log.d("BLEManager", "🔍 Scanning started... (will timeout in 30s)")

            // Auto-stop after 30 seconds
            android.os.Handler(android.os.Looper.getMainLooper()).postDelayed({
                if (isScanning) {
                    Log.w("BLEManager", "⏰ Scan timeout - no device found in 30 seconds")
                    stopScan()
                }
            }, 30000)
        } catch (e: Exception) {
            Log.e("BLEManager", "❌ Failed to start scan: ${e.message}", e)
            isScanning = false
        }
    }

    private fun stopScan() {
        if (isScanning) {
            try {
                leScanner?.stopScan(leScanCallback)
                isScanning = false
                Log.d("BLEManager", "Stopped BLE scan")
            } catch (e: Exception) {
                Log.e("BLEManager", "Failed to stop scan: ${e.message}")
            }
        }
    }

    fun connectToDevice(device: BluetoothDevice, onConnected: (Boolean) -> Unit) {
        connectCallback = onConnected
        connectedDevice = device

        try {
            // For this demo, just assume connection succeeds
            // In production, would use BluetoothSocket or BluetoothProfile
            Log.d("BLEManager", "Connecting to: ${device.name}")
            onConnected(true)
        } catch (e: Exception) {
            Log.e("BLEManager", "Connection failed: ${e.message}")
            onConnected(false)
        }
    }

    fun disconnect() {
        stopScan()
        connectedDevice = null
        Log.d("BLEManager", "Disconnected")
    }

    fun isBluetoothEnabled(): Boolean {
        return bluetoothAdapter?.isEnabled ?: false
    }

    fun enableBluetooth() {
        try {
            bluetoothAdapter?.enable()
            Log.d("BLEManager", "Bluetooth enabled")
        } catch (e: Exception) {
            Log.e("BLEManager", "Failed to enable Bluetooth: ${e.message}")
        }
    }

    fun disableBluetooth() {
        try {
            bluetoothAdapter?.disable()
            Log.d("BLEManager", "Bluetooth disabled")
        } catch (e: Exception) {
            Log.e("BLEManager", "Failed to disable Bluetooth: ${e.message}")
        }
    }

    fun getBluetoothState(): String {
        return when (bluetoothAdapter?.state) {
            BluetoothAdapter.STATE_OFF -> "OFF"
            BluetoothAdapter.STATE_ON -> "ON"
            BluetoothAdapter.STATE_TURNING_ON -> "TURNING_ON"
            BluetoothAdapter.STATE_TURNING_OFF -> "TURNING_OFF"
            else -> "UNKNOWN"
        }
    }
}
