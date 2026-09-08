// Pure thermal policy - no hardware includes, host-testable.
//
// What this is NOT: a charge gate. The XIAO ESP32S3's BQ25101 charger has
// no thermistor input and its enable pin is not routed to a GPIO, so
// firmware cannot stop it charging a hot cell. The LP603449 is rated to
// charge at 0-45 C and the cell sits against the back wall, where a black
// resin case in the sun can reach that. What firmware CAN do is stop adding
// heat of its own and put the excursion on the timeline:
//
//   WARM   bursts pause (the camera is the biggest heat source we control)
//   HOT    a running video session stops, the CPU drops to its floor
//
// The thresholds are on the S3 die, which under load runs 10-20 C above the
// cell. Verify the die-to-cell offset once with a thermocouple on the cell
// and move THERMAL_WARM_C / THERMAL_HOT_C in config.h accordingly.
#ifndef THERMAL_H
#define THERMAL_H

#include <stdint.h>

enum { THERMAL_NORMAL = 0, THERMAL_WARM = 1, THERMAL_HOT = 2 };

typedef struct {
    float warm_c;
    float hot_c;
    float hyst_c; // a state is left only this far below the threshold that entered it
} thermal_cfg_t;

typedef struct {
    int state;
    float temp_c; // last sample
} thermal_t;

static inline void thermal_init(thermal_t *t)
{
    t->state = THERMAL_NORMAL;
    t->temp_c = 0.0f;
}

// Returns the new state. Transitions are one step at a time so a single
// wild sample cannot jump NORMAL -> HOT; the sampler runs every 20 s, which
// is fast against any real thermal slope.
static inline int thermal_step(thermal_t *t, const thermal_cfg_t *c, float temp_c)
{
    t->temp_c = temp_c;
    switch (t->state) {
    case THERMAL_NORMAL:
        if (temp_c >= c->warm_c) {
            t->state = THERMAL_WARM;
        }
        break;
    case THERMAL_WARM:
        if (temp_c >= c->hot_c) {
            t->state = THERMAL_HOT;
        } else if (temp_c < c->warm_c - c->hyst_c) {
            t->state = THERMAL_NORMAL;
        }
        break;
    case THERMAL_HOT:
        if (temp_c < c->hot_c - c->hyst_c) {
            t->state = THERMAL_WARM;
        }
        break;
    default:
        t->state = THERMAL_NORMAL;
        break;
    }
    return t->state;
}

static inline const char *thermal_state_name(int s)
{
    switch (s) {
    case THERMAL_NORMAL: return "normal";
    case THERMAL_WARM:   return "warm";
    case THERMAL_HOT:    return "hot";
    default:             return "?";
    }
}

#endif // THERMAL_H
