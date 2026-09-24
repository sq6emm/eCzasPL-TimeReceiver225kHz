/*
 * Timekeeper: disciplines a local clock (FCY ticks) to decoded frames.
 *
 * Time is "seconds since 2000-01-01 00:00:00 UTC" without leap seconds (like
 * the transmitted value and like Unix time), ticks are FCY cycles counted by
 * the ADC timebase (Timer3 periods * TICKS_PER_SAMPLE + TMR3).
 *
 * Trust rules (the old firmware had none - it copied every frame into its
 * clock):
 *   - not synchronised: two frames must agree with each other (their time
 *     difference must match the local elapsed time within tolerance),
 *   - synchronised: a frame must match our own clock within SYNC_MAX_ERR_MS;
 *     frames that disagree are only believed when two of them agree with each
 *     other, then the clock is stepped.
 */
#ifndef TIMEKEEPER_H
#define TIMEKEEPER_H

#include <stdint.h>
#include "frame.h"

#define TK_NCAND 6

typedef enum {
    TK_ACCEPTED = 0,  /* consistent with our clock, used for discipline */
    TK_SYNCED,        /* first synchronisation from two agreeing frames */
    TK_STEPPED,       /* clock was wrong, re-synchronised */
    TK_CANDIDATE,     /* stored, waiting for confirmation */
    TK_REJECTED       /* inconsistent */
} tk_result_t;

typedef struct { int64_t tick; uint32_t sec; } tk_point_t;

typedef struct {
    uint8_t  synced;
    int64_t  anchor_tick;     /* tick at which second anchor_sec started */
    uint32_t anchor_sec;
    int64_t  rate_q16;        /* ticks per second, Q16 */
    int64_t  last_ok_tick;
    tk_point_t ref_old, ref_new;
    uint8_t  have_old, have_new;
    tk_point_t cand[TK_NCAND];
    uint8_t  ncand;

    /* last accepted frame content */
    uint8_t  tz, tzc, sk;
    int8_t   leap_pending;    /* +1 insert, -1 delete, 0 none */
    uint32_t leap_at;         /* second at which it takes effect */

    /* statistics */
    int32_t  last_err_us;
    uint32_t n_ok, n_bad, n_steps;
} tk_t;

void        tk_init(tk_t *t);
tk_result_t tk_frame(tk_t *t, int64_t frame_tick, const frame_info_t *fi);
void        tk_maintain(tk_t *t, int64_t now_tick);

/* Time at tick; returns 0 if not synchronised. */
int      tk_time(const tk_t *t, int64_t tick, uint32_t *sec, uint32_t *usec);
/* Tick at which second 'sec' begins (only meaningful when synchronised). */
int64_t  tk_second_tick(const tk_t *t, uint32_t sec);
/* Synchronised and last good frame less than HOLDOVER_VALID_S ago. */
int      tk_valid(const tk_t *t, int64_t now_tick);
/* Clock rate error in parts per billion relative to nominal FCY. */
int32_t  tk_rate_ppb(const tk_t *t);

/* Civil date helpers for seconds since 2000 */
typedef struct { uint16_t year; uint8_t mon, day, hour, min, sec; } tk_date_t;
void     tk_to_date(uint32_t sec, tk_date_t *d);
uint32_t tk_from_date(uint16_t year, uint8_t mon, uint8_t day);

#endif
