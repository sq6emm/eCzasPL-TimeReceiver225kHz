/*
 * e-CzasPL 225 kHz time receiver firmware - main loop.
 *
 * SV1 (UART1, 115200 8N1): human readable diagnostics, one event per line:
 *   [   123.4] SIGNAL  audio level and volume, carrier, radio RSSI/SNR, R27 advice
 *   [   123.4] FRAME   every time frame heard and how it was decoded
 *   [   123.4] CLOCK   synchronisation / step / rejected-frame events
 *   [   123.4] STATUS  every STATUS_PERIOD_S seconds
 *   [   123.4] RADIO   SI4735 events
 *   The number in brackets is the uptime in seconds.
 * SV2 (UART2, 115200 8N1): NMEA
 *   $GPRMC  every second; its '$' leaves at the 1PPS edge. Status 'A' only
 *           while synchronised and the last good frame is < 24 h old.
 *   $PGGUM  after each accepted frame: frame bytes 3..11 and the age of the
 *           previous fix in seconds (as the original firmware).
 *   $PECZ   copy of the SV1 diagnostics (DEBUG_TO_NMEA), sent between the
 *           $GPRMC sentences so they never delay them.
 */
#include <stdio.h>
#include <string.h>
#include "board.h"
#include "uart.h"
#include "i2c.h"
#include "si4735.h"
#include "sampler.h"
#include "frame.h"
#include "timekeeper.h"
#include "nmea.h"

static tk_t tk;
static char pending_extra[64];
static uint16_t pending_extra_len;
static uint32_t n_heard, n_undecodable, n_confirmed;
static frame_info_t last_fi;               /* last frame the clock accepted */
static uint8_t have_last_fi;

static int64_t now_tick(void)
{
    return (int64_t)sampler_blocks() * TICKS_PER_BLOCK;
}

/* "[   123.4] TAG     " */
static void log_head(const char *tag)
{
    uint32_t ds = (uint32_t)(sampler_blocks() / (DSP_RATE_HZ / 10));
    dbg_printf("[%7lu.%lu] %-7s ", (unsigned long)(ds / 10), (unsigned long)(ds % 10), tag);
}

static void put_date(uint32_t sec, int8_t tz)
{
    tk_date_t d;
    tk_to_date(sec, &d);
    dbg_printf("%04u-%02u-%02u %02u:%02u:%02u UTC", d.year, d.mon, d.day, d.hour, d.min, d.sec);
    if (tz >= 0) {
        tk_to_date(sec + 3600UL * tz, &d);
        dbg_printf(" (%02u:%02u:%02u local, UTC+%d)", d.hour, d.min, d.sec, tz);
    }
}

static void put_signed_ms(int32_t us)
{
    uint32_t a = (uint32_t)(us < 0 ? -us : us);
    dbg_printf("%c%lu.%02lu ms", us < 0 ? '-' : '+', (unsigned long)(a / 1000), (unsigned long)(a % 1000 / 10));
}

static uint16_t build_pggum(char *out, const frame_info_t *fi, uint32_t age)
{
    char *p = out;
    uint8_t cs = 0;
    const char *q;
    int i;
    p += sprintf(p, "$PGGUM,");
    for (i = 3; i < 12; i++) p += sprintf(p, "%02X,", fi->raw[i]);
    p += sprintf(p, "%lu", (unsigned long)age);
    for (q = out + 1; q < p; q++) cs ^= (uint8_t)*q;
    p += sprintf(p, "*%02X\r\n", cs);
    return (uint16_t)(p - out);
}

/* ---- audio level control and signal report -------------------------------- */

/* worst level / clip count over the last report period */
static uint16_t rep_pct, rep_clip;
static int16_t rep_centre;               /* last second, % of full range */
static uint8_t lvl_low_s;

/* once a second: measure the ADC swing and step the SI4735 volume */
static void level_control(void)
{
    int32_t pp = (int32_t)g_dsp.adc_max - g_dsp.adc_min;   /* 65536 = full range */
    uint16_t pct = (uint16_t)(pp * 100 / 65536), clip = g_dsp.adc_clip;
    uint8_t v = si4735_volume(), nv = v;

    rep_centre = (int16_t)(((int32_t)g_dsp.adc_max + g_dsp.adc_min) * 50 / 32768);
    dsp_level_reset(&g_dsp);
    if (pct > rep_pct) rep_pct = pct;
    rep_clip += clip;

    if (clip > LEVEL_CLIP_MAX)       nv = v > VOL_MIN + 2 ? v - 3 : VOL_MIN;
    else if (pct > LEVEL_HIGH_PCT)   nv = v > VOL_MIN ? v - 1 : VOL_MIN;
    if (pct < LEVEL_LOW_PCT && !clip) {
        if (++lvl_low_s >= LEVEL_UP_S) { lvl_low_s = 0; if (v < 63) nv = v + 1; }
    } else {
        lvl_low_s = 0;
    }
    if (nv != v) si4735_set_volume(nv);
}

static void signal_report(void)
{
    static const char *PLL[] = { "searching carrier", "locking (fast)", "locking", "locked" };
    si4735_rsq_t q;
    uint16_t pct = rep_pct, clip = rep_clip;
    uint8_t vol = si4735_volume();
    int32_t f = dsp_carrier_centihz(&g_dsp), df = f - AUDIO_CARRIER_HZ * 100L;
    char bar[21];
    uint8_t i;

    rep_pct = 0;
    rep_clip = 0;
    for (i = 0; i < 20; i++) bar[i] = (i < pct / 5) ? '#' : '-';
    bar[20] = 0;

    log_head("SIGNAL");
    dbg_printf("level %3u%% [%s] ", pct, bar);
    if (clip)            dbg_printf("CLIPPING (%u samples)", clip);
    else if (pct < 10)   dbg_puts("NO/VERY LOW AUDIO");
    else if (pct < 30)   dbg_puts("low");
    else if (pct <= 85)  dbg_puts("OK");
    else                 dbg_puts("high");
    dbg_printf(", volume %u/63", vol);
    if (clip > LEVEL_CLIP_MAX && vol <= VOL_MIN) dbg_puts(" - decrease gain R27");
    else if (pct < 30 && vol >= 63)              dbg_puts(pct < 10 ? " - check SI4735, increase gain R27" : " - increase gain R27");
    dbg_printf(", centre %d%%", (int)rep_centre);
    dbg_printf(" | carrier %ld.%02ld Hz (%c%ld.%02ld) %s", (long)(f / 100), (long)(f % 100),
               df < 0 ? '-' : '+', (long)((df < 0 ? -df : df) / 100), (long)((df < 0 ? -df : df) % 100),
               PLL[g_dsp.pll_state]);
    if (g_dsp.pll_state == PLL_TRACK)
        dbg_printf(", '0' bits at %d deg, noise %d deg",
                   (int)((int32_t)g_dsp.lvl0 * 360 / 65536), (int)((g_dsp.lock_err_avg >> 4) * 360 / 65536));
    if (si4735_rsq(&q) == 0) dbg_printf(" | radio RSSI %u dBuV SNR %u dB\r\n", q.rssi, q.snr);
    else dbg_puts(" | radio NOT RESPONDING\r\n");
}

/* ---- frames --------------------------------------------------------------- */

static void handle_candidate(void)
{
    dsp_cand_t *c = &g_dsp.cand;
    frame_info_t fi;
    frame_status_t st;
    int64_t tick = ((int64_t)(c->start_block - 1) * DSP_BLOCK) * TICKS_PER_SAMPLE;
    int i;

    memset(&fi, 0, sizeof fi);
    st = frame_decode(c->ph + CAND_PRE, &fi);
    if (st == FR_NOT_TIME) {               /* other services (Enea lighting etc.) */
        g_dsp.cand_ready = 0;
        return;
    }
    n_heard++;
    log_head("FRAME");
    dbg_printf("#%lu corr %d.%02d ", (unsigned long)n_heard, c->corr_q10 / 1024,
               (int)((int32_t)(c->corr_q10 % 1024) * 100 / 1024));   /* int is 16 bits here */
    if (st == FR_UNCORRECTABLE && !tk.synced) {
        /* not synchronised yet: does it match what a stored decoded frame predicts? */
        uint32_t n3p;
        uint8_t tz, flags, idx;
        if (tk_candidate_expected(&tk, tick, &n3p, &tz, &flags, &idx)) {
            uint8_t bits[FRAME_BITS];
            int32_t tq = 0;
            int16_t sc;
            frame_build(n3p, tz, flags, bits);
            sc = frame_confirm(c->ph + CAND_PRE, bits, &tq);
            if (sc >= CONFIRM_MIN_Q10) {
                tk_result_t r = tk_candidate_confirmed(&tk, idx, tick + (int64_t)tq * TICKS_PER_BLOCK / 32768, n3p);
                n_confirmed++;
                dbg_printf("- not decodable, but %d.%02d of it agrees with the frame predicted by an earlier one (snr %d dB): ",
                           sc / 1024, (int)((int32_t)(sc % 1024) * 100 / 1024), fi.snr_db);
                put_date(3UL * n3p, tz);
                dbg_puts("\r\n");
                log_head("CLOCK");
                if (r == TK_SYNCED) {
                    memset(&last_fi, 0, sizeof last_fi);    /* content for later confirmations */
                    last_fi.n3 = n3p; last_fi.tz = tz;
                    last_fi.ls = flags & 1; last_fi.lss = (flags >> 1) & 1; last_fi.tzc = (flags >> 2) & 1;
                    last_fi.sk = (uint8_t)((flags >> 3) & 3);
                    have_last_fi = 1;
                    dbg_puts("SYNCHRONISED - one decoded frame confirmed by two more, time set to ");
                    put_date(3UL * n3p, -1);
                    dbg_puts("\r\n");
                } else {
                    dbg_puts("not synchronised yet - one more matching frame needed\r\n");
                }
                g_dsp.cand_ready = 0;
                return;
            }
        }
    }
    if (st == FR_UNCORRECTABLE && have_last_fi) {
        /* synchronised: compare with the frame the clock expects now */
        uint32_t n3p;
        if (tk_expected_n3(&tk, tick, &n3p)) {
            uint8_t bits[FRAME_BITS];
            int32_t tq = 0;
            int16_t sc;
            frame_build(n3p, last_fi.tz, (uint8_t)(last_fi.ls | last_fi.lss << 1 | last_fi.tzc << 2 |
                        (last_fi.sk & 1) << 3 | (last_fi.sk >> 1) << 4), bits);
            sc = frame_confirm(c->ph + CAND_PRE, bits, &tq);
            if (sc >= CONFIRM_MIN_Q10) {
                frame_info_t fe = last_fi;
                tk_result_t r;
                fe.n3 = n3p;
                r = tk_frame(&tk, tick + (int64_t)tq * TICKS_PER_BLOCK / 32768, &fe);
                n_confirmed++;
                dbg_printf("- not decodable, but %d.%02d of it agrees with the expected frame (snr %d dB): ",
                           sc / 1024, (int)((int32_t)(sc % 1024) * 100 / 1024), fi.snr_db);
                put_date(3UL * n3p, fe.tz);
                dbg_puts("\r\n");
                log_head("CLOCK");
                if (r == TK_ACCEPTED) {
                    dbg_puts("confirmed, error ");
                    put_signed_ms(tk.last_err_us);
                    dbg_printf(", xtal %+ld ppb\r\n", (long)tk_rate_ppb(&tk));
                } else {
                    dbg_puts("confirmation outside the window, ignored\r\n");
                }
                g_dsp.cand_ready = 0;
                return;
            }
        }
    }
    if (st != FR_OK) {
        n_undecodable++;
        dbg_printf("- not decodable (%s, snr %d dB)\r\n",
                   st == FR_NO_SIGNAL ? "no modulation" : "too many bit errors", fi.snr_db);
        g_dsp.cand_ready = 0;
        return;
    }
    tick += (int64_t)fi.timing_q15 * TICKS_PER_BLOCK / 32768;
    {
        uint32_t age = tk.synced ? (uint32_t)((tick - tk.last_ok_tick) / FCY_HZ) : 0;
        uint8_t was_synced = tk.synced;
        int32_t off_ms = 0;
        tk_result_t r;

        if (was_synced) {
            int64_t e = tick - tk_second_tick(&tk, 3UL * fi.n3);
            off_ms = (int32_t)(e / (FCY_HZ / 1000));
        }
        r = tk_frame(&tk, tick, &fi);
        dbg_printf("snr %d dB, fixed %u symbol(s)%s%s: ", fi.snr_db, fi.rs_fixed,
                   fi.chase_flips ? " + soft retry" : "", fi.sk1_fixed ? " + SK1 via CRC" : "");
        put_date(3UL * fi.n3, fi.tz);
        if (fi.ls) dbg_printf(" leap second %s announced", fi.lss ? "removal" : "insertion");
        if (fi.tzc) dbg_puts(" DST change announced");
        if (fi.sk) dbg_printf(" transmitter maintenance planned (%u)", fi.sk);
        dbg_puts("\r\n");

        switch (r) {
        case TK_ACCEPTED:
            log_head("CLOCK");
            dbg_puts("frame agrees with clock, error ");
            put_signed_ms(tk.last_err_us);
            dbg_printf(", xtal %+ld ppb\r\n", (long)tk_rate_ppb(&tk));
            break;
        case TK_SYNCED:
            log_head("CLOCK");
            dbg_puts("SYNCHRONISED - two frames agree, time set to ");
            put_date(3UL * fi.n3, -1);
            dbg_puts("\r\n");
            break;
        case TK_STEPPED:
            log_head("CLOCK");
            dbg_printf("STEPPED - clock was off by %ld ms, confirmed by three frames\r\n", (long)off_ms);
            break;
        case TK_CANDIDATE:
            log_head("CLOCK");
            dbg_puts("not synchronised yet - waiting for a second frame to confirm\r\n");
            break;
        default:
            log_head("CLOCK");
            dbg_printf("REJECTED - frame differs from clock by %ld ms (kept as candidate)\r\n", (long)off_ms);
            break;
        }
        if (r == TK_ACCEPTED || r == TK_SYNCED || r == TK_STEPPED) {
            last_fi = fi;
            have_last_fi = 1;
            g_led1_on_pps = 1;
            if (!pending_extra_len)
                pending_extra_len = build_pggum(pending_extra, &fi, age);
        }
    }
    log_head("RAW");
    for (i = 0; i < 12; i++) dbg_printf("%02X ", fi.raw[i]);
    dbg_puts("\r\n");
    g_dsp.cand_ready = 0;
}

/* ---- once per second: next PPS + NMEA ------------------------------------ */

static void schedule_second(void)
{
    int64_t now = now_tick();
    uint32_t sec, us, next;
    int64_t at;
    tk_date_t d;
    uint16_t n;

    tk_maintain(&tk, now);
    if (!tk_time(&tk, now, &sec, &us)) return;
    next = sec + 1;
    at = tk_second_tick(&tk, next);
    if (at - now < 20L * (FCY_HZ / 1000)) {        /* too close to arm safely */
        next++;
        at = tk_second_tick(&tk, next);
    }
    tk_to_date(next, &d);
    n = (uint16_t)nmea_rmc(g_nmea_next, &d, 1, tk_valid(&tk, now));
    if (pending_extra_len && n + pending_extra_len < NMEA_NEXT_SIZE) {
        memcpy(g_nmea_next + n, pending_extra, pending_extra_len);
        n += pending_extra_len;
        pending_extra_len = 0;
    }
    g_nmea_next_len = n;
    g_pps.tick = at;
    g_pps.sec = next;
    g_pps.state = PPS_REQUESTED;
}

static void status_report(void)
{
    int64_t now = now_tick();
    uint32_t sec;
    log_head("STATUS");
    if (tk_time(&tk, now, &sec, 0)) {
        uint32_t age = (uint32_t)((now - tk.last_ok_tick) / FCY_HZ);
        put_date(sec, tk.tz);
        dbg_printf(" %s, last good frame %lu s ago", tk_valid(&tk, now) ? "VALID" : "HOLDOVER (>24 h, not valid)",
                   (unsigned long)age);
    } else {
        dbg_puts("no time yet");
    }
    dbg_printf(" | frames heard %lu, undecodable %lu, accepted %lu (%lu by matching the expected frame), rejected %lu, steps %lu",
               (unsigned long)n_heard, (unsigned long)n_undecodable, (unsigned long)tk.n_ok,
               (unsigned long)n_confirmed, (unsigned long)tk.n_bad, (unsigned long)tk.n_steps);
    if (g_pps.missed || dbg_dropped())
        dbg_printf(" | PPS missed %u, log dropped %u", g_pps.missed, dbg_dropped());
    dbg_puts("\r\n");
}

int main(void)
{
    uint16_t last_sec_blk, last_sts_blk, last_lvl_blk, last_alc_blk;
    uint8_t radio_fail = 0;
    int r, tries;

    board_init();
    uart_init();
    dbg_puts("\r\n\r\n");
    log_head("BOOT");
    dbg_puts("e-CzasPL 225 kHz receiver, firmware " FW_VERSION " (" __DATE__ ")\r\n");
    log_head("BOOT");
    dbg_puts("SV1: diagnostics, SV2: NMEA $GPRMC (+$PGGUM, $PECZ), 115200 8N1; LED1 good frame, "
             "LED2 receiving, LED3 time valid, LED4 1PPS\r\n");
    i2c_init();
    tk_init(&tk);
    for (tries = 0; tries < 3; tries++) {
        r = si4735_init();
        if (!r) break;
        log_head("RADIO");
        dbg_printf("SI4735 init failed (%d), retrying\r\n", r);
    }
    if (!r) si4735_antenna_tune();
    sampler_start();
    RCONbits.SWDTEN = 1;
    log_head("RADIO");
    dbg_puts("tuned to 224 kHz USB, waiting for the 225 kHz carrier (1 kHz tone)\r\n");

    last_sec_blk = last_sts_blk = last_lvl_blk = last_alc_blk = sampler_blocks16();
    for (;;) {
        uint16_t b = sampler_blocks16();
        uint16_t level_period = sampler_blocks() < (uint64_t)LEVEL_FAST_S * DSP_RATE_HZ
                                ? DSP_RATE_HZ : STATUS_PERIOD_S * DSP_RATE_HZ;
        ClrWdt();

        if (g_dsp.cand_ready) handle_candidate();

        if (tk.synced) {
            if (g_pps.state == PPS_FIRED || g_pps.state == PPS_IDLE) {
                if (g_pps.state == PPS_FIRED) LED3(tk_valid(&tk, now_tick()));
                g_pps.state = PPS_IDLE;
                schedule_second();
            }
            /* diagnostics go to SV2 only between 40 and 700 ms after the PPS */
            {
                uint16_t since = (uint16_t)(b - g_pps_fired_blk16);
                if (since >= 20 && since <= 350) dbg_mirror_flush();
            }
        } else {
            if ((uint16_t)(b - last_sec_blk) >= DSP_RATE_HZ) {
                /* no time yet: empty RMC (and PGGUM, if any) once a second */
                char s[NMEA_MAX];
                last_sec_blk = b;
                nmea_write(s, (uint16_t)nmea_rmc(s, 0, 0, 0));
                if (pending_extra_len) { nmea_write(pending_extra, pending_extra_len); pending_extra_len = 0; }
                LED3(0);
            }
            dbg_mirror_flush();
        }

        if ((uint16_t)(b - last_alc_blk) >= DSP_RATE_HZ) {
            last_alc_blk = b;
            level_control();
        }
        if ((uint16_t)(b - last_lvl_blk) >= level_period) {
            last_lvl_blk = b;
            signal_report();
        }
        if ((uint16_t)(b - last_sts_blk) >= STATUS_PERIOD_S * DSP_RATE_HZ) {
            si4735_rsq_t q;
            last_sts_blk = b;
            status_report();
            radio_fail = si4735_rsq(&q) == 0 ? 0 : radio_fail + 1;
            if (radio_fail >= 3) {
                log_head("RADIO");
                dbg_puts("SI4735 not responding - reinitialising\r\n");
                si4735_init();
                radio_fail = 0;
            }
        }
    }
}
