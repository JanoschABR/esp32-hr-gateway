# ESP32 HR Gateway
An ESP32 based WebSocket gateway for BLE connected Heartrate sensors.  

# Functionality
Once flashed onto the ESP32 and started, it will begin looking for a BLE device with the correct service (0x180D by default).
If multiple are detected, the one with the highest signal strength is choosen. Multiple sensors at once are not supported.  
It will connect to the configured WiFi network and open an unencrypted (ws://) WebSocket.  
MDNS is used to make the device available under a name ('hr-gateway' by default) instead of its IP-Address.

If everything is working correctly, the program will automatically detect if a WebSocket or Sensor connects/disconnected and manage everything automatically.
If none of the settings/code is changed, the gateway will then be available at ```ws://hr-gateway/``` and will output the current BPM as a string.
If no sensor is currently connected the websocket will still be available, but will send ```-1``` instead of the BPM.

# Compatibility
Currently only tested with a [CooSpo ‎H808S](https://www.amazon.de/H808S-Bluetooth-Accurate-Waterproof-Compatible/dp/B07PNNMSGS) but any BLE Heartrate sensor that uses the official
Bluetooth 'Heart Rate' service (0x180D) and supports the 'Heart Rate Measurement' characteristic (0x2A37) should work.

## Note
This is a very primitive implementation which only uses the generic 'Heart Rate Measurement' characteristic, instead of properly asking the device which type and resolution it supports, etc.  
If you are having trouble getting a proper readout from your sensor, you could try to change the service/characteristic ID to one of the alternative implementations (e.g. 0x2A7E, 0x2A88).  
For more information about Bluetooth services and characteristics see the [Bluetooth 'Assigned Numbers'](https://www.bluetooth.com/specifications/assigned-numbers/).
