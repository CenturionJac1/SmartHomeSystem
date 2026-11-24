#include <stdbool.h>

#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#define SERVO_GPIO 6
#define REST_US 1500
#define PRESS_US 1600
#define PRESS_MS 250
#define RETURN_MS 350

static const char *TAG = "humidifier";
static SemaphoreHandle_t servo_ready;
static TaskHandle_t servo_task;

static void start_network(void);

static unsigned pulse_to_duty(unsigned pulse_us)
{
    return (pulse_us * 8192U + 10000U) / 20000U;
}

static esp_err_t set_servo_pulse(unsigned pulse_us)
{
    esp_err_t result = ledc_set_duty(
        LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, pulse_to_duty(pulse_us)
    );
    if (result != ESP_OK) {
        return result;
    }
    return ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

static void run_servo(void *arg)
{
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(RETURN_MS));
    xSemaphoreGive(servo_ready);

    while (true) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        esp_err_t press_result = set_servo_pulse(PRESS_US);
        if (press_result == ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(PRESS_MS));
        }

        esp_err_t rest_result = set_servo_pulse(REST_US);
        if (rest_result != ESP_OK) {
            ESP_LOGE(TAG, "Servo failed to return: %s", esp_err_to_name(rest_result));
            ledc_stop(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
            vTaskDelete(NULL);
            return;
        }
        if (press_result != ESP_OK) {
            ESP_LOGE(TAG, "Servo press failed: %s", esp_err_to_name(press_result));
        }

        vTaskDelay(pdMS_TO_TICKS(RETURN_MS));
        xSemaphoreGive(servo_ready);
    }
}

static esp_err_t handle_toggle(httpd_req_t *req)
{
    if (xSemaphoreTake(servo_ready, 0) != pdTRUE) {
        httpd_resp_set_status(req, "409 Conflict");
        return httpd_resp_sendstr(req, "Servo unavailable");
    }

    xTaskNotifyGive(servo_task);
    httpd_resp_set_status(req, "202 Accepted");
    return httpd_resp_sendstr(req, "MIST press requested");
}

void app_main(void)
{
    const ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_13_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 50,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    const ledc_channel_config_t channel = {
        .gpio_num = SERVO_GPIO,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .duty = pulse_to_duty(REST_US),
    };

    ESP_ERROR_CHECK(ledc_timer_config(&timer));
    ESP_ERROR_CHECK(ledc_channel_config(&channel));

    servo_ready = xSemaphoreCreateBinary();
    ESP_ERROR_CHECK(servo_ready ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(xTaskCreate(run_servo, "mist_servo", 3072, NULL, 5,
                               &servo_task) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);

    start_network();

    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    const httpd_uri_t toggle_uri = {
        .uri = "/humidifier/toggle",
        .method = HTTP_POST,
        .handler = handle_toggle,
    };

    ESP_ERROR_CHECK(httpd_start(&server, &config));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &toggle_uri));
}

#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "protocol_examples_common.h"

static void start_network(void)
{
    esp_err_t result = nvs_flash_init();
    if (result == ESP_ERR_NVS_NO_FREE_PAGES ||
        result == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        result = nvs_flash_init();
    }

    ESP_ERROR_CHECK(result);
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(example_connect());
}