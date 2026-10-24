#ifndef CONFIG_H
#define CONFIG_H

#include "driver/gpio.h"

// GPIO connected to the relay control input.
#define CONFIG_RELAY_GPIO_PIN GPIO_NUM_5

// GPIO connected to the PIR sensor output.
#define CONFIG_PIR_OUTPUT_PIN GPIO_NUM_14

// Relay output levels: assumes an active-high relay.
#define CONFIG_RELAY_ON_LEVEL  1
#define CONFIG_RELAY_OFF_LEVEL 0

// Turn off after 10 seconds without a motion signal.
#define CONFIG_INACTIVITY_US 10000000LL

#endif