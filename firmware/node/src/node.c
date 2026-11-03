
#include "Node.h"

#include "Relay_Control.h"
#include "Timer.h"
#include "Sensor_Read.h"

#include "esp_log.h"
#include "esp_http_server.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

static const char *TAG = "NODE";

// Change this for each physical node.
#define NODE_ID 1

#define REQUEST_BODY_SIZE 128

static FSM *nodeFSM = NULL;
static SemaphoreHandle_t fsmMutex = NULL;
static httpd_handle_t server = NULL;


static const char *getLightState(void) {

    switch (nodeFSM->currentState) {

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

esp_err_t Node_Init(FSM *fsm) {

    if (fsm == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    nodeFSM = fsm;

    fsmMutex = xSemaphoreCreateMutex();

    if (fsmMutex == NULL) {
        ESP_LOGE(TAG, "Failed to create FSM mutex");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Node initialized. Node ID: %d", NODE_ID);

    return ESP_OK;
}


void Node_RunFSM(void) {

    if (nodeFSM == NULL || fsmMutex == NULL) {
        return;
    }

    xSemaphoreTake(fsmMutex, portMAX_DELAY);

    stateFSM(nodeFSM);

    xSemaphoreGive(fsmMutex);
}


static void copyLightState(char *output, size_t outputSize) {

    if (outputSize == 0) {
        return;
    }

    if (nodeFSM == NULL || fsmMutex == NULL) {
        snprintf(output, outputSize, "unknown");
        return;
    }

    xSemaphoreTake(fsmMutex, portMAX_DELAY);

    snprintf(
        output,
        outputSize,
        "%s",
        getLightState()
    );

    xSemaphoreGive(fsmMutex);
}

static bool getFormValue(
    const char *body,
    const char *key,
    char *output,
    size_t outputSize
) {

    size_t keyLength = strlen(key);
    const char *position = body;

    while (*position != '\0') {

        const char *end = strchr(position, '&');

        if (end == NULL) {
            end = position + strlen(position);
        }

        const char *equal = memchr(
            position,
            '=',
            end - position
        );

        if (equal != NULL &&
            (size_t)(equal - position) == keyLength &&
            strncmp(position, key, keyLength) == 0) {

            size_t valueLength = end - equal - 1;

            if (valueLength >= outputSize) {
                return false;
            }

            memcpy(output, equal + 1, valueLength);
            output[valueLength] = '\0';

            return true;
        }

        if (*end == '\0') {
            break;
        }

        position = end + 1;
    }

    return false;
}

static esp_err_t handlePing(httpd_req_t *req) {

    char lightState[16];
    char response[128];

    copyLightState(lightState, sizeof(lightState));

    snprintf(
        response,
        sizeof(response),
        "nodeID: %d is connected; light state: %s",
        NODE_ID,
        lightState
    );

    httpd_resp_set_type(req, "text/plain");

    return httpd_resp_sendstr(req, response);
}
// Expected form body:
// light=1&state=on
// light=1&state=off
// light=1&state=auto

static esp_err_t handleLight(httpd_req_t *req) {

    char body[REQUEST_BODY_SIZE];
    char lightValue[16];
    char state[16];

    // Validate request size.
    if (req->content_len <= 0 ||
        req->content_len >= sizeof(body)) {

        httpd_resp_send_err(
            req,
            HTTPD_400_BAD_REQUEST,
            "Invalid request body"
        );

        return ESP_OK;
    }

    // Read the entire HTTP request body.
    size_t totalReceived = 0;

    while (totalReceived < req->content_len) {

        int received = httpd_req_recv(
            req,
            body + totalReceived,
            req->content_len - totalReceived
        );

        if (received <= 0) {

            httpd_resp_send_err(
                req,
                HTTPD_400_BAD_REQUEST,
                "Failed to receive request body"
            );

            return ESP_OK;
        }

        totalReceived += received;
    }

    body[totalReceived] = '\0';

    // Extract the light ID and requested state.
    if (!getFormValue(
            body,
            "light",
            lightValue,
            sizeof(lightValue)
        ) ||
        !getFormValue(
            body,
            "state",
            state,
            sizeof(state)
        )) {

        httpd_resp_send_err(
            req,
            HTTPD_400_BAD_REQUEST,
            "Missing light or state"
        );

        return ESP_OK;
    }

    // Convert state to lowercase.
    for (int i = 0; state[i] != '\0'; i++) {
        state[i] = (char)tolower((unsigned char)state[i]);
    }

    // Validate light ID.
    char *endPointer;

    long lightID = strtol(lightValue, &endPointer, 10);

    if (endPointer == lightValue ||
        *endPointer != '\0' ||
        lightID <= 0) {

        httpd_resp_send_err(
            req,
            HTTPD_400_BAD_REQUEST,
            "Invalid light ID"
        );

        return ESP_OK;
    }

    // Validate requested state.
    if (strcmp(state, "on") != 0 &&
        strcmp(state, "off") != 0 &&
        strcmp(state, "auto") != 0) {

        httpd_resp_send_err(
            req,
            HTTPD_400_BAD_REQUEST,
            "State must be on, off, or auto"
        );

        return ESP_OK;
    }

    ESP_LOGI(
        TAG,
        "Received command: Light %ld, state %s",
        lightID,
        state
    );

    xSemaphoreTake(fsmMutex, portMAX_DELAY);

    if (strcmp(state, "on") == 0) {

        Timer_Stop();

        Turn_On();

        nodeFSM->currentState = STATE_ON_MANUAL;

        ESP_LOGI(TAG, "Light manually turned ON");
    }

    else if (strcmp(state, "off") == 0) {

        Timer_Stop();

        Turn_Off();

        nodeFSM->currentState = STATE_OFF_MANUAL;

        ESP_LOGI(TAG, "Light manually turned OFF");
    }

    else if (strcmp(state, "auto") == 0) {

        Timer_Stop();

        Turn_Off();

        nodeFSM->currentState = STATE_OFF;

        ESP_LOGI(TAG, "Automatic mode enabled");
    }

    xSemaphoreGive(fsmMutex);

    httpd_resp_set_type(req, "text/plain");

    return httpd_resp_sendstr(req, "OK");
}

esp_err_t Node_StartServer(void) {

    // Avoid starting multiple server instances.
    if (server != NULL) {
        return ESP_OK;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    config.server_port = 80;

    ESP_LOGI(TAG, "Starting node HTTP server");

    esp_err_t ret = httpd_start(&server, &config);

    if (ret != ESP_OK) {

        ESP_LOGE(TAG, "Failed to start HTTP server");

        server = NULL;

        return ret;
    }

    // GET /ping
    httpd_uri_t pingURI = {
        .uri = "/ping",
        .method = HTTP_GET,
        .handler = handlePing,
        .user_ctx = NULL
    };

    // POST /light
    httpd_uri_t lightURI = {
        .uri = "/light",
        .method = HTTP_POST,
        .handler = handleLight,
        .user_ctx = NULL
    };

    ret = httpd_register_uri_handler(server, &pingURI);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register /ping");
        Node_StopServer();
        return ret;
    }

    ret = httpd_register_uri_handler(server, &lightURI);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register /light");
        Node_StopServer();
        return ret;
    }

    ESP_LOGI(TAG, "Node HTTP server started");
    ESP_LOGI(TAG, "Endpoints: GET /ping, POST /light");

    return ESP_OK;
}

void Node_StopServer(void) {

    if (server != NULL) {

        httpd_stop(server);

        server = NULL;

        ESP_LOGI(TAG, "Node HTTP server stopped");
    }
}

void Node_ConnectHandler(
    void *arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void *event_data
) {

    ESP_LOGI(TAG, "Wi-Fi connected and IP acquired");

    esp_err_t ret = Node_StartServer();

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Could not start HTTP server");
    }
}

void Node_DisconnectHandler(
    void *arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void *event_data
) {

    ESP_LOGW(TAG, "Wi-Fi disconnected");

    Node_StopServer();
}
