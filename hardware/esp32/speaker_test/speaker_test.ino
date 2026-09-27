#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "driver/i2s.h"

// =====================================================
// Pin Definitions (MAX98357A I2S Amplifier)
// =====================================================
#define I2S_BCLK    26    // Bit Clock (BCLK)
#define I2S_LRC     25    // Word Select / Left-Right Clock (LRC / WS)
#define I2S_DOUT    22    // Serial Data Out (DIN / DOUT)
#define I2S_SD      21    // Shutdown pin (SD_MODE)
#define I2S_PORT    I2S_NUM_0
#define SAMPLE_RATE 16000

// =====================================================
// Wi-Fi and Gateway Configuration
// =====================================================
#if __has_include("../secrets.h")
  #include "../secrets.h"
#else
  const char* WIFI_SSID     = "Wi-Fi";
  const char* WIFI_PASSWORD = "YOUR_PASSWORD";
#endif

// Replace with your laptop's Wi-Fi / Hotspot IP address running FastAPI
const char* GATEWAY_HOST = "192.168.137.1";
const uint16_t GATEWAY_PORT = 8000;

// Static buffers to prevent stack overflow
const int CHUNK_SAMPLES = 256;
static uint8_t rawBuf[CHUNK_SAMPLES * 2];
static int16_t monoBuf[CHUNK_SAMPLES];
static int16_t stereoBuf[CHUNK_SAMPLES * 2];

// =====================================================
// URL Encoding Helper
// =====================================================
String urlEncode(const String &str) {
  String encoded = "";
  for (unsigned int i = 0; i < str.length(); i++) {
    char c = str.charAt(i);
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      encoded += c;
    } else if (c == ' ') {
      encoded += "%20";
    } else {
      char hex[4];
      sprintf(hex, "%%%02X", (unsigned char)c);
      encoded += hex;
    }
  }
  return encoded;
}

// Print human-readable reset reason
void printResetReason() {
  esp_reset_reason_t reason = esp_reset_reason();
  Serial.printf("[System] Reset reason: %d (", reason);
  switch (reason) {
    case ESP_RST_POWERON:   Serial.print("Power-on reset"); break;
    case ESP_RST_SW:        Serial.print("Software reset"); break;
    case ESP_RST_PANIC:     Serial.print("Crash / Panic reset"); break;
    case ESP_RST_INT_WDT:   Serial.print("Interrupt Watchdog reset"); break;
    case ESP_RST_TASK_WDT:  Serial.print("Task Watchdog reset"); break;
    case ESP_RST_BROWNOUT:  Serial.print("BROWNOUT DETECTED! (Voltage dropped - check power/wiring)"); break;
    case ESP_RST_SDIO:      Serial.print("SDIO reset"); break;
    default:                Serial.print("Other"); break;
  }
  Serial.println(")");
  Serial.flush();
}

// =====================================================
// I2S Configuration
// =====================================================
void setupI2S() {
  i2s_config_t i2s_config = {
    .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate          = SAMPLE_RATE,
    .bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format       = I2S_CHANNEL_FMT_RIGHT_LEFT, // Stereo framing required by MAX98357A
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags     = 0,
    .dma_buf_count        = 8,
    .dma_buf_len          = 128,
    .use_apll             = false,
    .tx_desc_auto_clear   = true,
    .fixed_mclk           = 0
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num   = I2S_BCLK,
    .ws_io_num    = I2S_LRC,
    .data_out_num = I2S_DOUT,
    .data_in_num  = I2S_PIN_NO_CHANGE
  };

  esp_err_t err = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  if (err != ESP_OK) {
    Serial.printf("[I2S] Driver install failed: %d\n", err);
    Serial.flush();
    return;
  }

  err = i2s_set_pin(I2S_PORT, &pin_config);
  if (err != ESP_OK) {
    Serial.printf("[I2S] Pin config failed: %d\n", err);
    Serial.flush();
    return;
  }

  i2s_zero_dma_buffer(I2S_PORT);
  Serial.println("[I2S] Initialized successfully.");
  Serial.flush();
}

// =====================================================
// Speak Text via Gateway Streaming
// =====================================================
void speakText(const char* text) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[HTTP] Wi-Fi not connected!");
    Serial.flush();
    return;
  }

  WiFiClient client;
  HTTPClient http;

  String encodedText = urlEncode(String(text));
  String url = String("http://") + GATEWAY_HOST + ":" + String(GATEWAY_PORT) + "/speak?text=" + encodedText;

  Serial.println();
  Serial.printf("[HTTP] Connecting to: %s\n", url.c_str());
  Serial.flush();

  // Use explicit WiFiClient for maximum stability across ESP32 core versions
  if (!http.begin(client, url)) {
    Serial.println("[HTTP] http.begin() failed!");
    Serial.flush();
    return;
  }

  http.setTimeout(15000); // 15s timeout for TTS generation and streaming

  Serial.println("[HTTP] Sending GET request...");
  Serial.flush();

  int httpCode = http.GET();
  if (httpCode != 200) {
    Serial.printf("[HTTP] GET failed, code: %d, error: %s\n", httpCode, http.errorToString(httpCode).c_str());
    Serial.flush();
    http.end();
    return;
  }

  int contentLength = http.getSize();
  Serial.printf("[HTTP] 200 OK! Audio size: %d bytes\n", contentLength);
  Serial.flush();

  WiFiClient* stream = http.getStreamPtr();
  if (!stream) {
    Serial.println("[HTTP] Failed to get stream pointer.");
    Serial.flush();
    http.end();
    return;
  }

  Serial.println("[Audio] Streaming audio to I2S speaker...");
  Serial.flush();

  unsigned long totalBytesRead = 0;
  unsigned long lastDataTime = millis();

  while (http.connected() || stream->available() > 0) {
    int availableBytes = stream->available();

    if (availableBytes == 0) {
      if (millis() - lastDataTime > 3000) {
        // No data for 3 seconds while stream ended
        break;
      }
      delay(2);
      continue;
    }

    lastDataTime = millis();

    // Read in chunks of 512 bytes max (must be an even number for 16-bit PCM)
    int toRead = min(availableBytes, (int)sizeof(rawBuf));
    if (toRead % 2 != 0) {
      toRead -= 1; // Always read an even number of bytes to preserve 16-bit alignment
    }

    if (toRead == 0) {
      delay(2);
      continue;
    }

    int bytesRead = stream->readBytes(rawBuf, toRead);
    if (bytesRead <= 0) {
      break;
    }

    totalBytesRead += bytesRead;
    int samples = bytesRead / 2;
    memcpy(monoBuf, rawBuf, bytesRead);

    // Duplicate mono sample into left and right channels for standard I2S stereo
    for (int i = 0; i < samples; i++) {
      stereoBuf[i * 2]     = monoBuf[i]; // Left
      stereoBuf[i * 2 + 1] = monoBuf[i]; // Right
    }

    size_t bytesWritten = 0;
    i2s_write(I2S_PORT, stereoBuf, samples * 4, &bytesWritten, portMAX_DELAY);
  }

  http.end();
  delay(150); // Allow DMA buffer to finish playing
  i2s_zero_dma_buffer(I2S_PORT);

  Serial.printf("[Audio] Playback finished. Total bytes played: %lu\n\n", totalBytesRead);
  Serial.flush();
}

// =====================================================
// Setup
// =====================================================
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("======================================");
  Serial.println("      VoxCal Speaker Test (I2S)       ");
  Serial.println("======================================");
  Serial.flush();

  // Print reset reason to catch crashes or brownouts
  printResetReason();

  // 1. Initialize I2S first so clock lines (BCLK, LRC) are stable and silent
  setupI2S();

  // 2. Connect to Wi-Fi
  Serial.print("Connecting to Wi-Fi: ");
  Serial.println(WIFI_SSID);
  Serial.flush();

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    Serial.flush();
  }

  Serial.println();
  Serial.println("Wi-Fi connected!");
  Serial.print("ESP32 IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("Gateway IP: ");
  Serial.println(GATEWAY_HOST);
  Serial.flush();
  delay(100);

  // Short single-word test
  Serial.println("\n--- Playing: Hello ---");
  Serial.flush();
  speakText("Hello");

  Serial.println("\nReady! Type any word in Serial Monitor and press Enter to hear it.");
  Serial.flush();
}

void loop() {
  // If user types text in Serial Monitor, speak it!
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    if (input.length() > 0) {
      Serial.printf("\n[Serial] Speaking: %s\n", input.c_str());
      Serial.flush();
      speakText(input.c_str());
    }
  }
}
