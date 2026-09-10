#ifndef FSM_H
#define FSM_H

typedef enum {
    STATE_ON,
    STATE_OFF,
    STATE_ERROR,
    STATE_ON_MANUAL,
    STATE_OFF_MANUAL
} State;

typedef struct {
    State currentState;
} FSM;

void initFSM(FSM *fsm);
void stateFSM(FSM *fsm);

#endif