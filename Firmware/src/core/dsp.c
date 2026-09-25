/*
 * Demodulator - see dsp.h.
 *
 * Chain per 2 ms block (20 ADC samples at 10 kHz):
 *   1. mix with a numerically controlled oscillator and integrate-and-dump
 *      -> one complex baseband sample (the dump has nulls at k*500 Hz, which
 *      removes the 2 kHz mixing image),
 *   2. carrier phase = atan2 of that sample,
 *   3. decision-directed PLL (levels 0 and lvl0) steers the NCO so that the
 *      idle carrier sits at phase 0 - it follows the SI4735's BFO offset
 *      (several Hz) and drift, but is far too slow to follow the 50 bit/s data,
 *   4. phase goes into a ring buffer; a normalised correlation against the
 *      27 known preamble bits finds frame starts with sub-block precision.
 */
#include <math.h>
#include <string.h>
#include "dsp.h"
#include "sintab.h"

/* Preamble template (zero mean) and its energy, built once in dsp_init. */
static int16_t tpl[TPL_LEN];
static float   tpl_norm;

static const uint8_t PREAMBLE[PREAMBLE_BITS] = {
    0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,   /* 0x55 0x55 */
    0,1,1,0,0,0,0,0,                   /* 0x60 - time frame marker */
    1,0,1
};

/* PLL loop gains per state: proportional (NCO phase per error unit) and
 * integral (NCO frequency per error unit, Q10). Bandwidths 5 / 2 / 1 Hz,
 * damping 0.707, update rate 500 Hz. */
static const int16_t KP[4]     = { 0, 5823, 2329, 1165 };
static const int16_t KI_Q10[4] = { 0, 13250, 2120, 530 };
#define FLL_BLOCKS      500     /* 1 s frequency estimate */
#define FLL_SETTLE      65536   /* |correction| < 0.15 Hz (NCO units): hand over to the PLL */
#define FLL_SHIFT       10      /* baseband scaling so 500 products fit in int64 */
#define FAST_BLOCKS     1500
#define MED_BLOCKS      1500
#define FREQ_RANGE      17179869L   /* +-40 Hz in NCO units */
#define UNLOCK_ERR      DEG2PH(25)
#define UNLOCK_BLOCKS   5000        /* 10 s */

static void build_template(void)
{
    int16_t v[TPL_LEN];
    int32_t s = 0;
    int64_t tt = 0;
    uint8_t prev = 1;
    int k, i, n = 0;

    /* level 0 (idle / bit 1) and -16 (bit 0); block-averaged linear ramp of
     * RAMP_BLOCKS blocks starting at the bit boundary */
    for (k = 0; k < PREAMBLE_BITS; k++) {
        int16_t a = prev ? 0 : -16, c = PREAMBLE[k] ? 0 : -16;
        for (i = 0; i < SPB; i++) {
            int16_t f = (int16_t)(2 * i + 1);
            if (f > 2 * RAMP_BLOCKS) f = 2 * RAMP_BLOCKS;
            v[n] = (int16_t)(a + (c - a) * f / (2 * RAMP_BLOCKS));
            s += v[n++];
        }
        prev = PREAMBLE[k];
    }
    for (n = 0; n < TPL_LEN; n++) {
        tpl[n] = (int16_t)(TPL_LEN * v[n] - s);   /* exactly zero mean */
        tt += (int32_t)tpl[n] * tpl[n];
    }
    tpl_norm = (float)tt;
}

void dsp_init(dsp_t *d)
{
    memset(d, 0, sizeof(*d));
    build_template();
    d->nco_freq_nominal = (int32_t)(((int64_t)AUDIO_CARRIER_HZ << 32) / ADC_FS_HZ);
    d->nco_freq = d->nco_freq_nominal;
    d->lvl0 = DEG2PH(-32);
    d->pll_state = PLL_ACQ_FLL;
    dsp_level_reset(d);
}

int16_t dsp_atan2(int32_t y, int32_t x)
{
    uint32_t ax = x < 0 ? (uint32_t)-x : (uint32_t)x;
    uint32_t ay = y < 0 ? (uint32_t)-y : (uint32_t)y;
    uint32_t mx = ax > ay ? ax : ay, mn = ax > ay ? ay : ax;
    int32_t r, a;

    if (mx == 0) return 0;
    while (mx > 0x7FFF) { mx >>= 1; mn >>= 1; }
    r = (int32_t)((mn << 15) / mx);                  /* 0..32768 */
    /* atan(r) ~= r * (pi/4 + 0.273 * (1 - r)), max error 0.22 deg */
    a = (r * (8192 + ((2848L * (32768L - r)) >> 15))) >> 15;
    if (ay > ax) a = 16384 - a;
    if (x < 0)   a = 32768 - a;
    if (y < 0)   a = -a;
    return (int16_t)a;
}

static void pll_update(dsp_t *d, int16_t ph, int32_t re, int32_t im)
{
    int32_t err;

    if (d->pll_state == PLL_ACQ_FLL) {
        /* carrier rotation per block = angle of sum z[n] * conj(z[n-1]) over 1 s;
         * averaging the vectors (not the noisy angles) keeps the estimate usable
         * well below 0 dB SNR */
        re >>= FLL_SHIFT;
        im >>= FLL_SHIFT;
        d->fll_re += (int64_t)re * d->prev_re + (int64_t)im * d->prev_im;
        d->fll_im += (int64_t)im * d->prev_re - (int64_t)re * d->prev_im;
        d->prev_re = re;
        d->prev_im = im;
        if (++d->pll_timer >= FLL_BLOCKS) {
            int64_t sr = d->fll_re, si = d->fll_im;
            int32_t df;
            while (sr > 0x3FFFFFFF || sr < -0x3FFFFFFF || si > 0x3FFFFFFF || si < -0x3FFFFFFF) {
                sr >>= 1;
                si >>= 1;
            }
            /* per-sample NCO increment = step per block * 2^16 / DSP_BLOCK */
            df = (int32_t)dsp_atan2((int32_t)si, (int32_t)sr) * (65536 / DSP_BLOCK);
            d->nco_freq += df;
            if (d->nco_freq > d->nco_freq_nominal + FREQ_RANGE) d->nco_freq = d->nco_freq_nominal + FREQ_RANGE;
            if (d->nco_freq < d->nco_freq_nominal - FREQ_RANGE) d->nco_freq = d->nco_freq_nominal - FREQ_RANGE;
            d->fll_re = d->fll_im = 0;
            d->pll_timer = 0;
            if (df < FLL_SETTLE && df > -FLL_SETTLE)
                d->pll_state = PLL_ACQ_FAST;
        }
        return;
    }

    /* decision-directed error: nearest of the two data levels */
    if (ph > d->lvl0 / 2) {
        err = ph;
    } else {
        err = (int32_t)ph - d->lvl0;
        if (d->pll_state == PLL_TRACK) {
            int32_t l = d->lvl0 + (err >> 8);
            if (l > DEG2PH(-18)) l = DEG2PH(-18);
            if (l < DEG2PH(-60)) l = DEG2PH(-60);
            d->lvl0 = (int16_t)l;
        }
    }

    d->nco_phase += (uint32_t)((int32_t)KP[d->pll_state] * err);
    d->nco_freq += (KI_Q10[d->pll_state] * err) >> 10;
    if (d->nco_freq > d->nco_freq_nominal + FREQ_RANGE) d->nco_freq = d->nco_freq_nominal + FREQ_RANGE;
    if (d->nco_freq < d->nco_freq_nominal - FREQ_RANGE) d->nco_freq = d->nco_freq_nominal - FREQ_RANGE;

    ++d->pll_timer;
    if (d->pll_state == PLL_ACQ_FAST && d->pll_timer >= FAST_BLOCKS) {
        d->pll_state = PLL_ACQ_MED; d->pll_timer = 0;
    } else if (d->pll_state == PLL_ACQ_MED && d->pll_timer >= MED_BLOCKS) {
        d->pll_state = PLL_TRACK; d->pll_timer = 0;
    }

    /* lock monitor */
    d->lock_err_avg += (((err < 0 ? -err : err) << 4) - d->lock_err_avg) >> 9;
    if (d->pll_state == PLL_TRACK) {
        if ((d->lock_err_avg >> 4) > UNLOCK_ERR) {
            if (++d->unlock_timer > UNLOCK_BLOCKS) {
                d->pll_state = PLL_ACQ_FLL;
                d->pll_timer = 0;
                d->fll_re = d->fll_im = 0;
                d->unlock_timer = 0;
                d->nco_freq = d->nco_freq_nominal;
            }
        } else {
            d->unlock_timer = 0;
        }
    }
}

static int16_t correlate(const dsp_t *d, uint32_t start)
{
    int32_t num = 0, e;
    int64_t en;
    float c;
    int k;

    for (k = 0; k < TPL_LEN; k++)
        num += (int32_t)tpl[k] * (d->ring[(start + k) & (DSP_RING - 1)] >> 4);
    if (num <= 0) return 0;
    /* TPL_LEN * |x - mean|^2 */
    en = (int64_t)TPL_LEN * d->sum_x2 - (int64_t)d->sum_x * d->sum_x;
    if (en <= 0) return 0;
    c = (float)num / sqrtf(tpl_norm * (float)en / TPL_LEN);
    e = (int32_t)(c * 1024.0f);
    return (int16_t)(e > 1024 ? 1024 : e);
}

static void detector(dsp_t *d)
{
    uint64_t n = d->block;                 /* newest sample index */
    uint64_t s;
    int16_t c;

    if (n < TPL_LEN) return;
    s = n - (TPL_LEN - 1);                 /* window start = candidate frame start */
    c = correlate(d, (uint32_t)s);

    switch (d->det_state) {
    case 0:
        if (c >= DET_THRESHOLD_Q10) {
            d->det_state = 1;
            d->det_count = 0;
            d->c_best = c;
            d->c_best_prev = d->c_prev;
            d->best_start = s;
            d->best_next_pending = 1;
        }
        break;
    case 1:
        if (d->best_next_pending) {
            d->c_best_next = c;
            d->best_next_pending = 0;
        }
        if (c > d->c_best) {
            d->c_best_prev = d->c_prev;
            d->c_best = c;
            d->best_start = s;
            d->best_next_pending = 1;
        }
        /* keep looking for the best alignment over a whole preamble length:
         * a window holding silence plus the first sync bits can already
         * exceed the threshold well before the true peak */
        if (++d->det_count >= TPL_LEN && !d->best_next_pending)
            d->det_state = 2;
        break;
    default:
        if (n >= d->best_start + CAND_LEN - CAND_PRE - 1) {
            if (!d->cand_ready) {
                dsp_cand_t *cd = &d->cand;
                uint32_t first = (uint32_t)(d->best_start - CAND_PRE);
                int32_t den = 2L * (d->c_best_prev - 2L * d->c_best + d->c_best_next);
                int32_t frac = 0;
                int k;
                if (den < 0)
                    frac = ((int32_t)(d->c_best_prev - d->c_best_next) << 15) / den;
                if (frac > 16384) frac = 16384;
                if (frac < -16384) frac = -16384;
                for (k = 0; k < CAND_LEN; k++)
                    cd->ph[k] = d->ring[(first + k) & (DSP_RING - 1)];
                cd->start_block = d->best_start;
                cd->frac_q15 = (int16_t)frac;
                cd->corr_q10 = d->c_best;
                d->cand_ready = 1;
            } else {
                d->cand_overrun++;
            }
            d->det_state = 0;
        }
        break;
    }
    d->c_prev = c;
}

void dsp_block(dsp_t *d, const int16_t *x)
{
    int32_t re = 0, im = 0;
    uint32_t ph_acc = d->nco_phase;
    int16_t ph, mn = d->adc_min, mx = d->adc_max;
    uint16_t clip = d->adc_clip;
    int32_t ax, ay, amp;
    int16_t old, xs;
    uint32_t idx;
    int i;

    for (i = 0; i < DSP_BLOCK; i++) {
        int16_t s = x[i];
        uint16_t t = (uint16_t)(ph_acc >> 22);
        re += ((int32_t)s * SIN1024[(t + 256) & 1023]) >> 8;
        im -= ((int32_t)s * SIN1024[t]) >> 8;
        ph_acc += (uint32_t)d->nco_freq;
        if (s > mx) mx = s;
        if (s < mn) mn = s;
        if (s > 32440 || s < -32440) clip++;
    }
    d->nco_phase = ph_acc;
    d->adc_min = mn;
    d->adc_max = mx;
    d->adc_clip = clip;

    ph = dsp_atan2(im, re);
    ax = re < 0 ? -re : re;
    ay = im < 0 ? -im : im;
    amp = ax > ay ? ax + (ay * 3 >> 3) : ay + (ax * 3 >> 3);
    d->amp_avg += (amp - d->amp_avg) >> 6;

    pll_update(d, ph, re, im);

    /* ring + running window sums for the correlator */
    d->block++;
    idx = (uint32_t)d->block & (DSP_RING - 1);
    d->ring[idx] = ph;
    xs = ph >> 4;
    d->sum_x += xs;
    d->sum_x2 += (int32_t)xs * xs;
    if (d->block >= TPL_LEN) {
        old = d->ring[(idx - TPL_LEN) & (DSP_RING - 1)] >> 4;
        d->sum_x -= old;
        d->sum_x2 -= (int32_t)old * old;
    }
    detector(d);
}

void dsp_level_reset(dsp_t *d)
{
    d->adc_min = 0x7FFF;
    d->adc_max = -0x7FFF;
    d->adc_clip = 0;
}

int32_t dsp_carrier_centihz(const dsp_t *d)
{
    return (int32_t)(((int64_t)d->nco_freq * ADC_FS_HZ * 100) >> 32);
}
