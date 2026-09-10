#include "Sensor_Read.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "rom/ets_sys.h"
#include "Config.h"
#include "driver/gpio.h"
#include <stdbool.h>
#include "esp_log.h"

static const char *TAG = "sensor";  // Log tag for this file
static bool sensor_read_error = false;  // Tracks whether the last read encountered an error

// Initializes the PIR motion sensor GPIO pin as a digital input
void Sensor_Init(void) {
    gpio_config_t pir_conf = {
        .pin_bit_mask = (1ULL << CONFIG_PIR_OUTPUT_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&pir_conf);
}

// Returns true if the PIR sensor is currently detecting motion
bool Sensor_IsMotionDetected(void) {
    sensor_read_error = false;
    return gpio_get_level(CONFIG_PIR_OUTPUT_PIN) == 1;
}

// Returns true if the last read encountered an error
bool Sensor_Error(void) {
    return sensor_read_error;
}