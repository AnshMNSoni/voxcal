#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>

// Wi-Fi Configuration

const char* WIFI_SSID = "Wi-Fi";
const char* WIFI_PASSWORD = "@nsh.mn.$oni";

// VoxCal Gateway Configuration

// Laptop's Mobile Hotspot IP
const char* GATEWAY_HOST = "192.168.137.1";

const uint16_t GATEWAY_PORT = 8000;
const char* GATEWAY_PATH = "/ws";

// Device Configuration

const char* DEVICE_ID = "esp32-01";

// WebSocket Client

WebSocketsClient webSocket;

// WebSocket Event Handler

void webSocketEvent(
  WStype_t type,
  uint8_t* payload,
  size_t length
) {

  switch (type) {

    case WStype_DISCONNECTED:

      Serial.println("[WebSocket] Disconnected from Gateway");

      break;


    case WStype_CONNECTED:

      Serial.println("[WebSocket] Connected to Gateway");

      // Create test JSON message

      {
        JsonDocument doc;

        doc["type"] = "ping";
        doc["device_id"] = DEVICE_ID;
        doc["message"] = "Hello VoxCal";

        String message;

        serializeJson(doc, message);

        Serial.println();
        Serial.println("Sending:");
        Serial.println(message);

        webSocket.sendTXT(message);
      }

      break;


    case WStype_TEXT:

      Serial.println();
      Serial.println("Received:");
      Serial.println((char*)payload);

      break;


    case WStype_ERROR:

      Serial.println("[WebSocket] Error");

      break;


    default:

      break;
  }
}

// Connect to Wi-Fi

void connectWiFi() {

  Serial.println();
  Serial.println("Connecting to Wi-Fi...");

  WiFi.mode(WIFI_STA);

  WiFi.begin(
    WIFI_SSID,
    WIFI_PASSWORD
  );

  while (WiFi.status() != WL_CONNECTED) {

    delay(500);

    Serial.print(".");
  }

  Serial.println();
  Serial.println("Wi-Fi connected!");

  Serial.print("ESP32 IP: ");
  Serial.println(WiFi.localIP());

  Serial.print("Wi-Fi Gateway: ");
  Serial.println(WiFi.gatewayIP());
}


// Setup

void setup() {

  Serial.begin(115200);

  delay(1000);

  Serial.println();
  Serial.println("==============================");
  Serial.println("         VoxCal ESP32");
  Serial.println("==============================");

  // Connect to Wi-Fi

  connectWiFi();

  // Configure WebSocket

  Serial.println();
  Serial.println("Connecting to Gateway...");

  webSocket.begin(
    GATEWAY_HOST,
    GATEWAY_PORT,
    GATEWAY_PATH
  );

  // Register event handler
  webSocket.onEvent(webSocketEvent);

  // Retry connection every 5 seconds
  webSocket.setReconnectInterval(5000);
}

// Main Loop

void loop() {

  webSocket.loop();

}