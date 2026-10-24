#include "Sensor_Read.h"
#include "Config.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include <stdbool.h>

static const char *TAG = "sensor";

static bool sensor_initialized = false;
static bool sensor_error = true;

// Configure the PIR sensor pin as a digital input.
void Sensor_Init(void) {
    sensor_initialized = false;
    sensor_error = true;

    // Validate the pin before creating the bit mask.
    if (!GPIO_IS_VALID_GPIO(CONFIG_PIR_OUTPUT_PIN)) {
        ESP_LOGE(TAG, "Invalid PIR GPIO pin");
        return;
    }

    gpio_config_t pir_conf = {
        .pin_bit_mask = (1ULL << CONFIG_PIR_OUTPUT_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t ret = gpio_config(&pir_conf);

    if (ret != ESP_OK) {
        ESP_LOGE(
            TAG,
            "PIR GPIO initialization failed: %s",
            esp_err_to_name(ret)
        );
        return;
    }

    sensor_initialized = true;
    sensor_error = false;

    ESP_LOGI(TAG, "PIR input initialized on GPIO %d",
             (int)CONFIG_PIR_OUTPUT_PIN);
}

// Read motion only after successful initialization.
bool Sensor_IsMotionDetected(void) {
    if (!sensor_initialized) {
        return false;
    }

    return gpio_get_level(CONFIG_PIR_OUTPUT_PIN) == 1;
}

// Report GPIO initialization errors.
bool Sensor_Error(void) {
    return sensor_error;
}