# VoxCal Gateway

The Gateway is a lightweight Python service that acts as the bridge between the ESP32 (communicating via WebSockets) and n8n workflows (communicating via HTTP Webhooks).

## Architecture

```text
ESP32 (Client) ──(WebSocket)──► Gateway (FastAPI) ──(HTTP POST)──► n8n Webhook
ESP32 (Client) ◄──(WebSocket)── Gateway (FastAPI) ◄──(HTTP JSON)─── n8n Webhook
```

## Setup & Running

### 1. Install Dependencies

Using `pip` or virtual environment:

```powershell
cd gateway
pip install -r requirements.txt
```

### 2. Configuration (Optional)

Create a `.env` file in the `gateway/` directory if you need custom settings (defaults to listening on all interfaces `0.0.0.0:8000` and n8n at `http://localhost:5678/webhook/voxcal/test`):

```env
GATEWAY_HOST=0.0.0.0
GATEWAY_PORT=8000
N8N_WEBHOOK_URL=http://localhost:5678/webhook/voxcal/test
```

### 3. Start the Server

```powershell
python server.py
```

or via Uvicorn CLI:

```powershell
uvicorn server:app --host 0.0.0.0 --port 8000
```

## Finding Your Computer's LAN IP for ESP32

On Windows PowerShell:
```powershell
ipconfig
```
Look for `IPv4 Address` under your active Wi-Fi / Ethernet adapter (e.g. `192.168.1.100`). This is the IP you must configure in the ESP32 firmware sketch.
