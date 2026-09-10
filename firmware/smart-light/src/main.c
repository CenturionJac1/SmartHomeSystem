#include "FSM.h"
#include "Relay_Control.h"
#include "Timer.h"
#include "Sensor_Read.h"
#include "Web_Server.h"      
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <nvs_flash.h>      
#include "esp_netif.h"         
#include "esp_event.h"     
#include "protocol_examples_common.h" 
#include "esp_wifi.h"   
#include "esp_log.h" 

static const char *TAG = "main";  // Log tag for this file

void app_main(void) {
    static httpd_handle_t server = NULL;  // HTTP server handle
    FSM fsm;

    // System Initialization
    ESP_ERROR_CHECK(nvs_flash_init());  // Initialize NVS flash storage
    ESP_ERROR_CHECK(esp_netif_init());   // Initialize TCP/IP network stack
    ESP_ERROR_CHECK(esp_event_loop_create_default());  // Create default system event loop
    ESP_ERROR_CHECK(example_connect());  // Connect to WiFi using menuconfig credentials

    // Register Wifi Event Handlers
    // Start web server when IP address is obtained
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &connect_handler, &server));
    // Stop web server when WiFi is disconnected
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &disconnect_handler, &server));

    // Peripheral and Logic Initialization
    Relay_Init();
    Timer_Init();
    Sensor_Init();
    initFSM(&fsm);
    WebServer_Init(&fsm);
    server = start_webserver();

    while (1) {
        // Read current motion sensor state
        bool motion = Sensor_IsMotionDetected();
        ESP_LOGI(TAG, "PIR: %s", motion ? "MOTION DETECTED" : "NO MOTION");

        stateFSM(&fsm);
        vTaskDelay(pdMS_TO_TICKS(100));  // Poll every 100ms
    }
}
