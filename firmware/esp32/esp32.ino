// =========================
// Includes & Configuration
// =========================
#include <WiFi.h>
#include <PubSubClient.h>
#include "config.h"


// WiFi and MQTT client
WiFiClient espClient;

PubSubClient client(espClient);

// =========================
// Helper Functions
// =========================

// Connect to WiFi with retries
void connectToWiFi() {
    Serial.println("Connecting to WiFi...");
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    int retries = 0;

    while (WiFi.status() != WL_CONNECTED && retries < WIFI_CONNECT_RETRIES) {
        delay(WIFI_RETRY_DELAY_MS);
        Serial.print(".");
        retries++;
    }

    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.print("WiFi connected! IP address: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("Failed to connect to WiFi.");
    }
}

// Connect to MQTT broker with retries
void connectToMQTT() {
    while (!client.connected()) {
        Serial.print("Attempting MQTT connection...");

        if (client.connect(MQTT_DEVICE_ID)) {
            Serial.println("connected");
            String message = "hello from " + String(MQTT_DEVICE_ID);
            client.publish(mqtt_topic, message.c_str());
            Serial.println("Published hello message to MQTT");
        } else {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 5 seconds");
            delay(MQTT_RETRY_DELAY_MS);
        }

    }
}


// =========================
// Arduino Setup & Loop
// =========================

void setup() {
    Serial.begin(SERIAL_BAUD_RATE);
    delay(1000);
    connectToWiFi();
    client.setServer(MQTT_SERVER, MQTT_PORT);
    if (WiFi.status() == WL_CONNECTED) {
        connectToMQTT();
    }
}


void loop() {
    // Check WiFi connection
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi lost. Attempting to reconnect...");
        connectToWiFi();
        if (WiFi.status() == WL_CONNECTED) {
            client.setServer(MQTT_SERVER, MQTT_PORT);
            connectToMQTT();
        }
        delay(1000);
        return;
    }
    // Check MQTT connection
    if (!client.connected()) {
        connectToMQTT();
    }
    client.loop();
}
