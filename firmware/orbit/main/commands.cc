// Ledger and pairing commands from the log port (M2 stage 2a). One line per
// command, "orbit <verb> ...", typed into the CDC port the log comes out of.
// Meant for testing without the screen (M3 replaces it) and for the local
// session's checks. Runs on the main loop, which owns TinyUSB.

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "tusb.h"

#include "ble.h"
#include "commands.h"
#include "ledger.h"
#include "log.h"
#include "orbit.h"

static char line[96];
static int line_len;

void orbit_commands_log_ledger() {
    orbit_ledger_row_t row;
    int n = 0;
    for (int i = 0; orbit_ledger_nth(i, &row); i++, n++) {
        char name[64];
        orbit_ledger_display_name(&row, name, sizeof(name));
        // Lowest Battery Level in % while connected; "-" when not connected,
        // without a Battery Service, or not read yet.
        char battery[8] = "-";
        uint8_t level = orbit_ble_port_battery(row.port);
        if (level != ORBIT_BATTERY_UNKNOWN) {
            snprintf(battery, sizeof(battery), "%u", level);
        }
        olog("M1 LDG t=%.3f port=%d addr=..:%02x:%02x key=%s kind=%s vid=%04x pid=%04x hash=%08x last=%lu "
             "battery=%s name=\"%s\" manufacturer=\"%s\" model=\"%s\" alias=\"%s\" shown=\"%s\"\n",
             orbit_now_s(), row.port, row.addr.val[1], row.addr.val[0],
             (row.flags & ORBIT_LEDGER_NO_KEY) ? "none" : "yes", orbit_ledger_kind_name(row.kind), row.vid, row.pid,
             (unsigned) row.map_hash, (unsigned long) row.last_used, battery, row.name, row.manufacturer, row.model,
             row.alias, name);
    }
    olog("M1 LDG t=%.3f %d of %d ports used\n", orbit_now_s(), n, ORBIT_LEDGER_MAX);
}

static void run(char* cmd) {
    char* verb = strtok(cmd, " ");
    if (verb == NULL || strcmp(verb, "orbit") != 0) {
        return; // not for us (a terminal's stray keystrokes)
    }
    verb = strtok(NULL, " ");
    if (verb == NULL || strcmp(verb, "help") == 0) {
        olog("M1 CMD commands: orbit list | forget <port> | move <new port> <old port> | alias <port> <text> | "
             "pair | stop | approve\n");
        return;
    }
    if (strcmp(verb, "list") == 0) {
        orbit_commands_log_ledger();
    } else if (strcmp(verb, "forget") == 0) {
        const char* a = strtok(NULL, " ");
        int port = a != NULL ? atoi(a) : 0;
        olog("M1 CMD forget port %d\n", port);
        orbit_ble_forget(port);
    } else if (strcmp(verb, "move") == 0) {
        const char* a = strtok(NULL, " ");
        const char* b = strtok(NULL, " ");
        int new_port = a != NULL ? atoi(a) : 0;
        int old_port = b != NULL ? atoi(b) : 0;
        olog("M1 CMD move port %d -> %d\n", new_port, old_port);
        orbit_ble_move(new_port, old_port);
    } else if (strcmp(verb, "alias") == 0) {
        const char* a = strtok(NULL, " ");
        const char* text = strtok(NULL, ""); // the rest of the line, blanks included
        int port = a != NULL ? atoi(a) : 0;
        bool ok = orbit_ledger_set_alias(port, text != NULL ? text : "");
        olog("M1 CMD alias port %d \"%s\": %s\n", port, text != NULL ? text : "", ok ? "set" : "no such port");
    } else if (strcmp(verb, "pair") == 0) {
        olog("M1 CMD pair new device\n");
        orbit_ble_pair_new_device();
    } else if (strcmp(verb, "stop") == 0) {
        olog("M1 CMD stop pairing\n");
        orbit_ble_stop_pairing();
    } else if (strcmp(verb, "approve") == 0) {
        olog("M1 CMD approve\n");
        orbit_ble_approve();
    } else {
        olog("M1 CMD unknown verb \"%s\" (orbit help)\n", verb);
    }
}

void orbit_commands_poll() {
    while (tud_cdc_available() > 0) {
        char c;
        if (tud_cdc_read(&c, 1) != 1) {
            return;
        }
        if (c == '\r' || c == '\n') {
            if (line_len > 0) {
                line[line_len] = '\0';
                run(line);
                line_len = 0;
            }
        } else if (line_len < (int) sizeof(line) - 1) {
            line[line_len++] = c;
        } else {
            line_len = 0; // too long: drop the line
        }
    }
}

void orbit_commands_log_state(double t) {
    orbit_approval_t ap;
    orbit_ble_approval(&ap);
    int pairing = orbit_ble_pairing_remaining_s();
    char pairing_text[16];
    if (!orbit_ble_pairing()) {
        snprintf(pairing_text, sizeof(pairing_text), "off");
    } else if (pairing < 0) {
        snprintf(pairing_text, sizeof(pairing_text), "open");
    } else {
        snprintf(pairing_text, sizeof(pairing_text), "%ds", pairing);
    }
    olog("M1 ORB t=%.0f devices=%d/%d pairing=%s full=%d ask=%d:%s:%ds granted=%d:%ds duplicates=%d\n", t,
         orbit_ledger_count(), ORBIT_LEDGER_MAX, pairing_text, orbit_ble_bonds_full(), ap.port,
         ap.wanted ? ap.reason : "-", ap.remaining_s, ap.granted_port, ap.granted_remaining_s,
         orbit_ble_duplicates());
}
