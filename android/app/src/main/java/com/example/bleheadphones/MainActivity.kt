package com.example.bleheadphones

import android.Manifest
import android.app.AlertDialog
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothManager
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.provider.Settings
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
        log("BLE Scanner v6.0 - NO Location Needed")
        log("════════════════════════════════════════")

        requestBTPermissions()

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
            log("✅ Bluetooth is ON - ready to scan!")
        } else {
            log("❌ Bluetooth is OFF")
        }
    }

    private fun requestBTPermissions() {
        val needed = mutableListOf<String>()

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            if (ContextCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_SCAN) != PackageManager.PERMISSION_GRANTED) {
                needed.add(Manifest.permission.BLUETOOTH_SCAN)
            }
            if (ContextCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED) {
                needed.add(Manifest.permission.BLUETOOTH_CONNECT)
            }
        }

        if (needed.isNotEmpty()) {
            log("🔐 Requesting Bluetooth permissions...")
            ActivityCompat.requestPermissions(this, needed.toTypedArray(), 42)
        } else {
            log("✅ Bluetooth permissions OK")
        }
    }

    override fun onRequestPermissionsResult(rq: Int, perms: Array<String>, grants: IntArray) {
        super.onRequestPermissionsResult(rq, perms, grants)
        if (rq == 42) {
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
            log("❌ Bluetooth OFF - turn it on!")
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
                if (ActivityCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_SCAN) == PackageManager.PERMISSION_GRANTED) {
                    bluetoothAdapter?.startLeScan(callback)
                    log("✅ Scan started (Android 12+)")
                } else {
                    log("❌ BLUETOOTH_SCAN permission denied")
                    isScanning = false
                    return
                }
            } else {
                bluetoothAdapter?.startLeScan(callback)
                log("✅ Scan started (pre-Android 12)")
            }

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
                        log("📊 Total devices found: $count")
                        log("════════════════════════════════════════")
                    } catch (e: Exception) {
                        log("Error: ${e.message}")
                    }
                }
            }, 10000)

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
