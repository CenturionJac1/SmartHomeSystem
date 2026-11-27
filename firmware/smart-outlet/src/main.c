#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs_flash.h"
#include "protocol_examples_common.h"

#define US_PER_SECOND INT64_C(1000000)
#define TIMER_CHECK_US 50000

static SemaphoreHandle_t outlet_lock;
static bool outlet_on;
static int64_t off_at_us;

// These two functions run while outlet_lock is held.
static void set_outlet(bool on, int seconds, int64_t now_us)
{
    int level = on ? OUTLET_ACTIVE_LEVEL : !OUTLET_ACTIVE_LEVEL;
    ESP_ERROR_CHECK(gpio_set_level(OUTLET_GPIO, level));

    outlet_on = on;
    off_at_us = on && seconds > 0 ? now_us + seconds * US_PER_SECOND : 0;
}

static void turn_off_if_due(int64_t now_us)
{
    if (off_at_us != 0 && now_us >= off_at_us) {
        set_outlet(false, 0, now_us);
    }
}

static void check_timer(void *arg)
{
    (void)arg;

    if (xSemaphoreTake(outlet_lock, 0) == pdTRUE) {
        turn_off_if_due(esp_timer_get_time());
        xSemaphoreGive(outlet_lock);
    }
}

static esp_err_t read_command(httpd_req_t *request, bool *on, int *seconds)
{
    char control_header[2];
    char query[64];
    char state[4];

    if (request->content_len != 0 ||
        httpd_req_get_hdr_value_str(request, "X-Outlet-Control",
                                   control_header, sizeof(control_header)) != ESP_OK ||
        strcmp(control_header, "1") != 0 ||
        httpd_req_get_url_query_str(request, query, sizeof(query)) != ESP_OK ||
        httpd_query_key_value(query, "state", state, sizeof(state)) != ESP_OK ||
        (strcmp(state, "on") != 0 && strcmp(state, "off") != 0)) {
        return ESP_ERR_INVALID_ARG;
    }

    *on = strcmp(state, "on") == 0;
    *seconds = 0;

    char duration[5];
    esp_err_t result =
        httpd_query_key_value(query, "seconds", duration, sizeof(duration));

    if (result == ESP_ERR_NOT_FOUND) {
        return ESP_OK;
    }

    if (result != ESP_OK || !*on || duration[0] == '\0' ||
        strspn(duration, "0123456789") != strlen(duration)) {
        return ESP_ERR_INVALID_ARG;
    }

    *seconds = atoi(duration);
    if (*seconds < 1 || *seconds > OUTLET_MAX_SECONDS) {
        return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

static esp_err_t handle_outlet(httpd_req_t *request)
{
    bool update = request->method == HTTP_POST;
    bool requested_on = false;
    int seconds = 0;

    if (update && read_command(request, &requested_on, &seconds) != ESP_OK) {
        return httpd_resp_send_err(
            request, HTTPD_400_BAD_REQUEST,
            "Use X-Outlet-Control: 1 and state=on or state=off. "
            "Optional seconds=1..3600 is for state=on only.");
    }

    xSemaphoreTake(outlet_lock, portMAX_DELAY);
    int64_t now_us = esp_timer_get_time();

    if (update) {
        set_outlet(requested_on, seconds, now_us);
    } else {
        turn_off_if_due(now_us);
    }

    bool is_on = outlet_on;
    int remaining_seconds = 0;
    if (off_at_us > now_us) {
        remaining_seconds =
            (int)((off_at_us - now_us + US_PER_SECOND - 1) / US_PER_SECOND);
    }
    xSemaphoreGive(outlet_lock);

    char response[64];
    snprintf(response, sizeof(response),
             "{\"on\":%s,\"remaining_seconds\":%d}",
             is_on ? "true" : "false", remaining_seconds);

    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_sendstr(request, response);
}

void app_main(void)
{
    // Start with the outlet OFF.
    ESP_ERROR_CHECK(gpio_set_level(OUTLET_GPIO, !OUTLET_ACTIVE_LEVEL));
    ESP_ERROR_CHECK(gpio_set_direction(OUTLET_GPIO, GPIO_MODE_OUTPUT));

    outlet_lock = xSemaphoreCreateMutex();
    ESP_ERROR_CHECK(outlet_lock != NULL ? ESP_OK : ESP_ERR_NO_MEM);

    const esp_timer_create_args_t timer_args = {
        .callback = check_timer,
        .name = "outlet_timer",
    };
    esp_timer_handle_t timer;
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(timer, TIMER_CHECK_US));

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(example_connect());

    httpd_handle_t server = NULL;
    httpd_config_t server_config = HTTPD_DEFAULT_CONFIG();
    ESP_ERROR_CHECK(httpd_start(&server, &server_config));

    const httpd_uri_t get_route = {
        .uri = "/outlet",
        .method = HTTP_GET,
        .handler = handle_outlet,
    };
    const httpd_uri_t post_route = {
        .uri = "/outlet",
        .method = HTTP_POST,
        .handler = handle_outlet,
    };

    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &get_route));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &post_route));
}