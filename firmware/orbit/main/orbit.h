#pragma once

#include <stdint.h>

// Seconds since boot, for log lines.
double orbit_now_s();

void orbit_usb_init();
bool do_send_report(uint8_t interface, const uint8_t* report_with_id, uint8_t len);

// Called by do_send_report() right after a report went to the PC on HID 0.
void orbit_report_sent();

// Asks the main loop to reboot into the ROM's download (flashing) mode.
void orbit_request_download_mode(const char* why);

// Pulls the USB Serial/JTAG off the bus (D+ pull-up off) and waits, so the
// PC and any hub see a clean unplug before the port changes identity.
// esp_restart() does not reset the USB Serial/JTAG, so every path into the
// ROM must call orbit_usj_attach() again first.
void orbit_usj_detach(int ms);
void orbit_usj_attach();

// Reboots into the ROM's download mode now. Hands the USB pins back to the
// USB Serial/JTAG after a clean unplug, so the browser flasher and esptool
// find the chip.
[[noreturn]] void orbit_enter_download_mode();
