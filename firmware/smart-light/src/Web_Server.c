#include "Web_Server.h"
#include "FSM.h"
#include "Relay_Control.h"
#include "Timer.h"
#include "Sensor_Read.h"

#include "esp_http_server.h"
#include "esp_event.h"
#include "esp_log.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "web_server";
static FSM *fsm_ptr = NULL;

// dashboard.html is embedded by CMake.
extern const unsigned char dashboard_start[]
    asm("_binary_dashboard_html_start");

extern const unsigned char dashboard_end[]
    asm("_binary_dashboard_html_end");

static esp_err_t send_json(
    httpd_req_t *req,
    const char *status,
    const char *body
) {
    httpd_resp_set_status(req, status);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");

    return httpd_resp_send(req, body, HTTPD_RESP_USE_STRLEN);
}

// GET / : serve the dashboard from ESP32.
static esp_err_t dashboard_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");

    // EMBED_TXTFILES adds a terminating null byte.
    size_t length = dashboard_end - dashboard_start - 1;

    return httpd_resp_send(
        req,
        (const char *)dashboard_start,
        length
    );
}

// GET /status : report firmware state and current PIR input.
static esp_err_t status_handler(httpd_req_t *req) {
    if (fsm_ptr == NULL) {
        return send_json(
            req,
            "503 Service Unavailable",
            "{\"error\":\"Controller not initialized\"}"
        );
    }

    State state = fsm_ptr->currentState;
    const char *mode = "auto";
    const char *output = "UNKNOWN";

    switch (state) {
        case STATE_ON:
            output = "ON";
            break;

        case STATE_OFF:
            output = "OFF";
            break;

        case STATE_ON_MANUAL:
            mode = "on";
            output = "ON";
            break;

        case STATE_OFF_MANUAL:
            mode = "off";
            output = "OFF";
            break;

        case STATE_ERROR:
            mode = "error";
            break;

        default:
            mode = "unknown";
            break;
    }

    bool motion = Sensor_IsMotionDetected();
    bool sensor_error = Sensor_Error();

    char response[192];

    snprintf(
        response,
        sizeof(response),
        "{\"mode\":\"%s\",\"output\":\"%s\","
        "\"motion\":%s,\"sensorError\":%s}",
        mode,
        output,
        motion ? "true" : "false",
        sensor_error ? "true" : "false"
    );

    return send_json(req, "200 OK", response);
}

// POST /control : accept cmd=on, cmd=off, or cmd=auto.
static esp_err_t control_handler(httpd_req_t *req) {
    char body[64];
    size_t received = 0;

    if (req->content_len == 0 ||
        req->content_len >= sizeof(body)) {
        send_json(
            req,
            "400 Bad Request",
            "{\"error\":\"Invalid request length\"}"
        );
        return ESP_FAIL;
    }

    while (received < req->content_len) {
        int ret = httpd_req_recv(
            req,
            body + received,
            req->content_len - received
        );

        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            send_json(
                req,
                "408 Request Timeout",
                "{\"error\":\"Request timed out\"}"
            );
            return ESP_FAIL;
        }

        if (ret <= 0) {
            return ESP_FAIL;
        }

        received += (size_t)ret;
    }

    body[received] = '\0';

    if (fsm_ptr == NULL) {
        return send_json(
            req,
            "503 Service Unavailable",
            "{\"error\":\"Controller not initialized\"}"
        );
    }

    // Reject embedded nulls as well as unknown commands.
    if (strlen(body) != received) {
        return send_json(
            req,
            "400 Bad Request",
            "{\"error\":\"Invalid command\"}"
        );
    }

    if (strcmp(body, "cmd=on") == 0) {
        fsm_ptr->currentState = STATE_ON_MANUAL;
        Timer_Stop();
        Turn_On();

    } else if (strcmp(body, "cmd=off") == 0) {
        fsm_ptr->currentState = STATE_OFF_MANUAL;
        Timer_Stop();
        Turn_Off();

    } else if (strcmp(body, "cmd=auto") == 0) {
        fsm_ptr->currentState = STATE_OFF_MANUAL;
        Timer_Stop();
        Turn_Off();
        fsm_ptr->currentState = STATE_OFF;

    } else {
        return send_json(
            req,
            "400 Bad Request",
            "{\"error\":\"Unknown command\"}"
        );
    }

    ESP_LOGI(TAG, "Command accepted: %s", body);

    return send_json(
        req,
        "200 OK",
        "{\"success\":true}"
    );
}

static const httpd_uri_t dashboard_uri = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = dashboard_handler
};

static const httpd_uri_t status_uri = {
    .uri = "/status",
    .method = HTTP_GET,
    .handler = status_handler
};

static const httpd_uri_t control_uri = {
    .uri = "/control",
    .method = HTTP_POST,
    .handler = control_handler
};

httpd_handle_t start_webserver(void) {
    if (fsm_ptr == NULL) {
        ESP_LOGE(TAG, "Initialize FSM reference before starting server");
        return NULL;
    }

    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;

    esp_err_t ret = httpd_start(&server, &config);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "HTTP server failed to start");
        return NULL;
    }

    ret = httpd_register_uri_handler(server, &dashboard_uri);

    if (ret == ESP_OK) {
        ret = httpd_register_uri_handler(server, &status_uri);
    }

    if (ret == ESP_OK) {
        ret = httpd_register_uri_handler(server, &control_uri);
    }

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register HTTP handlers");
        httpd_stop(server);
        return NULL;
    }

    ESP_LOGI(TAG, "Dashboard started on port %u",
             (unsigned)config.server_port);

    return server;
}

esp_err_t stop_webserver(httpd_handle_t server) {
    return httpd_stop(server);
}

void connect_handler(
    void *arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void *event_data
) {
    httpd_handle_t *server = (httpd_handle_t *)arg;

    if (*server == NULL) {
        *server = start_webserver();
    }
}

void disconnect_handler(
    void *arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void *event_data
) {
    httpd_handle_t *server = (httpd_handle_t *)arg;

    if (*server != NULL) {
        if (stop_webserver(*server) == ESP_OK) {
            *server = NULL;
        }
    }
}

void WebServer_Init(FSM *fsm) {
    fsm_ptr = fsm;
}