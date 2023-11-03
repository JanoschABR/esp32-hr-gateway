#include <WiFi.h>
#include <WiFiClient.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebSrv.h>
#include <ESPmDNS.h>

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>


// Configuration
const char*   apSSID      = "";
const char*   apPassword  = "";
const char*   mdnsName    = "hr-gateway";

BLEUUID       hrService   = BLEUUID::fromString("0000180d-0000-1000-8000-00805f9b34fb"); // Heart Rate Service
BLEUUID       hrChar      = BLEUUID::fromString("00002a37-0000-1000-8000-00805f9b34fb"); // Heart Rate Measurement

// Server and Websocket
AsyncWebServer server (80);
AsyncWebSocket ws ("/");


// BLE scan result
static bool                 scanResultFound   = false;
static int                  scanResultRSSI    = -128;
static BLEAdvertisedDevice  scanResultDevice;

// BLE scan callback
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice device) {

      // Consider using this device if it supports heart rate measurement
      if (device.getServiceUUID().equals(hrService)) {

        // If specified, use the RSSI, otherwise set to -127 (which is still better than the default -128).
        // This makes it possible to use this device as a fallback if no other is found
        int rssi = -127;
        if (device.haveRSSI()) {
          rssi = device.getRSSI();
        }

        // Is it better than our current best?
        if (rssi > scanResultRSSI) {

          // Save this device as our new best
          scanResultFound = true;
          scanResultDevice = device;
          scanResultRSSI = device.getRSSI();
        }
      }
    }
};


// Status variables
static bool   hrConnected   = false;  // Is the heart rate sensor connected?
static bool   hrConnecting  = false;  // Is the heart rate sensor connecting?
static bool   wsConnected   = false;  // Is the websocket connected?
static bool   wsConnecting  = false;  // Is the websocket connecting?

// BLE device callbacks
class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {
    Serial.println("BLE device connected!");
    hrConnected = true;
  }

  void onDisconnect(BLEClient* pclient) {
    Serial.println("BLE device disconnected!");
    hrConnected = false;
  }
};

// BLE notify callback
static void notifyCallback (BLERemoteCharacteristic* rchar, uint8_t* data, size_t length, bool notify) {
    //Serial.print("Received heartrate: ");
    //Serial.println(data[1]);

    ws.printfAll("%d", data[1]);
    Serial.print("Received heartrate: ");
    Serial.println(data[1]);
}

bool setupBLE () {
  // Setup BLE
  BLEDevice::init("");

  // Reset scan results (incase this isn't the first scan)
  scanResultFound   = false;
  scanResultRSSI    = -128;

  // Prepare a BLE scan
  BLEScan* scan = BLEDevice::getScan();
  scan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  scan->setActiveScan(true); // Active scan uses more power but gets results faster
  scan->setInterval(100);
  scan->setWindow(99);

  // Scan for BLE devices
  BLEScanResults result = scan->start(5, false);
  Serial.printf("Scan complete. Found %d device(s)\n", result.getCount());
  scan->clearResults();

  // Check if the scan actually returned a valid result
  if (!scanResultFound) {
    Serial.println("No sensor found!");
    return false;
  } else {

    // Print some basic info about the sensor
    const char* name = (scanResultDevice.haveName() ? scanResultDevice.getName() : scanResultDevice.getAddress().toString()).c_str();
    Serial.printf("Found compatible sensor: %s (RSSI: %d)\n", name, scanResultRSSI);

    // Connect the client
    BLEClient* client = BLEDevice::createClient();
    client->setClientCallbacks(new MyClientCallback());
    client->connect(&scanResultDevice);

    // Retrieve remote references
    BLERemoteService* hrs = client->getService(hrService);
    BLERemoteCharacteristic* hrc = hrs->getCharacteristic(hrChar);

    // Setup notifications
    if (hrc->canNotify()) {
      Serial.printf("Registered notify callback for %s characteristic.\n", hrChar.toString().c_str());
      hrc->registerForNotify(notifyCallback);
      return true;
    } else {
      Serial.println("Failed to register notify callback.");
      return false;
    }
  }
}


// Websocket events
void onWebsocketEvent (AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      wsConnected = true;
      wsConnecting = false;
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      wsConnected = false;
      wsConnecting = false;
      break;
    case WS_EVT_DATA:
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}


void setup() {
  // Begin serial port
  Serial.begin(115200);
  Serial.println();

  // Connect to the WiFi
  WiFi.begin(apSSID, apPassword);
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
  }

  // Print IP address
  Serial.print("Connected! IP Address is ");
  Serial.println(WiFi.localIP());

  // Setup MDNS name
  if (MDNS.begin(mdnsName)) {
    Serial.printf("MDNS responder started. Device should now be reachable as '%s'\n", mdnsName);
  }

  // Setup websocket handler
  ws.onEvent(onWebsocketEvent);
  server.addHandler(&ws);

  // Start the server
  server.begin();
  Serial.println();
}

void loop() {

  Serial.printf("Status updated: Waiting for HR or WS connection.. (ws=%d%d,hr=%d%d)\n", wsConnected, wsConnecting, hrConnected, hrConnecting);
  while (!wsConnected || !wsConnecting || !hrConnected || !hrConnecting) {
    delay(2);
    ws.cleanupClients();

    if (!(hrConnected || hrConnecting)) {
      if (!setupBLE()) {
        // If BLE setup failed, try again in a bit
        hrConnected = false;
        hrConnecting = false;
        ws.textAll("-1"); // Send a heartrate of -1 to indicate that no sensor is connected
      } else {
        delay(1000);
      }
    }
  }

  Serial.printf("Status updated: Running. (ws=%d%d,hr=%d%d)\n", wsConnected, wsConnecting, hrConnected, hrConnecting);
  while ((wsConnected || wsConnecting) && hrConnected) {
    delay(2);
    ws.cleanupClients();
  }
}














