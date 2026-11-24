#include <stdbool.h>
#include <stdatomic.h>
#include <stdio.h>

#include "esp_adc/adc_oneshot.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "protocol_examples_common.h"

#define SOIL_TEST_MODE 1
#define SOIL_CHANNEL ADC_CHANNEL_3
#define SOIL_SAMPLE_COUNT 8
#define SOIL_SAMPLE_MS 2000

static const char *TAG = "moisture_sensor";
static atomic_int soil_raw = -1;

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
    ESP_LOGI(TAG, "Network initialization complete");
}

static esp_err_t handle_moisture(httpd_req_t *req)
{
    int raw = atomic_load(&soil_raw);
    if (raw < 0) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        return httpd_resp_sendstr(req, "Soil reading unavailable");
    }

    char body[32];
    snprintf(body, sizeof(body), "{\"raw\":%d}", raw);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, body);
}

#if !SOIL_TEST_MODE
static void sample_soil(void *arg)
{
    adc_oneshot_unit_handle_t adc = arg;

    while (true) {
        int sum = 0;
        esp_err_t result = ESP_OK;

        for (int i = 0; i < SOIL_SAMPLE_COUNT; ++i) {
            int raw;
            result = adc_oneshot_read(adc, SOIL_CHANNEL, &raw);
            if (result != ESP_OK) {
                break;
            }
            sum += raw;
        }

        if (result == ESP_OK) {
            int average = sum / SOIL_SAMPLE_COUNT;
            atomic_store(&soil_raw, average);
            ESP_LOGI(TAG, "Soil ADC raw: %d", average);
        } else {
            atomic_store(&soil_raw, -1);
            ESP_LOGW(TAG, "ADC read failed: %s", esp_err_to_name(result));
        }

        vTaskDelay(pdMS_TO_TICKS(SOIL_SAMPLE_MS));
    }
}
#endif

void app_main(void)
{
#if SOIL_TEST_MODE
    atomic_store(&soil_raw, 2371);
    ESP_LOGI(TAG, "Soil test mode enabled, raw=2371");
#else
    adc_oneshot_unit_handle_t adc;
    const adc_oneshot_unit_init_cfg_t unit = {
        .unit_id = ADC_UNIT_1,
    };
    const adc_oneshot_chan_cfg_t channel = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };

    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit, &adc));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc, SOIL_CHANNEL, &channel));
    BaseType_t task_created =
        xTaskCreate(sample_soil, "soil_adc", 3072, adc, 5, NULL);
    ESP_ERROR_CHECK(task_created == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
#endif

    start_network();

    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    const httpd_uri_t moisture_uri = {
        .uri = "/moisture",
        .method = HTTP_GET,
        .handler = handle_moisture,
    };

    ESP_ERROR_CHECK(httpd_start(&server, &config));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &moisture_uri));
    ESP_LOGI(TAG, "HTTP server ready at GET /moisture");
}