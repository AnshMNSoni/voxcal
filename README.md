# VoxCal — Voice-Controlled Calendar & Task Assistant

VoxCal is an open-source, voice-controlled calendar and task management assistant. It connects edge hardware (ESP32) through a local Python Gateway to an automation and orchestration engine (n8n).

---

## Architecture

```text
┌─────────────┐       WebSocket        ┌─────────────┐       HTTP POST       ┌─────────────┐
│    ESP32    │ ─────────────────────► │   Gateway   │ ────────────────────► │     n8n     │
│  (Hardware) │ ◄───────────────────── │  (FastAPI)  │ ◄──────────────────── │  (Webhook)  │
└─────────────┘       WebSocket        └─────────────┘     JSON Response     └─────────────┘
```

### Communication Flow
1. **ESP32** connects via Wi-Fi and establishes a persistent **WebSocket** connection with the local Gateway.
2. The **Gateway** receives incoming JSON payloads from the ESP32 and relays them via **HTTP POST** to the configured **n8n Webhook**.
3. **n8n** executes the workflow and returns a JSON response.
4. The **Gateway** forwards the response back to the ESP32 over the WebSocket connection.
5. The **ESP32** processes or outputs the response (Serial / Display / Audio).

---

## Project Structure

```text
voxcal/
├── hardware/
│   └── esp32/
│       ├── esp32.ino            # ESP32 Arduino firmware sketch
│       └── README.md            # Hardware setup & library dependencies
├── gateway/
│   ├── server.py                # FastAPI/Uvicorn WebSocket-to-HTTP Gateway
│   ├── requirements.txt         # Python dependencies
│   ├── .env.example             # Gateway environment variable template
│   └── README.md                # Gateway documentation
├── software/
│   ├── docker-compose.yml       # Docker Compose setup for n8n
│   ├── .env.example             # n8n environment configuration
│   └── README.md                # n8n deployment guide
└── docs/                        # Architecture notes and documentation
```

---

## Quick Start & Connectivity Testing

### 1. Start n8n Workflow Engine
Navigate to the `software` directory and start the n8n container:
```powershell
cd software
docker compose up -d
```
Access the n8n UI at `http://localhost:5678` and configure your webhook (e.g. `/webhook/voxcal/test`).

---

### 2. Start the Gateway Server
Navigate to the `gateway` directory, install dependencies, and launch the server:
```powershell
cd gateway
pip install -r requirements.txt
python server.py
```
The Gateway listens on `0.0.0.0:8000/ws` and forwards incoming messages to n8n.

> **Note:** Find your computer's LAN IP address by running `ipconfig` in PowerShell (e.g., `192.168.1.100` or `192.168.137.1` when using Mobile Hotspot).

---

### 3. Flash & Run ESP32 Firmware
1. Open [`hardware/esp32/esp32.ino`](hardware/esp32/esp32.ino) in Arduino IDE.
2. Install the required libraries via Library Manager:
   - **WebSockets** by *Markus Sattler*
   - **ArduinoJson** by *Benoit Blanchon*
3. Update your Wi-Fi credentials and `GATEWAY_HOST` with your computer's LAN IP.
4. Upload to the ESP32 and open Serial Monitor at **`115200`** baud.

---

## Test Payload

**ESP32 ➔ Gateway ➔ n8n:**
```json
{
  "type": "ping",
  "device_id": "esp32-01",
  "message": "Hello VoxCal"
}
```

**n8n ➔ Gateway ➔ ESP32:**
```json
{
  "status": "success",
  "message": "Hello from VoxCal n8n",
  "source": "n8n"
}
```

---

## Roadmap

- [x] **Phase 1: Connectivity & Network Pipeline** (ESP32 ↔ Gateway ↔ n8n)
- [ ] **Phase 2: Audio Hardware Integration** (Microphone I2S input & Speaker I2S output)
- [ ] **Phase 3: Speech Pipeline** (Wake word, STT, and TTS streaming)
- [ ] **Phase 4: Agent & Calendar Logic** (Task creation, updates, querying, Google Calendar / CalDAV integration)

---

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
