package com.example.bleheadphones

import android.Manifest
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothManager
import android.content.Context
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
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

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_test)
        supportActionBar?.hide()

        logText = findViewById(R.id.log_text)
        findViewById<Button>(R.id.scan_button).setOnClickListener { scanBLE() }
        findViewById<Button>(R.id.clear_button).setOnClickListener { logText?.text = "" }

        log("════════════════════════════════════════")
        log("BLE Scanner - Ultra Simple")
        log("════════════════════════════════════════")

        requestPermissions()

        val bluetoothManager = getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
        bluetoothAdapter = bluetoothManager.adapter

        if (bluetoothAdapter == null) {
            log("❌ NO BLUETOOTH")
            return
        }

        log("✅ Bluetooth OK")
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
            ActivityCompat.requestPermissions(this, needed.toTypedArray(), 100)
        }
    }

    override fun onRequestPermissionsResult(rq: Int, perms: Array<String>, grants: IntArray) {
        super.onRequestPermissionsResult(rq, perms, grants)
        if (rq == 100) {
            log("✅ Permissions done")
        }
    }

    private fun scanBLE() {
        if (isScanning) {
            log("Already scanning...")
            return
        }

        log("\n🔍 SCAN START")

        if (!bluetoothAdapter?.isEnabled!!) {
            log("❌ Bluetooth OFF")
            return
        }

        try {
            val scanner = bluetoothAdapter?.bluetoothLeScanner ?: run {
                log("❌ No scanner")
                return
            }

            isScanning = true
            var count = 0

            val callback = object : android.bluetooth.le.ScanCallback() {
                override fun onScanResult(callbackType: Int, result: android.bluetooth.le.ScanResult?) {
                    result?.device?.let { device ->
                        count++
                        val name = device.name ?: "???"
                        val addr = device.address
                        log("[$count] $name / $addr")
                    }
                }

                override fun onScanFailed(errorCode: Int) {
                    log("❌ Scan error: $errorCode")
                }
            }

            // NO SETTINGS - use default
            scanner.startScan(callback)
            log("✅ Scanning (30s)...")

            Thread {
                Thread.sleep(30000)
                runOnUiThread {
                    try {
                        scanner.stopScan(callback)
                        isScanning = false
                        log("🛑 Done. Found: $count")
                    } catch (e: Exception) {
                        log("Error: ${e.message}")
                    }
                }
            }.start()

        } catch (e: Exception) {
            log("❌ ${e.message}")
            isScanning = false
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
