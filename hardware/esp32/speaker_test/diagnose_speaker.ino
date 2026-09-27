#include <Arduino.h>
#include "driver/i2s.h"
#include <math.h>

// =====================================================
// Pin Definitions
// =====================================================
#define I2S_BCLK    26    // Bit Clock
#define I2S_LRC     25    // Word Select / Left-Right Clock
#define I2S_DOUT    22    // Serial Data Out (DIN)
#define I2S_SD      21    // Shutdown / Enable (if wired)
#define I2S_PORT    I2S_NUM_0
#define SAMPLE_RATE 16000

// 500 Hz clean tone (at 16000 Hz, 1 cycle = 32 samples)
#define SINE_SAMPLES 32
int16_t sineBuffer[SINE_SAMPLES * 2]; // Stereo (L + R)
int16_t silenceBuffer[SINE_SAMPLES * 2]; // All zeros

void setupI2S() {
  i2s_config_t i2s_config = {
    .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate          = SAMPLE_RATE,
    .bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format       = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags     = 0,
    .dma_buf_count        = 4,
    .dma_buf_len          = 64,
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

  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_PORT, &pin_config);
  i2s_zero_dma_buffer(I2S_PORT);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n==========================================");
  Serial.println("  VoxCal Hardware Diagnostic Tool (I2S)   ");
  Serial.println("==========================================");

  // Pre-generate 500 Hz clean sine wave at gentle volume (amplitude 6000 out of 32767)
  for (int i = 0; i < SINE_SAMPLES; i++) {
    int16_t val = (int16_t)(6000.0 * sin(2.0 * PI * (float)i / (float)SINE_SAMPLES));
    sineBuffer[i * 2]     = val; // Left
    sineBuffer[i * 2 + 1] = val; // Right

    silenceBuffer[i * 2]     = 0;
    silenceBuffer[i * 2 + 1] = 0;
  }

  setupI2S();

  Serial.println("\nI2S Started.");
  Serial.println("Test loop will cycle:");
  Serial.println("  [1] PLAYING 500Hz TONE for 2 seconds");
  Serial.println("  [2] TOTAL SILENCE for 2 seconds");
  Serial.println("------------------------------------------");
}

void loop() {
  // Phase 1: Play 500 Hz tone for 2 seconds (500 cycles = ~1000 buffer iterations)
  Serial.println("\n>> [PHASE 1] PLAYING 500Hz TONE (2 seconds)...");
  unsigned long start = millis();
  while (millis() - start < 2000) {
    size_t written = 0;
    i2s_write(I2S_PORT, sineBuffer, sizeof(sineBuffer), &written, portMAX_DELAY);
  }

  // Phase 2: Send pure silence (all 0s) for 2 seconds
  Serial.println(">> [PHASE 2] SENDING SILENCE (2 seconds)...");
  start = millis();
  while (millis() - start < 2000) {
    size_t written = 0;
    i2s_write(I2S_PORT, silenceBuffer, sizeof(silenceBuffer), &written, portMAX_DELAY);
  }
}
