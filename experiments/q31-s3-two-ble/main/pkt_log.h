#pragma once

// Starts printing the ATT / SMP / L2CAP signalling packets recorded on the
// host <-> controller path as "Q31 PKT" lines (from a task on core 1).
void pkt_log_start(void);
