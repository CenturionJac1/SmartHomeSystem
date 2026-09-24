#include <WiFi.h>
#include <HTTPClient.h>

const char* ssid = "YOUR_WIFI_NAME";
const char* password = "YOUR_WIFI_PASSWORD";

const int nodeID = 1;

unsigned long lastReconnectAttempt = 0;
unsigned long lastPing = 0;

const unsigned long reconnectInterval = 5000;
const unsigned long pingInterval = 5000;

// IP addresses of nodes, filled with fake values for now
const char* nodeIPs[] = {
    "192.168.1.101",
    "192.168.1.102",
    "192.168.1.103"
};

const int numNodes = sizeof(nodeIPs) / sizeof(nodeIPs[0]);


void setup() {

    Serial.begin(115200);

    WiFi.begin(ssid, password);

    Serial.println("Connecting to WiFi...");
}


void loop() {
    if (WiFi.status() != WL_CONNECTED) {

        if (millis() - lastReconnectAttempt >= reconnectInterval) {

            lastReconnectAttempt = millis();

            Serial.println("WiFi disconnected. Reconnecting...");

            WiFi.disconnect();
            WiFi.begin(ssid, password);
        }

        return;
    }
    if (millis() - lastPing >= pingInterval) {

        lastPing = millis();

        for (int i = 0; i < numNodes; i++) {

            HTTPClient http;

            String url = "http://" + String(nodeIPs[i]) + "/ping";

            Serial.print("Pinging node ");
            Serial.print(i + 1);
            Serial.print(" at ");
            Serial.println(url);

            http.begin(url);

            int httpCode = http.GET();

            if (httpCode > 0) {

                Serial.print("HTTP response: ");
                Serial.println(httpCode);

                String response = http.getString();

                Serial.print("Node response: ");
                Serial.println(response);

            } 
            else {

                Serial.print("HTTP request failed: ");
                Serial.println(http.errorToString(httpCode));
            }

            http.end();
        }
    }

    TODO: Add information processing here
}