
#ifndef NODE_H
#define NODE_H

#include "FSM.h"
#include "esp_err.h"
#include "esp_event.h"

// Initialize the node and connect it to the FSM.
esp_err_t Node_Init(FSM *fsm);

// Start and stop the node's HTTP server.
esp_err_t Node_StartServer(void);
void Node_StopServer(void);

// Wi-Fi event callbacks.
void Node_ConnectHandler(
    void *arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void *event_data
);

void Node_DisconnectHandler(
    void *arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void *event_data
);

// Run the FSM while protecting it from simultaneous HTTP commands.
void Node_RunFSM(void);

#endif
