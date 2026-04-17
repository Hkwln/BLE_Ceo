package com.example.bleheadphones

import android.content.Context
import android.media.AudioAttributes
import android.media.AudioFormat
import android.media.AudioPlaybackCaptureConfiguration
import android.media.AudioRecord
import android.media.MediaRecorder
import android.media.projection.MediaProjection
import android.util.Log

class AudioCaptureManager(val context: Context) {

    private var audioRecord: AudioRecord? = null
    private var isCapturing = false
    private val SAMPLE_RATE = 44100
    private val CHANNEL_CONFIG = AudioFormat.CHANNEL_IN_STEREO
    private val AUDIO_FORMAT = AudioFormat.ENCODING_PCM_16BIT

    fun startCapture(mediaProjection: MediaProjection?, onAudioData: (ByteArray) -> Unit) {
        if (isCapturing) return

        isCapturing = true
        Log.d("AudioCaptureManager", "Starting audio capture")

        try {
            val bufferSize = AudioRecord.getMinBufferSize(SAMPLE_RATE, CHANNEL_CONFIG, AUDIO_FORMAT)

            val config = AudioPlaybackCaptureConfiguration.Builder(mediaProjection!!)
                .addMatchingUsage(AudioAttributes.USAGE_MEDIA)
                .build()

            audioRecord = AudioRecord.Builder()
                .setAudioSource(MediaRecorder.AudioSource.DEFAULT)
                .setAudioFormat(AudioFormat.Builder()
                    .setEncoding(AUDIO_FORMAT)
                    .setSampleRate(SAMPLE_RATE)
                    .setChannelMask(CHANNEL_CONFIG)
                    .build())
                .setBufferSizeInBytes(bufferSize)
                .setAudioPlaybackCaptureConfig(config)
                .build()

            audioRecord?.startRecording()
            Log.d("AudioCaptureManager", "Audio recording started")

            // Capture audio in background thread
            Thread {
                val buffer = ByteArray(bufferSize)
                while (isCapturing) {
                    val bytesRead = audioRecord?.read(buffer, 0, bufferSize) ?: 0
                    if (bytesRead > 0) {
                        onAudioData(buffer.copyOf(bytesRead))
                    }
                }
            }.start()
        } catch (e: Exception) {
            Log.e("AudioCaptureManager", "Failed to start capture: ${e.message}")
            isCapturing = false
        }
    }

    fun stopCapture() {
        isCapturing = false
        try {
            audioRecord?.stop()
            audioRecord?.release()
            Log.d("AudioCaptureManager", "Audio capture stopped")
        } catch (e: Exception) {
            Log.e("AudioCaptureManager", "Error stopping capture: ${e.message}")
        }
    }
}
