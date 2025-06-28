#include <WiFi.h>
#include <PubSubClient.h>
#include <ESP32Servo.h>
#include <ArduinoJson.h>
#include <esp_wifi.h>
#include "config.h"


// WiFi credentials
const char* password = WIFI_PASS;
const char* ssid = WIFI_SSID;

// MQTT Configuration
const char* mqtt_server = MQTT_SERVER;
const int mqtt_port = MQTT_PORT;
const char* device_id = DEVICE_ID;

// Servo setup
Servo myServo;
const int servoPin = SERVO_PIN;
int currentAngle = 90;
int targetAngle = 90;


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


// Add this BEFORE setup()
void WiFiEvent(WiFiEvent_t event) {
    Serial.printf("[WiFi-event] event: %d - ", event);
    
    switch(event) {
        case SYSTEM_EVENT_STA_GOT_IP:
            Serial.print("Got IP: ");
            Serial.println(WiFi.localIP());
            break;
        case SYSTEM_EVENT_STA_CONNECTED:
            Serial.println("Connected to AP");
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            Serial.println("Disconnected from AP");
            break;
        case SYSTEM_EVENT_STA_START:
            Serial.println("Station started");
            break;
        case SYSTEM_EVENT_STA_STOP:
            Serial.println("Station stopped");
            break;
        case SYSTEM_EVENT_STA_AUTHMODE_CHANGE:
            Serial.println("Auth mode changed");
            break;
        case SYSTEM_EVENT_STA_LOST_IP:
            Serial.println("Lost IP");
            break;
        default:
            Serial.println("Other event");
            break;
    }
}


void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n\n======================================");
    Serial.println("ESP32 MQTT Servo Controller");
    Serial.println("Device ID: " + String(device_id));
    Serial.println("======================================\n");

    Serial.println("Initializing servo...");
    myServo.attach(servoPin);
    myServo.write(currentAngle);
    delay(500);
    Serial.println("Servo initialized to " + String(currentAngle) + "°");

    // Disable WiFi
    esp_wifi_set_ps(WIFI_PS_NONE);
    WiFi.setSleep(false);

    // NOTE: ESPC3 specific
    // Set lower power before connecting to WIFI
    WiFi.setTxPower(WIFI_POWER_8_5dBm);
    Serial.println("Set WIFI power to 8.5dBm for compatibility");

    // Scan WIFI network
    Serial.println("Scanning for WIFI networks...");
    scanWiFiNetworks();

    Serial.println("Connecting to WIFI...");
    connectToWiFi();

    // Setup MQTT
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nSetting up MQTT...");
        client.setServer(mqtt_server, mqtt_port);
        client.setCallback(onMqttMessage);
        client.setBufferSize(512);
    
        // Connect to MQTT broker
        connectToMQTT();
    
        Serial.println("\n========================================");
        Serial.println("System Ready!");
        Serial.println("Device ID: " + String(device_id));
        Serial.println("IP Address: " + WiFi.localIP().toString());
        Serial.println("MQTT Server: " + String(mqtt_server));
        Serial.println("Command Topic: " + commandTopic);
        Serial.println("Status Topic: " + statusTopic);
        Serial.println("========================================\n");
    
        // Send initial status
        sendStatusUpdate();
    } else {
        Serial.println("\n Failed to connect to WiFi. System halted.");
        Serial.println("Please check your configuration and restart.");
    
        // Halt
        while(true) {
            delay(1000);
        }
    }
}


void connectToWiFi() {
    // Disconnect from any previous connection
    WiFi.disconnect(true);
    delay(100);
    
    // Set WIFI mode
    WiFi.mode(WIFI_STA);
    delay(100);
    
    Serial.print("Connecting to: ");
    Serial.println(ssid);
    
    // Start connection
    WiFi.begin(ssid, password);
    
    // Wait for connection...
    int attempts = 0;
    const int maxAttempts = 60;
    int previousStatus = -1;
    
    while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts) {
        int currentStatus = WiFi.status();
        
        // Print status when it changes
        if (currentStatus != previousStatus) {
            Serial.println();
            Serial.print("Status: ");
            switch(currentStatus) {
                case WL_NO_SSID_AVAIL:
                    Serial.println("SSID not found");
                    break;
                case WL_CONNECT_FAILED:
                    Serial.println("Connection failed");
                    break;
                case WL_DISCONNECTED:
                    Serial.println("Disconnected (attempting connection...)");
                    break;
                default:
                    Serial.print("Code ");
                    Serial.println(currentStatus);
            }
            previousStatus = currentStatus;
        } else {
            Serial.print(".");
        }
        
        delay(500);
        attempts++;
        
        // Show progress every 5 seconds
        if (attempts % 10 == 0) {
            Serial.print(" (");
            Serial.print(attempts/2);
            Serial.print("s)");
        }
    }
    
    // Check if connected
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi Connected!");
        Serial.print("  IP Address: ");
        Serial.println(WiFi.localIP());
        Serial.print("  Signal: ");
        Serial.print(WiFi.RSSI());
        Serial.println(" dBm");
    } else {
        Serial.println("\nWiFi Connection Failed!");
        Serial.print("  Final status: ");
        Serial.println(WiFi.status());
        
        // Try with even lower power as last resort
        Serial.println("\nRetrying with 5dBm power...");
        WiFi.setTxPower(WIFI_POWER_5dBm);
        WiFi.disconnect();
        delay(100);
        WiFi.begin(ssid, password);
        
        attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 30) {
            delay(500);
            Serial.print(".");
            attempts++;
        }
        
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("\nConnected with reduced power!");
        }
    }
}

void scanWiFiNetworks() {
    Serial.println("Scanning for WiFi networks...");
    WiFi.mode(WIFI_STA);
    int n = WiFi.scanNetworks();
    
    if (n == 0) {
        Serial.println("No networks found");
    } else {
        Serial.print(n);
        Serial.println(" networks found:");
        
        // Look for our target SSID
        bool targetFound = false;
        
        for (int i = 0; i < n; ++i) {
            String currentSSID = WiFi.SSID(i);
            bool isTarget = (currentSSID == String(ssid));
            
            if (isTarget) {
                Serial.print("*");
                targetFound = true;
            } else {
                Serial.print(" ");
            }
            
            Serial.print(i + 1);
            Serial.print(": \"");
            Serial.print(currentSSID);
            Serial.print("\" (");
            Serial.print(WiFi.RSSI(i));
            Serial.print(" dBm, Ch:");
            Serial.print(WiFi.channel(i));
            Serial.print(") ");
            Serial.println((WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "Open" : "Encrypted");
        }
        
        if (targetFound) {
            Serial.println("\nTarget SSID found in scan");
        } else {
            Serial.println("\nTarget SSID NOT found in scan!");
            Serial.print("Looking for: \"");
            Serial.print(ssid);
            Serial.println("\"");
        }
    }
    Serial.println("");
}

void loop() {
    // Check WiFi connection first
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi disconnected! Attempting reconnection...");
        connectToWiFi();
        
        // If still not connected, skip this loop iteration
        if (WiFi.status() != WL_CONNECTED) {
            delay(1000);
            return;
        }
    }
    
    // Maintain MQTT connection
    if (!client.connected()) {
        connectToMQTT();
    }
    client.loop();
    
    // Handle servo movement
    handleServoMovement();
    
    // Send status updates every 5 seconds
    unsigned long now = millis();
    if (now - lastStatusTime > 5000) {
        sendStatusUpdate();
        lastStatusTime = now;
    }
}


void connectToMQTT() {
    int retries = 0;
    const int maxRetries = 3;
    
    while (!client.connected() && retries < maxRetries) {
        Serial.print("Attempting MQTT connection (");
        Serial.print(retries + 1);
        Serial.print("/");
        Serial.print(maxRetries);
        Serial.print(")...");
        
        if (client.connect(device_id)) {
            Serial.println(" connected!");
            
            // Subscribe to command topic
            if (client.subscribe(commandTopic.c_str())) {
                Serial.println("Subscribed to: " + commandTopic);
            } else {
                Serial.println("Failed to subscribe to command topic");
            }
            
            // Send discovery message
            sendDiscoveryMessage();
            
            // Send initial status
            sendStatusUpdate();
            
            return; // Successfully connected
            
        } else {
            Serial.print(" failed, rc=");
            Serial.print(client.state());
            Serial.println();
            
            // Detailed error messages
            switch(client.state()) {
                case -4: Serial.println("  MQTT_CONNECTION_TIMEOUT"); break;
                case -3: Serial.println("  MQTT_CONNECTION_LOST"); break;
                case -2: Serial.println("  MQTT_CONNECT_FAILED"); break;
                case -1: Serial.println("  MQTT_DISCONNECTED"); break;
                case 1: Serial.println("  MQTT_CONNECT_BAD_PROTOCOL"); break;
                case 2: Serial.println("  MQTT_CONNECT_BAD_CLIENT_ID"); break;
                case 3: Serial.println("  MQTT_CONNECT_UNAVAILABLE"); break;
                case 4: Serial.println("  MQTT_CONNECT_BAD_CREDENTIALS"); break;
                case 5: Serial.println("  MQTT_CONNECT_UNAUTHORIZED"); break;
            }
            
            retries++;
            if (retries < maxRetries) {
                Serial.println("  Retrying in 5 seconds...");
                delay(5000);
            }
        }
    }
    
    if (!client.connected()) {
        Serial.println("Failed to connect to MQTT after " + String(maxRetries) + " attempts");
        Serial.println("Will retry on next loop iteration");
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
    } else {
        Serial.println("Unknown command type: " + commandType);
    }
}

void handleMoveCommand(DynamicJsonDocument& doc) {
    int angle = doc["angle"].as<int>();
    int speed = 100; // Default speed
    if (!doc["speed"].isNull()) {
        speed = doc["speed"].as<int>();
    }
    
    if (angle >= 0 && angle <= 180) {
        targetAngle = angle;
        moveSpeed = speed;
        isMoving = true;
        // sweepMode removed, no longer needed
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
    // Check if we're connected before trying to publish
    if (!client.connected()) {
        Serial.println("Warning: Cannot send status - MQTT not connected");
        return;
    }
    
    DynamicJsonDocument doc(512);
    doc["device_id"] = device_id;
    doc["angle"] = currentAngle;
    doc["target_angle"] = targetAngle;
    doc["moving"] = isMoving;
    doc["timestamp"] = millis();
    doc["wifi_rssi"] = WiFi.RSSI();
    doc["free_heap"] = ESP.getFreeHeap(); // Add memory info
    
    String statusMessage;
    serializeJson(doc, statusMessage);
    
    // Check if publish was successful
    if (client.publish(statusTopic.c_str(), statusMessage.c_str())) {
        Serial.println("Status sent: " + statusMessage);
    } else {
        Serial.println("Failed to publish status");
    }
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