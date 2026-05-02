package com.example.bleheadphones

import android.Manifest
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothManager
import android.content.Context
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.util.Log
import android.widget.Button
import android.widget.ScrollView
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat

class MainActivity : AppCompatActivity() {

    private var logText: TextView? = null
    private var bluetoothAdapter: BluetoothAdapter? = null
    private var isScanning = false
    private val handler = Handler(Looper.getMainLooper())
    private val REQUEST_PERMISSIONS = 999

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_test)
        supportActionBar?.hide()

        logText = findViewById(R.id.log_text)
        findViewById<Button>(R.id.scan_button).setOnClickListener { onScanClicked() }
        findViewById<Button>(R.id.clear_button).setOnClickListener { logText?.text = "" }

        log("════════════════════════════════════════")
        log("BLE Scanner v7.0 - Maps-Style Permissions")
        log("════════════════════════════════════════")

        val bluetoothManager = getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
        bluetoothAdapter = bluetoothManager.adapter

        if (bluetoothAdapter == null) {
            log("❌ NO BLUETOOTH SUPPORT")
            return
        }

        log("✅ Bluetooth adapter found")
        
        // Try to enable Bluetooth
        if (!bluetoothAdapter!!.isEnabled) {
            log("⚠️  Bluetooth OFF - trying to enable...")
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
            log("❌ Bluetooth is OFF - please enable manually")
        }

        // Request permissions like Google Maps does
        checkAndRequestPermissions()
    }

    private fun checkAndRequestPermissions() {
        val requiredPermissions = buildList {
            // Bluetooth permissions
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                add(Manifest.permission.BLUETOOTH_SCAN)
                add(Manifest.permission.BLUETOOTH_CONNECT)
            }
            // Location - CRITICAL for BLE
            add(Manifest.permission.ACCESS_FINE_LOCATION)
            add(Manifest.permission.ACCESS_COARSE_LOCATION)
        }

        val missingPermissions = requiredPermissions.filter {
            ContextCompat.checkSelfPermission(this, it) != PackageManager.PERMISSION_GRANTED
        }

        if (missingPermissions.isNotEmpty()) {
            log("🔐 Requesting ${missingPermissions.size} permissions:")
            missingPermissions.forEach { 
                val shortName = it.split(".").last()
                log("   - $shortName")
            }
            // Request like Google Maps - all at once
            ActivityCompat.requestPermissions(
                this,
                missingPermissions.toTypedArray(),
                REQUEST_PERMISSIONS
            )
        } else {
            log("✅ All permissions granted")
        }
    }

    override fun onRequestPermissionsResult(
        requestCode: Int,
        permissions: Array<String>,
        grantResults: IntArray
    ) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)

        if (requestCode == REQUEST_PERMISSIONS) {
            log("\n📋 Permission results:")
            var allGranted = true
            
            for (i in permissions.indices) {
                val perm = permissions[i].split(".").last()
                val granted = grantResults.getOrNull(i) == PackageManager.PERMISSION_GRANTED
                log("  ${if(granted) "✅" else "❌"} $perm")
                if (!granted) allGranted = false
            }

            if (allGranted) {
                log("\n✅ ALL PERMISSIONS GRANTED!")
            } else {
                log("\n⚠️  Some permissions were denied")
            }
        }
    }

    private fun onScanClicked() {
        if (isScanning) {
            stopScan()
            return
        }

        // Check permissions before scan
        val hasLocationPerm = ContextCompat.checkSelfPermission(
            this,
            Manifest.permission.ACCESS_FINE_LOCATION
        ) == PackageManager.PERMISSION_GRANTED

        if (!hasLocationPerm) {
            log("❌ Location permission required for BLE scan")
            Toast.makeText(this, "Please grant Location permission", Toast.LENGTH_SHORT).show()
            checkAndRequestPermissions()
            return
        }

        startScan()
    }

    private fun startScan() {
        log("\n════════════════════════════════════════")
        log("🔍 SCAN START (10 seconds)")
        log("════════════════════════════════════════")

        if (!bluetoothAdapter!!.isEnabled) {
            log("❌ Bluetooth OFF")
            return
        }

        try {
            isScanning = true
            var count = 0

            val callback = object : android.bluetooth.BluetoothAdapter.LeScanCallback {
                override fun onLeScan(
                    device: android.bluetooth.BluetoothDevice?,
                    rssi: Int,
                    scanRecord: ByteArray?
                ) {
                    if (device != null) {
                        count++
                        val name = device.name ?: "(unnamed)"
                        val addr = device.address
                        log("   [$count] $name / $addr (RSSI: $rssi)")
                    }
                }
            }

            // Check permission for Android 12+
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                if (ActivityCompat.checkSelfPermission(
                        this,
                        Manifest.permission.BLUETOOTH_SCAN
                    ) != PackageManager.PERMISSION_GRANTED
                ) {
                    log("❌ BLUETOOTH_SCAN permission denied")
                    isScanning = false
                    return
                }
            }

            bluetoothAdapter!!.startLeScan(callback)
            log("✅ Scan started")

            // Stop after 10 seconds
            handler.postDelayed({
                if (isScanning) {
                    try {
                        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                            if (ActivityCompat.checkSelfPermission(
                                    this,
                                    Manifest.permission.BLUETOOTH_SCAN
                                ) == PackageManager.PERMISSION_GRANTED
                            ) {
                                bluetoothAdapter!!.stopLeScan(callback)
                            }
                        } else {
                            bluetoothAdapter!!.stopLeScan(callback)
                        }
                        isScanning = false
                        log("")
                        log("════════════════════════════════════════")
                        log("🛑 SCAN STOPPED")
                        log("📊 Total devices found: $count")
                        log("════════════════════════════════════════")
                    } catch (e: Exception) {
                        log("❌ Stop error: ${e.message}")
                    }
                }
            }, 10000)

        } catch (e: SecurityException) {
            log("❌ SecurityException: ${e.message}")
            isScanning = false
        } catch (e: Exception) {
            log("❌ Error: ${e.message}")
            isScanning = false
        }
    }

    private fun stopScan() {
        isScanning = false
        log("🛑 Scan stopped by user")
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
