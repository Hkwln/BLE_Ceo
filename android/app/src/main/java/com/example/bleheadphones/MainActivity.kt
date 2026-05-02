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
    private var statusText: TextView? = null
    private var bluetoothAdapter: BluetoothAdapter? = null
    private var scanCallback: android.bluetooth.le.ScanCallback? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_test)
        supportActionBar?.hide()

        statusText = findViewById(R.id.status_text)
        logText = findViewById(R.id.log_text)

        log("════════════════════════════════════════")
        log("BLE Test App - Simple Scan Mode")
        log("════════════════════════════════════════")

        requestPermissions()

        val bluetoothManager = getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
        bluetoothAdapter = bluetoothManager.adapter

        if (bluetoothAdapter == null) {
            log("❌ NO BLUETOOTH SUPPORT")
            return
        }

        log("✅ Bluetooth adapter found")
        checkState()

        findViewById<Button>(R.id.bt_on_button).setOnClickListener { turnBtOn() }
        findViewById<Button>(R.id.bt_off_button).setOnClickListener { turnBtOff() }
        findViewById<Button>(R.id.scan_button).setOnClickListener { doScan() }
        findViewById<Button>(R.id.clear_button).setOnClickListener { logText?.text = "" }
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
        } else {
            if (ContextCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH) != PackageManager.PERMISSION_GRANTED) {
                needed.add(Manifest.permission.BLUETOOTH)
            }
            if (ContextCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_ADMIN) != PackageManager.PERMISSION_GRANTED) {
                needed.add(Manifest.permission.BLUETOOTH_ADMIN)
            }
        }

        if (ContextCompat.checkSelfPermission(this, Manifest.permission.ACCESS_FINE_LOCATION) != PackageManager.PERMISSION_GRANTED) {
            needed.add(Manifest.permission.ACCESS_FINE_LOCATION)
        }

        if (needed.isNotEmpty()) {
            log("🔐 Requesting: ${needed.joinToString()}")
            ActivityCompat.requestPermissions(this, needed.toTypedArray(), 100)
        } else {
            log("✅ All permissions OK")
        }
    }

    override fun onRequestPermissionsResult(rq: Int, perms: Array<String>, grants: IntArray) {
        super.onRequestPermissionsResult(rq, perms, grants)
        if (rq == 100) {
            log("✅ Permissions resolved")
        }
    }

    private fun checkState() {
        val s = bluetoothAdapter?.state
        val txt = when (s) {
            BluetoothAdapter.STATE_ON -> "✅ ON"
            BluetoothAdapter.STATE_OFF -> "❌ OFF"
            else -> "? ($s)"
        }
        log("Bluetooth: $txt")
        updateStatus(txt)
    }

    private fun turnBtOn() {
        try {
            log("→ Turning ON...")
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                if (ActivityCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_CONNECT) == PackageManager.PERMISSION_GRANTED) {
                    bluetoothAdapter?.enable()
                } else {
                    log("❌ Missing BLUETOOTH_CONNECT")
                }
            } else {
                bluetoothAdapter?.enable()
            }
            Thread.sleep(500)
            checkState()
        } catch (e: Exception) {
            log("❌ ${e.message}")
        }
    }

    private fun turnBtOff() {
        try {
            log("→ Turning OFF...")
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                if (ActivityCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_CONNECT) == PackageManager.PERMISSION_GRANTED) {
                    bluetoothAdapter?.disable()
                } else {
                    log("❌ Missing BLUETOOTH_CONNECT")
                }
            } else {
                bluetoothAdapter?.disable()
            }
            Thread.sleep(500)
            checkState()
        } catch (e: Exception) {
            log("❌ ${e.message}")
        }
    }

    private fun doScan() {
        log("")
        log("═══════════════════════════════════════")
        log("🔍 SCAN START (10 seconds)")
        log("═══════════════════════════════════════")

        if (!bluetoothAdapter?.isEnabled!!) {
            log("❌ Bluetooth OFF!")
            return
        }

        try {
            val scanner = bluetoothAdapter?.bluetoothLeScanner
            if (scanner == null) {
                log("❌ Scanner is null")
                return
            }

            val settings = android.bluetooth.le.ScanSettings.Builder()
                .setScanMode(android.bluetooth.le.ScanSettings.SCAN_MODE_LOW_LATENCY)
                .build()

            var count = 0

            scanCallback = object : android.bluetooth.le.ScanCallback() {
                override fun onScanResult(callbackType: Int, result: android.bluetooth.le.ScanResult?) {
                    if (result != null) {
                        count++
                        val name = result.device?.name ?: "unknown"
                        val addr = result.device?.address ?: "?"
                        log("   [$count] $name / $addr (RSSI: ${result.rssi})")
                    }
                }

                override fun onScanFailed(errorCode: Int) {
                    log("❌ Scan error: $errorCode")
                }
            }

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                if (ActivityCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_SCAN) != PackageManager.PERMISSION_GRANTED) {
                    log("❌ Missing BLUETOOTH_SCAN permission")
                    return
                }
            }

            scanner.startScan(null, settings, scanCallback!!)
            log("✅ Scanning started...")
            updateStatus("🔍 Scanning")

            Thread {
                Thread.sleep(10000)
                runOnUiThread {
                    try {
                        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                            if (ActivityCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_SCAN) == PackageManager.PERMISSION_GRANTED) {
                                scanner.stopScan(scanCallback!!)
                            }
                        } else {
                            scanner.stopScan(scanCallback!!)
                        }
                        log("🛑 Scan stopped")
                        log("📊 TOTAL: $count devices")
                        log("═══════════════════════════════════════")
                        updateStatus("Found: $count")
                    } catch (e: Exception) {
                        log("❌ Stop error: ${e.message}")
                    }
                }
            }.start()

        } catch (e: Exception) {
            log("❌ ${e.message}")
            e.printStackTrace()
        }
    }

    private fun log(msg: String) {
        Log.d("BLETest", msg)
        runOnUiThread {
            val current = logText?.text.toString()
            logText?.text = if (current.isEmpty()) msg else "$current\n$msg"
            (logText?.parent?.parent as? ScrollView)?.post {
                (logText?.parent?.parent as? ScrollView)?.fullScroll(ScrollView.FOCUS_DOWN)
            }
        }
    }

    private fun updateStatus(msg: String) {
        runOnUiThread {
            statusText?.text = msg
        }
    }
}
