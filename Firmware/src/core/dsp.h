/*
 * Demodulator: ~1 kHz audio from the SI4735 -> carrier phase -> frame candidates.
 *
 * Signal model (verified on off-air recordings): bit 1 = idle carrier phase,
 * bit 0 = phase retarded by ~45 deg (seen as ~32 deg after the SI4735), with a
 * linear slew of ~16 ms. 50 bit/s, 96-bit frames starting on whole seconds.
 *
 * Phase values are int16 with 65536 = 360 degrees.
 */
#ifndef DSP_H
#define DSP_H

#include <stdint.h>
#include "eczas_cfg.h"

#define DSP_RING       1024                    /* phase history (2 s), power of 2 */
#define TPL_LEN        (PREAMBLE_BITS * SPB)   /* 270 */
#define CAND_PRE       8                       /* extra samples kept before frame start */
#define CAND_LEN       (FRAME_BITS * SPB + 2 * CAND_PRE)

#define DEG2PH(d)      ((int16_t)((d) * 65536L / 360))

typedef enum { PLL_ACQ_FLL = 0, PLL_ACQ_FAST, PLL_ACQ_MED, PLL_TRACK } pll_state_t;

/* A detected preamble together with the phase samples of the whole frame. */
typedef struct {
    uint64_t start_block;   /* block index of the first frame sample */
    int16_t  frac_q15;      /* sub-block timing correction, -0.5..0.5 block */
    int16_t  corr_q10;      /* preamble correlation, 1024 = perfect */
    int16_t  ph[CAND_LEN];  /* ph[CAND_PRE] is the first sample of the frame */
} dsp_cand_t;

typedef struct {
    /* NCO / mixer */
    uint32_t nco_phase;
    int32_t  nco_freq;           /* phase increment per ADC sample */
    int32_t  nco_freq_nominal;

    /* PLL */
    pll_state_t pll_state;
    uint16_t pll_timer;          /* blocks spent in current state */
    int32_t  fll_acc;            /* sum of block-to-block phase steps */
    int16_t  prev_ph;
    int16_t  lvl0;               /* phase of a '0' bit relative to idle (negative) */
    int32_t  lock_err_avg;       /* IIR of |phase error| (x16) */
    uint16_t unlock_timer;

    /* Signal statistics */
    int32_t  amp_avg;            /* IIR of block amplitude */
    int16_t  adc_min, adc_max;   /* extremes since last dsp_level_reset() */
    uint16_t adc_clip;           /* samples within 1% of full scale */

    /* Phase ring and correlator */
    int16_t  ring[DSP_RING];
    uint64_t block;              /* number of blocks processed */
    int32_t  sum_x;              /* sum of phase over correlator window */
    uint32_t sum_x2;             /* sum of (phase/16)^2 over window */

    /* Detector state */
    uint8_t  det_state;
    uint16_t det_count;
    int16_t  c_prev, c_best, c_best_prev, c_best_next;
    uint64_t best_start;
    uint8_t  best_next_pending;

    /* Output */
    volatile uint8_t cand_ready; /* set by dsp, cleared by consumer */
    uint8_t  cand_overrun;
    dsp_cand_t cand;
} dsp_t;

void dsp_init(dsp_t *d);

/* Process DSP_BLOCK raw ADC samples (signed Q15). */
void dsp_block(dsp_t *d, const int16_t *x);

/* ADC level statistics (for adjusting the gain trimmer R27). */
void dsp_level_reset(dsp_t *d);

/* Carrier frequency estimate in 1/100 Hz. */
int32_t dsp_carrier_centihz(const dsp_t *d);

/* Low-level helpers, exported for tests. */
int16_t dsp_atan2(int32_t y, int32_t x);

#endif
