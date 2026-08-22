#ifndef APP_H
#define APP_H

#include <Arduino.h>

void setup_app();
void loop_app();

// Mark user/recorder activity so idle power management stays out of the way.
void app_register_activity();

#endif // APP_H
