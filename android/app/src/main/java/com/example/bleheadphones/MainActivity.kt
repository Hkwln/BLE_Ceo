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
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat

class MainActivity : AppCompatActivity() {

    private var logText: TextView? = null
    private var bluetoothAdapter: BluetoothAdapter? = null
    private var isScanning = false
    private val handler = Handler(Looper.getMainLooper())
    private val PERMISSION_REQUEST_CODE = 42

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_test)
        supportActionBar?.hide()

        logText = findViewById(R.id.log_text)
        findViewById<Button>(R.id.scan_button).setOnClickListener { startScan() }
        findViewById<Button>(R.id.clear_button).setOnClickListener { logText?.text = "" }

        log("════════════════════════════════════════")
        log("BLE Scanner v5.1")
        log("════════════════════════════════════════")

        // REQUEST PERMISSIONS IMMEDIATELY!
        requestAllPermissionsNow()

        val bluetoothManager = getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
        bluetoothAdapter = bluetoothManager.adapter

        if (bluetoothAdapter == null) {
            log("❌ NO BLUETOOTH")
            return
        }

        log("✅ Bluetooth adapter found")
        
        if (!bluetoothAdapter?.isEnabled!!) {
            log("⚠️  Bluetooth is OFF - enabling...")
            try {
                bluetoothAdapter?.enable()
                Thread.sleep(500)
            } catch (e: Exception) {
                log("Could not enable: ${e.message}")
            }
        }
        
        if (bluetoothAdapter?.isEnabled!!) {
            log("✅ Bluetooth is ON")
        } else {
            log("❌ Bluetooth is OFF")
        }
    }

    private fun requestAllPermissionsNow() {
        val allPermissions = mutableListOf<String>()

        // Add all permissions we need
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            allPermissions.add(Manifest.permission.BLUETOOTH_SCAN)
            allPermissions.add(Manifest.permission.BLUETOOTH_CONNECT)
        } else {
            allPermissions.add(Manifest.permission.BLUETOOTH)
            allPermissions.add(Manifest.permission.BLUETOOTH_ADMIN)
        }

        // LOCATION - THIS IS CRITICAL
        allPermissions.add(Manifest.permission.ACCESS_FINE_LOCATION)
        allPermissions.add(Manifest.permission.ACCESS_COARSE_LOCATION)

        // Check which ones are missing
        val needPermissions = allPermissions.filter {
            ContextCompat.checkSelfPermission(this, it) != PackageManager.PERMISSION_GRANTED
        }

        if (needPermissions.isNotEmpty()) {
            log("🔐 REQUESTING PERMISSIONS (dialog will appear):")
            needPermissions.forEach { log("   - $it") }
            
            // Request ALL at once
            ActivityCompat.requestPermissions(
                this,
                needPermissions.toTypedArray(),
                PERMISSION_REQUEST_CODE
            )
        } else {
            log("✅ ALL PERMISSIONS ALREADY GRANTED")
        }
    }

    override fun onRequestPermissionsResult(
        requestCode: Int,
        permissions: Array<String>,
        grantResults: IntArray
    ) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        
        if (requestCode == PERMISSION_REQUEST_CODE) {
            log("\n📋 Permission Results:")
            
            var allGranted = true
            for (i in permissions.indices) {
                val perm = permissions[i].split(".").last()
                val granted = grantResults[i] == PackageManager.PERMISSION_GRANTED
                log("  ${if(granted) "✅" else "❌"} $perm")
                if (!granted) allGranted = false
            }
            
            if (allGranted) {
                log("\n✅ ALL PERMISSIONS GRANTED - Ready to scan!")
            } else {
                log("\n❌ Some permissions still denied")
                log("   Go to Settings → Apps → BLE Headphones → Permissions")
                log("   and enable ALL permissions")
            }
        }
    }

    private fun startScan() {
        if (isScanning) {
            stopScan()
            return
        }

        log("\n════════════════════════════════════════")
        log("🔍 SCAN START (10 seconds)")
        log("════════════════════════════════════════")

        // Final checks before scan
        if (!bluetoothAdapter?.isEnabled!!) {
            log("❌ Bluetooth OFF - turn it on!")
            return
        }

        if (ContextCompat.checkSelfPermission(
                this,
                Manifest.permission.ACCESS_FINE_LOCATION
            ) != PackageManager.PERMISSION_GRANTED
        ) {
            log("❌ Location permission STILL DENIED")
            log("   Go to Settings and enable it!")
            return
        }

        try {
            isScanning = true
            var count = 0

            val callback = android.bluetooth.BluetoothAdapter.LeScanCallback { device, rssi, scanRecord ->
                if (device != null) {
                    count++
                    val name = device.name ?: "(no name)"
                    val addr = device.address
                    log("   [$count] $name / $addr (RSSI: $rssi)")
                }
            }

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                if (ActivityCompat.checkSelfPermission(
                        this,
                        Manifest.permission.BLUETOOTH_SCAN
                    ) == PackageManager.PERMISSION_GRANTED
                ) {
                    bluetoothAdapter?.startLeScan(callback)
                    log("✅ Scan started")
                } else {
                    log("❌ BLUETOOTH_SCAN permission denied")
                    isScanning = false
                    return
                }
            } else {
                bluetoothAdapter?.startLeScan(callback)
                log("✅ Scan started")
            }

            handler.postDelayed({
                if (isScanning) {
                    try {
                        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                            if (ActivityCompat.checkSelfPermission(
                                    this,
                                    Manifest.permission.BLUETOOTH_SCAN
                                ) == PackageManager.PERMISSION_GRANTED
                            ) {
                                bluetoothAdapter?.stopLeScan(callback)
                            }
                        } else {
                            bluetoothAdapter?.stopLeScan(callback)
                        }
                        isScanning = false
                        log("")
                        log("════════════════════════════════════════")
                        log("🛑 SCAN STOPPED")
                        log("📊 Total devices found: $count")
                        log("════════════════════════════════════════")
                    } catch (e: Exception) {
                        log("Error: ${e.message}")
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
        if (isScanning) {
            isScanning = false
            log("🛑 Scan stopped")
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
