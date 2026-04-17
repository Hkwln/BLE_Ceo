package com.example.bleheadphones

import android.content.Context
import android.hardware.usb.UsbManager
import android.util.Log
import com.hoho.android.usbserial.driver.UsbSerialPort
import com.hoho.android.usbserial.driver.UsbSerialProber
import com.hoho.android.usbserial.util.SerialInputOutputManager
import java.io.IOException

class USBSerialManager(val context: Context) : SerialInputOutputManager.OnNewDataListener {

    private val usbManager = context.getSystemService(Context.USB_SERVICE) as UsbManager
    var serialPort: UsbSerialPort? = null
    private var ioManager: SerialInputOutputManager? = null
    private var onDataReceived: ((String) -> Unit)? = null

    fun connect(): Boolean {
        return try {
            val availablePorts = UsbSerialProber.getDefaultProber().findAllPorts(usbManager)

            if (availablePorts.isEmpty()) {
                Log.e("USBSerialManager", "No USB ports found")
                return false
            }

            serialPort = availablePorts[0]
            val usbDevice = serialPort?.device
            val connection = usbManager.openDevice(usbDevice)

            if (connection == null) {
                Log.e("USBSerialManager", "Failed to open USB device")
                return false
            }

            serialPort?.open(connection)
            serialPort?.setParameters(115200, 8, UsbSerialPort.STOPBITS_1, UsbSerialPort.PARITY_NONE)

            ioManager = SerialInputOutputManager(serialPort, this)
            ioManager?.start()

            Log.d("USBSerialManager", "USB Serial connected")
            true
        } catch (e: Exception) {
            Log.e("USBSerialManager", "USB connection failed: ${e.message}")
            false
        }
    }

    fun sendData(data: ByteArray): Boolean {
        return try {
            serialPort?.write(data, 1000)
            true
        } catch (e: IOException) {
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
            ioManager?.stop()
            serialPort?.close()
            Log.d("USBSerialManager", "USB Serial closed")
        } catch (e: IOException) {
            Log.e("USBSerialManager", "Error closing: ${e.message}")
        }
    }

    override fun onNewData(data: ByteArray) {
        val message = String(data).trim()
        Log.d("USBSerialManager", "Received: $message")
        onDataReceived?.invoke(message)
    }
}
