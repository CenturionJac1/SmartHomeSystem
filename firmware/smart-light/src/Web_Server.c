/*
    The code is based on the HTTP Server example from the ESP-IDF repository
    Source: https://github.com/espressif/esp-idf/blob/master/examples/protocols/http_server/simple/main/main.c
*/


#include "Web_Server.h"
#include "FSM.h"
#include "Relay_Control.h"
#include "Timer.h"
#include <esp_log.h>
#include <esp_http_server.h>
#include <nvs_flash.h>
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "protocol_examples_common.h"
#include <string.h>
#include <stdlib.h>
#include "esp_check.h" 
#include "esp_rom_sys.h"

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

static const char *TAG = "web_server";  // Log tag for this file
static FSM *fsm_ptr = NULL;  // Pointer to the shared FSM instance

// HTTP GET handler
static esp_err_t get_handler(httpd_req_t *req) {
    char *buf;
    size_t buf_len;
    //  Get header value string length and allocate memory for length + 1
    buf_len = httpd_req_get_hdr_value_len(req, "Host") + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        ESP_RETURN_ON_FALSE(buf, ESP_ERR_NO_MEM, TAG, "buffer alloc failed");
        if (httpd_req_get_hdr_value_str(req, "Host", buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found header => Host: %s", buf);
        }
        free(buf);
    }

    // Inline HTML dashboard with three control buttons
    const char *resp_str =  "<!DOCTYPE html><html><body>"
                            "<h2>Smart Light Control</h2>"
                            "<form action='/control' method='POST'>"
                            "<button name='cmd' value='on'>ON</button>"
                            "<button name='cmd' value='off'>OFF</button>"
                            "<button name='cmd' value='auto'>AUTO</button>"
                            "</form>"
                            "</body></html>";

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// URI registration for the dashboard (GET /)
static const httpd_uri_t dashboard = {
    .uri     = "/",
    .method  = HTTP_GET,
    .handler = get_handler,
    .user_ctx = NULL
};

// HTTP POST handler
static esp_err_t post_handler(httpd_req_t *req) {
    char buf[100];
    size_t received = 0;

    // Leave room for the null terminator.
    if (req->content_len == 0 ||
        req->content_len >= sizeof(buf)) {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_sendstr(req, "Invalid command length");
        return ESP_FAIL;
    }

    // Collect the entire request body.
    while (received < req->content_len) {
        int ret = httpd_req_recv(
            req,
            buf + received,
            req->content_len - received
        );

        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_err(
                req,
                HTTPD_408_REQ_TIMEOUT,
                "Request timed out"
            );
            return ESP_FAIL;
        }

        if (ret <= 0) {
            return ESP_FAIL;
        }

        received += (size_t)ret;
    }

    buf[received] = '\0';

    // Check that the FSM has been initialized.
    if (fsm_ptr == NULL) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        return httpd_resp_sendstr(req, "Controller not ready");
    }

    ESP_LOGI(TAG, "Received command: %s", buf);

    // Match the complete command.
    if (strcmp(buf, "cmd=on") == 0) {
        fsm_ptr->currentState = STATE_ON_MANUAL;
        Turn_On();
        Timer_Stop();
        ESP_LOGI(TAG, "Manual ON");

    } else if (strcmp(buf, "cmd=off") == 0) {
        fsm_ptr->currentState = STATE_OFF_MANUAL;
        Turn_Off();
        Timer_Stop();
        ESP_LOGI(TAG, "Manual OFF");

    } else if (strcmp(buf, "cmd=auto") == 0) {
        fsm_ptr->currentState = STATE_OFF;
        Turn_Off();
        Timer_Stop();
        ESP_LOGI(TAG, "Auto mode");

    } else {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_sendstr(req, "Unknown command");
    }

    // Return to the dashboard.
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
    return httpd_resp_send(req, NULL, 0);
}

// URI registration for the control endpoint (POST /control)
static const httpd_uri_t control = {
    .uri     = "/control",
    .method  = HTTP_POST,
    .handler = post_handler,
    .user_ctx = NULL
};

// 404 Error Handler
esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err) {
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Page not found");
    return ESP_FAIL;
}

// Starts the HTTP server and registers all URI handlers
httpd_handle_t start_webserver(void) {
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;

    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &dashboard);  // Register GET /
        httpd_register_uri_handler(server, &control);  // Register POST /control
        httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, http_404_error_handler);
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

// Stops the HTTP server
esp_err_t stop_webserver(httpd_handle_t server) {
    return httpd_stop(server);
}

// WiFi disconnect event handler
void disconnect_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    httpd_handle_t *server = (httpd_handle_t *) arg;
    if (*server) {
        ESP_LOGI(TAG, "Stopping webserver");
        if (stop_webserver(*server) == ESP_OK) *server = NULL;
        else ESP_LOGE(TAG, "Failed to stop http server");
    }
}

// WiFi connect event handler
void connect_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data){
    httpd_handle_t *server = (httpd_handle_t *) arg;
    if (*server == NULL) {
        ESP_LOGI(TAG, "Starting webserver");
        *server = start_webserver();
    }
}

// Stores a reference to the FSM so handlers can update the light state directly
void WebServer_Init(FSM *fsm) {
    fsm_ptr = fsm;
}