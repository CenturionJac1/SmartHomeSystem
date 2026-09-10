#ifndef SENSOR_READ_H
#define SENSOR_READ_H

#include <stdbool.h>

void Sensor_Init(void);
bool Sensor_IsMotionDetected(void);
bool Sensor_Error(void);

#endif