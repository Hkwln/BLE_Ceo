package com.example.bleheadphones

import android.Manifest
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothManager
import android.bluetooth.le.BluetoothLeScanner
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanFilter
import android.bluetooth.le.ScanSettings
import android.bluetooth.le.ScanResult
import android.content.Context
import android.util.Log

class BLEManager(val context: Context, val bluetoothAdapter: BluetoothAdapter?) {

    private var scanCallback: ((List<BluetoothDevice>) -> Unit)? = null
    private var deviceSelectedCallback: ((BluetoothDevice) -> Unit)? = null
    private var connectCallback: ((Boolean) -> Unit)? = null
    
    private var connectedDevice: BluetoothDevice? = null
    private var isScanning = false
    private var leScanner: BluetoothLeScanner? = null
    
    private val discoveredDevices = mutableMapOf<String, BluetoothDevice>()

    private val leScanCallback = object : ScanCallback() {
        override fun onScanResult(callbackType: Int, result: ScanResult) {
            super.onScanResult(callbackType, result)
            val device = result.device
            val deviceName = device.name ?: "Unknown"
            val rssi = result.rssi
            
            Log.d("BLEManager", "📡 Found: $deviceName (${device.address}) RSSI: $rssi dBm")
            
            // Store device
            discoveredDevices[device.address] = device
            
            // Notify UI of all discovered devices
            scanCallback?.invoke(discoveredDevices.values.toList())
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
        }
    }

    fun isBluetoothEnabled(): Boolean {
        return bluetoothAdapter?.isEnabled ?: false
    }

    fun enableBluetooth() {
        try {
            bluetoothAdapter?.enable()
            Log.d("BLEManager", "Bluetooth enable requested")
        } catch (e: Exception) {
            Log.e("BLEManager", "Error enabling Bluetooth: ${e.message}")
        }
    }

    fun disableBluetooth() {
        try {
            bluetoothAdapter?.disable()
            Log.d("BLEManager", "Bluetooth disabled")
        } catch (e: Exception) {
            Log.e("BLEManager", "Error disabling Bluetooth: ${e.message}")
        }
    }

    fun getBluetoothState(): String {
        return when(bluetoothAdapter?.state) {
            BluetoothAdapter.STATE_ON -> "ON"
            BluetoothAdapter.STATE_OFF -> "OFF"
            BluetoothAdapter.STATE_TURNING_ON -> "TURNING_ON"
            BluetoothAdapter.STATE_TURNING_OFF -> "TURNING_OFF"
            else -> "UNKNOWN"
        }
    }

    fun startScan(onDevicesFound: (List<BluetoothDevice>) -> Unit) {
        if (isScanning) {
            Log.w("BLEManager", "Already scanning, ignoring request")
            return
        }

        scanCallback = onDevicesFound
        discoveredDevices.clear()
        Log.d("BLEManager", "=== Starting BLE Scan ===")

        // Check if Bluetooth is enabled
        if (!isBluetoothEnabled()) {
            Log.e("BLEManager", "❌ Bluetooth is OFF!")
            return
        }

        try {
            leScanner = bluetoothAdapter?.bluetoothLeScanner
            if (leScanner == null) {
                Log.e("BLEManager", "❌ BluetoothLeScanner is null")
                return
            }

            // Configure scan settings for BLE Low Energy
            val scanSettings = ScanSettings.Builder()
                .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
                .setCallbackType(ScanSettings.CALLBACK_TYPE_ALL_MATCHES)
                .build()

            // Start scan with settings
            leScanner?.startScan(null, scanSettings, leScanCallback)
            isScanning = true
            Log.d("BLEManager", "🔍 BLE Scan started (Low Energy mode)")

            // Auto-stop after 30 seconds
            android.os.Handler(android.os.Looper.getMainLooper()).postDelayed({
                if (isScanning) {
                    Log.w("BLEManager", "⏰ Scan timeout - stopping after 30 seconds")
                    stopScan()
                }
            }, 30000)

        } catch (e: Exception) {
            Log.e("BLEManager", "❌ Failed to start scan: ${e.message}", e)
            isScanning = false
        }
    }

    fun stopScan() {
        if (isScanning) {
            try {
                leScanner?.stopScan(leScanCallback)
                isScanning = false
                Log.d("BLEManager", "🛑 BLE Scan stopped")
                Log.d("BLEManager", "📊 Total devices found: ${discoveredDevices.size}")
            } catch (e: Exception) {
                Log.e("BLEManager", "Failed to stop scan: ${e.message}")
            }
        }
    }

    fun connectToDevice(device: BluetoothDevice, onConnected: (Boolean) -> Unit) {
        connectCallback = onConnected
        connectedDevice = device

        try {
            Log.d("BLEManager", "Connecting to: ${device.name} (${device.address})")
            // In real implementation, would use BluetoothGatt
            // For now, just indicate success
            onConnected(true)
        } catch (e: Exception) {
            Log.e("BLEManager", "Connection failed: ${e.message}")
            onConnected(false)
        }
    }

    fun disconnect() {
        stopScan()
        connectedDevice = null
        discoveredDevices.clear()
        Log.d("BLEManager", "Disconnected")
    }

    fun getDiscoveredDevices(): List<BluetoothDevice> {
        return discoveredDevices.values.toList()
    }

    fun isScanning(): Boolean {
        return isScanning
    }
}
