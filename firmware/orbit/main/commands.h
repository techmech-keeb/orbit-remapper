#pragma once

// Serial commands and the ORB state line (M2 stage 2a). poll() is main loop
// only (TinyUSB); the two log functions read shared state and may run from
// the status task.

// Reads the log port and runs any complete "orbit ..." line.
void orbit_commands_poll();
// One "M1 LDG" line per ledger row, plus a count.
void orbit_commands_log_ledger();
// The once-a-second "M1 ORB" line: what the screen will show in M3.
void orbit_commands_log_state(double t);
