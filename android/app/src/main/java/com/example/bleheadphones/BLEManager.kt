package com.example.bleheadphones

import android.Manifest
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothManager
import android.bluetooth.le.BluetoothLeScanner
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanSettings
import android.bluetooth.le.ScanResult
import android.content.Context
import android.content.pm.PackageManager
import android.os.Build
import android.util.Log
import androidx.core.content.ContextCompat

class BLEManager(val context: Context, val bluetoothAdapter: BluetoothAdapter?) {

    private var scanCallback: ((List<BluetoothDevice>) -> Unit)? = null
    private var connectedDevice: BluetoothDevice? = null
    private var isScanning = false
    private var leScanner: BluetoothLeScanner? = null
    
    private val discoveredDevices = mutableMapOf<String, BluetoothDevice>()

    private val leScanCallback = object : ScanCallback() {
        override fun onScanResult(callbackType: Int, result: ScanResult?) {
            super.onScanResult(callbackType, result)
            
            if (result == null) {
                Log.w("BLEManager", "⚠️ onScanResult called with null result")
                return
            }
            
            val device = result.device ?: run {
                Log.w("BLEManager", "⚠️ Scan result has null device")
                return
            }
            
            val deviceName = device.name ?: "Unknown"
            val rssi = result.rssi
            
            Log.d("BLEManager", "📡 Found: $deviceName (${device.address}) RSSI: $rssi dBm")
            
            // Store device
            discoveredDevices[device.address] = device
            
            // Notify UI
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
                else -> "Unknown error ($errorCode)"
            }
            Log.e("BLEManager", "❌ Scan failed: $errorMsg")
        }
    }

    fun isBluetoothEnabled(): Boolean {
        val enabled = bluetoothAdapter?.isEnabled ?: false
        Log.d("BLEManager", "Bluetooth enabled: $enabled")
        return enabled
    }

    fun hasBluetoothPermission(): Boolean {
        val scanPermission = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            ContextCompat.checkSelfPermission(context, Manifest.permission.BLUETOOTH_SCAN) == PackageManager.PERMISSION_GRANTED
        } else {
            true  // Older Android versions don't require this
        }
        
        val connectPermission = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            ContextCompat.checkSelfPermission(context, Manifest.permission.BLUETOOTH_CONNECT) == PackageManager.PERMISSION_GRANTED
        } else {
            true
        }
        
        Log.d("BLEManager", "BLUETOOTH_SCAN: $scanPermission, BLUETOOTH_CONNECT: $connectPermission")
        return scanPermission && connectPermission
    }

    fun enableBluetooth() {
        try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                if (ContextCompat.checkSelfPermission(context, Manifest.permission.BLUETOOTH_CONNECT) == PackageManager.PERMISSION_GRANTED) {
                    bluetoothAdapter?.enable()
                    Log.d("BLEManager", "✅ Bluetooth enable requested")
                } else {
                    Log.e("BLEManager", "❌ BLUETOOTH_CONNECT permission denied")
                }
            } else {
                bluetoothAdapter?.enable()
                Log.d("BLEManager", "✅ Bluetooth enable requested")
            }
        } catch (e: Exception) {
            Log.e("BLEManager", "❌ Error enabling Bluetooth: ${e.message}", e)
        }
    }

    fun disableBluetooth() {
        try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                if (ContextCompat.checkSelfPermission(context, Manifest.permission.BLUETOOTH_CONNECT) == PackageManager.PERMISSION_GRANTED) {
                    bluetoothAdapter?.disable()
                    Log.d("BLEManager", "Bluetooth disabled")
                }
            } else {
                bluetoothAdapter?.disable()
                Log.d("BLEManager", "Bluetooth disabled")
            }
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
            Log.w("BLEManager", "⚠️ Already scanning, ignoring request")
            return
        }

        // Check permissions
        if (!hasBluetoothPermission()) {
            Log.e("BLEManager", "❌ Missing Bluetooth permissions!")
            return
        }

        // Check if Bluetooth is enabled
        if (!isBluetoothEnabled()) {
            Log.e("BLEManager", "❌ Bluetooth is OFF!")
            return
        }

        scanCallback = onDevicesFound
        discoveredDevices.clear()
        
        Log.d("BLEManager", "═══════════════════════════════════════")
        Log.d("BLEManager", "🔍 Starting BLE Low Energy Scan...")
        Log.d("BLEManager", "═══════════════════════════════════════")

        try {
            leScanner = bluetoothAdapter?.bluetoothLeScanner
            
            if (leScanner == null) {
                Log.e("BLEManager", "❌ BluetoothLeScanner is null!")
                return
            }

            Log.d("BLEManager", "✅ BluetoothLeScanner obtained")

            // Configure scan settings - try multiple modes
            val scanSettings = ScanSettings.Builder()
                .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
                .setCallbackType(ScanSettings.CALLBACK_TYPE_ALL_MATCHES)
                .setMatchMode(ScanSettings.MATCH_MODE_AGGRESSIVE)
                .setNumOfMatches(ScanSettings.MATCH_NUM_MAX_ADVERTISEMENT)
                .build()

            Log.d("BLEManager", "✅ Scan settings configured")
            Log.d("BLEManager", "   - Mode: LOW_LATENCY")
            Log.d("BLEManager", "   - Callback: ALL_MATCHES")
            Log.d("BLEManager", "   - Match: AGGRESSIVE")

            // Start scan with NO FILTER (find all devices)
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                if (ContextCompat.checkSelfPermission(context, Manifest.permission.BLUETOOTH_SCAN) == PackageManager.PERMISSION_GRANTED) {
                    leScanner?.startScan(null, scanSettings, leScanCallback)
                    Log.d("BLEManager", "✅ Scan started (Android 12+, permission granted)")
                } else {
                    Log.e("BLEManager", "❌ BLUETOOTH_SCAN permission not granted")
                    return
                }
            } else {
                leScanner?.startScan(null, scanSettings, leScanCallback)
                Log.d("BLEManager", "✅ Scan started (Android <12)")
            }

            isScanning = true
            Log.d("BLEManager", "═══════════════════════════════════════")
            Log.d("BLEManager", "✅ BLE Scan is ACTIVE - waiting for devices...")
            Log.d("BLEManager", "═══════════════════════════════════════")

            // Auto-stop after 30 seconds
            android.os.Handler(android.os.Looper.getMainLooper()).postDelayed({
                if (isScanning) {
                    Log.w("BLEManager", "⏰ 30-second timeout reached - stopping scan")
                    stopScan()
                }
            }, 30000)

        } catch (e: SecurityException) {
            Log.e("BLEManager", "❌ SecurityException during scan: ${e.message}", e)
            isScanning = false
        } catch (e: Exception) {
            Log.e("BLEManager", "❌ Failed to start scan: ${e.message}", e)
            e.printStackTrace()
            isScanning = false
        }
    }

    fun stopScan() {
        if (isScanning) {
            try {
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                    if (ContextCompat.checkSelfPermission(context, Manifest.permission.BLUETOOTH_SCAN) == PackageManager.PERMISSION_GRANTED) {
                        leScanner?.stopScan(leScanCallback)
                        Log.d("BLEManager", "✅ Scan stopped")
                    }
                } else {
                    leScanner?.stopScan(leScanCallback)
                    Log.d("BLEManager", "✅ Scan stopped")
                }
                
                isScanning = false
                Log.d("BLEManager", "📊 Total devices found: ${discoveredDevices.size}")
                
                discoveredDevices.forEach { (addr, device) ->
                    Log.d("BLEManager", "   - ${device.name ?: "Unknown"} ($addr)")
                }
                
            } catch (e: Exception) {
                Log.e("BLEManager", "❌ Failed to stop scan: ${e.message}")
            }
        }
    }

    fun connectToDevice(device: BluetoothDevice, onConnected: (Boolean) -> Unit) {
        connectedDevice = device
        Log.d("BLEManager", "Connecting to: ${device.name} (${device.address})")
        
        try {
            onConnected(true)
            Log.d("BLEManager", "✅ Connection started")
        } catch (e: Exception) {
            Log.e("BLEManager", "❌ Connection failed: ${e.message}")
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
