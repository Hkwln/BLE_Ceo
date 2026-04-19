package com.example.bleheadphones

import android.util.Log

class HandshakeProtocol(val bleManager: BLEManager, val usbSerialManager: USBSerialManager) {

    private var isUSBConnected = false
    private var isBLEAudioActive = false
    private var monitorThread: Thread? = null

    fun startAudioStream() {
        isBLEAudioActive = true
        monitorThread = Thread {
            try {
                monitorUSBConnection()
            } catch (e: Exception) {
                Log.e("HandshakeProtocol", "Monitor thread error: ${e.message}")
            }
        }
        monitorThread?.start()

        usbSerialManager.setOnDataReceived { message ->
            handleUSBMessage(message)
        }
    }

    private fun monitorUSBConnection() {
        while (isBLEAudioActive) {
            // Check USB status
            val usbConnected = usbSerialManager.connect() // Attempt connection as check

            if (usbConnected && !isUSBConnected) {
                Log.d("HandshakeProtocol", "USB detected!")
                switchToUSBMode()
            } else if (!usbConnected && isUSBConnected) {
                Log.d("HandshakeProtocol", "USB disconnected!")
                switchToBLEMode()
            }

            Thread.sleep(500) // Check every 500ms
        }
    }

    private fun handleUSBMessage(message: String) {
        if (message.contains("USB_AUDIO_MODE_READY")) {
            Log.d("HandshakeProtocol", "ESP32 ready for USB audio!")
            usbSerialManager.sendString("USB_AUDIO_MODE_ACK")
            isUSBConnected = true
        }
    }

    private fun switchToUSBMode() {
        Log.d("HandshakeProtocol", "Switching to USB mode")
        isUSBConnected = true
        isBLEAudioActive = false

        // Connect to USB and wait for handshake
        if (usbSerialManager.connect()) {
            Log.d("HandshakeProtocol", "USB connected, waiting for handshake...")
        } else {
            Log.e("HandshakeProtocol", "Failed to connect to USB")
        }
    }

    private fun switchToBLEMode() {
        Log.d("HandshakeProtocol", "Switching back to BLE mode")
        isUSBConnected = false
        isBLEAudioActive = true
        usbSerialManager.close()
    }

    fun stop() {
        isBLEAudioActive = false
        monitorThread?.join(1000)
    }

    fun isUSBMode(): Boolean = isUSBConnected
    fun isBLEMode(): Boolean = isBLEAudioActive
}
