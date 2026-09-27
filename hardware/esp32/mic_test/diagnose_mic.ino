#include <Arduino.h>
#include "driver/i2s.h"

// =====================================================
// Pin Definitions (Change if using different pins)
// =====================================================
#define I2S_MIC_SCK    32    // Serial Clock (SCK / CLK)
#define I2S_MIC_WS     33    // Word Select (WS / LRC)
#define I2S_MIC_SD     34    // Serial Data (SD / OUT)
#define I2S_PORT       I2S_NUM_0
#define SAMPLE_RATE    16000

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n==========================================");
  Serial.println("  INMP441 Hardware Diagnostic & Plotter   ");
  Serial.println("==========================================");
  Serial.println("Wiring Check:");
  Serial.println("  VDD  -> 3.3V (NOT 5V)");
  Serial.println("  GND  -> GND");
  Serial.println("  L/R  -> GND (or 3.3V)");
  Serial.println("  SCK  -> GPIO 32");
  Serial.println("  WS   -> GPIO 33");
  Serial.println("  SD   -> GPIO 34");
  Serial.println("------------------------------------------");
  Serial.println("TIP: Open Tools -> 'Serial Plotter' in Arduino IDE");
  Serial.println("     to see the live sound waveform graph!");
  Serial.println("==========================================\n");

  // Configure I2S on primary I2S_NUM_0 in Stereo 32-bit master mode
  i2s_config_t i2s_config = {
    .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate          = SAMPLE_RATE,
    .bits_per_sample      = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format       = I2S_CHANNEL_FMT_RIGHT_LEFT, // Stereo
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags     = 0,
    .dma_buf_count        = 8,
    .dma_buf_len          = 64,
    .use_apll             = false,
    .tx_desc_auto_clear   = false,
    .fixed_mclk           = 0
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num   = I2S_MIC_SCK,
    .ws_io_num    = I2S_MIC_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num  = I2S_MIC_SD
  };

  esp_err_t err = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  if (err != ESP_OK) {
    Serial.printf("ERROR: i2s_driver_install failed: 0x%x\n", err);
    while (1);
  }

  err = i2s_set_pin(I2S_PORT, &pin_config);
  if (err != ESP_OK) {
    Serial.printf("ERROR: i2s_set_pin failed: 0x%x\n", err);
    while (1);
  }

  i2s_zero_dma_buffer(I2S_PORT);
  Serial.println("I2S driver started. Listening to INMP441...\n");
}

void loop() {
  const int SAMPLES_PER_READ = 64;
  int32_t buffer[SAMPLES_PER_READ * 2];
  size_t bytesRead = 0;

  esp_err_t res = i2s_read(I2S_PORT, buffer, sizeof(buffer), &bytesRead, 100 / portTICK_PERIOD_MS);
  if (res != ESP_OK || bytesRead == 0) {
    Serial.println("[Error] i2s_read timed out (no clock or data)");
    delay(500);
    return;
  }

  int pairs = bytesRead / 8;
  int32_t peakL = 0;
  int32_t peakR = 0;
  int32_t rawSampleL = 0;
  int32_t rawSampleR = 0;

  for (int i = 0; i < pairs; i++) {
    int32_t l = buffer[i * 2]     >> 14;
    int32_t r = buffer[i * 2 + 1] >> 14;

    if (abs(l) > peakL) { peakL = abs(l); rawSampleL = buffer[i * 2]; }
    if (abs(r) > peakR) { peakR = abs(r); rawSampleR = buffer[i * 2 + 1]; }
  }

  // Format suitable for both Serial Monitor and Serial Plotter:
  // Tap the mic or speak loudly: watch the peaks jump!
  Serial.printf("Left_Peak:%d  Right_Peak:%d  (Raw_L:0x%08X Raw_R:0x%08X)\n",
                peakL, peakR, rawSampleL, rawSampleR);

  delay(50);
}
