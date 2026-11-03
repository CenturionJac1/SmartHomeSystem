
#include "FSM.h"
#include "Relay_Control.h"
#include "Timer.h"
#include "Sensor_Read.h"
#include "Node.h"

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

    static FSM fsm;

    Relay_Init();
    Timer_Init();
    Sensor_Init();
    initFSM(&fsm);
    ESP_ERROR_CHECK(Node_Init(&fsm));
    esp_err_t ret = nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {

        ESP_ERROR_CHECK(nvs_flash_erase());

        ret = nvs_flash_init();
    }

    ESP_ERROR_CHECK(ret);
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_ERROR_CHECK(esp_event_handler_register(
        IP_EVENT,
        IP_EVENT_STA_GOT_IP,
        Node_ConnectHandler,
        NULL
    ));

    ESP_ERROR_CHECK(esp_event_handler_register(
        WIFI_EVENT,
        WIFI_EVENT_STA_DISCONNECTED,
        Node_DisconnectHandler,
        NULL
    ));

    ESP_LOGI(TAG, "Connecting to Wi-Fi...");
    ESP_ERROR_CHECK(example_connect());
    ESP_LOGI(TAG, "Wi-Fi connection established");
    ESP_ERROR_CHECK(Node_StartServer());
    bool previous_motion = false;
    bool first_read = true;

    while (1) {

        bool motion = Sensor_IsMotionDetected();
        if (first_read || motion != previous_motion) {

            ESP_LOGI(
                TAG,
                "PIR: %s",
                motion ? "MOTION DETECTED" : "NO MOTION"
            );

            previous_motion = motion;
            first_read = false;
        }
        Node_RunFSM();

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
