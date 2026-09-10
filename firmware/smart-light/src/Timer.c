#include "Timer.h"
#include "esp_timer.h"
#include "Config.h"

static int64_t timer_start_time = 0;
static bool timer_running = false;

// Initialize the timer to a stop state
void Timer_Init(void) {
    timer_start_time = 0;
    timer_running = false;
}

// Start the timer from the current time
void Timer_Start(void) {
    timer_start_time = esp_timer_get_time();
    timer_running = true;
}

// Restart the timer
void Timer_Reset(void) {
    timer_start_time = esp_timer_get_time();  
    timer_running = true;                    
}

// Stop the timer
void Timer_Stop(void) {
    timer_start_time = 0;  // Reset to 0
    timer_running = false; 
}

// Returns true if current time exceeds CONFIG_INACTIVITY_US
// Returns false immediately if the timer is not running
bool Is_Timeout(void) {
    if (!timer_running) return false;
    int64_t cur_time = esp_timer_get_time() - timer_start_time;
    return cur_time >= ((int64_t)CONFIG_INACTIVITY_US);
}