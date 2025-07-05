"""FastAPI MQTT Servo Control Server.

Manages multiple ESP32 servo controllers via MQTT.
"""

import logging
from contextlib import asynccontextmanager

import uvicorn
from fastapi import FastAPI, HTTPException, Request
from fastapi.responses import HTMLResponse
from fastapi.staticfiles import StaticFiles
from fastapi.templating import Jinja2Templates

import server.model as model
from server.device import connected_devices
from server.mqtt import MQTTManager
import json


templates = Jinja2Templates(directory="server/templates")
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)


@asynccontextmanager
async def lifespan(app: FastAPI):
    """Lifespan event handler."""
    app.state.devices = connected_devices
    if not mqtt_manager.connect():
        logger.error("Failed to initialize MQTT connection")

    yield

    mqtt_manager.client.loop_stop()
    mqtt_manager.client.disconnect()

app = FastAPI(lifespan=lifespan, title="ESP32 Servo Control Server")
mqtt_manager = MQTTManager()

# Mount static directory for CSS/JS
app.mount("/static", StaticFiles(directory="server/static"), name="static")


@app.get("/fragments/devices", response_class=HTMLResponse)
async def get_devices_fragment(request: Request) -> HTMLResponse:
    """Serve the devices fragment for HTMX."""
    devices = app.state.devices
    return templates.TemplateResponse("fragments/devices.html", {"request": request, "devices": devices})


@app.get("/", response_class=HTMLResponse)
async def get_dashboard(request: Request) -> HTMLResponse:
    """Serve the web dashboard."""
    return templates.TemplateResponse("dashboard.html", {"request": request})

@app.get("/devices", response_model=dict[str, model.DeviceStatus])
async def get_devices() -> dict[str, model.DeviceStatus]:
    """Get all connected devices and their status."""
    return app.state.devices

@app.get("/fragments/servo_rotate", response_class=HTMLResponse)
async def get_servo_rotate_fragment(request: Request) -> HTMLResponse:
    devices = app.state.devices
    return templates.TemplateResponse("fragments/servo_rotate.html", {"request": request, "devices": devices})

@app.post("/servo/rotate", response_model=dict[str, str | int])
async def rotate_servo(command: model.RotateCommand) -> dict[str, str | int]:
    """Rotate servo to specific angle (matches firmware topic and payload)."""
    if command.device_id not in app.state.devices:
        raise HTTPException(status_code=404, detail="Device not found")

    mqtt_command = {
        "angle": command.angle,
        "speed": command.speed,
    }
    # Publish to the /servo/{command.device_id}/rotate topic as expected by firmware
    mqtt_manager.client.publish(f"/servo/{command.device_id}/rotate", json.dumps(mqtt_command))
    return {"status": "rotate_command_sent", "device_id": command.device_id, "angle": command.angle}


@app.get("/health")
async def health_check() -> dict[str, int | str]:
    """Health check endpoint."""
    return {"status": "healthy", "devices": len(app.state.devices)}


if __name__ == "__main__":
    uvicorn.run(app, host="localhost", port=4567)
