#include <WiFi.h>
#include <HTTPClient.h>

const char* ssid = "YOUR_WIFI_NAME";
const char* password = "YOUR_WIFI_PASSWORD";

struct Node {
    int nodeID;
    const char* IP;
};

Node nodes[] = {
    {1, "192.168.1.101"},
    {2, "192.168.1.102"},
    {3, "192.168.1.103"}
};

const int numNodes = sizeof(nodes) / sizeof(nodes[0]);
unsigned long lastReconnectAttempt = 0;
const unsigned long reconnectInterval = 5000;

bool pingNode(int nodeID) {
    const char* nodeIP = nullptr;
    for (int i = 0; i < numNodes; i++) {

        if (nodes[i].nodeID == nodeID) {
            nodeIP = nodes[i].IP;
            break;
        }
    }
    if (nodeIP == nullptr) {
        Serial.println("Node not found");
        return false;
    }
    HTTPClient http;
    String url = "http://" + String(nodeIP) + "/ping";
    http.begin(url);
    int httpCode = http.GET();
    if (httpCode == 200) {
        String response = http.getString();
        Serial.print("Node ");
        Serial.print(nodeID);
        Serial.print(" online: ");
        Serial.println(response);
        http.end();
        return true;
    }
    Serial.print("Node ");
    Serial.print(nodeID);
    Serial.println(" did not respond.");
    http.end();
    return false;
}

bool sendLightCommand(int nodeID, int lightID, bool turnOn) {
    const char* nodeIP = nullptr;
    for (int i = 0; i < numNodes; i++) {

        if (nodes[i].nodeID == nodeID) {
            nodeIP = nodes[i].IP;
            break;
        }
    }
    if (nodeIP == nullptr) {
        Serial.println("Node not found");
        return false;
    }
    HTTPClient http;
    String state;
    if (turnOn) {
        state = "on";
    }
    else {
        state = "off";
    }
    String url = "http://" + String(nodeIP)
               + "/light?light=" + String(lightID)
               + "&state=" + state;
    http.begin(url);
    int httpCode = http.POST("");
    if (httpCode == 200) {

        Serial.print("Light command sent to node ");
        Serial.println(nodeID);

        http.end();

        return true;
    }
    Serial.print("Failed to send light command to node ");
    Serial.println(nodeID);
    http.end();
    return false;
}

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

}