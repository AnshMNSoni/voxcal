import os
import json
import httpx
import uvicorn
from fastapi import FastAPI, WebSocket, WebSocketDisconnect
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


if __name__ == "__main__":
    uvicorn.run("server:app", host=HOST, port=PORT, reload=False)
