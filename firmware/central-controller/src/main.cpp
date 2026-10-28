
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

const char* ssid = "YOUR_WIFI_NAME";
const char* password = "YOUR_WIFI_PASSWORD";

// Central controller's own static IP.
IPAddress controllerIP(192, 168, 1, 100);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

WebServer server(80);

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

const Node* findNode(int nodeID) {
    for (int i = 0; i < numNodes; i++) {
        if (nodes[i].nodeID == nodeID) {
            return &nodes[i];
        }
    }
    return nullptr;
}

void addCorsHeaders() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader(
        "Access-Control-Allow-Methods",
        "GET, POST, OPTIONS"
    );
    server.sendHeader(
        "Access-Control-Allow-Headers",
        "Content-Type"
    );
}

void sendJson(int code, JsonDocument& doc) {
    String output;
    serializeJson(doc, output);

    addCorsHeaders();
    server.send(code, "application/json", output);
}

void handleOptions() {
    addCorsHeaders();
    server.send(204);
}

bool getNodeStatus(
    const Node& node,
    bool& online,
    String& message,
    String& lightState
) {
    HTTPClient http;

    String url = "http://" + String(node.IP) + "/ping";

    http.setTimeout(1500);

    if (!http.begin(url)) {
        online = false;
        message = "Could not start HTTP request";
        lightState = "unknown";
        return false;
    }

    int code = http.GET();

    online = (code == 200);

    if (online) {
        message = http.getString();

        int pos = message.indexOf("light state: ");

        if (pos >= 0) {
            lightState = message.substring(pos + 13);
            lightState.trim();
        } else {
            lightState = "unknown";
        }
    } else {
        message = "Node did not respond";
        lightState = "unknown";
    }

    http.end();
    return online;
}

// GET /ping
// Simple central-controller heartbeat.
void handlePing() {
    JsonDocument doc;

    doc["controller"] = "online";
    doc["wifiConnected"] = WiFi.status() == WL_CONNECTED;
    doc["ip"] = WiFi.localIP().toString();

    sendJson(200, doc);
}

// GET /status
// GET /status?node=1
void handleStatus() {
    int requestedNode = -1;

    if (server.hasArg("node")) {
        requestedNode = server.arg("node").toInt();

        if (findNode(requestedNode) == nullptr) {
            JsonDocument error;
            error["error"] = "Unknown node ID";
            sendJson(404, error);
            return;
        }
    }

    JsonDocument doc;
    JsonArray results = doc["nodes"].to<JsonArray>();

    bool allOnline = true;

    for (int i = 0; i < numNodes; i++) {
        const Node& node = nodes[i];

        if (requestedNode != -1 &&
            node.nodeID != requestedNode) {
            continue;
        }

        bool online;
        String message;
        String lightState;

        getNodeStatus(node, online, message, lightState);

        JsonObject item = results.add<JsonObject>();

        item["node"] = node.nodeID;
        item["online"] = online;
        item["lightState"] = lightState;
        item["message"] = message;

        if (!online) {
            allOnline = false;
        }
    }

    doc["allOnline"] = allOnline;
    sendJson(200, doc);
}

// Node firmware accepts POST /light with form-encoded fields.
bool sendNodeCommand(
    const Node& node,
    int lightID,
    const String& state,
    String& response
) {
    HTTPClient http;

    String url = "http://" + String(node.IP) + "/light";

    http.setTimeout(2000);

    if (!http.begin(url)) {
        response = "Could not start HTTP request";
        return false;
    }

    http.addHeader(
        "Content-Type",
        "application/x-www-form-urlencoded"
    );

    String body = "light=" + String(lightID)
                + "&state=" + state;

    int code = http.POST(body);

    if (code > 0) {
        response = http.getString();
    } else {
        response = "Node connection failed";
    }

    http.end();

    return code == 200;
}

// POST /control
// JSON: {"node":1,"light":1,"state":"on"}
void handleControl() {
    JsonDocument request;

    DeserializationError error =
        deserializeJson(request, server.arg("plain"));

    if (error) {
        JsonDocument doc;
        doc["error"] = "Invalid JSON request body";
        sendJson(400, doc);
        return;
    }

    if (!request["node"].is<int>() ||
        !request["light"].is<int>() ||
        !request["state"].is<const char*>()) {
        JsonDocument doc;
        doc["error"] =
            "Required fields: node, light, state";
        sendJson(400, doc);
        return;
    }

    int nodeID = request["node"].as<int>();
    int lightID = request["light"].as<int>();
    String state = request["state"].as<String>();
    state.toLowerCase();

    if (nodeID <= 0 || lightID <= 0) {
        JsonDocument doc;
        doc["error"] = "Node and light IDs must be positive";
        sendJson(400, doc);
        return;
    }

    if (state != "on" &&
        state != "off" &&
        state != "auto") {
        JsonDocument doc;
        doc["error"] = "State must be on, off, or auto";
        sendJson(400, doc);
        return;
    }

    const Node* node = findNode(nodeID);

    if (node == nullptr) {
        JsonDocument doc;
        doc["error"] = "Unknown node ID";
        sendJson(404, doc);
        return;
    }

    String response;

    bool success = sendNodeCommand(
        *node,
        lightID,
        state,
        response
    );

    JsonDocument doc;
    doc["success"] = success;
    doc["node"] = nodeID;
    doc["light"] = lightID;
    doc["requestedState"] = state;
    doc["message"] = response;

    sendJson(success ? 200 : 502, doc);
}

void setup() {
    Serial.begin(115200);

    WiFi.mode(WIFI_STA);

    if (!WiFi.config(controllerIP, gateway, subnet)) {
        Serial.println("Failed to configure static IP");
    }

    WiFi.begin(ssid, password);
    Serial.println("Connecting to WiFi...");

    server.on("/ping", HTTP_GET, handlePing);
    server.on("/status", HTTP_GET, handleStatus);
    server.on("/control", HTTP_POST, handleControl);

    server.on("/ping", HTTP_OPTIONS, handleOptions);
    server.on("/status", HTTP_OPTIONS, handleOptions);
    server.on("/control", HTTP_OPTIONS, handleOptions);

    server.begin();
    Serial.println("Central controller HTTP server started");
}

void loop() {
    if (WiFi.status() != WL_CONNECTED) {
        if (millis() - lastReconnectAttempt >=
            reconnectInterval) {
            lastReconnectAttempt = millis();

            Serial.println("WiFi disconnected. Reconnecting...");

            WiFi.disconnect();
            WiFi.begin(ssid, password);
        }
    }

    server.handleClient();
}
