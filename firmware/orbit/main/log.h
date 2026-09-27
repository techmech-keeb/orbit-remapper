#pragma once

#include <stdbool.h>
#include <stdint.h>

// Log lines to the USB CDC interface without blocking the caller.
// Lines go into a ring buffer from any task; orbit_log_pump(), called from
// the main loop (the only task that touches TinyUSB), sends them while a
// terminal has the port open (DTR set). When the buffer is full, new lines
// are dropped and counted ("M1 LOG lost N byte(s)").

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*orbit_log_connect_cb_t)(void);

// on_connect runs each time a terminal opens the port (DTR goes up), so the
// START line can be repeated there.
void orbit_log_init(orbit_log_connect_cb_t on_connect);

void olog(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

// Main loop only.
void orbit_log_pump(void);

typedef struct {
    bool connected;      // DTR set by the terminal
    uint32_t sent;       // bytes handed to the CDC interface
    uint32_t lost;       // bytes dropped because the buffer was full
} orbit_log_stats_t;

void orbit_log_get_stats(orbit_log_stats_t* out);

#ifdef __cplusplus
}
#endif
