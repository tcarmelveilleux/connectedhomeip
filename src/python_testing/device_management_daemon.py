import asyncio
import json
import random
from contextlib import asynccontextmanager
from typing import List, Dict
import logging
import threading
import nest_asyncio


from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.responses import FileResponse
from fastapi.staticfiles import StaticFiles

nest_asyncio.apply()

# Configure a daemon-specific logger
logger = logging.getLogger("DeviceManagerDaemon")

json_handler_lock = threading.Lock()
global json_handler
json_handler = None

def set_json_handler(handler):
    global json_handler
    with json_handler_lock:
        json_handler = handler

# --- Models (could be in a separate models.py file) ---
class Device:
    def __init__(self, id: str, type: str, state: dict):
        self.id = id
        self.type = type
        self.state = state

# --- Connection Manager ---
class ConnectionManager:
    def __init__(self):
        self.active_connections: list = []

    async def connect(self, websocket: WebSocket):
        await websocket.accept()
        self.active_connections.append(websocket)
        logger.info(f"New connection. Total clients: {len(self.active_connections)}")

    def disconnect(self, websocket: WebSocket):
        if websocket in self.active_connections:
            self.active_connections.remove(websocket)
            logger.info(f"Client disconnected. Total clients: {len(self.active_connections)}")

    async def broadcast_json(self, data: dict):
        if not self.active_connections:
            return
        message_str = json.dumps(data)
        tasks = [client.send_text(message_str) for client in self.active_connections]
        await asyncio.gather(*tasks)

manager = ConnectionManager()

# --- Application State and Logic ---
update_queue = asyncio.Queue()
DEVICE_STATE_DB: dict = {
    "light-livingroom": {"type": "dimmable", "state": "OFF", "brightness": 0, "online": True, "errors": []},
    "sensor-outside": {"type": "sensor", "value": 18.5, "online": True, "errors": []},
    "switch-garage": {"type": "switch", "state": "not_pressed", "online": False, "errors": ["Initial connection failed"]},
}

async def state_broadcaster():
    while True:
        try:
            update_data = await update_queue.get()
            await manager.broadcast_json(update_data)
            update_queue.task_done()
        except asyncio.CancelledError:
            logger.info("State broadcaster cancelled.")
        except Exception as e:
            logger.error(f"Error in state broadcaster: {e}")

async def publish_update(data: dict):
    await update_queue.put(data) # type: ignore

# --- FastAPI Lifespan and App Definition ---
@asynccontextmanager
async def lifespan(app: FastAPI):
    logger.info("Daemon starting up...")
    broadcaster_task = asyncio.create_task(state_broadcaster())
    # simulator_task = asyncio.create_task(mock_device_simulator())
    yield
    print("Daemon shutting down...")
    # simulator_task.cancel()
    broadcaster_task.cancel()
    await asyncio.gather(broadcaster_task, return_exceptions=True)

def get_app():
    app = FastAPI(title="Device Control Daemon", lifespan=lifespan)

    # --- API Endpoints ---
    @app.websocket("/ws")
    async def websocket_endpoint(websocket: WebSocket):
        await manager.connect(websocket)
        # # Send initial full state sync to the newly connected client
        # initial_sync = {"type": "FULL_STATE_SYNC", "payload": {"devices": DEVICE_STATE_DB}}
        # await websocket.send_text(json.dumps(initial_sync))

        with json_handler_lock:
            if json_handler is not None:
                await json_handler({"type": "ON_CONNECT"})
        try:
            while True:
                data = await websocket.receive_json()
                with json_handler_lock:
                    if json_handler is not None:
                        await json_handler(data)
        except WebSocketDisconnect:
            manager.disconnect(websocket)
        except Exception as e:
            logger.error(f"Unhandled error in websocket endpoint: {e}")
            manager.disconnect(websocket)

    # --- Static File Serving ---
    # Assumes the compiled Angular app is in a 'www' directory.
    # This must be placed after API routes.
    app.mount("/", StaticFiles(directory="www/"), name="static_assets")

    @app.get("/{full_path:path}")
    async def serve_angular_app(full_path: str):
        # This catch-all route serves the index.html for Angular routing.
        return FileResponse("www/index.html")

    return app
