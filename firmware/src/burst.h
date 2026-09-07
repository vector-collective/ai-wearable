#ifndef BURST_H
#define BURST_H

#include <stddef.h>
#include <stdint.h>

// New-voice photo burst controller. Owns the policy (burst_logic.h), parses
// the CAPTURE_CTRL characteristic, and asks the recorder for frames.

void burst_init();
void burst_loop(uint32_t now);                          // expires hold-offs

// CAPTURE_CTRL write. Ops are in docs/SPEC.md section 3.1.
void burst_handle_ctrl(const uint8_t *data, size_t len);

// The device-tier detector's hook. Not wired yet - needs the own-voice gate.
void burst_local_candidate(uint32_t hint);

bool burst_quiet();

#endif // BURST_H
