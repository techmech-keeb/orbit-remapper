#pragma once

// Types a short text to the PC as the USB keyboard (M3: "Open config page").
// The text goes straight to HID 0, not through the core, so that the user's
// remapping cannot change it. Only keys that sit on the same place on the US
// and the Japanese layout are used (letters, digits, . / - and Enter).

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// True when the PC sees a keyboard it can type on: the kb_mouse or absolute
// descriptor, or the boot protocol. The gamepad descriptors have none.
bool orbit_typist_available(void);
// Asks the main loop to type the config tool's address and Enter. Any task.
void orbit_typist_request_config_url(void);
bool orbit_typist_busy(void);
// Main loop, every iteration: sends the next press or release when due.
void orbit_typist_poll(void);

#ifdef __cplusplus
}
#endif
