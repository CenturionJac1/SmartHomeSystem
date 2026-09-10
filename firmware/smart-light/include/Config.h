#ifndef CONFIG_H
#define CONFIG_H

#include "driver/gpio.h"

// TODO: Change as you want

// TODO: Set the GPIO pin connected to the relay control input
#define CONFIG_RELAY_GPIO_PIN GPIO_NUM_5

// TODO: Set the GPIO pin connected to the PIR motion sensor output
#define CONFIG_PIR_OUTPUT_PIN GPIO_NUM_14

// TODO: Set the desired inactivity timeout in microseconds before the light turns off
#define CONFIG_INACTIVITY_US 10000000 // 10 seconds

#endif 