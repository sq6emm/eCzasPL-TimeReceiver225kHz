/*
 * Host test harness: runs the firmware's DSP, frame decoder and timekeeper on
 * a recording (raw signed 16-bit little-endian mono, 10 kHz).
 *
 *   sim [options] file.raw
 *     -n SNR_dB   add white Gaussian noise (relative to input RMS)
 *     -f HZ       Rayleigh-ish fading rate (multiplicative, 0 = off)
 *     -i RATE     impulsive noise bursts per second (atmospherics)
 *     -s SEED     random seed
 *     -x PPM      crystal error of the receiver's clock (ticks run PPM fast)
 *     -r T0 N0    truth: the frame starting at T0 seconds is number N0 (3 s
 *                 periods); every frame the timekeeper accepts is checked
 *                 against it and wrong ones are counted ("clock_wrong")
 *     -C          no expected-frame confirmation of undecodable frames
 *     -q          summary only
 *   sim -t        codec / date self-test, captured-frame regression
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dsp.h"
#include "frame.h"
#include "timekeeper.h"
#include "nmea.h"

static uint64_t rng_state = 88172645463325252ULL;
static double urand(void)
{
    rng_state ^= rng_state << 13; rng_state ^= rng_state >> 7; rng_state ^= rng_state << 17;
    return (rng_state >> 11) * (1.0 / 9007199254740992.0);
}
static double grand(void)
{
    double u = urand() + 1e-300, v = urand();
    return sqrt(-2 * log(u)) * cos(2 * M_PI * v);
}

/*
 * Regression: frames captured off-air on 2026-09-25 (weak-signal bench) from
 * the ORIGINAL firmware's $PGGUM sentences, i.e. frame bytes 3..11 as hard
 * decisions, before correction. None is a valid frame. The last one "passes"
 * RS(15,9) by correcting 3 symbols but its CRC is wrong (it also claims a leap
 * second); the original firmware, which never checks the CRC, set its clock
 * from it to 2026-10-04 15:12:20 - 9 days ahead - at 12:43:56 UTC.
 */
static const uint8_t captured_frames[][9] = {
    { 0xA6, 0x66, 0xD3, 0x4B, 0x7B, 0x90, 0x8A, 0x27, 0xBD },  /* 12:24:38 */
    { 0xA2, 0x2E, 0xD3, 0x79, 0x1B, 0xC9, 0xD7, 0xE5, 0x80 },  /* 12:27:41 */
    { 0xE6, 0x2E, 0xD2, 0x38, 0x1A, 0x51, 0xB1, 0x7E, 0x12 },  /* 12:28:56, bad 101 */
    { 0xA4, 0x2E, 0x96, 0x32, 0x8E, 0x4B, 0xAC, 0x48, 0xCF },  /* 12:31:56 */
    { 0xA3, 0x2E, 0x93, 0x61, 0x9B, 0x7F, 0x93, 0x4D, 0x3C },  /* 12:33:17 */
    { 0x26, 0x66, 0xD3, 0x8B, 0x0F, 0x79, 0xBF, 0x6A, 0xAC },  /* 12:42:05, bad 101 */
    { 0xA2, 0x24, 0xD3, 0xBA, 0x5B, 0xEF, 0x17, 0x62, 0x6D },  /* 12:43:56, RS "ok" */
};
#define N_CAPTURED (int)(sizeof captured_frames / sizeof captured_frames[0])
#define MISCORRECTED_N3 281480646UL   /* 2026-10-04 15:12:18, what RS alone yields */

static int captured_frames_test(void)
{
    static const uint8_t pre[3] = { 0x55, 0x55, 0x60 };
    uint8_t bits[FRAME_BITS], cw[15];
    frame_info_t fi;
    tk_t tk;
    tk_result_t r1, r2, r3, r4;
    const int64_t t0 = 1000LL * FCY_HZ;
    const uint32_t n3 = 281218475UL;              /* 2026-09-25 12:43:45 */
    int i, k, j, fails = 0, rejected = 0, rs_fixed;

    for (i = 0; i < N_CAPTURED; i++) {
        for (k = 0; k < FRAME_BITS; k++) {
            uint8_t byte = k < 24 ? pre[k / 8] : captured_frames[i][k / 8 - 3];
            bits[k] = (byte >> (7 - k % 8)) & 1;
        }
        if (i == N_CAPTURED - 1) {
            /* RS alone must accept it, so it really is the CRC that rejects it */
            for (k = 0; k < 9; k++)
                for (cw[6 + k] = 0, j = 0; j < 4; j++) cw[6 + k] = (uint8_t)((cw[6 + k] << 1) | bits[27 + 4 * k + j]);
            for (k = 0; k < 6; k++)
                for (cw[k] = 0, j = 0; j < 4; j++) cw[k] = (uint8_t)((cw[k] << 1) | bits[64 + 4 * k + j]);
            rs_fixed = frame_rs_decode(cw);
            if (rs_fixed != 3) { printf("captured #%d: RS fixed %d, expected 3\n", i, rs_fixed); fails++; }
        }
        if (frame_check_bits(bits, &fi)) { printf("captured #%d: ACCEPTED, must be rejected\n", i); fails++; }
        else rejected++;
    }

    /* Even if such a frame got past the CRC, a synchronised clock must not follow it. */
    tk_init(&tk);
    frame_build(n3, 2, 0, bits);
    frame_check_bits(bits, &fi);
    r1 = tk_frame(&tk, t0, &fi);
    frame_build(n3 + 2, 2, 0, bits);
    frame_check_bits(bits, &fi);
    r2 = tk_frame(&tk, t0 + 6LL * FCY_HZ, &fi);
    memset(&fi, 0, sizeof fi);
    fi.n3 = MISCORRECTED_N3; fi.tz = 2; fi.ls = 1;
    r3 = tk_frame(&tk, t0 + 9LL * FCY_HZ, &fi);
    frame_build(n3 + 4, 2, 0, bits);
    frame_check_bits(bits, &fi);
    r4 = tk_frame(&tk, t0 + 12LL * FCY_HZ, &fi);
    if (r1 != TK_CANDIDATE || r2 != TK_SYNCED || r3 != TK_REJECTED || r4 != TK_ACCEPTED) {
        printf("captured: timekeeper results %d %d %d %d, expected %d %d %d %d\n",
               r1, r2, r3, r4, TK_CANDIDATE, TK_SYNCED, TK_REJECTED, TK_ACCEPTED);
        fails++;
    }
    printf("captured frames (2026-09-25): %d/%d rejected by decoder, miscorrected time %s by timekeeper: %s\n",
           rejected, N_CAPTURED, r3 == TK_REJECTED ? "rejected" : "NOT rejected", fails ? "FAIL" : "ok");
    return fails;
}

/*
 * Synthetic signal in the conditions the bench receiver met on 2026-09-25:
 * the 1 kHz tone offset by +14 Hz (SI4735 crystal), noise, and an
 * overdriven, clipping ADC. One time frame every 3 s; bit '0' is a -32 deg
 * phase step with the observed 16 ms slew. Returns the seconds until the
 * carrier loop tracks, the learned '0' level, decoded frames, frames that
 * decoded to a wrong time (Chase/RS miscorrections, see frame.c) and wrong
 * times the timekeeper let into the clock (must be none).
 */
typedef struct { double lock_s; int lvl0_deg, ok, wrong, cand, clock_wrong; } synth_res_t;

static void run_synth(double snr_db, double offset_hz, double gain, double secs, synth_res_t *res)
{
    static dsp_t dsp;
    tk_t tk;
    uint8_t bits[FRAME_BITS];
    int16_t blk[DSP_BLOCK];
    const double amp = 12000.0, lvl0 = -32.0 * M_PI / 180.0;
    const double sigma = amp / sqrt(2.0) / pow(10, snr_db / 20);
    const long spf = 3 * ADC_FS_HZ, spb = ADC_FS_HZ / BIT_RATE, ramp = RAMP_BLOCKS * DSP_BLOCK;
    const uint32_t n3_0 = 281218475UL;
    double ph = 0, prev_lvl = 0, cur_lvl = 0;
    long n, total = (long)(secs * ADC_FS_HZ), k;
    int built = -1;

    memset(res, 0, sizeof *res);
    res->lock_s = -1;
    dsp_init(&dsp);
    tk_init(&tk);
    for (n = 0; n < total; n += DSP_BLOCK) {
        for (k = 0; k < DSP_BLOCK; k++) {
            long t = n + k, f = t / spf, in = t % spf, b = in / spb, ib = in % spb;
            double target, v, lv;
            if ((int)f != built) { frame_build(n3_0 + (uint32_t)f, 2, 0, bits); built = (int)f; }
            target = (b < FRAME_BITS && !bits[b]) ? lvl0 : 0.0;
            if (ib == 0) { prev_lvl = cur_lvl; cur_lvl = target; }
            lv = ib < ramp ? prev_lvl + (cur_lvl - prev_lvl) * (ib + 1) / ramp : cur_lvl;
            ph += 2 * M_PI * (AUDIO_CARRIER_HZ + offset_hz) / ADC_FS_HZ;
            v = gain * (amp * sin(ph + lv) + grand() * sigma);
            if (v > 32767) v = 32767;
            if (v < -32768) v = -32768;
            blk[k] = (int16_t)v;
        }
        dsp_block(&dsp, blk);
        if (res->lock_s < 0 && dsp.pll_state == PLL_TRACK) res->lock_s = (double)n / ADC_FS_HZ;
        if (dsp.cand_ready) {
            frame_info_t fi;
            /* frame start in samples -> which frame was sent then */
            long start = (long)(dsp.cand.start_block - 1) * DSP_BLOCK;
            uint32_t expect = n3_0 + (uint32_t)((start + spf / 2) / spf);
            res->cand++;
            if (frame_decode(dsp.cand.ph + CAND_PRE, &fi) == FR_OK) {
                int64_t tick = (int64_t)start * TICKS_PER_SAMPLE + (int64_t)fi.timing_q15 * TICKS_PER_BLOCK / 32768;
                tk_result_t r = tk_frame(&tk, tick, &fi);
                if (fi.n3 == expect) res->ok++; else res->wrong++;
                if ((r == TK_ACCEPTED || r == TK_SYNCED || r == TK_STEPPED) && fi.n3 != expect) res->clock_wrong++;
            }
            dsp.cand_ready = 0;
        }
    }
    res->lvl0_deg = (int)lround(dsp.lvl0 * 360.0 / 65536);
}

static int synth_test(void)
{
    /* lock / frame limits are well inside what 2.0.3 does and fail the
     * 2.0.0 carrier search (-8 dB: no lock) and 2.0.2 '0' level (-8 dB: -59 deg) */
    static const struct { double snr, gain, max_lock_s; int min_ok, check_lvl; } C[] = {
        {  6, 2.5, 10, 40, 1 },   /* strong and clipping, like the bench before 2.0.2 */
        {  0, 1.0, 15, 10, 1 },
        { -3, 1.0, 25,  5, 1 },
        { -8, 1.0, 60,  0, 0 },   /* no decoding expected; must still lock, level must not drift */
    };
    int i, fails = 0;
    for (i = 0; i < (int)(sizeof C / sizeof C[0]); i++) {
        synth_res_t r;
        int bad;
        run_synth(C[i].snr, 14.0, C[i].gain, 180, &r);
        bad = r.lock_s < 0 || r.lock_s > C[i].max_lock_s || r.ok < C[i].min_ok || r.clock_wrong ||
              (C[i].check_lvl ? (r.lvl0_deg < -40 || r.lvl0_deg > -24) : r.lvl0_deg < -50);
        printf("synthetic %+3.0f dB, +14 Hz%s: tracking after %.1f s, '0' level %d deg, %d/%d frames ok, "
               "%d miscorrected (%d into the clock): %s\n",
               C[i].snr, C[i].gain > 1 ? ", clipping" : "", r.lock_s, r.lvl0_deg, r.ok, r.cand,
               r.wrong, r.clock_wrong, bad ? "FAIL" : "ok");
        fails += bad;
    }
    return fails;
}

static int selftest(void)
{
    uint8_t bits[FRAME_BITS];
    frame_info_t fi;
    int t, fails = 0, wrong = 0, ok = 0;
    tk_date_t d;

    tk_to_date(3UL * 258787930UL, &d);
    printf("N=258787930 -> %04d-%02d-%02d %02d:%02d:%02d (expect 2024-08-07 16:36:30)\n",
           d.year, d.mon, d.day, d.hour, d.min, d.sec);
    if (d.year != 2024 || d.mon != 8 || d.day != 7 || d.hour != 16 || d.min != 36 || d.sec != 30) fails++;
    for (t = 0; t < 200000; t++) {
        uint32_t s = (uint32_t)(urand() * 4.0e9);
        tk_to_date(s, &d);
        if (tk_from_date(d.year, d.mon, d.day) + d.hour * 3600UL + d.min * 60UL + d.sec != s) { fails++; break; }
    }
    for (t = 0; t < 20000; t++) {
        uint32_t n3 = (uint32_t)(urand() * (1 << 30));
        int nerr = t % 5, k;
        frame_build(n3, (uint8_t)(t & 3), (uint8_t)(t % 32), bits);
        for (k = 0; k < nerr; k++) {                 /* corrupt nerr symbols */
            int sym = (int)(urand() * 15), j;
            int base = sym < 9 ? 27 + 4 * sym : 64 + 4 * (sym - 9);
            int v = 1 + (int)(urand() * 15);
            for (j = 0; j < 4; j++) bits[base + j] ^= (v >> (3 - j)) & 1;
        }
        memset(&fi, 0, sizeof fi);
        if (frame_check_bits(bits, &fi)) {
            if (fi.n3 == n3 && fi.tz == (t & 3)) ok++; else wrong++;
        } else if (nerr <= 3) {
            /* two corrupted picks may hit the same symbol - still <= 3 */
            fails++;
        }
    }
    printf("codec: ok %d wrong %d failures %d\n", ok, wrong, fails);
    {   /* errors-and-erasures RS: every pattern with 2e + f <= 6 must decode exactly */
        int bad = 0, tried = 0;
        for (t = 0; t < 20000; t++) {
            uint8_t cw[15], rx[15], er[6];
            int f = t % 7, e = (6 - f) / 2, k, j, used[15] = { 0 }, r;
            for (k = 6; k < 15; k++) cw[k] = (uint8_t)(urand() * 16);
            frame_rs_encode(cw);
            memcpy(rx, cw, 15);
            for (k = 0; k < f + e; k++) {
                do j = (int)(urand() * 15); while (used[j]);
                used[j] = 1;
                if (k < f) { er[k] = (uint8_t)j; rx[j] = (uint8_t)(urand() * 16); }
                else rx[j] ^= (uint8_t)(1 + urand() * 15);
            }
            r = frame_rs_decode_erasures(rx, er, f);
            tried++;
            if (r < 0 || memcmp(rx, cw, 15)) bad++;
        }
        printf("rs errors+erasures: %d/%d wrong\n", bad, tried);
        fails += bad;
    }
    fails += captured_frames_test();
    {   /* a synced clock steps only on three consistent frames, never on two */
        tk_t tk2;
        uint8_t b2[FRAME_BITS];
        frame_info_t f2;
        const int64_t T = 1000LL * FCY_HZ;
        const uint32_t n0 = 281218475UL, off = 1000;   /* wrong frames: 50 min ahead */
        tk_result_t r[6];
        int q;
        tk_init(&tk2);
        for (q = 0; q < 6; q++) {
            uint32_t n = n0 + (uint32_t)q + (q >= 2 ? off : 0);
            frame_build(n, 2, 0, b2);
            frame_check_bits(b2, &f2);
            r[q] = tk_frame(&tk2, T + (int64_t)q * 3 * FCY_HZ, &f2);
        }
        /* 0,1: sync; 2,3: two agreeing wrong frames -> rejected; 4: third -> step */
        q = r[0] == TK_CANDIDATE && r[1] == TK_SYNCED && r[2] == TK_REJECTED &&
            r[3] == TK_REJECTED && r[4] == TK_STEPPED && r[5] == TK_ACCEPTED;
        printf("step needs three consistent frames: %s\n", q ? "ok" : "FAIL");
        fails += !q;
    }
    fails += synth_test();
    return fails || wrong > 20;
}

/* The firmware's expected-frame confirmation (main.c handle_candidate). */
static int confirm(dsp_t *dsp, tk_t *tk, const frame_info_t *last, int64_t tick,
                   double ref_t0, uint32_t ref_n0, double tsec, int *wrong, frame_info_t *synced_fi)
{
    uint32_t n3p;
    uint8_t bits[FRAME_BITS];
    int32_t tq = 0;
    frame_info_t fe;
    if (!tk->synced) {                 /* before the first sync: candidates' predictions */
        uint8_t tz, flags, idx;
        if (!tk_candidate_expected(tk, tick, &n3p, &tz, &flags, &idx)) return 0;
        frame_build(n3p, tz, flags, bits);
        if (frame_confirm(dsp->cand.ph + CAND_PRE, bits, &tq) < CONFIRM_MIN_Q10) return 0;
        if (tk_candidate_confirmed(tk, idx, tick + (int64_t)tq * TICKS_PER_BLOCK / 32768, n3p) == TK_SYNCED) {
            memset(synced_fi, 0, sizeof *synced_fi);
            synced_fi->n3 = n3p; synced_fi->tz = tz;
            synced_fi->ls = flags & 1; synced_fi->lss = (flags >> 1) & 1; synced_fi->tzc = (flags >> 2) & 1;
            synced_fi->sk = (uint8_t)((flags >> 3) & 3);
            if (ref_t0 >= 0 && n3p != ref_n0 + (uint32_t)lround((tsec - ref_t0) / 3.0)) (*wrong)++;
            return 2;
        }
        return 1;
    }
    if (!last) return 0;
    if (!tk_expected_n3(tk, tick, &n3p)) return 0;
    frame_build(n3p, last->tz, (uint8_t)(last->ls | last->lss << 1 | last->tzc << 2 |
                (last->sk & 1) << 3 | (last->sk >> 1) << 4), bits);
    if (frame_confirm(dsp->cand.ph + CAND_PRE, bits, &tq) < CONFIRM_MIN_Q10) return 0;
    fe = *last;
    fe.n3 = n3p;
    if (tk_frame(tk, tick + (int64_t)tq * TICKS_PER_BLOCK / 32768, &fe) != TK_ACCEPTED) return 0;
    if (ref_t0 >= 0 && n3p != ref_n0 + (uint32_t)lround((tsec - ref_t0) / 3.0)) (*wrong)++;
    return 1;
}

int main(int argc, char **argv)
{
    static dsp_t dsp;
    tk_t tk;
    FILE *f;
    int16_t blk[DSP_BLOCK];
    double snr = 1e9, fade_hz = 0, imp_rate = 0, rms = 0;
    int quiet = 0, a;
    const char *path = NULL;
    long nsamp = 0;
    int16_t *all;
    long i, cap = 1 << 20;
    int n_cand = 0, n_ok = 0, n_nottime = 0, n_fail = 0;
    int n_tk[5] = { 0 };
    double sum_err = 0, sum_err2 = 0; int n_err = 0;
    double fade_ph = 0, fade_a = 1;
    double xtal_ppm = 0, ref_t0 = -1; uint32_t ref_n0 = 0; int clock_wrong = 0; double first_sync = -1;
    int n_conf = 0, conf_wrong = 0, no_confirm = 0, cr, n_presync = 0; frame_info_t last_fi, sfi; int have_last = 0;

    for (a = 1; a < argc; a++) {
        if (!strcmp(argv[a], "-t")) return selftest();
        else if (!strcmp(argv[a], "-n")) snr = atof(argv[++a]);
        else if (!strcmp(argv[a], "-f")) fade_hz = atof(argv[++a]);
        else if (!strcmp(argv[a], "-i")) imp_rate = atof(argv[++a]);
        else if (!strcmp(argv[a], "-s")) rng_state ^= (uint64_t)atoll(argv[++a]) * 0x9E3779B97F4A7C15ULL;
        else if (!strcmp(argv[a], "-x")) xtal_ppm = atof(argv[++a]);
        else if (!strcmp(argv[a], "-r")) { ref_t0 = atof(argv[++a]); ref_n0 = (uint32_t)atol(argv[++a]); }
        else if (!strcmp(argv[a], "-C")) no_confirm = 1;
        else if (!strcmp(argv[a], "-q")) quiet = 1;
        else path = argv[a];
    }
    if (!path || !(f = fopen(path, "rb"))) { fprintf(stderr, "usage: sim [-n snr] file.raw\n"); return 2; }
    all = malloc(cap * sizeof(int16_t));
    while (fread(&all[nsamp], 2, 1, f) == 1) {
        rms += (double)all[nsamp] * all[nsamp];
        if (++nsamp == cap) { cap *= 2; all = realloc(all, cap * sizeof(int16_t)); }
    }
    fclose(f);
    rms = sqrt(rms / nsamp);

    dsp_init(&dsp);
    tk_init(&tk);
    for (i = 0; i + DSP_BLOCK <= nsamp; i += DSP_BLOCK) {
        int k;
        for (k = 0; k < DSP_BLOCK; k++) {
            double v = all[i + k];
            if (fade_hz > 0) {
                /* slow random-walk fading of amplitude, 0.05 .. 1 */
                fade_ph += 2 * M_PI * fade_hz / ADC_FS_HZ;
                fade_a = 0.525 + 0.475 * sin(fade_ph + 0.7 * sin(fade_ph * 0.37));
                v *= fade_a;
            }
            if (snr < 1e8) v += grand() * rms / pow(10, snr / 20);
            if (imp_rate > 0 && urand() < imp_rate / ADC_FS_HZ) v += (urand() - 0.5) * 40 * rms;
            v *= 0.5;                                  /* headroom like the real ADC */
            if (v > 32767) v = 32767;
            if (v < -32768) v = -32768;
            blk[k] = (int16_t)v;
        }
        dsp_block(&dsp, blk);
        if (dsp.cand_ready) {
            frame_info_t fi;
            frame_status_t st = frame_decode(dsp.cand.ph + CAND_PRE, &fi);
            int64_t tick;
            if (st == FR_OK)
                tick = ((int64_t)(dsp.cand.start_block - 1) * DSP_BLOCK) * TICKS_PER_SAMPLE
                     + (int64_t)fi.timing_q15 * TICKS_PER_BLOCK / 32768;
            else
                tick = ((int64_t)(dsp.cand.start_block - 1) * DSP_BLOCK) * TICKS_PER_SAMPLE
                     + (int64_t)dsp.cand.frac_q15 * TICKS_PER_BLOCK / 32768;
            tick += (int64_t)((double)tick * xtal_ppm * 1e-6);   /* receiver crystal error */
            int64_t tick_base = ((int64_t)(dsp.cand.start_block - 1) * DSP_BLOCK) * TICKS_PER_SAMPLE;
            tick_base += (int64_t)((double)tick_base * xtal_ppm * 1e-6);
            double tsec = (double)tick / FCY_HZ;
            n_cand++;
            if (st == FR_OK) {
                tk_result_t r = tk_frame(&tk, tick, &fi);
                if (r == TK_ACCEPTED || r == TK_SYNCED || r == TK_STEPPED) {
                    last_fi = fi; have_last = 1;
                    if (first_sync < 0) first_sync = tsec;
                    if (ref_t0 >= 0 && fi.n3 != ref_n0 + (uint32_t)lround((tsec - ref_t0) / 3.0)) clock_wrong++;
                }
                static const char *RN[] = { "ACCEPT", "SYNC", "STEP", "CAND", "REJECT" };
                tk_date_t d;
                n_ok++; n_tk[r]++;
                tk_to_date(3 * fi.n3, &d);
                if (r == TK_ACCEPTED) {
                    sum_err += tk.last_err_us; sum_err2 += (double)tk.last_err_us * tk.last_err_us; n_err++;
                }
                if (!quiet)
                    printf("%9.4f corr=%.2f N=%u %04d-%02d-%02d %02d:%02d:%02d tz=%d rs=%d chase=%d sk1fix=%d snr=%ddB %s err=%dus f=%.2fHz\n",
                           tsec, dsp.cand.corr_q10 / 1024.0, fi.n3, d.year, d.mon, d.day, d.hour, d.min, d.sec,
                           fi.tz, fi.rs_fixed, fi.chase_flips, fi.sk1_fixed, fi.snr_db, RN[r],
                           tk.last_err_us, dsp_carrier_centihz(&dsp) / 100.0);
            } else if (st == FR_UNCORRECTABLE && !no_confirm &&
                       (cr = confirm(&dsp, &tk, have_last ? &last_fi : NULL, tick_base,
                                     ref_t0, ref_n0, tsec, &conf_wrong, &sfi)) != 0) {
                n_conf++;
                if (cr == 2) { last_fi = sfi; have_last = 1; if (first_sync < 0) first_sync = tsec; n_presync++; }
            } else if (st == FR_NOT_TIME) {
                n_nottime++;
            } else {
                n_fail++;
                if (!quiet)
                    printf("%9.4f corr=%.2f decode failed (%d) snr=%d\n", tsec, dsp.cand.corr_q10 / 1024.0, st, fi.snr_db);
            }
            dsp.cand_ready = 0;
        }
        {
            int64_t now = (int64_t)(i + DSP_BLOCK) * TICKS_PER_SAMPLE;
            tk_maintain(&tk, now + (int64_t)((double)now * xtal_ppm * 1e-6));
        }
    }
    printf("SUMMARY snr=%g cand=%d time_ok=%d other=%d failed=%d accept=%d sync=%d step=%d cand=%d reject=%d "
           "jitter_rms=%.0fus err_mean=%.0fus rate=%dppb first_sync=%.0fs clock_wrong=%d confirmed=%d confirm_wrong=%d presync=%d\n",
           snr, n_cand, n_ok, n_nottime, n_fail, n_tk[0], n_tk[1], n_tk[2], n_tk[3], n_tk[4],
           n_err ? sqrt(sum_err2 / n_err - (sum_err / n_err) * (sum_err / n_err)) : 0.0,
           n_err ? sum_err / n_err : 0.0, tk_rate_ppb(&tk), first_sync, clock_wrong, n_conf, conf_wrong, n_presync);
    free(all);
    return 0;
}
