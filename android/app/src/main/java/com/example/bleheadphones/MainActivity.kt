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
        log("BLE Scanner v4.1 - Strict Permissions")
        log("════════════════════════════════════════")

        requestAllPermissions()

        val bluetoothManager = getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
        bluetoothAdapter = bluetoothManager.adapter

        if (bluetoothAdapter == null) {
            log("❌ NO BLUETOOTH")
            return
        }

        log("✅ Bluetooth adapter found")
        checkAllPermissions()
    }

    private fun requestAllPermissions() {
        val needed = mutableListOf<String>()

        // BLUETOOTH permissions
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            if (ContextCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_SCAN) != PackageManager.PERMISSION_GRANTED) {
                needed.add(Manifest.permission.BLUETOOTH_SCAN)
                log("⚠️ BLUETOOTH_SCAN needed")
            }
            if (ContextCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED) {
                needed.add(Manifest.permission.BLUETOOTH_CONNECT)
                log("⚠️ BLUETOOTH_CONNECT needed")
            }
        } else {
            if (ContextCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH) != PackageManager.PERMISSION_GRANTED) {
                needed.add(Manifest.permission.BLUETOOTH)
                log("⚠️ BLUETOOTH needed")
            }
            if (ContextCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_ADMIN) != PackageManager.PERMISSION_GRANTED) {
                needed.add(Manifest.permission.BLUETOOTH_ADMIN)
                log("⚠️ BLUETOOTH_ADMIN needed")
            }
        }

        // LOCATION permissions (CRITICAL for BLE)
        if (ContextCompat.checkSelfPermission(this, Manifest.permission.ACCESS_FINE_LOCATION) != PackageManager.PERMISSION_GRANTED) {
            needed.add(Manifest.permission.ACCESS_FINE_LOCATION)
            log("⚠️ ACCESS_FINE_LOCATION needed (CRITICAL!)")
        }

        if (ContextCompat.checkSelfPermission(this, Manifest.permission.ACCESS_COARSE_LOCATION) != PackageManager.PERMISSION_GRANTED) {
            needed.add(Manifest.permission.ACCESS_COARSE_LOCATION)
            log("⚠️ ACCESS_COARSE_LOCATION needed")
        }

        if (needed.isNotEmpty()) {
            log("🔐 Requesting ${needed.size} permissions...")
            ActivityCompat.requestPermissions(this, needed.toTypedArray(), 100)
        } else {
            log("✅ All permissions already granted!")
        }
    }

    private fun checkAllPermissions() {
        log("\n📋 Permission Status:")
        
        val perms = mutableListOf<String>()
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            perms.add(Manifest.permission.BLUETOOTH_SCAN)
            perms.add(Manifest.permission.BLUETOOTH_CONNECT)
        } else {
            perms.add(Manifest.permission.BLUETOOTH)
            perms.add(Manifest.permission.BLUETOOTH_ADMIN)
        }
        perms.add(Manifest.permission.ACCESS_FINE_LOCATION)
        perms.add(Manifest.permission.ACCESS_COARSE_LOCATION)

        var allOK = true
        for (perm in perms) {
            val granted = ContextCompat.checkSelfPermission(this, perm) == PackageManager.PERMISSION_GRANTED
            log("  ${if(granted) "✅" else "❌"} $perm")
            if (!granted) allOK = false
        }

        if (allOK) {
            log("✅ ALL PERMISSIONS OK - Ready to scan!")
        } else {
            log("❌ MISSING PERMISSIONS - Some denied!")
        }
    }

    override fun onRequestPermissionsResult(rq: Int, perms: Array<String>, grants: IntArray) {
        super.onRequestPermissionsResult(rq, perms, grants)
        if (rq == 100) {
            log("🔄 Permission dialog closed")
            Thread.sleep(500)
            checkAllPermissions()
        }
    }

    private fun scanBLE() {
        if (isScanning) {
            log("Already scanning...")
            return
        }

        log("\n════════════════════════════════════════")
        log("🔍 SCAN START (30 seconds)")
        log("════════════════════════════════════════")

        // Check Bluetooth is ON
        if (!bluetoothAdapter?.isEnabled!!) {
            log("❌ Bluetooth is OFF")
            return
        }
        log("✅ Bluetooth is ON")

        // Check FINE_LOCATION permission
        if (ContextCompat.checkSelfPermission(this, Manifest.permission.ACCESS_FINE_LOCATION) != PackageManager.PERMISSION_GRANTED) {
            log("❌ ACCESS_FINE_LOCATION permission DENIED!")
            log("   → Go to Settings, grant permission, restart app")
            return
        }
        log("✅ Location permission OK")

        // Check BLUETOOTH_SCAN permission (Android 12+)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            if (ContextCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_SCAN) != PackageManager.PERMISSION_GRANTED) {
                log("❌ BLUETOOTH_SCAN permission DENIED!")
                return
            }
            log("✅ BLUETOOTH_SCAN permission OK")
        }

        try {
            val scanner = bluetoothAdapter?.bluetoothLeScanner ?: run {
                log("❌ BluetoothLeScanner is NULL")
                return
            }
            log("✅ BluetoothLeScanner obtained")

            isScanning = true
            var count = 0

            val callback = object : android.bluetooth.le.ScanCallback() {
                override fun onScanResult(callbackType: Int, result: android.bluetooth.le.ScanResult?) {
                    if (result != null && result.device != null) {
                        count++
                        val name = result.device.name ?: "(no name)"
                        val addr = result.device.address ?: "(no addr)"
                        val rssi = result.rssi
                        log("   [$count] $name / $addr (RSSI: $rssi)")
                    }
                }

                override fun onScanFailed(errorCode: Int) {
                    val msg = when(errorCode) {
                        1 -> "ALREADY_STARTED"
                        2 -> "APP_REGISTRATION_FAILED"
                        3 -> "INTERNAL_ERROR"
                        4 -> "FEATURE_UNSUPPORTED"
                        5 -> "OUT_OF_HARDWARE_RESOURCES"
                        else -> "CODE_$errorCode"
                    }
                    log("❌ SCAN FAILED: $msg")
                    isScanning = false
                }
            }

            log("✅ Starting scan with NO settings (default)...")
            scanner.startScan(callback)
            log("✅ Scan is RUNNING")

            Thread {
                Thread.sleep(30000)
                runOnUiThread {
                    try {
                        scanner.stopScan(callback)
                        isScanning = false
                        log("")
                        log("════════════════════════════════════════")
                        log("🛑 SCAN STOPPED")
                        log("📊 TOTAL DEVICES FOUND: $count")
                        log("════════════════════════════════════════")
                    } catch (e: Exception) {
                        log("❌ Stop error: ${e.message}")
                    }
                }
            }.start()

        } catch (e: SecurityException) {
            log("❌ SecurityException: ${e.message}")
            log("   (Permission issue)")
            isScanning = false
        } catch (e: Exception) {
            log("❌ Exception: ${e.message}")
            e.printStackTrace()
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
