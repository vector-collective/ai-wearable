#ifndef APP_H
#define APP_H

#include <Arduino.h>

void setup_app();
void loop_app();

// Mark user/recorder activity so idle power management stays out of the way.
void app_register_activity();

// True while the BLE photo path owns a camera frame buffer. The SD recorder
// skips its grab rather than blocking on the camera's 4s acquire timeout.
bool app_camera_busy();

#endif // APP_H
