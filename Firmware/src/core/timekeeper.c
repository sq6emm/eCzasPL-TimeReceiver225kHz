/*
 * Timekeeper - see timekeeper.h.
 */
#include <string.h>
#include "timekeeper.h"

#define TICKS_PER_MS      (FCY_HZ / 1000L)
#define LATENCY_TICKS     ((int64_t)(TX_DELAY_US + RX_DELAY_US) * (FCY_HZ / 1000000L))
#define NOMINAL_RATE_Q16  ((int64_t)FCY_HZ << 16)
#define PAIR_MAX_SPAN_S   1200      /* candidates older than this are dropped */
#define RATE_MIN_BASE_S   600       /* rate is measured over at least this */
#define RATE_EARLY_BASE_S 120       /* ...but a clearly large offset is used from here */
#define RATE_EARLY_NOISE_TICKS (2 * TICKS_PER_MS)   /* frame timing noise allowance */
#define REF_ROTATE_S      7200
#define STEP_CONFIRMATIONS 2        /* older frames that must agree before a synced clock is stepped */

static int64_t div_floor(int64_t a, int64_t b)   /* b > 0 */
{
    int64_t q = a / b;
    if ((a % b) != 0 && a < 0) q--;
    return q;
}

void tk_init(tk_t *t)
{
    memset(t, 0, sizeof(*t));
    t->rate_q16 = NOMINAL_RATE_Q16;
}

int64_t tk_second_tick(const tk_t *t, uint32_t sec)
{
    int64_t ds = (int64_t)sec - (int64_t)t->anchor_sec;
    return t->anchor_tick + div_floor(ds * t->rate_q16, 65536);
}

int tk_time(const tk_t *t, int64_t tick, uint32_t *sec, uint32_t *usec)
{
    int64_t d, s, rem;
    if (!t->synced) return 0;
    d = tick - t->anchor_tick;
    s = div_floor(d * 65536, t->rate_q16);
    rem = d * 65536 - s * t->rate_q16;               /* 0 .. rate */
    if (sec) *sec = (uint32_t)(t->anchor_sec + s);
    if (usec) *usec = (uint32_t)((rem / 65536) * 1000000 / (t->rate_q16 >> 16));
    return 1;
}

int tk_valid(const tk_t *t, int64_t now_tick)
{
    return t->synced &&
           (now_tick - t->last_ok_tick) < (int64_t)HOLDOVER_VALID_S * FCY_HZ;
}

int32_t tk_rate_ppb(const tk_t *t)
{
    return (int32_t)(((t->rate_q16 - NOMINAL_RATE_Q16) * 1000) / (NOMINAL_RATE_Q16 / 1000000));
}

/* ---- civil date ------------------------------------------------------- */

uint32_t tk_from_date(uint16_t year, uint8_t mon, uint8_t day)
{
    int32_t y = (int32_t)year - (mon <= 2);
    int32_t era = y / 400;
    int32_t yoe = y - era * 400;
    int32_t doy = (153L * (mon + (mon > 2 ? -3 : 9)) + 2) / 5 + day - 1;
    int32_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    int32_t days1970 = era * 146097L + doe - 719468L;
    return (uint32_t)(days1970 - 10957L) * 86400UL;
}

void tk_to_date(uint32_t sec, tk_date_t *d)
{
    int32_t z = (int32_t)(sec / 86400UL) + 10957L + 719468L;
    uint32_t sod = sec % 86400UL;
    int32_t era = z / 146097L;
    int32_t doe = z - era * 146097L;
    int32_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    int32_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    int32_t mp = (5 * doy + 2) / 153;
    int32_t m = mp < 10 ? mp + 3 : mp - 9;
    d->day = (uint8_t)(doy - (153 * mp + 2) / 5 + 1);
    d->mon = (uint8_t)m;
    d->year = (uint16_t)(yoe + era * 400 + (m <= 2));
    d->hour = (uint8_t)(sod / 3600);
    d->min = (uint8_t)(sod / 60 % 60);
    d->sec = (uint8_t)(sod % 60);
}

/* ---- frame handling --------------------------------------------------- */

static void add_cand(tk_t *t, int64_t tick, uint32_t sec)
{
    uint8_t i, j = 0;
    /* drop stale candidates */
    for (i = 0; i < t->ncand; i++)
        if (tick - t->cand[i].tick < (int64_t)PAIR_MAX_SPAN_S * FCY_HZ)
            t->cand[j++] = t->cand[i];
    t->ncand = j;
    if (t->ncand == TK_NCAND) {
        memmove(&t->cand[0], &t->cand[1], sizeof(tk_point_t) * (TK_NCAND - 1));
        t->ncand--;
    }
    t->cand[t->ncand].tick = tick;
    t->cand[t->ncand].sec = sec;
    t->ncand++;
}

/* How many older candidates agree with the newest one (their time
 * difference matches the elapsed ticks). */
static int cand_confirmations(const tk_t *t)
{
    const tk_point_t *b = &t->cand[t->ncand - 1];
    uint8_t i;
    int n = 0;
    for (i = 0; i + 1 < t->ncand; i++) {
        const tk_point_t *a = &t->cand[i];
        int64_t dt = b->tick - a->tick;
        int64_t ds = (int64_t)b->sec - a->sec;
        int64_t expect, tol;
        if (ds <= 0 || dt <= 0) continue;
        expect = div_floor(ds * t->rate_q16, 65536);
        tol = 20 * TICKS_PER_MS + dt / 5000;           /* 20 ms + 200 ppm */
        if (dt - expect <= tol && expect - dt <= tol) n++;
    }
    return n;
}

static void set_leap(tk_t *t, const frame_info_t *fi, uint32_t sec)
{
    tk_date_t d;
    uint8_t m;
    if (!fi->ls) {
        if (t->leap_pending && sec + 8UL * 86400UL < t->leap_at) t->leap_pending = 0;
        return;
    }
    tk_to_date(sec, &d);
    m = (uint8_t)((d.mon - 1) / 3 * 3 + 4);            /* first month of next quarter */
    t->leap_at = m > 12 ? tk_from_date((uint16_t)(d.year + 1), 1, 1)
                        : tk_from_date(d.year, m, 1);
    t->leap_pending = fi->lss ? -1 : 1;
}

static void sync_to(tk_t *t, int64_t tick, uint32_t sec)
{
    t->synced = 1;
    t->anchor_tick = tick;
    t->anchor_sec = sec;
    t->last_ok_tick = tick;
    t->ref_old.tick = tick; t->ref_old.sec = sec;
    t->have_old = 1; t->have_new = 0;
    t->ncand = 0;
}

tk_result_t tk_frame(tk_t *t, int64_t frame_tick, const frame_info_t *fi)
{
    uint32_t sec = 3UL * fi->n3;
    int64_t tick = frame_tick - LATENCY_TICKS;
    tk_result_t r;

    if (t->synced) {
        int64_t pred = tk_second_tick(t, sec);
        int64_t err = tick - pred;
        if (err <= (int64_t)SYNC_MAX_ERR_MS * TICKS_PER_MS &&
            -err <= (int64_t)SYNC_MAX_ERR_MS * TICKS_PER_MS) {
            /* accepted: phase follows with a 1/4 gain (averages jitter),
             * rate comes from a long baseline to a reference point */
            t->last_err_us = (int32_t)(err / (FCY_HZ / 1000000L));
            t->anchor_sec = sec;
            t->anchor_tick = pred + err / 4;
            t->last_ok_tick = tick;
            if (t->have_old && sec - t->ref_old.sec >= RATE_MIN_BASE_S) {
                t->rate_q16 = ((tick - t->ref_old.tick) * 65536) / (int64_t)(sec - t->ref_old.sec);
            } else if (t->have_old && sec - t->ref_old.sec >= RATE_EARLY_BASE_S) {
                /* Before the full baseline: use the short-baseline rate only
                 * when the offset it shows is well above its own noise
                 * (2 ms over the baseline), so a poor crystal (the bench
                 * board's is -24 ppm) is corrected after a few minutes
                 * without making a good one worse. */
                int64_t base = (int64_t)(sec - t->ref_old.sec);
                int64_t est = ((tick - t->ref_old.tick) * 65536) / base;
                int64_t off = est - NOMINAL_RATE_Q16;
                int64_t noise = ((int64_t)RATE_EARLY_NOISE_TICKS * 65536) / base;
                if (off > 2 * noise || -off > 2 * noise) t->rate_q16 = est;
            }
            if (!t->have_new && sec - t->ref_old.sec >= REF_ROTATE_S) {
                t->ref_new.tick = tick; t->ref_new.sec = sec; t->have_new = 1;
            } else if (t->have_new && sec - t->ref_new.sec >= REF_ROTATE_S) {
                t->ref_old = t->ref_new;
                t->ref_new.tick = tick; t->ref_new.sec = sec;
            }
            t->ncand = 0;
            t->n_ok++;
            r = TK_ACCEPTED;
        } else {
            /* A running clock hardly ever needs a real step (holdover drift
             * is ms per day), while miscorrected frames with the same wrong
             * offset could in principle agree in pairs; so a step needs
             * three consistent frames, the first sync only two. */
            add_cand(t, tick, sec);
            if (cand_confirmations(t) >= STEP_CONFIRMATIONS) {
                sync_to(t, tick, sec);
                t->n_steps++;
                r = TK_STEPPED;
            } else {
                t->n_bad++;
                return TK_REJECTED;
            }
        }
    } else {
        add_cand(t, tick, sec);
        if (cand_confirmations(t) < 1) return TK_CANDIDATE;
        sync_to(t, tick, sec);
        r = TK_SYNCED;
    }
    t->tz = fi->tz;
    t->tzc = fi->tzc;
    t->sk = fi->sk;
    set_leap(t, fi, sec);
    return r;
}

void tk_maintain(tk_t *t, int64_t now_tick)
{
    uint32_t sec;
    if (!t->synced) return;

    /* leap second: the transmitted count excludes leap seconds, so the
     * count simply pauses (insert) or skips (delete) at the boundary */
    if (t->leap_pending && tk_time(t, now_tick, &sec, 0) && sec >= t->leap_at) {
        int64_t at = tk_second_tick(t, t->leap_at);
        t->anchor_tick = at;
        /* insert: our clock reads L when the true count is still L-1;
         * delete: our clock reads L when the true count is already L+1 */
        t->anchor_sec = (uint32_t)((int32_t)t->leap_at - t->leap_pending);
        t->leap_pending = 0;
        t->have_old = t->have_new = 0;
    }

    /* keep the anchor recent so 64-bit products never overflow in holdover */
    if (now_tick - t->anchor_tick > 3600LL * FCY_HZ) {
        uint32_t s = t->anchor_sec + 3000;
        t->anchor_tick = tk_second_tick(t, s);
        t->anchor_sec = s;
    }
}
