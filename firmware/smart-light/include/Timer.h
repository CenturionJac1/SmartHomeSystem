#ifndef TIMER_H
#define TIMER_H

#include "stdbool.h"


void Timer_Init(void);
void Timer_Start(void);
void Timer_Reset(void);
void Timer_Stop(void);
bool Is_Timeout(void);

#endif