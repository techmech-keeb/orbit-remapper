#pragma once

// Orbit's own config-tool commands on HID 1 (M2 stage 2a, requirement E).
// They ride the same 32-byte feature report as upstream's commands, with
// command numbers 0x80 and up so that upstream can keep adding its own
// (1..25 at 51ab8b3). Upstream's config format and commands are untouched;
// its web tool keeps working. The reply to a GET command is read with the
// next GET_REPORT, as upstream does. Layouts are in tool.cc.

#include <stdint.h>

// Returns true when the report carried an Orbit command (handled here).
bool orbit_tool_set_report(const uint8_t* buffer, uint16_t len);
// Fills the reply to the last Orbit GET command; 0 when none is pending.
uint16_t orbit_tool_get_report(uint8_t* buffer, uint16_t reqlen);
