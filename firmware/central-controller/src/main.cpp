#include <WiFi.h>
#include <WiFiUdp.h>

const char* ssid = "YOUR_WIFI_NAME";
const char* password = "YOUR_WIFI_PASSWORD";

const int nodeID = 1;
const int UDP_PORT = 4210;

WiFiUDP udp;

unsigned long lastReconnectAttempt = 0;

void setup() {
    Serial.begin(115200);

    WiFi.begin(ssid, password);

    Serial.println("Connecting to WiFi...");

    udp.begin(UDP_PORT);
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

    int packetSize = udp.parsePacket();

    if (packetSize > 0) {

        char incoming[256];

        int length = udp.read(incoming, sizeof(incoming) - 1);

        if (length > 0) {
            incoming[length] = '\0';

            Serial.print("Received: ");
            Serial.println(incoming);
        }
    }

    //add info processing here

}