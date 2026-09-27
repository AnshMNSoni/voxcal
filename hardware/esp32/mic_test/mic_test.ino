#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "driver/i2s.h"

// =====================================================
// Pin Definitions
// =====================================================
#define I2S_PORT       I2S_NUM_0    // Both devices share I2S_NUM_0 via dynamic switching
#define SAMPLE_RATE    16000

// INMP441 (Mic Input)
#define I2S_MIC_SCK    32
#define I2S_MIC_WS     33
#define I2S_MIC_SD     34

// MAX98357A (Speaker Output)
#define I2S_SPK_BCLK   26
#define I2S_SPK_LRC    25
#define I2S_SPK_DOUT   22

// =====================================================
// Wi-Fi and Gateway Configuration
// =====================================================
#if __has_include("../secrets.h")
  #include "../secrets.h"
#else
  const char* WIFI_SSID     = "Wi-Fi";
  const char* WIFI_PASSWORD = "YOUR_PASSWORD";
#endif

const char* GATEWAY_HOST = "192.168.137.1";
const uint16_t GATEWAY_PORT = 8000;

// =====================================================
// Audio Settings
// =====================================================
#define RECORD_SECONDS 5
#define SAMPLE_COUNT   (SAMPLE_RATE * RECORD_SECONDS)

int16_t* recording;
enum I2SMode { MODE_NONE, MODE_MIC, MODE_SPEAKER };
I2SMode currentMode = MODE_NONE;

// =====================================================
// Dynamic I2S Switching (Ensures each device gets I2S_NUM_0)
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
// Record 3 Seconds from Mic into RAM buffer
// =====================================================
void recordAudio() {
  enableMic();
  Serial.printf("\n>>> RECORDING: Speak now (%d seconds)... <<<\n", RECORD_SECONDS);
  delay(50);
  i2s_zero_dma_buffer(I2S_PORT);

  const int CHUNK = 128;
  int32_t raw32[CHUNK * 2]; // 128 stereo pairs
  int recorded = 0;
  int32_t maxPeakLeft = 0;
  int32_t maxPeakRight = 0;
  int64_t sumSq = 0;

  while (recorded < SAMPLE_COUNT) {
    size_t bytesRead = 0;
    i2s_read(I2S_PORT, raw32, sizeof(raw32), &bytesRead, portMAX_DELAY);
    int pairsRead = bytesRead / 8; // 8 bytes per stereo pair (2 x 32-bit)
    for (int i = 0; i < pairsRead && recorded < SAMPLE_COUNT; i++) {
      int32_t sLeft  = raw32[i * 2]     >> 14;
      int32_t sRight = raw32[i * 2 + 1] >> 14;

      if (sLeft > 32767) sLeft = 32767; else if (sLeft < -32768) sLeft = -32768;
      if (sRight > 32767) sRight = 32767; else if (sRight < -32768) sRight = -32768;

      if (abs(sLeft) > maxPeakLeft) maxPeakLeft = abs(sLeft);
      if (abs(sRight) > maxPeakRight) maxPeakRight = abs(sRight);

      // Select active channel (Right channel receives INMP441 audio)
      int16_t sample16 = (abs(sRight) >= abs(sLeft)) ? (int16_t)sRight : (int16_t)sLeft;
      recording[recorded++] = sample16;
      sumSq += (int64_t)sample16 * sample16;
    }
  }

  int maxPeak = max(maxPeakLeft, maxPeakRight);
  int rms = (recorded > 0) ? (int)sqrt(sumSq / recorded) : 0;
  Serial.println(">>> RECORDING DONE! <<<");
  Serial.printf("[Mic Stats] Captured %d samples | Left Peak: %d | Right Peak: %d | RMS: %d\n",
                recorded, maxPeakLeft, maxPeakRight, rms);

  if (maxPeak < 100) {
    Serial.println("⚠️  [MIC WARNING] Signal is near ZERO / SILENT! Check mic connections.");
  } else {
    Serial.printf("✅ [Mic Signal OK] Captured audio! (Active Channel: %s)\n",
                  (maxPeakRight >= maxPeakLeft) ? "RIGHT" : "LEFT");
  }
}

// =====================================================
// Play recorded audio directly through MAX98357A speaker
// =====================================================
void playLocalRecording() {
  enableSpeaker();
  Serial.println("[Speaker] Playing back your recorded voice locally...");

  const int CHUNK = 256;
  int16_t stereo[CHUNK * 2];
  size_t bytesWritten = 0;

  for (int i = 0; i < SAMPLE_COUNT; i += CHUNK) {
    int count = min(CHUNK, SAMPLE_COUNT - i);
    for (int j = 0; j < count; j++) {
      stereo[j * 2]     = recording[i + j];
      stereo[j * 2 + 1] = recording[i + j];
    }
    i2s_write(I2S_PORT, stereo, count * 4, &bytesWritten, portMAX_DELAY);
  }
  delay(100);
  i2s_zero_dma_buffer(I2S_PORT);
  Serial.println("[Speaker] Playback finished.");
}

// =====================================================
// Send PCM to /transcribe and return transcribed text
// =====================================================
String transcribeAudio() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[STT] Wi-Fi not connected!");
    return "";
  }

  WiFiClient client;
  HTTPClient http;
  String url = String("http://") + GATEWAY_HOST + ":" + String(GATEWAY_PORT) + "/transcribe";

  Serial.println("[STT] Sending audio to server for transcription...");

  http.begin(client, url);
  http.setTimeout(15000);
  http.addHeader("Content-Type", "application/octet-stream");
  http.addHeader("X-Sample-Rate", String(SAMPLE_RATE));

  // POST raw PCM bytes
  int httpCode = http.POST((uint8_t*)recording, SAMPLE_COUNT * sizeof(int16_t));

  if (httpCode != 200) {
    Serial.printf("[STT] POST failed, code: %d\n", httpCode);
    http.end();
    return "";
  }

  String response = http.getString();
  http.end();

  // Parse JSON: {"text": "...", "error": ...}
  String text = "";
  int textKey = response.indexOf("\"text\":");
  if (textKey != -1) {
    int startQuote = response.indexOf("\"", textKey + 7);
    if (startQuote != -1) {
      int endQuote = response.indexOf("\"", startQuote + 1);
      if (endQuote != -1) {
        text = response.substring(startQuote + 1, endQuote);
      }
    }
  }

  if (text.length() == 0) {
    int errKey = response.indexOf("\"error\":");
    if (errKey != -1) {
      int startQuote = response.indexOf("\"", errKey + 8);
      if (startQuote != -1) {
        int endQuote = response.indexOf("\"", startQuote + 1);
        if (endQuote != -1) {
          String errMsg = response.substring(startQuote + 1, endQuote);
          Serial.printf("[STT Server Note] %s\n", errMsg.c_str());
        }
      }
    }
  }

  return text;
}

// =====================================================
// Stream TTS from /speak and play through speaker
// =====================================================
void speakText(const String &text) {
  if (text.length() == 0) return;

  WiFiClient client;
  HTTPClient http;
  String encodedText = "";
  for (int i = 0; i < text.length(); i++) {
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
  http.begin(client, url);
  http.setTimeout(15000);
  int httpCode = http.GET();

  if (httpCode != 200) {
    Serial.printf("[TTS] GET failed, code: %d\n", httpCode);
    http.end();
    return;
  }

  enableSpeaker();
  Serial.printf("[Speaker] Playing TTS: \"%s\"\n", text.c_str());

  WiFiClient* stream = http.getStreamPtr();
  const int CHUNK_SAMPLES = 256;
  static uint8_t rawBuf[CHUNK_SAMPLES * 2];
  static int16_t monoBuf[CHUNK_SAMPLES];
  static int16_t stereoBuf[CHUNK_SAMPLES * 2];

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
}

// =====================================================
// Full pipeline: Record → Play Locally → Transcribe → Speak
// =====================================================
void recordTranscribeSpeak() {
  // Step 1: Record from mic (uses I2S_NUM_0)
  recordAudio();
  delay(150);

  // Step 2: Local playback (disabled so you only hear the speaker response)
  // To hear your voice directly for hardware debugging, uncomment the next line:
  // playLocalRecording();
  delay(100);

  // Step 3: Transcribe (send audio to server)
  String text = transcribeAudio();

  if (text.length() == 0) {
    Serial.println("[STT] Nothing recognized. (Listen: did you hear your voice clearly from the speaker above?)");
    return;
  }

  // Step 4: Print recognized text
  Serial.println("\n==========================================");
  Serial.print("  You said: \"");
  Serial.print(text);
  Serial.println("\"");
  Serial.println("==========================================\n");

  // Step 5: Speak it back through speaker via TTS (switches I2S_NUM_0 to speaker)
  speakText(text);
}

// =====================================================
// Setup
// =====================================================
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n==========================================");
  Serial.println(" VoxCal Mic → STT → Speaker Test");
  Serial.println("==========================================");

  // Allocate recording buffer (160 KB for 5s @ 16kHz 16-bit mono)
  #if defined(BOARD_HAS_PSRAM)
  if (psramFound()) {
    recording = (int16_t*)ps_malloc(SAMPLE_COUNT * sizeof(int16_t));
  }
  #endif
  if (!recording) {
    recording = (int16_t*)malloc(SAMPLE_COUNT * sizeof(int16_t));
  }
  if (!recording) {
    Serial.println("ERROR: Not enough memory!");
    while (1);
  }

  // Connect Wi-Fi
  Serial.print("Connecting to Wi-Fi: ");
  Serial.println(WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Wi-Fi connected! IP: ");
  Serial.println(WiFi.localIP());

  delay(500);

  // Run first test immediately
  recordTranscribeSpeak();

  Serial.println("\nPress Enter anytime in Serial Monitor to record again!");
}

// =====================================================
// Loop
// =====================================================
void loop() {
  if (Serial.available()) {
    while (Serial.available()) Serial.read();
    recordTranscribeSpeak();
  }
}
