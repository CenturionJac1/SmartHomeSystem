#include "FSM.h"
#include "Relay_Control.h"
#include "Timer.h"
#include "Sensor_Read.h"

void initFSM(FSM *fsm) {
    fsm->currentState = STATE_OFF;
}

void stateFSM(FSM *fsm) {
    bool input = Sensor_IsMotionDetected();
    if (Sensor_Error() && fsm->currentState != STATE_ON_MANUAL && fsm->currentState != STATE_OFF_MANUAL) {
        fsm->currentState = STATE_ERROR;
    }
    switch (fsm->currentState) {
        case STATE_ON:
            if (input) Timer_Reset();
            else if (Is_Timeout()) {
                Turn_Off();
                Timer_Stop();
                fsm->currentState = STATE_OFF;
            }
            break;
        case STATE_OFF:
            Turn_Off();
            if (input) {
                Turn_On();
                Timer_Start();
                fsm->currentState = STATE_ON;
            }
            break;
        case STATE_ERROR:
            Turn_Off();
            Timer_Stop();
            fsm->currentState = STATE_OFF;
            break;
        // These two cases are handled by Web_Server.c
        case STATE_ON_MANUAL:
            break;
        case STATE_OFF_MANUAL:
            break;
    }
}