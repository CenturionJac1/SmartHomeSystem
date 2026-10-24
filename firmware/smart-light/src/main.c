#include "FSM.h"
#include "Relay_Control.h"
#include "Timer.h"
#include "Sensor_Read.h"
#include "Web_Server.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "protocol_examples_common.h"
#include "esp_wifi.h"
#include "esp_log.h"

static const char *TAG = "main";

void app_main(void) {
    static httpd_handle_t server = NULL;
    static FSM fsm;

    // Initialize hardware and control logic before connecting to Wi-Fi.
    Relay_Init();
    Timer_Init();
    Sensor_Init();
    initFSM(&fsm);
    WebServer_Init(&fsm);

    // Initialize NVS, recovering from specific storage errors.
    esp_err_t ret = nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    ESP_ERROR_CHECK(ret);

    // Initialize networking.
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Connect using the credentials configured in menuconfig.
    ESP_ERROR_CHECK(example_connect());

    // Handle later disconnections and reconnections.
    ESP_ERROR_CHECK(esp_event_handler_register(
        IP_EVENT,
        IP_EVENT_STA_GOT_IP,
        &connect_handler,
        &server
    ));

    ESP_ERROR_CHECK(esp_event_handler_register(
        WIFI_EVENT,
        WIFI_EVENT_STA_DISCONNECTED,
        &disconnect_handler,
        &server
    ));

    // Start the server for the initial connection.
    server = start_webserver();

    if (server == NULL) {
        ESP_LOGE(TAG, "Web server failed to start");
    }

    bool previous_motion = false;
    bool first_read = true;

    while (1) {
        bool motion = Sensor_IsMotionDetected();

        // Log only the first reading and changes in sensor output.
        if (first_read || motion != previous_motion) {
            ESP_LOGI(
                TAG,
                "PIR: %s",
                motion ? "MOTION DETECTED" : "NO MOTION"
            );

            previous_motion = motion;
            first_read = false;
        }

        stateFSM(&fsm);

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}