#include "Relay_Control.h"
#include "stdint.h"
#include "Config.h"
#include "driver/gpio.h"

void Relay_Init(void) {
    gpio_reset_pin(CONFIG_RELAY_GPIO_PIN);
    // Set pin as output
    gpio_set_direction(CONFIG_RELAY_GPIO_PIN, GPIO_MODE_OUTPUT);
    // Ensure relay off before start
    Turn_Off();
}

void Turn_On(void) {
    // Set pin high to activate relay
    gpio_set_level(CONFIG_RELAY_GPIO_PIN, 1); 
}

void Turn_Off(void) {
    // Set pin low to deactivate relay
    gpio_set_level(CONFIG_RELAY_GPIO_PIN, 0);
}