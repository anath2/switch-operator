"""MQTT manager for ESP32 servo control server."""
import json
import logging
from datetime import datetime

import paho.mqtt.client as mqtt

from server.device import connected_devices
from server.model import DeviceStatus


logger = logging.getLogger(__name__)

# MQTT Configuration
MQTT_BROKER = "localhost"  # Change to your broker IP if different
MQTT_PORT = 1883
MQTT_TOPICS = {
    "command": "servo/+/command",      # servo/{device_id}/command
    "status": "servo/+/status",        # servo/{device_id}/status
    "discovery": "servo/discovery",      # Device discovery
}

# Global state
mqtt_client = None


class MQTTManager:
    """Manager for MQTT communication with ESP32 servo devices."""
    def __init__(self) -> None:
        """Initialize the MQTTManager and set up callbacks."""
        self.client = mqtt.Client()
        self.client.on_connect = self.on_connect
        self.client.on_message = self.on_message
        self.client.on_disconnect = self.on_disconnect

    def on_connect(self, client: mqtt.Client, userdata: object, flags: dict, rc: int) -> None:
        """Handle MQTT connection event."""
        if rc == 0:
            logger.info("Connected to MQTT broker")
            # Subscribe to all servo topics
            for topic in MQTT_TOPICS.values():
                client.subscribe(topic)
                logger.info(f"Subscribed to {topic}")
        else:
            logger.error(f"Failed to connect to MQTT broker: {rc}")

    def on_message(self, client: mqtt.Client, userdata: object, msg: mqtt.MQTTMessage) -> None:
        """Handle incoming MQTT messages."""
        try:
            topic_parts = msg.topic.split('/')
            if len(topic_parts) >= 3:
                device_id = topic_parts[1]
                message_type = topic_parts[2]
                payload = json.loads(msg.payload.decode())

                if message_type == "status":
                    self.handle_device_status(device_id, payload)
                elif topic_parts[1] == "discovery":
                    self.handle_device_discovery(payload)

        except Exception as e:
            logger.error(f"Error processing MQTT message: {e}")

    def on_disconnect(self, client: mqtt.Client, userdata: object, rc: int) -> None:
        """Handle MQTT disconnection event."""
        logger.warning("Disconnected from MQTT broker")

    def handle_device_status(self, device_id: str, payload: dict) -> None:
        """Handle status updates from ESP32 devices."""
        connected_devices[device_id] = DeviceStatus(
            device_id=device_id,
            online=True,
            last_seen=datetime.now(),
            current_angle=payload.get("angle"),
            is_sweeping=payload.get("sweeping", False),
        )
        logger.info(f"Status update from {device_id}: {payload}")

    def handle_device_discovery(self, payload: dict) -> None:
        """Handle new device discovery."""
        device_id = payload.get("device_id")
        if device_id:
            logger.info(f"New device discovered: {device_id}")

    def publish_command(self, device_id: str, command: dict) -> None:
        """Send command to specific ESP32."""
        topic = f"servo/{device_id}/command"
        self.client.publish(topic, json.dumps(command))
        logger.info(f"Sent command to {device_id}: {command}")

    def connect(self) -> bool:
        """Connect to the MQTT broker and start the loop."""
        try:
            self.client.connect(MQTT_BROKER, MQTT_PORT, 60)
            self.client.loop_start()
            return True
        except Exception as e:
            logger.error(f"Failed to connect to MQTT: {e}")
            return False

