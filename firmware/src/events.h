#ifndef EVENTS_H
#define EVENTS_H

#include <stdint.h>

#include "event_ring.h"

// Device-side owner of the timeline: the NVS boot counter, the wall-clock
// sync, and the event ring that the recorder task drains to
// /rec/events.csv. Callable from any core; the ring is spinlocked.

void events_init();                                   // reads and bumps boot_id, logs EV_BOOT
uint32_t events_boot_id();

void events_time_sync(uint64_t epoch_ms);             // from the TIME_SYNC characteristic
bool events_time_synced();
uint64_t events_epoch_now();                          // 0 when unsynced

void events_log(uint8_t type, const char *detail);    // detail may be NULL
void events_logf(uint8_t type, const char *fmt, ...);

// Recorder task only. Appends everything pending to the CSV; returns the
// number written. Safe to call when nothing is pending.
int events_flush_to_sd();

#endif // EVENTS_H
