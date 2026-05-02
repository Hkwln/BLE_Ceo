package com.example.bleheadphones

import android.Manifest
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothManager
import android.content.Context
import android.content.pm.PackageManager
import android.location.LocationManager
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
        log("BLE Test App v3.1 - Diagnostic Mode")
        log("════════════════════════════════════════")

        requestPermissions()

        val bluetoothManager = getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
        bluetoothAdapter = bluetoothManager.adapter

        if (bluetoothAdapter == null) {
            log("❌ NO BLUETOOTH SUPPORT")
            return
        }

        log("✅ Bluetooth adapter found")
        
        // Check Android version
        log("Android: API ${Build.VERSION.SDK_INT}")
        
        // Check Location Services
        checkLocationServices()
        
        checkState()

        findViewById<Button>(R.id.bt_on_button).setOnClickListener { turnBtOn() }
        findViewById<Button>(R.id.bt_off_button).setOnClickListener { turnBtOff() }
        findViewById<Button>(R.id.scan_button).setOnClickListener { doScan() }
        findViewById<Button>(R.id.clear_button).setOnClickListener { logText?.text = "" }
    }

    private fun checkLocationServices() {
        val locMgr = getSystemService(Context.LOCATION_SERVICE) as LocationManager
        val gpsEnabled = locMgr.isProviderEnabled(LocationManager.GPS_PROVIDER)
        val networkEnabled = locMgr.isProviderEnabled(LocationManager.NETWORK_PROVIDER)
        
        log("Location GPS: ${if(gpsEnabled) "✅" else "❌"}")
        log("Location Network: ${if(networkEnabled) "✅" else "❌"}")
        
        if (!gpsEnabled && !networkEnabled) {
            log("⚠️  Location Services OFF - BLE scan may fail!")
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
        } else {
            if (ContextCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH) != PackageManager.PERMISSION_GRANTED) {
                needed.add(Manifest.permission.BLUETOOTH)
            }
            if (ContextCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_ADMIN) != PackageManager.PERMISSION_GRANTED) {
                needed.add(Manifest.permission.BLUETOOTH_ADMIN)
            }
        }

        // Location is CRITICAL for BLE scan on Android 6+
        if (ContextCompat.checkSelfPermission(this, Manifest.permission.ACCESS_FINE_LOCATION) != PackageManager.PERMISSION_GRANTED) {
            needed.add(Manifest.permission.ACCESS_FINE_LOCATION)
        }
        if (ContextCompat.checkSelfPermission(this, Manifest.permission.ACCESS_COARSE_LOCATION) != PackageManager.PERMISSION_GRANTED) {
            needed.add(Manifest.permission.ACCESS_COARSE_LOCATION)
        }

        if (needed.isNotEmpty()) {
            log("🔐 Requesting permissions:")
            needed.forEach { log("   - $it") }
            ActivityCompat.requestPermissions(this, needed.toTypedArray(), 100)
        } else {
            log("✅ All permissions granted")
        }
    }

    override fun onRequestPermissionsResult(rq: Int, perms: Array<String>, grants: IntArray) {
        super.onRequestPermissionsResult(rq, perms, grants)
        if (rq == 100) {
            val missing = perms.filterIndexed { idx, _ -> 
                grants.getOrNull(idx) != PackageManager.PERMISSION_GRANTED 
            }
            if (missing.isEmpty()) {
                log("✅ All permissions GRANTED")
            } else {
                log("❌ Missing: ${missing.joinToString()}")
            }
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

        // Pre-scan checks
        if (!bluetoothAdapter?.isEnabled!!) {
            log("❌ Bluetooth OFF!")
            return
        }

        if (ContextCompat.checkSelfPermission(this, Manifest.permission.ACCESS_FINE_LOCATION) != PackageManager.PERMISSION_GRANTED) {
            log("❌ Location permission DENIED")
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
                .setMatchMode(android.bluetooth.le.ScanSettings.MATCH_MODE_AGGRESSIVE)
                .build()

            log("✅ Scanner ready, settings built")
            var count = 0

            scanCallback = object : android.bluetooth.le.ScanCallback() {
                override fun onScanResult(callbackType: Int, result: android.bluetooth.le.ScanResult?) {
                    if (result != null) {
                        count++
                        val name = result.device?.name ?: "unknown"
                        val addr = result.device?.address ?: "?"
                        val rssi = result.rssi
                        log("   [$count] $name ($addr) RSSI:$rssi")
                    }
                }

                override fun onScanFailed(errorCode: Int) {
                    val msg = when(errorCode) {
                        1 -> "ALREADY_STARTED"
                        2 -> "APP_REGISTRATION_FAILED"
                        3 -> "INTERNAL_ERROR"
                        4 -> "FEATURE_UNSUPPORTED"
                        5 -> "OUT_OF_HARDWARE_RESOURCES"
                        else -> "ERROR_$errorCode"
                    }
                    log("❌ Scan failed: $msg")
                }
            }

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                if (ActivityCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_SCAN) != PackageManager.PERMISSION_GRANTED) {
                    log("❌ Missing BLUETOOTH_SCAN")
                    return
                }
            }

            scanner.startScan(null, settings, scanCallback!!)
            log("✅ Scan STARTED")
            updateStatus("🔍 Scanning...")

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
                        log("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━")
                        log("📊 TOTAL FOUND: $count devices")
                        log("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━")
                        updateStatus("Found: $count")
                    } catch (e: Exception) {
                        log("❌ Stop: ${e.message}")
                    }
                }
            }.start()

        } catch (e: SecurityException) {
            log("❌ SecurityException: ${e.message}")
        } catch (e: Exception) {
            log("❌ Error: ${e.message}")
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
