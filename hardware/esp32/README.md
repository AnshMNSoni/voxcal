# VoxCal ESP32 Firmware

This directory contains the firmware for the ESP32 to establish WebSocket communication with the local VoxCal Gateway.

## Prerequisites & Libraries

In the Arduino IDE Library Manager (`Ctrl+Shift+I` or **Sketch -> Include Library -> Manage Libraries...**):

1. **WebSockets** by *Markus Sattler* (`arduinoWebSockets`)
2. **ArduinoJson** by *Benoit Blanchon* (v6 or v7)
3. **ESP32 Board Package** (by *Espressif Systems*) installed via Boards Manager

## Configuration

1. Copy `secrets.h.example` to `secrets.h` (or `.env.example` to `.env`):
   ```bash
   cp secrets.h.example secrets.h
   # or copy .env.example to .env
   cp .env.example .env
   ```

2. Open `secrets.h` and update your Wi-Fi credentials:
   ```cpp
   const char* WIFI_SSID     = "YOUR_WIFI_SSID";
   const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
   ```
   > **Note:** `secrets.h` and `.env` are listed in `.gitignore` so your private credentials will not be committed to GitHub.

3. In `main.ino`, update the Gateway configuration if needed:
   ```cpp
   // Replace with the IPv4 address of your computer running the Gateway
   const char* GATEWAY_HOST  = "192.168.1.xxx"; 
   const uint16_t GATEWAY_PORT = 8000;
   const char* GATEWAY_PATH  = "/ws";
   ```

> **Note:** Do NOT use `localhost` or `127.0.0.1`. Use your computer's local network IP (run `ipconfig` in PowerShell to find it).

## Flashing the ESP32

1. Connect your ESP32 board via USB.
2. Select your board from **Tools -> Board -> ESP32 Arduino** (e.g. *ESP32 Dev Module*).
3. Select the correct COM port from **Tools -> Port**.
4. Click **Upload** (`Ctrl+U`).
5. Open the **Serial Monitor** (`Ctrl+Shift+M`) at baud rate **`115200`**.
