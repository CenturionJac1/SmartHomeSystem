#include <WiFi.h>
#include <WebServer.h>

const char* ssid = "YOUR_WIFI_NAME";
const char* password = "YOUR_WIFI_PASSWORD";

const int nodeID = 1;

// Change for each node whenever we figure out what IP addresses are safe to use.
IPAddress nodeIP(192, 168, 1, 101);

IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

WebServer server(80);

unsigned long lastReconnectAttempt = 0;
void handlePing() {

    Serial.println("Received: ping");

    server.send(200, "text/plain", "nodeID: " + String(nodeID));
}
void setup() {
    Serial.begin(115200);
    if (!WiFi.config(nodeIP, gateway, subnet)) {
        Serial.println("Failed to configure static IP");
    }
    WiFi.begin(ssid, password);
    Serial.println("Connecting to WiFi...");
    server.on("/ping", handlePing);
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
    // add info processing here
}