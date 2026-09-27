#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include "driver/i2s.h"

// =====================================================
// Pin Definitions
// =====================================================
#define I2S_PORT       I2S_NUM_0    // Both mic & speaker dynamically share Port 0
#define SAMPLE_RATE    16000

// INMP441 MEMS Microphone
#define I2S_MIC_SCK    32
#define I2S_MIC_WS     33
#define I2S_MIC_SD     34

// MAX98357A I2S Amplifier
#define I2S_SPK_BCLK   26
#define I2S_SPK_LRC    25
#define I2S_SPK_DOUT   22

// =====================================================
// Wi-Fi and Gateway Configuration
// =====================================================
#if __has_include("secrets.h")
  #include "secrets.h"
#else
  const char* WIFI_SSID     = "Wi-Fi";
  const char* WIFI_PASSWORD = "YOUR_PASSWORD";
#endif

// Replace with your laptop's Wi-Fi / Hotspot IP address
const char* GATEWAY_HOST = "192.168.137.1";
const uint16_t GATEWAY_PORT = 8000;
const char* GATEWAY_WS_PATH = "/ws";
const char* DEVICE_ID = "esp32-01";

// =====================================================
// Audio & State Settings
// =====================================================
#define RECORD_SECONDS 5
#define TOTAL_SAMPLES  (SAMPLE_RATE * RECORD_SECONDS) // 80,000 samples = 5.0 seconds
enum I2SMode { MODE_NONE, MODE_MIC, MODE_SPEAKER };
I2SMode currentMode = MODE_NONE;

WebSocketsClient webSocket;
bool isRecording = false;
bool isSpeaking = false;
String serialInput = "";

// Forward declarations
void speakText(const String &text);
void recordVoiceAndSend();
void sendCommand(const String &command);

// =====================================================
// Dynamic I2S Switching (Ensures each module gets I2S_NUM_0)
// =====================================================
void enableMic() {
  if (currentMode == MODE_MIC) return;

  i2s_driver_uninstall(I2S_PORT);

  i2s_config_t mic_config = {
    .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate          = SAMPLE_RATE,
    .bits_per_sample      = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format       = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags     = 0,
    .dma_buf_count        = 8,
    .dma_buf_len          = 64,
    .use_apll             = false,
    .tx_desc_auto_clear   = false,
    .fixed_mclk           = 0
  };

  i2s_pin_config_t mic_pins = {
    .bck_io_num   = I2S_MIC_SCK,
    .ws_io_num    = I2S_MIC_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num  = I2S_MIC_SD
  };

  i2s_driver_install(I2S_PORT, &mic_config, 0, NULL);
  i2s_set_pin(I2S_PORT, &mic_pins);
  i2s_zero_dma_buffer(I2S_PORT);
  currentMode = MODE_MIC;
}

void enableSpeaker() {
  if (currentMode == MODE_SPEAKER) return;

  i2s_driver_uninstall(I2S_PORT);

  i2s_config_t spk_config = {
    .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate          = SAMPLE_RATE,
    .bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format       = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags     = 0,
    .dma_buf_count        = 8,
    .dma_buf_len          = 128,
    .use_apll             = false,
    .tx_desc_auto_clear   = true,
    .fixed_mclk           = 0
  };

  i2s_pin_config_t spk_pins = {
    .bck_io_num   = I2S_SPK_BCLK,
    .ws_io_num    = I2S_SPK_LRC,
    .data_out_num = I2S_SPK_DOUT,
    .data_in_num  = I2S_PIN_NO_CHANGE
  };

  i2s_driver_install(I2S_PORT, &spk_config, 0, NULL);
  i2s_set_pin(I2S_PORT, &spk_pins);
  i2s_zero_dma_buffer(I2S_PORT);
  currentMode = MODE_SPEAKER;
}

// =====================================================
// Stream TTS from Gateway /speak and play through speaker
// =====================================================
void speakText(const String &text) {
  if (text.length() == 0) return;
  isSpeaking = true;

  WiFiClient client;
  HTTPClient http;

  // URL encode text
  String encodedText = "";
  for (unsigned int i = 0; i < text.length(); i++) {
    char c = text.charAt(i);
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      encodedText += c;
    } else if (c == ' ') {
      encodedText += "%20";
    } else {
      char hex[4];
      sprintf(hex, "%%%02X", (unsigned char)c);
      encodedText += hex;
    }
  }

  String url = String("http://") + GATEWAY_HOST + ":" + String(GATEWAY_PORT) + "/speak?text=" + encodedText;

  Serial.println();
  Serial.printf("[Speaker] Requesting TTS audio from Gateway: \"%s\"\n", text.c_str());

  http.begin(client, url);
  http.setTimeout(15000);
  int httpCode = http.GET();

  if (httpCode != 200) {
    Serial.printf("[Speaker] TTS GET failed, HTTP code: %d\n", httpCode);
    http.end();
    isSpeaking = false;
    return;
  }

  enableSpeaker();
  Serial.println("[Speaker] Playing audio stream through MAX98357A...");

  WiFiClient* stream = http.getStreamPtr();
  const int SPK_CHUNK_SAMPLES = 256;
  static uint8_t rawBuf[SPK_CHUNK_SAMPLES * 2];
  static int16_t monoBuf[SPK_CHUNK_SAMPLES];
  static int16_t stereoBuf[SPK_CHUNK_SAMPLES * 2];

  unsigned long lastDataTime = millis();
  while (http.connected() || stream->available() > 0) {
    int avail = stream->available();
    if (avail == 0) {
      if (millis() - lastDataTime > 3000) break;
      delay(2);
      continue;
    }
    lastDataTime = millis();
    int toRead = min(avail, (int)sizeof(rawBuf));
    if (toRead % 2 != 0) toRead--;
    if (toRead == 0) { delay(2); continue; }
    int bytesRead = stream->readBytes(rawBuf, toRead);
    if (bytesRead <= 0) break;

    int samples = bytesRead / 2;
    memcpy(monoBuf, rawBuf, bytesRead);
    for (int i = 0; i < samples; i++) {
      stereoBuf[i * 2]     = monoBuf[i];
      stereoBuf[i * 2 + 1] = monoBuf[i];
    }
    size_t written = 0;
    i2s_write(I2S_PORT, stereoBuf, samples * 4, &written, portMAX_DELAY);
  }

  http.end();
  delay(100);
  i2s_zero_dma_buffer(I2S_PORT);
  Serial.println("[Speaker] Done speaking.");
  Serial.println("\n----------------------------------------------------");
  Serial.println("Press Enter anytime in Serial Monitor to speak again!");
  Serial.println("----------------------------------------------------\n");
  isSpeaking = false;
}

// =====================================================
// Send Command via WebSocket to Gateway -> n8n
// =====================================================
void sendCommand(const String &command) {
  String cmd = command;
  cmd.trim();
  if (cmd.length() == 0) return;

  if (!webSocket.isConnected()) {
    Serial.println("[WebSocket] Gateway is not connected! Cannot send command.");
    return;
  }

  JsonDocument doc;
  doc["type"] = "command";
  doc["device_id"] = DEVICE_ID;
  doc["message"] = cmd;

  String jsonMessage;
  serializeJson(doc, jsonMessage);

  Serial.println("\n[WebSocket] Forwarding command to n8n ReAct Agent:");
  Serial.println("----------------------------------------------------");
  Serial.println(jsonMessage);
  Serial.println("----------------------------------------------------");

  webSocket.sendTXT(jsonMessage);
  Serial.println("[WebSocket] Command sent! Waiting for n8n response...");
}

// =====================================================
// Record Voice -> Transcribe via Gateway STT -> Send Command
// =====================================================
void recordVoiceAndSend() {
  if (isRecording || isSpeaking) return;
  isRecording = true;

  // Connect to Gateway STT server first before recording
  Serial.println("\n[STT] Connecting to Gateway (/transcribe)...");
  WiFiClient client;
  if (!client.connect(GATEWAY_HOST, GATEWAY_PORT)) {
    Serial.println("[STT] Failed to connect to Gateway server!");
    isRecording = false;
    return;
  }

  // Send HTTP POST headers with predetermined audio payload size
  size_t totalBytes = (size_t)TOTAL_SAMPLES * sizeof(int16_t);
  client.print("POST /transcribe HTTP/1.1\r\n");
  client.printf("Host: %s:%d\r\n", GATEWAY_HOST, GATEWAY_PORT);
  client.printf("Content-Length: %u\r\n", totalBytes);
  client.print("Content-Type: application/octet-stream\r\n");
  client.printf("X-Sample-Rate: %d\r\n", SAMPLE_RATE);
  client.print("Connection: close\r\n\r\n");

  enableMic();
  Serial.println("====================================================");
  Serial.printf(" >>> RECORDING: Speak your command now (%d sec)... <<<\n", RECORD_SECONDS);
  Serial.println("====================================================");
  delay(50);
  i2s_zero_dma_buffer(I2S_PORT);

  const int CHUNK = 256;
  int32_t raw32[CHUNK * 2]; // 256 stereo pairs
  int16_t pcm16[CHUNK];
  int recorded = 0;
  int32_t maxPeakLeft = 0;
  int32_t maxPeakRight = 0;
  int64_t sumSq = 0;

  // Stream PCM audio directly to the Gateway as it is being recorded
  // Eliminates large RAM allocations and prevents memory errors
  while (recorded < TOTAL_SAMPLES && client.connected()) {
    size_t bytesRead = 0;
    i2s_read(I2S_PORT, raw32, sizeof(raw32), &bytesRead, portMAX_DELAY);
    int pairsRead = bytesRead / 8;
    if (pairsRead <= 0) continue;

    int samplesInBatch = min(pairsRead, (int)(TOTAL_SAMPLES - recorded));
    for (int i = 0; i < samplesInBatch; i++) {
      int32_t sLeft  = raw32[i * 2]     >> 14;
      int32_t sRight = raw32[i * 2 + 1] >> 14;

      if (sLeft > 32767) sLeft = 32767; else if (sLeft < -32768) sLeft = -32768;
      if (sRight > 32767) sRight = 32767; else if (sRight < -32768) sRight = -32768;

      if (abs(sLeft) > maxPeakLeft) maxPeakLeft = abs(sLeft);
      if (abs(sRight) > maxPeakRight) maxPeakRight = abs(sRight);

      int16_t sample16 = (abs(sRight) >= abs(sLeft)) ? (int16_t)sRight : (int16_t)sLeft;
      pcm16[i] = sample16;
      sumSq += (int64_t)sample16 * sample16;
    }

    // Write chunk directly to TCP stream
    client.write((uint8_t*)pcm16, samplesInBatch * sizeof(int16_t));
    recorded += samplesInBatch;
  }

  int maxPeak = max(maxPeakLeft, maxPeakRight);
  int rms = (recorded > 0) ? (int)sqrt(sumSq / recorded) : 0;
  Serial.println(">>> RECORDING COMPLETE! <<<");
  Serial.printf("[Mic Stats] Captured & streamed %d samples | Peak: %d/32767 | RMS: %d\n", recorded, maxPeak, rms);

  if (maxPeak < 100) {
    Serial.println("[Mic Warning] Signal is near ZERO! Check microphone wiring.");
    client.stop();
    isRecording = false;
    return;
  }

  Serial.println("[STT] Waiting for transcription response from Gateway...");

  // Wait for response with 15s timeout
  unsigned long timeout = millis();
  while (client.connected() && !client.available()) {
    if (millis() - timeout > 15000) {
      Serial.println("[STT] Response timed out!");
      client.stop();
      isRecording = false;
      return;
    }
    delay(10);
  }

  // Skip HTTP response headers
  while (client.available()) {
    String line = client.readStringUntil('\n');
    if (line == "\r" || line.length() == 0) break;
  }

  String response = client.readString();
  client.stop();

  // Parse JSON response: {"text": "...", "error": ...}
  String transcribedText = "";
  int textKey = response.indexOf("\"text\":");
  if (textKey != -1) {
    int startQuote = response.indexOf("\"", textKey + 7);
    if (startQuote != -1) {
      int endQuote = response.indexOf("\"", startQuote + 1);
      if (endQuote != -1) {
        transcribedText = response.substring(startQuote + 1, endQuote);
      }
    }
  }

  if (transcribedText.length() == 0) {
    Serial.println("[STT] No speech recognized. Please speak closer to the mic and try again.");
    isRecording = false;
    return;
  }

  Serial.println("\n====================================================");
  Serial.printf("  [Voice Recognized] You said: \"%s\"\n", transcribedText.c_str());
  Serial.println("====================================================");

  // Forward recognized command to n8n via WebSocket
  sendCommand(transcribedText);
  isRecording = false;
}

// =====================================================
// WebSocket Event Handler (Receives response from n8n)
// =====================================================
void webSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.println("[WebSocket] Disconnected from Gateway.");
      break;

    case WStype_CONNECTED:
      Serial.println("\n[WebSocket] Connected to Gateway successfully!");
      Serial.println("====================================================");
      Serial.println("  VoxCal Voice Assistant is READY.");
      Serial.println("  Press ENTER in Serial Monitor to record your voice!");
      Serial.println("  Or type a command and hit Enter.");
      Serial.println("====================================================\n");
      break;

    case WStype_TEXT: {
      String responseStr = String((char*)payload);
      Serial.println("\n====================================================");
      Serial.println("[VoxCal] Agent Response Received from n8n:");
      Serial.println("----------------------------------------------------");
      Serial.println(responseStr);
      Serial.println("====================================================");

      // Parse agent message to speak out
      JsonDocument doc;
      DeserializationError err = deserializeJson(doc, responseStr);
      String spokenMessage = "";

      if (!err) {
        if (doc.containsKey("message") && doc["message"].is<const char*>()) {
          spokenMessage = doc["message"].as<String>();
        } else if (doc.containsKey("output") && doc["output"].is<const char*>()) {
          spokenMessage = doc["output"].as<String>();
        } else if (doc.containsKey("text") && doc["text"].is<const char*>()) {
          spokenMessage = doc["text"].as<String>();
        }
      }

      if (spokenMessage.length() == 0) {
        spokenMessage = responseStr;
      }

      // Speak response aloud through MAX98357A speaker!
      speakText(spokenMessage);
      break;
    }

    case WStype_ERROR:
      Serial.println("[WebSocket] Error occurred.");
      break;

    default:
      break;
  }
}

// =====================================================
// Connect Wi-Fi
// =====================================================
void connectWiFi() {
  Serial.print("Connecting to Wi-Fi: ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.print("Wi-Fi connected! ESP32 IP: ");
  Serial.println(WiFi.localIP());
}

// =====================================================
// Setup
// =====================================================
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("====================================================");
  Serial.println("          VoxCal Voice Assistant (ESP32)            ");
  Serial.println("====================================================");
  Serial.printf("[Memory] Free Heap: %u bytes (Zero-buffer direct streaming)\n", ESP.getFreeHeap());

  // Connect to Wi-Fi
  connectWiFi();

  // Connect WebSocket to Gateway
  Serial.printf("Connecting to Gateway WebSocket: ws://%s:%d%s\n", GATEWAY_HOST, GATEWAY_PORT, GATEWAY_WS_PATH);
  webSocket.begin(GATEWAY_HOST, GATEWAY_PORT, GATEWAY_WS_PATH);
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);

  delay(1000);

  // Automatically start the first voice command test!
  recordVoiceAndSend();
}

// =====================================================
// Main Loop
// =====================================================
void loop() {
  webSocket.loop();

  // Check Serial Monitor for user input (Enter to speak, or type command)
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      serialInput.trim();
      if (serialInput.length() == 0) {
        // Empty Enter pressed -> Trigger voice recording!
        recordVoiceAndSend();
      } else {
        // User typed a command -> Send directly!
        Serial.printf("\n[Serial] Typed Command: \"%s\"\n", serialInput.c_str());
        sendCommand(serialInput);
        serialInput = "";
      }
    } else {
      serialInput += c;
    }
  }
}