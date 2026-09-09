#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>

// =====================================================
// Wi-Fi Configuration
// =====================================================

// Wi-Fi credentials are kept private in secrets.h / .env.
// Copy secrets.h.example to secrets.h and set your Wi-Fi credentials.
#if __has_include("secrets.h")
  #include "secrets.h"
#else
  const char* WIFI_SSID = "YOUR_WIFI_SSID";
  const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
#endif

// =====================================================
// VoxCal Gateway Configuration
// =====================================================

// Laptop's Mobile Hotspot IP
const char* GATEWAY_HOST = "192.168.137.1";

const uint16_t GATEWAY_PORT = 8000;
const char* GATEWAY_PATH = "/ws";

// =====================================================
// Device Configuration
// =====================================================

const char* DEVICE_ID = "esp32-01";

// =====================================================
// WebSocket Client
// =====================================================

WebSocketsClient webSocket;

// =====================================================
// Serial Command Buffer
// =====================================================

String serialCommand = "";

// =====================================================
// WebSocket Event Handler
// =====================================================

void webSocketEvent(
  WStype_t type,
  uint8_t* payload,
  size_t length
) {

  switch (type) {

    // -------------------------------------------------
    // Disconnected
    // -------------------------------------------------

    case WStype_DISCONNECTED:

      Serial.println();
      Serial.println("[WebSocket] Disconnected from Gateway");

      break;


    // -------------------------------------------------
    // Connected
    // -------------------------------------------------

    case WStype_CONNECTED:

      Serial.println();
      Serial.println("[WebSocket] Connected to Gateway");

      Serial.println();
      Serial.println("======================================");
      Serial.println("VoxCal is ready.");
      Serial.println("Type your calendar command below.");
      Serial.println("Example:");
      Serial.println("Create an Event tomorrow named Ansh7505 between 3 PM to 4 PM");
      Serial.println("======================================");
      Serial.println();

      break;


    // -------------------------------------------------
    // Text received from Gateway
    // -------------------------------------------------

    case WStype_TEXT:

      Serial.println();
      Serial.println("[VoxCal] Response received:");
      Serial.println("--------------------------------------");
      Serial.println((char*)payload);
      Serial.println("--------------------------------------");
      Serial.println();

      break;


    // -------------------------------------------------
    // WebSocket error
    // -------------------------------------------------

    case WStype_ERROR:

      Serial.println();
      Serial.println("[WebSocket] Error");

      break;


    default:

      break;
  }
}

// =====================================================
// Connect to Wi-Fi
// =====================================================

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

// =====================================================
// Send Command to VoxCal Gateway
// =====================================================

void sendCommand(String command) {

  // Ignore empty commands
  command.trim();

  if (command.length() == 0) {
    return;
  }

  // Check WebSocket connection
  if (!webSocket.isConnected()) {

    Serial.println();
    Serial.println("[VoxCal] Gateway is not connected.");
    Serial.println("Command was not sent.");
    Serial.println();

    return;
  }

  // Create JSON document
  JsonDocument doc;

  doc["type"] = "command";
  doc["device_id"] = DEVICE_ID;
  doc["message"] = command;

  // Convert JSON to String
  String message;

  serializeJson(doc, message);

  // Show what is being sent
  Serial.println();
  Serial.println("[VoxCal] Sending command:");
  Serial.println("--------------------------------------");
  Serial.println(message);
  Serial.println("--------------------------------------");

  // Send through WebSocket
  webSocket.sendTXT(message);

  Serial.println("[VoxCal] Command sent to Gateway.");
  Serial.println();
}

// =====================================================
// Read Command from Serial Monitor
// =====================================================

void readSerialCommand() {

  while (Serial.available()) {

    char c = Serial.read();

    // Enter / newline means command is complete
    if (c == '\n' || c == '\r') {

      if (serialCommand.length() > 0) {

        sendCommand(serialCommand);

        serialCommand = "";
      }

    } else {

      serialCommand += c;
    }
  }
}

// =====================================================
// Setup
// =====================================================

void setup() {

  Serial.begin(115200);

  delay(1000);

  Serial.println();
  Serial.println("==============================");
  Serial.println("        VoxCal ESP32");
  Serial.println("==============================");

  // ---------------------------------------------------
  // Connect to Wi-Fi
  // ---------------------------------------------------

  connectWiFi();

  // ---------------------------------------------------
  // Configure WebSocket
  // ---------------------------------------------------

  Serial.println();
  Serial.println("Connecting to Gateway...");

  webSocket.begin(
    GATEWAY_HOST,
    GATEWAY_PORT,
    GATEWAY_PATH
  );

  // Register WebSocket event handler
  webSocket.onEvent(webSocketEvent);

  // Retry connection every 5 seconds
  webSocket.setReconnectInterval(5000);
}

// =====================================================
// Main Loop
// =====================================================

void loop() {

  // Keep WebSocket connection alive
  webSocket.loop();

  // Check Serial Monitor for commands
  readSerialCommand();
}