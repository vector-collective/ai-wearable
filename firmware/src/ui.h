#ifndef UI_H
#define UI_H

#include <stdint.h>

// Case button + WS2812 RGB LED + battery gauge.
// Short press idle: battery check (LED shows charge color, cool=full warm=low).
// Long press idle: start recording (single blink in battery color).
// Short press recording: bookmark + segment split (quick cyan blink).
// Long press recording: stop (double blink in SD-free-space color).

void ui_init();
void ui_loop(uint32_t now);

#endif // UI_H
