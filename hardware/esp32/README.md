# VoxCal ESP32 Firmware

This directory contains the firmware sketches and unit tests for the ESP32 microcontroller, interfacing with the INMP441 I2S microphone and MAX98357A I2S audio amplifier.

## Directory Structure

```text
hardware/esp32/
├── main.ino                   # Production firmware (WebSocket client & command processor)
├── secrets.h                  # Private Wi-Fi credentials (git-ignored)
├── secrets.h.example          # Credentials template for version control
├── .env                       # Environment configuration (git-ignored)
├── .env.example               # Environment template
├── README.md                  # This documentation file
├── speaker_test/
│   ├── diagnose_speaker.ino   # Unit Test 1: Offline sine tone generator
│   └── speaker_test.ino       # Unit Test 2: Network TTS audio streaming test
└── mic_test/
    ├── diagnose_mic.ino       # Unit Test 3: Offline mic waveform amplitude plotter
    └── mic_test.ino           # Unit Test 4: Combined audio pipeline (Mic -> STT -> TTS -> Speaker)
```

## Hardware Pinout & Wiring

Both audio peripherals share I2S Port 0 (`I2S_NUM_0`) via dynamic time-sharing to avoid hardware clock conflicts.

### INMP441 MEMS Microphone Pinout

| Module Pin | ESP32 Pin | Description |
|:---|:---|:---|
| VDD | 3.3V | 3.3V Power (Do NOT connect to 5V) |
| GND | GND | Ground |
| L/R | GND | Channel Select: GND selects Left channel |
| SCK | GPIO 32 | Serial Clock (Bit Clock / BCLK) |
| WS | GPIO 33 | Word Select (Left-Right Clock / LRC) |
| SD | GPIO 34 | Serial Data Out from Microphone |

### MAX98357A I2S Amplifier Pinout

| Module Pin | ESP32 Pin | Description |
|:---|:---|:---|
| VIN | 5V / VBUS | 5V Power for amplifier |
| GND | GND | Ground |
| BCLK | GPIO 26 | Bit Clock |
| LRC | GPIO 25 | Word Select (Left-Right Clock) |
| DIN | GPIO 22 | Serial Data In to Amplifier |
| GAIN | Unconnected | Default 9dB gain (or connect to GND) |
| SD_MODE | Unconnected | Default stereo average downmix |

## Dynamic I2S Architecture

The ESP32 hardware routes GPIO 25 and GPIO 26 through internal DAC channels tied to `I2S_NUM_0`. Attempting to drive them from `I2S_NUM_1` prevents the speaker from receiving clock signals. Additionally, the INMP441 microphone requires 32-bit RX master clock generation on `I2S_NUM_0`.

To allow both modules to operate reliably on a single ESP32:
- When recording, `I2S_NUM_0` is initialized in 32-bit RX Master mode on GPIO 32, 33, 34.
- When playing audio, `I2S_NUM_0` is dynamically reconfigured to 16-bit TX Master mode on GPIO 26, 25, 22.
- The driver uninstall and re-installation takes less than 1 millisecond.

## Unit Testing Guide

Before running the full system, verify each peripheral independently using the provided test sketches:

### 1. Speaker Offline Hardware Test
- **File**: `speaker_test/diagnose_speaker.ino`
- **Scope**: Generates a 500 Hz sine wave directly on the ESP32.
- **Requirement**: No Wi-Fi or server needed.
- **Result**: Speaker outputs a clean repeating tone (2s tone, 2s silence).

### 2. Speaker Streaming & TTS Test
- **File**: `speaker_test/speaker_test.ino`
- **Scope**: Connects to Wi-Fi and streams 16kHz PCM audio from FastAPI Gateway `/speak`.
- **Requirement**: `server.py` running on your local network.
- **Result**: Speaker speaks a welcome phrase and accepts text commands from Serial Monitor.

### 3. Microphone Offline Hardware Test
- **File**: `mic_test/diagnose_mic.ino`
- **Scope**: Reads 32-bit audio frames from INMP441 and plots live amplitude.
- **Requirement**: No Wi-Fi or server needed.
- **Result**: Open **Tools -> Serial Plotter** at 115200 baud. Tapping the mic causes visual waveform spikes.

### 4. Combined Audio Pipeline Test
- **File**: `mic_test/mic_test.ino`
- **Scope**: Records 3 seconds of voice -> sends PCM to Gateway `/transcribe` -> receives transcribed text -> speaks response through speaker via `/speak`.
- **Requirement**: `server.py` running on your local network.
- **Result**: Full round-trip voice test verifying both dynamic I2S switching and speech recognition.

## Prerequisites & Libraries

In the Arduino IDE Library Manager (`Ctrl+Shift+I` or **Sketch -> Include Library -> Manage Libraries...**):

1. **WebSockets** by *Markus Sattler* (`arduinoWebSockets`)
2. **ArduinoJson** by *Benoit Blanchon* (v6 or v7)
3. **ESP32 Board Package** (by *Espressif Systems*) installed via Boards Manager

## Configuration & Flashing

1. Copy `secrets.h.example` to `secrets.h`:
   ```bash
   cp secrets.h.example secrets.h
   ```

2. Open `secrets.h` and insert your Wi-Fi credentials:
   ```cpp
   const char* WIFI_SSID     = "YOUR_WIFI_SSID";
   const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
   ```

3. Configure `GATEWAY_HOST` in your sketch to match your computer's local IP address (find with `ipconfig` on Windows).

4. Connect your ESP32 via USB, select your board and COM port, and upload (`Ctrl+U`).

5. Open Serial Monitor at **115200** baud.
