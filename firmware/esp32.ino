#include <WiFi.h>
#include <PubSubClient.h>
#include <ESP32Servo.h>
#include <ArduinoJson.h>
#include "config.h"


// WiFi credentials
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASS;

// MQTT Configuration
const char* mqtt_server = MQTT_SERVER;
const int mqtt_port = MQTT_PORT;
const char* device_id = DEVICE_ID;

// Servo setup
Servo myServo;
const int servoPin = 18;
int currentAngle = 90;
int targetAngle = 90;

// Sweep variables
bool sweepMode = false;
bool sweepDirection = true;
int sweepMin = 0;
int sweepMax = 180;
int sweepSpeed = 50;
unsigned long lastSweepTime = 0;
unsigned long lastStatusTime = 0;

// Movement variables
bool isMoving = false;
int moveSpeed = 100;
unsigned long lastMoveTime = 0;

// MQTT topics
String commandTopic = "servo/" + String(device_id) + "/command";
String statusTopic = "servo/" + String(device_id) + "/status";
String discoveryTopic = "servo/discovery";

WiFiClient espClient;
PubSubClient client(espClient);

void setup() {
    Serial.begin(115200);
    
    // Initialize servo
    myServo.attach(servoPin);
    myServo.write(currentAngle);
    delay(500);
    
    // Connect to WiFi
    setupWiFi();
    
    // Setup MQTT
    client.setServer(mqtt_server, mqtt_port);
    client.setCallback(onMqttMessage);
    
    // Connect to MQTT broker
    connectToMQTT();
    
    Serial.println("ESP32 MQTT Servo Client Ready!");
    Serial.println("Device ID: " + String(device_id));
}

void loop() {
    // Maintain MQTT connection
    if (!client.connected()) {
        connectToMQTT();
    }
    client.loop();
    
    // Handle servo movement
    handleServoMovement();
    
    // Handle sweep mode
    if (sweepMode) {
        handleSweepMode();
    }
    
    // Send status updates every 5 seconds
    unsigned long now = millis();
    if (now - lastStatusTime > 5000) {
        sendStatusUpdate();
        lastStatusTime = now;
    }
}

void setupWiFi() {
    delay(10);
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);
    
    WiFi.begin(ssid, password);
    
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    
    randomSeed(micros());
    
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
}

void connectToMQTT() {
    while (!client.connected()) {
        Serial.print("Attempting MQTT connection...");
        
        if (client.connect(device_id.c_str())) {
            Serial.println("connected");
            
            // Subscribe to command topic
            client.subscribe(commandTopic.c_str());
            Serial.println("Subscribed to: " + commandTopic);
            
            // Send discovery message
            sendDiscoveryMessage();
            
            // Send initial status
            sendStatusUpdate();
            
        } else {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 5 seconds");
            delay(5000);
        }
    }
}

void onMqttMessage(char* topic, byte* payload, unsigned int length) {
    // Convert payload to string
    String message = "";
    for (int i = 0; i < length; i++) {
        message += (char)payload[i];
    }
    
    Serial.println("Received message on topic: " + String(topic));
    Serial.println("Message: " + message);
    
    // Parse JSON command
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, message);
    
    if (error) {
        Serial.println("Failed to parse JSON command");
        return;
    }
    
    String commandType = doc["type"].as<String>();
    
    if (commandType == "move") {
        handleMoveCommand(doc);
    } else if (commandType == "sweep") {
        handleSweepCommand(doc);
    } else {
        Serial.println("Unknown command type: " + commandType);
    }
}

void handleMoveCommand(DynamicJsonDocument& doc) {
    int angle = doc["angle"].as<int>();
    int speed = doc["speed"] | 100;  // Default speed if not specified
    
    if (angle >= 0 && angle <= 180) {
        targetAngle = angle;
        moveSpeed = speed;
        isMoving = true;
        sweepMode = false;  // Stop sweep mode
        
        Serial.println("Moving to angle: " + String(angle) + "° at speed: " + String(speed));
    } else {
        Serial.println("Invalid angle: " + String(angle));
    }
}

void handleServoMovement() {
    if (isMoving && currentAngle != targetAngle) {
        unsigned long now = millis();
        if (now - lastMoveTime >= moveSpeed) {
            if (currentAngle < targetAngle) {
                currentAngle++;
            } else {
                currentAngle--;
            }
            
            myServo.write(currentAngle);
            lastMoveTime = now;
            
            if (currentAngle == targetAngle) {
                isMoving = false;
                Serial.println("Reached target angle: " + String(currentAngle) + "°");
            }
        }
    }
}

void sendStatusUpdate() {
    DynamicJsonDocument doc(512);
    doc["device_id"] = device_id;
    doc["angle"] = currentAngle;
    doc["target_angle"] = targetAngle;
    doc["sweeping"] = sweepMode;
    doc["moving"] = isMoving;
    doc["timestamp"] = millis();
    doc["wifi_rssi"] = WiFi.RSSI();
    
    String statusMessage;
    serializeJson(doc, statusMessage);
    
    client.publish(statusTopic.c_str(), statusMessage.c_str());
    Serial.println("Status sent: " + statusMessage);
}

void sendDiscoveryMessage() {
    DynamicJsonDocument doc(256);
    doc["device_id"] = device_id;
    doc["type"] = "servo_controller";
    doc["ip"] = WiFi.localIP().toString();
    doc["mac"] = WiFi.macAddress();
    
    String discoveryMessage;
    serializeJson(doc, discoveryMessage);
    
    client.publish(discoveryTopic.c_str(), discoveryMessage.c_str());
    Serial.println("Discovery message sent");
}