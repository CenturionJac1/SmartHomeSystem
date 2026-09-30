#include <WiFi.h>
#include <WebServer.h>
#include "FSM.h"
#include "Relay_Control.h"
#include "Timer.h"
#include "Sensor_Read.h"

const char* ssid = "YOUR_WIFI_NAME";
const char* password = "YOUR_WIFI_PASSWORD";
const int nodeID = 1;

// Change this for each node
IPAddress nodeIP(192, 168, 1, 101);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
WebServer server(80);

unsigned long lastReconnectAttempt = 0;

void controlLight(int lightID, bool turnOn) {

    Serial.print("Light ");
    Serial.print(lightID);

    if (turnOn) {
        Serial.println(" ON");
        stateFSM(STATE_ON);
    }
    else {
        Serial.println(" OFF");
        stateFSM(STATE_OFF);
    }
}

void handleLight() {
    if (!server.hasArg("light") || !server.hasArg("state")) {
        server.send(
            400,
            "text/plain",
            "Missing light or state"
        );
        return;
    }
    int lightID = server.arg("light").toInt();
    String state = server.arg("state");
    bool turnOn;

    if (state == "on") {
        turnOn = true;
    }
    else if (state == "off") {
        turnOn = false;
    }
    else {

        server.send(
            400,
            "text/plain",
            "Invalid state"
        );

        return;
    }
    controlLight(lightID, turnOn);
    server.send(
        200,
        "text/plain",
        "OK"
    );
}
void handlePing() {

    Serial.println("Received: ping");

    server.send(
        200,
        "text/plain",
        "nodeID: " + String(nodeID) + "is " + String(WiFi.status() == WL_CONNECTED ? "connected" : "disconnected") + " and the light is " + String(stateFSM() == STATE_ON ? "on" : "off")
    );
}
void setup() {
    Serial.begin(115200);
    if (!WiFi.config(nodeIP, gateway, subnet)) {
        Serial.println("Failed to configure static IP");
    }
    WiFi.begin(ssid, password);
    Serial.println("Connecting to WiFi...");
    server.on("/ping", HTTP_GET, handlePing);
    server.on("/light", HTTP_POST, handleLight);
    server.begin();
    Serial.println("HTTP server started");
}

void loop() {

    if (WiFi.status() != WL_CONNECTED) {

        if (millis() - lastReconnectAttempt >= 5000) {

            lastReconnectAttempt = millis();

            Serial.println("WiFi disconnected. Reconnecting...");

            WiFi.disconnect();
            WiFi.begin(ssid, password);
        }
    }
    server.handleClient();
}