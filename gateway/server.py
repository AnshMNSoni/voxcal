import os
import io
import json
import math
import wave
import tempfile
import httpx
import uvicorn
import numpy as np
from scipy.signal import resample_poly
import pyttsx3
import speech_recognition as sr
from pydantic import BaseModel
from fastapi import FastAPI, Request, WebSocket, WebSocketDisconnect
from fastapi.responses import JSONResponse, StreamingResponse
from dotenv import load_dotenv

# Load environment variables
load_dotenv()

HOST = os.getenv("GATEWAY_HOST", "0.0.0.0")
PORT = int(os.getenv("GATEWAY_PORT", "8000"))
N8N_WEBHOOK_URL = os.getenv("N8N_WEBHOOK_URL", "http://localhost:5678/webhook/voxcal/test")

app = FastAPI(title="VoxCal Gateway")


@app.on_event("startup")
async def startup_event():
    print("=" * 60)
    print("[VoxCal Gateway] Starting...")
    print(f"[Gateway] WebSocket server listening on {HOST}:{PORT}")
    print(f"[Gateway] n8n URL: {N8N_WEBHOOK_URL}")
    print("=" * 60)


@app.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):
    await websocket.accept()
    client_ip = websocket.client.host if websocket.client else "unknown"
    print(f"\n[WebSocket] ESP32 connected ({client_ip})")

    try:
        while True:
            # Receive raw text from ESP32
            raw_data = await websocket.receive_text()
            print(f"[WebSocket] Received:\n{raw_data}")

            # Parse JSON payload
            try:
                payload = json.loads(raw_data)
            except json.JSONDecodeError as err:
                print(f"[WebSocket] Invalid JSON received: {err}")
                error_response = {
                    "status": "error",
                    "message": f"Invalid JSON payload: {str(err)}",
                    "source": "gateway"
                }
                await websocket.send_text(json.dumps(error_response))
                continue

            # Forward JSON payload to n8n
            print("\n[n8n] Sending request...")
            async with httpx.AsyncClient(timeout=240.0) as client:
                try:
                    resp = await client.post(
                        N8N_WEBHOOK_URL,
                        json=payload,
                        headers={"Content-Type": "application/json"}
                    )
                    
                    try:
                        n8n_data = resp.json()
                        response_str = json.dumps(n8n_data)
                    except Exception:
                        response_str = resp.text
                        n8n_data = {"raw_response": response_str, "status_code": resp.status_code}

                    print(f"[n8n] Response:\n{response_str}")

                except httpx.RequestError as exc:
                    print(f"[n8n] HTTP Request failed: {exc}")
                    n8n_data = {
                        "status": "error",
                        "message": f"Failed to reach n8n webhook: {str(exc)}",
                        "source": "gateway"
                    }

            # Return response back to ESP32 over WebSocket
            print("[WebSocket] Sending response to ESP32...")
            await websocket.send_text(json.dumps(n8n_data))
            print("[WebSocket] Response sent successfully.\n")

    except WebSocketDisconnect:
        print(f"[WebSocket] ESP32 disconnected ({client_ip})")
    except Exception as e:
        print(f"[WebSocket] Unexpected error: {e}")


def synthesize_speech_pcm(text: str, volume: float = 1.0) -> tuple[bytes, int]:
    """
    Synthesize text into raw 16-bit 16000Hz mono PCM audio using pyttsx3.
    """
    text = (text or "").strip()
    if not text:
        text = "Hello from VoxCal."

    # Use a unique temporary file
    temp_wav = os.path.join(tempfile.gettempdir(), f"voxcal_tts_{os.getpid()}_{os.urandom(4).hex()}.wav")

    try:
        engine = pyttsx3.init()
        engine.setProperty("rate", 130)
        engine.setProperty("volume", 1.0)
        engine.save_to_file(text, temp_wav)
        engine.runAndWait()

        # Read and resample to 16000 Hz mono 16-bit PCM
        with wave.open(temp_wav, "rb") as f:
            n_channels = f.getnchannels()
            rate = f.getframerate()
            raw = f.readframes(f.getnframes())

        samples = np.frombuffer(raw, dtype=np.int16)
        if n_channels == 2:
            samples = samples.reshape(-1, 2).mean(axis=1).astype(np.int16)

        if rate != 16000:
            g = math.gcd(16000, rate)
            samples = resample_poly(samples, 16000 // g, rate // g)
            samples = np.clip(samples, -32768, 32767).astype(np.int16)

        # Normalize volume (target ~28000-30000 peak for clear, loud speech)
        target_peak = float(np.clip(28000.0 * volume, 4000.0, 32500.0))
        max_amp = np.max(np.abs(samples)) if len(samples) > 0 else 0
        if max_amp > 0:
            samples = (samples * (target_peak / max_amp)).astype(np.int16)

        pcm_bytes = samples.astype(np.int16).tobytes()
        return pcm_bytes, len(samples)
    finally:
        if os.path.exists(temp_wav):
            try:
                os.remove(temp_wav)
            except Exception:
                pass


class SpeakRequest(BaseModel):
    text: str = "Hello World"
    volume: float = 1.0


@app.get("/speak")
def speak_get(text: str = "Hello World", volume: float = 1.0):
    """
    HTTP GET endpoint for ESP32 speech output:
    e.g. GET /speak?text=Hello%20World&volume=1.0
    """
    text_clean = (text or "").strip()
    print(f"\n[TTS GET] Generating speech: \"{text_clean}\" (volume: {volume})")
    pcm_bytes, sample_count = synthesize_speech_pcm(text_clean, volume=volume)
    print(f"[TTS] Returning {sample_count} PCM samples ({len(pcm_bytes)} bytes)")
    return StreamingResponse(
        io.BytesIO(pcm_bytes),
        media_type="application/octet-stream",
        headers={
            "X-Sample-Count": str(sample_count),
            "Content-Length": str(len(pcm_bytes)),
        }
    )


@app.post("/speak")
def speak_post(payload: SpeakRequest):
    """
    HTTP POST endpoint with JSON body:
    e.g. POST /speak  {"text": "Hello World", "volume": 1.0}
    """
    return speak_get(text=payload.text, volume=payload.volume)


@app.post("/transcribe")
async def transcribe(request: Request):
    """
    Accepts raw 16-bit 16000Hz mono PCM audio from ESP32, normalizes volume,
    and returns the transcribed text as JSON.
    """
    sample_rate = int(request.headers.get("X-Sample-Rate", "16000"))
    pcm_bytes = await request.body()

    if len(pcm_bytes) < 100:
        return JSONResponse({"text": "", "error": "Audio too short"}, status_code=400)

    # Convert raw PCM bytes to numpy int16 array
    samples = np.frombuffer(pcm_bytes, dtype=np.int16).astype(np.float32)
    sample_count = len(samples)
    duration_sec = sample_count / sample_rate

    # Audio diagnostics
    peak = float(np.max(np.abs(samples))) if sample_count > 0 else 0.0
    rms = float(np.sqrt(np.mean(samples ** 2))) if sample_count > 0 else 0.0
    dc_offset = float(np.mean(samples)) if sample_count > 0 else 0.0

    print(f"\n[STT] Audio received: {len(pcm_bytes)} bytes ({duration_sec:.1f}s @ {sample_rate}Hz)")
    print(f"[STT] Stats -> Peak: {peak:.0f}/32767, RMS: {rms:.1f}, DC Offset: {dc_offset:.1f}")

    # Check for silence or disconnected microphone
    if peak < 80.0:
        print("[STT] ⚠️ WARNING: Microphone audio is almost completely SILENT (all zeros or background noise).")
        print("[STT] Check INMP441 wiring: L/R pin must be connected to GND, SD to GPIO 34, SCK to 32, WS to 33, VDD to 3.3V.")
        return JSONResponse({
            "text": "",
            "error": "Audio is silent. Check microphone wiring (L/R to GND, SD to GPIO 34)."
        })

    # 1. Remove DC offset
    samples = samples - dc_offset

    # 2. Digital Auto-Gain: if peak is under 20000, normalize/boost
    if peak < 20000.0 and peak > 0:
        gain = min(22000.0 / peak, 15.0)  # up to 15x gain
        samples = samples * gain
        print(f"[STT] Applied auto-gain: {gain:.2f}x (new peak: {np.max(np.abs(samples)):.0f})")

    # Clip to int16 range
    processed_pcm = np.clip(samples, -32768, 32767).astype(np.int16).tobytes()

    # Save to debug_mic.wav on disk for inspection
    debug_wav_path = os.path.join(os.path.dirname(__file__), "debug_mic.wav")
    try:
        with wave.open(debug_wav_path, "wb") as wf:
            wf.setnchannels(1)
            wf.setsampwidth(2)
            wf.setframerate(sample_rate)
            wf.writeframes(processed_pcm)
        print(f"[STT] Saved debug audio to {debug_wav_path}")
    except Exception as e:
        print(f"[STT] Note: Could not save debug WAV: {e}")

    # In-memory WAV for SpeechRecognition
    wav_buf = io.BytesIO()
    with wave.open(wav_buf, "wb") as wf:
        wf.setnchannels(1)
        wf.setsampwidth(2)
        wf.setframerate(sample_rate)
        wf.writeframes(processed_pcm)
    wav_buf.seek(0)

    recognizer = sr.Recognizer()
    recognizer.dynamic_energy_threshold = True

    try:
        with sr.AudioFile(wav_buf) as source:
            # Adjust for ambient noise slightly
            recognizer.adjust_for_ambient_noise(source, duration=0.2)
            audio_data = recognizer.record(source)

        # Try Indian English first, then US English, then Hindi
        text = ""
        languages = ["en-IN", "en-US", "hi-IN"]
        for lang in languages:
            try:
                text = recognizer.recognize_google(audio_data, language=lang)
                if text:
                    print(f"[STT] Transcribed ({lang}): \"{text}\"")
                    break
            except sr.UnknownValueError:
                continue

        if not text:
            print("[STT] Could not understand audio in any recognized language.")
            return JSONResponse({"text": "", "error": "Could not understand audio. Try speaking closer and louder."})

        return JSONResponse({"text": text, "error": None})

    except sr.RequestError as e:
        print(f"[STT] Google API error: {e}")
        return JSONResponse({"text": "", "error": f"STT API error: {str(e)}"}, status_code=503)
    except Exception as e:
        print(f"[STT] Unexpected error: {e}")
        return JSONResponse({"text": "", "error": str(e)}, status_code=500)


if __name__ == "__main__":
    uvicorn.run("server:app", host=HOST, port=PORT, reload=True)
