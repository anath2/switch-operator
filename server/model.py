"""Pydantic models for ESP32 servo control server."""
from datetime import datetime

from pydantic import BaseModel, Field


class DeviceStatus(BaseModel):
    """Status of a connected ESP32 device. Supports minimal payloads (only device_id required)."""
    device_id: str
    online: bool
    last_seen: datetime


class SweepCommand(BaseModel):
    """Command to start or stop servo sweep."""
    device_id: str = Field(..., description="ESP32 device identifier")
    action: str = Field(..., description="start or stop")
    min_angle: int | None = Field(0, ge=0, le=180)
    max_angle: int | None = Field(180, ge=0, le=180)
    speed: int | None = Field(50, ge=1, le=1000, description="Sweep speed (ms)")


class RotateCommand(BaseModel):
    """Command to rotate a servo to a specific angle and speed (matches firmware expectations).
    Attributes:
        device_id (str): ESP32 device identifier
        angle (int): Servo angle (0-180)
        speed (int): Rotation speed (ms)
    """
    device_id: str = Field(..., description="ESP32 device identifier")
    angle: int = Field(..., ge=0, le=180, description="Servo angle (0-180)")
    speed: int | None = Field(100, ge=1, le=1000, description="Rotation speed (ms)")


class VoiceCommand(BaseModel):
    """Command to control devices via natural language."""
    command: str = Field(..., description="Natural language command")
    devices: list[str] | None = Field(None, description="Target devices (optional)")
