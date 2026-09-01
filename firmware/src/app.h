#ifndef APP_H
#define APP_H

#include <Arduino.h>

void setup_app();
void loop_app();

// Mark user/recorder activity so idle power management stays out of the way.
void app_register_activity();

// True while the BLE photo path owns a camera frame buffer.
bool app_camera_busy();

// Camera lifecycle. The sensor is powered up only for the duration of a
// video session, so it costs nothing during always-on audio capture.
bool app_camera_start();
void app_camera_stop();
bool app_camera_ready();

#endif // APP_H
