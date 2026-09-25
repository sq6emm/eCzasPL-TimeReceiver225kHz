/*
 * Cycle / correctness benchmark of the firmware core compiled with XC16,
 * run in the sim30 instruction set simulator (see host/run_sim30.sh).
 *
 * Synthesises a 1003.7 Hz carrier with three e-CzasPL time frames, runs
 * dsp_block() and frame_decode() exactly as the firmware does, and prints
 * decode results and cycle counts into simlog[].
 */
#include <xc.h>
#include <stdio.h>
#include <string.h>
#include "dsp.h"
#include "frame.h"
#include "sintab.h"

static dsp_t dsp;
char simlog[1024];                 /* dumped by run_sim30.sh at bench_done() */
static uint16_t loglen;
#define LOG(...) (loglen += (uint16_t)snprintf(simlog + loglen, sizeof simlog - loglen, __VA_ARGS__))
void __attribute__((noinline)) bench_done(void) { __asm__ volatile ("nop"); }
static uint8_t bits[3][FRAME_BITS];

static int32_t mod_phase(uint32_t sample)          /* phase offset in 1/65536 turn */
{
    /* frames start at 9, 12 and 15 s */
    int32_t t = (int32_t)sample - 90000L;
    int f, k, i;
    uint8_t prev, cur;
    if (t < 0) return 0;
    f = (int)(t / 30000L);
    if (f > 2) return 0;
    t -= (int32_t)f * 30000L;
    k = (int)(t / 200);                            /* bit index (20 ms = 200 samples) */
    if (k >= FRAME_BITS) return 0;
    i = (int)(t % 200);
    prev = k ? bits[f][k - 1] : 1;
    cur = bits[f][k];
    {
        int32_t a = prev ? 0 : -7282, b = cur ? 0 : -7282;   /* -40 deg */
        int32_t r = i < 160 ? i : 160;                        /* 16 ms slew */
        return a + (b - a) * r / 160;
    }
}

int main(void)
{
    uint32_t n = 0, ph = 0, lcg = 1;
    uint32_t freq = (uint32_t)((1003.7 / 10000.0) * 4294967296.0);
    uint16_t blk, maxc = 0, t;
    uint32_t sumc = 0;
    int16_t x[DSP_BLOCK];
    int f, i;

    for (f = 0; f < 3; f++) frame_build(279988040UL + f, 2, 0, bits[f]);
    dsp_init(&dsp);
    T1CON = 0; PR1 = 0xFFFF; T1CONbits.TON = 1;

    for (blk = 0; blk < 8500; blk++) {
        for (i = 0; i < DSP_BLOCK; i++, n++) {
            uint32_t p = ph + ((uint32_t)mod_phase(n) << 16);
            lcg = lcg * 1103515245UL + 12345UL;
            x[i] = (int16_t)((SIN1024[p >> 22] >> 2) + ((int16_t)(lcg >> 16) >> 5));
            ph += freq;
        }
        TMR1 = 0;
        dsp_block(&dsp, x);
        t = TMR1;
        if (blk > 600) { sumc += t; if (t > maxc) maxc = t; }
        if (dsp.cand_ready) {
            frame_info_t fi;
            frame_status_t st;
            memset(&fi, 0, sizeof fi);
            T1CONbits.TCKPS = 2;                   /* 1:64 */
            TMR1 = 0;
            st = frame_decode(dsp.cand.ph + CAND_PRE, &fi);
            t = TMR1;
            T1CONbits.TCKPS = 0;
            LOG("cand blk=%lu corr=%d st=%d N=%lu tz=%u rs=%u chase=%u timing=%ld decode_cycles=%lu\n",
                   (unsigned long)dsp.cand.start_block, dsp.cand.corr_q10, st, (unsigned long)fi.n3,
                   fi.tz, fi.rs_fixed, fi.chase_flips, (long)fi.timing_q15, (unsigned long)t * 64);
            if (st == FR_OK) {
                /* worst case: the same candidate with its payload replaced by
                 * noise, so RS, every Chase variant and the GMD retries fail */
                static int16_t bad[CAND_LEN];
                int k;
                memcpy(bad, dsp.cand.ph, sizeof bad);
                for (k = CAND_PRE + PREAMBLE_BITS * SPB; k < CAND_LEN; k++) {
                    lcg = lcg * 1103515245UL + 12345UL;
                    bad[k] = (int16_t)(lcg >> 16);
                }
                T1CONbits.TCKPS = 2;
                TMR1 = 0;
                st = frame_decode(bad + CAND_PRE, &fi);
                t = TMR1;
                T1CONbits.TCKPS = 0;
                LOG("  worst case (noise payload): st=%d decode_cycles=%lu\n", st, (unsigned long)t * 64);
            }
            dsp.cand_ready = 0;
        }
    }
    LOG("dsp_block cycles: avg %lu max %u  (budget %lu per block at 40 MIPS)\n",
           (unsigned long)(sumc / (8500 - 601)), maxc, (unsigned long)TICKS_PER_BLOCK);
    LOG("carrier %ld centiHz pll_state %d\n", (long)dsp_carrier_centihz(&dsp), dsp.pll_state);
    LOG("DONE\n");
    bench_done();
    for (;;) ;
}
