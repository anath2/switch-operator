// =========================
// Includes & Configuration
// =========================

#include <WiFi.h>
#include <PubSubClient.h>
#include "config.h"
#include <ESP32Servo.h>
#include <ArduinoJson.h>

// =========================
// MQTT Topics
// =========================

#define SERVO_SWEEP_TOPIC   "servo/+/sweep"
#define SERVO_ROTATE_TOPIC  "servo/+/rotate"

// =========================
// WiFi, MQTT, Servo Init
// =========================

WiFiClient espClient;
PubSubClient client(espClient);
Servo myServo;

// =========================
// Servo Control
// =========================

void sweepServo() {
    Serial.println("Sweeping servo...");
    for (int pos = 0; pos <= 180; pos += 5) {
        myServo.write(pos);
        delay(15);
    }
    for (int pos = 180; pos >= 0; pos -= 5) {
        myServo.write(pos);
        delay(15);
    }
    Serial.println("Sweep complete.");
}

void rotateServo(int angle, int speed) {
    const int k = 300; // Tune this
    int delayMs = k / max(speed, 1); // Avoid division by zero
    delayMs = constrain(delayMs, 5, 100); // Clamp between 5ms and 100ms
    
    Serial.print("Rotating servo to ");
    Serial.print(angle);
    Serial.print(" at speed ");
    Serial.println(speed);
   
    myServo.write(angle);
    delay(delayMs);
}

// =========================
// MQTT Callback
// =========================

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    String message;
    for (unsigned int i = 0; i < length; i++) {
        message += (char)payload[i];
    }
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("]: ");
    Serial.println(message);

    if (String(topic) == SERVO_SWEEP_TOPIC && message == "sweep") {
        sweepServo();
    }
    if (String(topic) == SERVO_ROTATE_TOPIC) {
        StaticJsonDocument<128> doc;
        DeserializationError error = deserializeJson(doc, message);
        if (error) {
            Serial.print("Failed to parse rotate payload: ");
            Serial.println(error.c_str());
            return;
        }
        int angle = doc["angle"];
        int speed = doc["speed"];
        rotateServo(angle, speed);
    }
}

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
            client.subscribe(SERVO_SWEEP_TOPIC);
            Serial.print("Subscribed to ");
            Serial.println(SERVO_SWEEP_TOPIC);
            client.subscribe(SERVO_ROTATE_TOPIC);
            Serial.print("Subscribed to ");
            Serial.println(SERVO_ROTATE_TOPIC);
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
    myServo.attach(SERVO_PIN);
    myServo.write(90); // Center servo
    connectToWiFi();
    client.setServer(MQTT_SERVER, MQTT_PORT);
    if (WiFi.status() == WL_CONNECTED) {
        connectToMQTT();
        client.setCallback(mqttCallback);
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
