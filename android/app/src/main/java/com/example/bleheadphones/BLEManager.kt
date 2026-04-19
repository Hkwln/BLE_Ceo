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
            Log.d("BLEManager", "Found device: ${device.name} (${device.address})")

            if (device.name == "BLE-Headphones") {
                Log.d("BLEManager", "Found target device!")
                stopScan()
                scanCallback?.invoke(device)
            }
        }

        override fun onScanFailed(errorCode: Int) {
            super.onScanFailed(errorCode)
            Log.e("BLEManager", "Scan failed with error code: $errorCode")
            connectCallback?.invoke(false)
        }
    }

    fun startScan(targetName: String, onDeviceFound: (BluetoothDevice) -> Unit) {
        if (isScanning) return

        scanCallback = onDeviceFound

        // First check if device is already paired
        val pairedDevices = bluetoothAdapter?.bondedDevices ?: emptySet()
        val targetDevice = pairedDevices.firstOrNull { it.name == targetName }

        if (targetDevice != null) {
            Log.d("BLEManager", "Device already paired: $targetName")
            onDeviceFound(targetDevice)
            return
        }

        // Start BLE scanning
        try {
            leScanner = bluetoothAdapter?.bluetoothLeScanner
            leScanner?.startScan(leScanCallback)
            isScanning = true
            Log.d("BLEManager", "Started BLE scan for: $targetName")
        } catch (e: Exception) {
            Log.e("BLEManager", "Failed to start scan: ${e.message}")
            connectCallback?.invoke(false)
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
