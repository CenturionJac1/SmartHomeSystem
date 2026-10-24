#include "Relay_Control.h"
#include "Config.h"
#include "driver/gpio.h"

void Relay_Init(void) {
    gpio_reset_pin(CONFIG_RELAY_GPIO_PIN);

    // Set the OFF level before enabling output.
    Turn_Off();

    gpio_set_direction(CONFIG_RELAY_GPIO_PIN, GPIO_MODE_OUTPUT);
}

void Turn_On(void) {
    gpio_set_level(CONFIG_RELAY_GPIO_PIN,
                   CONFIG_RELAY_ON_LEVEL);
}

void Turn_Off(void) {
    gpio_set_level(CONFIG_RELAY_GPIO_PIN,
                   CONFIG_RELAY_OFF_LEVEL);
}