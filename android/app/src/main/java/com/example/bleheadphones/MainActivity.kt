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

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_test)
        supportActionBar?.hide()

        logText = findViewById(R.id.log_text)
        findViewById<Button>(R.id.scan_button).setOnClickListener { startScan() }
        findViewById<Button>(R.id.clear_button).setOnClickListener { logText?.text = "" }

        log("════════════════════════════════════════")
        log("BLE Scanner v5.0 - Google Official API")
        log("════════════════════════════════════════")

        requestPermissions()

        val bluetoothManager = getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
        bluetoothAdapter = bluetoothManager.adapter

        if (bluetoothAdapter == null) {
            log("❌ NO BLUETOOTH")
            return
        }

        log("✅ Bluetooth adapter found")
        
        // Check if Bluetooth is enabled
        if (!bluetoothAdapter?.isEnabled!!) {
            log("⚠️  Bluetooth is OFF - trying to enable...")
            try {
                bluetoothAdapter?.enable()
                Thread.sleep(1000)
            } catch (e: Exception) {
                log("Bluetooth enable failed: ${e.message}")
            }
        }
        
        if (bluetoothAdapter?.isEnabled!!) {
            log("✅ Bluetooth is ON")
        } else {
            log("❌ Bluetooth is OFF - turn it on manually!")
        }
    }

    private fun requestPermissions() {
        val needed = mutableListOf<String>()

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            if (ContextCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_SCAN) != PackageManager.PERMISSION_GRANTED) {
                needed.add(Manifest.permission.BLUETOOTH_SCAN)
            }
            if (ContextCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED) {
                needed.add(Manifest.permission.BLUETOOTH_CONNECT)
            }
        }

        if (ContextCompat.checkSelfPermission(this, Manifest.permission.ACCESS_FINE_LOCATION) != PackageManager.PERMISSION_GRANTED) {
            needed.add(Manifest.permission.ACCESS_FINE_LOCATION)
        }

        if (needed.isNotEmpty()) {
            log("🔐 Requesting permissions...")
            ActivityCompat.requestPermissions(this, needed.toTypedArray(), 100)
        }
    }

    override fun onRequestPermissionsResult(rq: Int, perms: Array<String>, grants: IntArray) {
        super.onRequestPermissionsResult(rq, perms, grants)
        if (rq == 100) {
            log("✅ Permission dialog closed")
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

        if (!bluetoothAdapter?.isEnabled!!) {
            log("❌ Bluetooth OFF")
            return
        }

        if (ContextCompat.checkSelfPermission(this, Manifest.permission.ACCESS_FINE_LOCATION) != PackageManager.PERMISSION_GRANTED) {
            log("❌ Location permission DENIED")
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

            // Use official deprecated API like Google sample
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                if (ActivityCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_SCAN) == PackageManager.PERMISSION_GRANTED) {
                    bluetoothAdapter?.startLeScan(callback)
                    log("✅ Scan started (Android 12+ with permission)")
                } else {
                    log("❌ BLUETOOTH_SCAN permission denied")
                    isScanning = false
                    return
                }
            } else {
                bluetoothAdapter?.startLeScan(callback)
                log("✅ Scan started (pre-Android 12)")
            }

            // Stop after 10 seconds
            handler.postDelayed({
                if (isScanning) {
                    try {
                        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                            if (ActivityCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_SCAN) == PackageManager.PERMISSION_GRANTED) {
                                bluetoothAdapter?.stopLeScan(callback)
                            }
                        } else {
                            bluetoothAdapter?.stopLeScan(callback)
                        }
                        isScanning = false
                        log("")
                        log("════════════════════════════════════════")
                        log("🛑 SCAN STOPPED")
                        log("📊 Total devices: $count")
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
        if (isScanning) {
            try {
                bluetoothAdapter?.stopLeScan(null)
                isScanning = false
                log("🛑 Scan stopped manually")
            } catch (e: Exception) {
                log("Error stopping: ${e.message}")
            }
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
