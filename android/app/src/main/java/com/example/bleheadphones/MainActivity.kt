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
        log("BLE Scanner v5.2 - Permission Helper")
        log("════════════════════════════════════════")

        val bluetoothManager = getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
        bluetoothAdapter = bluetoothManager.adapter

        if (bluetoothAdapter == null) {
            log("❌ NO BLUETOOTH")
            return
        }

        log("✅ Bluetooth adapter found")
        
        if (!bluetoothAdapter?.isEnabled!!) {
            log("⚠️  Bluetooth is OFF")
        } else {
            log("✅ Bluetooth is ON")
        }

        // Check permissions and show help if needed
        checkAndFixPermissions()
    }

    private fun checkAndFixPermissions() {
        log("\n📋 Checking permissions...")

        val locationOK = ContextCompat.checkSelfPermission(
            this,
            Manifest.permission.ACCESS_FINE_LOCATION
        ) == PackageManager.PERMISSION_GRANTED

        val bluetoothScanOK = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            ContextCompat.checkSelfPermission(
                this,
                Manifest.permission.BLUETOOTH_SCAN
            ) == PackageManager.PERMISSION_GRANTED
        } else {
            true
        }

        log("Location: ${if(locationOK) "✅" else "❌"}")
        log("BLE Scan: ${if(bluetoothScanOK) "✅" else "❌"}")

        if (!locationOK || !bluetoothScanOK) {
            log("\n⚠️  Permissions missing!")
            showPermissionDialog()
        } else {
            log("\n✅ All permissions OK!")
        }
    }

    private fun showPermissionDialog() {
        AlertDialog.Builder(this)
            .setTitle("⚠️ Permissions Needed")
            .setMessage(
                "BLE Scanning requires:\n\n" +
                "• Location (Standort)\n" +
                "• Bluetooth Scan\n\n" +
                "Tap 'Open Settings' and enable ALL permissions"
            )
            .setPositiveButton("Open Settings") { _, _ ->
                openAppSettings()
            }
            .setNegativeButton("Cancel", null)
            .show()
    }

    private fun openAppSettings() {
        log("\n→ Opening app settings...")
        
        val intent = Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS).apply {
            data = Uri.fromParts("package", packageName, null)
        }
        startActivity(intent)
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

        val locationOK = ContextCompat.checkSelfPermission(
            this,
            Manifest.permission.ACCESS_FINE_LOCATION
        ) == PackageManager.PERMISSION_GRANTED

        if (!locationOK) {
            log("❌ Location permission DENIED")
            AlertDialog.Builder(this)
                .setTitle("Need Location Permission")
                .setMessage("Go to Settings → Permissions → Location and enable it")
                .setPositiveButton("Open Settings") { _, _ ->
                    openAppSettings()
                }
                .show()
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
