// =========================
// Includes & Configuration
// =========================

#include <WiFi.h>
#include <PubSubClient.h>
#include <ESP32Servo.h>
#include <ArduinoJson.h>
#include "config.h"
// =========================
// MQTT Topics
// =========================

char servo_sweep_topic[64];
char servo_rotate_topic[64];

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

void rotateServo(int offset, int speed) {
    static int currentAngle = 0;
    myServo.write(currentAngle);
    delay(speed);

    int targetAngle = currentAngle + offset;
    targetAngle = constrain(targetAngle, 0, 180);

    Serial.print("Rotating servo from ");
    Serial.print(currentAngle);
    Serial.print(" to ");
    Serial.print(targetAngle);
    Serial.print(" (offset: ");
    Serial.print(offset);
    Serial.print(", speed: ");
    Serial.print(speed);
    Serial.println(")");

    int step = (targetAngle > currentAngle) ? 1 : -1;
    for (int pos = currentAngle + step; pos != targetAngle + step; pos += step) {
        myServo.write(pos);
        delay(speed);
        Serial.print("Servo at: ");
        Serial.println(pos);
    }

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

    String topicStr = String(topic);
    if (topicStr == servo_sweep_topic && message == "sweep") {
        sweepServo();
    }
    if (topicStr == servo_rotate_topic) {
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
    // Connect to MQTT broker
    while (!client.connected()) {
        Serial.print("Attempting MQTT connection...");

        if (client.connect(MQTT_DEVICE_ID)) {
            Serial.println("connected");
            client.subscribe(servo_sweep_topic);
            Serial.print("Subscribed to ");
            Serial.println(servo_sweep_topic);
            client.subscribe(servo_rotate_topic);
            Serial.print("Subscribed to ");
            Serial.println(servo_rotate_topic);
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
    // Construct topics
    snprintf(servo_sweep_topic, sizeof(servo_sweep_topic), "/servo/%s/sweep", MQTT_DEVICE_ID);
    snprintf(servo_rotate_topic, sizeof(servo_rotate_topic), "/servo/%s/rotate", MQTT_DEVICE_ID);

    Serial.begin(SERIAL_BAUD_RATE);
    delay(1000);
    myServo.attach(SERVO_PIN);
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
