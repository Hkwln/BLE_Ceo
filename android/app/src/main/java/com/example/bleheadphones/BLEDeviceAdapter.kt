package com.example.bleheadphones

import android.bluetooth.BluetoothDevice
import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.RecyclerView
import com.google.android.material.card.MaterialCardView
import android.widget.TextView

class BLEDeviceAdapter(
    private var devices: List<BluetoothDevice>,
    private val onDeviceClick: (BluetoothDevice) -> Unit
) : RecyclerView.Adapter<BLEDeviceAdapter.DeviceViewHolder>() {

    class DeviceViewHolder(itemView: MaterialCardView) : RecyclerView.ViewHolder(itemView) {
        val deviceNameView: TextView = itemView.findViewById(R.id.device_name)
        val deviceAddressView: TextView = itemView.findViewById(R.id.device_address)
        val card: MaterialCardView = itemView

        fun bind(device: BluetoothDevice, onDeviceClick: (BluetoothDevice) -> Unit) {
            deviceNameView.text = device.name ?: "Unknown Device"
            deviceAddressView.text = device.address
            
            card.setOnClickListener {
                onDeviceClick(device)
            }
        }
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): DeviceViewHolder {
        val card = LayoutInflater.from(parent.context)
            .inflate(R.layout.device_card, parent, false) as MaterialCardView
        return DeviceViewHolder(card)
    }

    override fun onBindViewHolder(holder: DeviceViewHolder, position: Int) {
        holder.bind(devices[position], onDeviceClick)
    }

    override fun getItemCount() = devices.size

    fun updateDevices(newDevices: List<BluetoothDevice>) {
        devices = newDevices
        notifyDataSetChanged()
    }
}
