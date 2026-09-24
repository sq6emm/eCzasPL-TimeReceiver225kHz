/*
 * Host test harness: runs the firmware's DSP, frame decoder and timekeeper on
 * a recording (raw signed 16-bit little-endian mono, 10 kHz).
 *
 *   sim [options] file.raw
 *     -n SNR_dB   add white Gaussian noise (relative to input RMS)
 *     -f HZ       Rayleigh-ish fading rate (multiplicative, 0 = off)
 *     -i RATE     impulsive noise bursts per second (atmospherics)
 *     -s SEED     random seed
 *     -q          summary only
 *   sim -t        codec / date self-test
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
    return fails || wrong > 20;
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

    for (a = 1; a < argc; a++) {
        if (!strcmp(argv[a], "-t")) return selftest();
        else if (!strcmp(argv[a], "-n")) snr = atof(argv[++a]);
        else if (!strcmp(argv[a], "-f")) fade_hz = atof(argv[++a]);
        else if (!strcmp(argv[a], "-i")) imp_rate = atof(argv[++a]);
        else if (!strcmp(argv[a], "-s")) rng_state ^= (uint64_t)atoll(argv[++a]) * 0x9E3779B97F4A7C15ULL;
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
            double tsec = (double)tick / FCY_HZ;
            n_cand++;
            if (st == FR_OK) {
                tk_result_t r = tk_frame(&tk, tick, &fi);
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
            } else if (st == FR_NOT_TIME) {
                n_nottime++;
            } else {
                n_fail++;
                if (!quiet)
                    printf("%9.4f corr=%.2f decode failed (%d) snr=%d\n", tsec, dsp.cand.corr_q10 / 1024.0, st, fi.snr_db);
            }
            dsp.cand_ready = 0;
        }
        tk_maintain(&tk, (int64_t)(i + DSP_BLOCK) * TICKS_PER_SAMPLE);
    }
    printf("SUMMARY snr=%g cand=%d time_ok=%d other=%d failed=%d accept=%d sync=%d step=%d cand=%d reject=%d "
           "jitter_rms=%.0fus rate=%dppb\n",
           snr, n_cand, n_ok, n_nottime, n_fail, n_tk[0], n_tk[1], n_tk[2], n_tk[3], n_tk[4],
           n_err ? sqrt(sum_err2 / n_err - (sum_err / n_err) * (sum_err / n_err)) : 0.0, tk_rate_ppb(&tk));
    free(all);
    return 0;
}
