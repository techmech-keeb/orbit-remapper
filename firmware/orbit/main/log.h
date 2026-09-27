#pragma once

// Log lines to the USB CDC interface without blocking the caller.
// Lines go into a ring buffer; a low-priority task sends them when a
// terminal has the port open. When the buffer is full, new lines are
// dropped and counted ("M1 LOG lost N byte(s)").

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*orbit_log_connect_cb_t)(void);

// on_connect runs (on the log task) each time a terminal opens the port,
// so the START line can be repeated there.
void orbit_log_init(orbit_log_connect_cb_t on_connect);

void olog(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

#ifdef __cplusplus
}
#endif
