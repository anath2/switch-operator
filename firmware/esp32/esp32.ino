#include <WiFi.h>
#include <PubSubClient.h>
#include "config.h"

const char* mqtt_server = MQTT_SERVER;
const int mqtt_port = MQTT_PORT;
const char* mqtt_topic = MQTT_TOPIC;
const char* device_id = DEVICE_ID;

WiFiClient espClient;

PubSubClient client(espClient);


void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(device_id)) {
      Serial.println("connected");
      // Once connected, publish a message
      String message = "hello from " + String(device_id);
      client.publish(mqtt_topic, message.c_str());
      Serial.println("Published hello message to MQTT");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}


void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("Connecting to WiFi...");
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    int retries = 0;
    while (WiFi.status() != WL_CONNECTED && retries < 30) {
        delay(500);
        Serial.print(".");
        retries++;
    }

    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.print("WiFi connected! IP address: ");
        Serial.println(WiFi.localIP());
        client.setServer(mqtt_server, mqtt_port);
        reconnect();
    } else {
        Serial.println("Failed to connect to WiFi.");
    }
}


void loop() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi lost. Reconnecting...");
        WiFi.reconnect();
        delay(1000);
        return;
    }
    if (!client.connected()) {
        reconnect();
    }
    client.loop();
}
