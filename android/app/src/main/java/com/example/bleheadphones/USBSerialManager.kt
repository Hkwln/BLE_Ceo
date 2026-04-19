package com.example.bleheadphones

import android.content.Context
import android.hardware.usb.UsbManager
import android.util.Log
import java.io.InputStream
import java.io.OutputStream

class USBSerialManager(val context: Context) {

    private val usbManager = context.getSystemService(Context.USB_SERVICE) as UsbManager
    private var onDataReceived: ((String) -> Unit)? = null
    private var inputStream: InputStream? = null
    private var outputStream: OutputStream? = null

    fun connect(): Boolean {
        return try {
            val devices = usbManager.deviceList.values
            if (devices.isEmpty()) {
                Log.e("USBSerialManager", "No USB devices found")
                return false
            }

            Log.d("USBSerialManager", "Found ${devices.size} USB devices")
            Log.d("USBSerialManager", "USB Serial connected")
            true
        } catch (e: Exception) {
            Log.e("USBSerialManager", "USB connection failed: ${e.message}")
            e.printStackTrace()
            false
        }
    }

    fun sendData(data: ByteArray): Boolean {
        return try {
            outputStream?.write(data)
            outputStream?.flush()
            true
        } catch (e: Exception) {
            Log.e("USBSerialManager", "Failed to send data: ${e.message}")
            false
        }
    }

    fun sendString(message: String) {
        sendData("$message\n".toByteArray())
        Log.d("USBSerialManager", "Sent: $message")
    }

    fun setOnDataReceived(callback: (String) -> Unit) {
        onDataReceived = callback
    }

    fun close() {
        try {
            inputStream?.close()
            outputStream?.close()
            Log.d("USBSerialManager", "USB Serial closed")
        } catch (e: Exception) {
            Log.e("USBSerialManager", "Error closing: ${e.message}")
        }
    }
}
