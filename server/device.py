"""Device registry for ESP32 servo control server."""
from server.model import DeviceStatus


connected_devices: dict[str, DeviceStatus] = {}
