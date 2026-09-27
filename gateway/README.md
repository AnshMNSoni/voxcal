# VoxCal Gateway

The Gateway is an asynchronous Python service built on FastAPI. It acts as the communication and audio bridge between the ESP32 edge microcontroller, the local speech processing engines, and the n8n autonomous calendar agent.

## Architecture & Endpoints

```text
+-------------------+             +-----------------------+             +----------------------+
|   ESP32 Client    |             |    FastAPI Gateway    |             |     n8n Workflow     |
+-------------------+             +-----------------------+             +----------------------+
          |                                   |                                     |
          |--- HTTP POST /transcribe (PCM) -->| [SpeechRecognition STT]             |
          |<-- JSON {"text": "..."} ----------|                                     |
          |                                   |                                     |
          |--- WebSocket JSON Command ------->|--- HTTP POST /webhook/voxcal/test ->|
          |<-- WebSocket Agent Response ------|<-- JSON Response -------------------|
          |                                   |                                     |
          |--- HTTP GET /speak?text=... ----->| [pyttsx3 16kHz PCM Stream]          |
          |<-- Binary PCM Stream -------------|                                     |
```

### Endpoints Overview

1. `POST /transcribe`:
   - Accepts raw 16-bit 16000Hz mono PCM audio in the request body.
   - Applies DC offset removal and automatic gain normalization.
   - Transcribes audio to text via Google Speech Recognition with multi-language fallback (`en-IN`, `en-US`, `hi-IN`).
   - Returns JSON: `{"text": "<transcribed_text>", "error": null}`.

2. `GET /speak?text=<text>&volume=1.0`:
   - Generates text-to-speech audio via `pyttsx3`.
   - Resamples audio cleanly to 16000Hz mono 16-bit PCM.
   - Normalizes amplitude to target peak for clear speaker volume.
   - Streams raw PCM bytes directly to the ESP32 over HTTP.

3. `POST /speak`:
   - JSON payload alternative for the speech synthesis endpoint (`{"text": "...", "volume": 1.0}`).

4. `WebSocket /ws`:
   - Bi-directional real-time connection for text commands and agent responses.
   - Forwards commands to the n8n webhook URL configured in `.env`.

## Setup & Running

### 1. Install Dependencies

Using `pip` in your Python 3.10+ environment:

```powershell
cd gateway
pip install -r requirements.txt
```

### 2. Configuration

Copy `.env.example` to `.env` if you need custom settings:

```env
GATEWAY_HOST=0.0.0.0
GATEWAY_PORT=8000
N8N_WEBHOOK_URL=http://localhost:5678/webhook/voxcal/test
```

### 3. Start the Server

```powershell
python server.py
```

Or via Uvicorn CLI:

```powershell
uvicorn server:app --host 0.0.0.0 --port 8000
```

## Finding Your Computer's LAN IP for ESP32

On Windows PowerShell:
```powershell
ipconfig
```
Look for `IPv4 Address` under your active Wi-Fi or Ethernet adapter (e.g. `192.168.1.100` or `192.168.137.1`). This is the IP address you configure in the ESP32 firmware sketch (`GATEWAY_HOST`).
