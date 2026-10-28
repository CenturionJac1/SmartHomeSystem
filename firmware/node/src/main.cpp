
#include <WiFi.h>
#include <WebServer.h>

#include "FSM.h"
#include "Relay_Control.h"
#include "Timer.h"
#include "Sensor_Read.h"

const char* ssid = "YOUR_WIFI_NAME";
const char* password = "YOUR_WIFI_PASSWORD";

const int nodeID = 1;

// Change these for each node as needed.
IPAddress nodeIP(192, 168, 1, 101);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

WebServer server(80);

// Actual FSM instance used by the node.
FSM fsm;

unsigned long lastReconnectAttempt = 0;
const unsigned long reconnectInterval = 5000;

const char* getLightState() {
    switch (fsm.currentState) {
        case STATE_ON:
            return "on";
        case STATE_OFF:
            return "off";
        case STATE_ON_MANUAL:
            return "on_manual";
        case STATE_OFF_MANUAL:
            return "off_manual";
        case STATE_ERROR:
            return "error";
        default:
            return "unknown";
    }
}

// Handle a manual light command from the central controller.
void controlLight(int lightID, bool turnOn) {
    Serial.print("Light ");
    Serial.print(lightID);

    // Prevent an automatic timeout from interfering with a
    // manual command.
    Timer_Stop();

    if (turnOn) {
        Serial.println(" ON (manual)");

        Turn_On();
        fsm.currentState = STATE_ON_MANUAL;
    } else {
        Serial.println(" OFF (manual)");

        Turn_Off();
        fsm.currentState = STATE_OFF_MANUAL;
    }
}

void enableAutomaticMode() {
    Serial.println("Returning to automatic mode");

    Turn_Off();
    Timer_Stop();
    fsm.currentState = STATE_OFF;
}

// POST /light
// Expected form body: light=1&state=on
// Supported states: on, off, auto
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
    state.trim();
    state.toLowerCase();

    if (lightID <= 0) {
        server.send(
            400,
            "text/plain",
            "Invalid light ID"
        );
        return;
    }

    if (state == "on") {
        controlLight(lightID, true);
    }
    else if (state == "off") {
        controlLight(lightID, false);
    }
    else if (state == "auto") {
        enableAutomaticMode();
    }
    else {
        server.send(
            400,
            "text/plain",
            "Invalid state. Use on, off, or auto."
        );
        return;
    }

    server.send(200, "text/plain", "OK");
}

// GET /ping
// Used by the central controller to check node connectivity.
void handlePing() {
    String message = "nodeID: ";
    message += String(nodeID);
    message += " is ";
    message += (WiFi.status() == WL_CONNECTED)
                   ? "connected"
                   : "disconnected";
    message += "; light state: ";
    message += getLightState();

    server.send(200, "text/plain", message);
}

void setup() {
    Serial.begin(115200);

    // Initialize the FSM to start in automatic OFF mode.
    initFSM(&fsm);

    WiFi.mode(WIFI_STA);

    if (!WiFi.config(nodeIP, gateway, subnet)) {
        Serial.println("Failed to configure static IP");
    }

    WiFi.begin(ssid, password);
    Serial.println("Connecting to WiFi...");

    // Register HTTP endpoints.
    server.on("/ping", HTTP_GET, handlePing);
    server.on("/light", HTTP_POST, handleLight);

    server.begin();
    Serial.println("HTTP server started");
    Serial.print("Node IP: ");
    Serial.println(nodeIP);
}

void loop() {
    // Keep attempting to reconnect to Wi-Fi.
    if (WiFi.status() != WL_CONNECTED) {
        if (millis() - lastReconnectAttempt >= reconnectInterval) {
            lastReconnectAttempt = millis();

            Serial.println("WiFi disconnected. Reconnecting...");

            WiFi.disconnect();
            WiFi.begin(ssid, password);
        }
    }

    // Process incoming commands from the central controller.
    server.handleClient();

    // Run the automatic FSM continuously. Manual states are
    // intentionally left unchanged by stateFSM().
    stateFSM(&fsm);
}
