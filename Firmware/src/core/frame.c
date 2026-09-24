/*
 * e-CzasPL frame decoding - see frame.h.
 *
 * Unlike the original firmware, a frame is only accepted when the
 * Reed-Solomon decoder succeeds AND the CRC-8 matches afterwards. RS(15,9)
 * is a bounded-distance code: with 4+ symbol errors it "corrects" to a wrong
 * codeword ~9% of the time, which is what made the old receiver jump to
 * bogus times. The CRC catches all but ~1/256 of those, and the timekeeper
 * adds a consistency check on top.
 */
#include <math.h>
#include <string.h>
#include "frame.h"

/* GF(16), primitive polynomial x^4 + x + 1, alpha = 2 */
static const uint8_t GF_EXP[30] = {
    1,2,4,8,3,6,12,11,5,10,7,14,15,13,9,
    1,2,4,8,3,6,12,11,5,10,7,14,15,13,9
};
static const uint8_t GF_LOG[16] = { 0,0,1,4,2,8,5,10,3,14,9,7,6,13,11,12 };
/* generator g(x) = prod_{i=1..6} (x - alpha^i), coefficients low -> high */
static const uint8_t RS_GEN[7] = { 12,10,12,3,9,7,1 };

#define PH_10DEG 1820    /* 10 degrees in 65536-per-turn units */

static const uint8_t PREAMBLE[PREAMBLE_BITS] = {
    0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1, 0,1,1,0,0,0,0,0, 1,0,1
};

static uint8_t gmul(uint8_t a, uint8_t b)
{
    return (a && b) ? GF_EXP[GF_LOG[a] + GF_LOG[b]] : 0;
}

static uint8_t gdiv(uint8_t a, uint8_t b)
{
    return a ? GF_EXP[(GF_LOG[a] + 15 - GF_LOG[b]) % 15] : 0;
}

static uint8_t scramble_bit(int i)   /* i = 0..36 -> frame bit 27+i */
{
    /* 37 LSBs of 0x0A47554D2B, MSB first */
    static const uint8_t S[5] = { 0x0A, 0x47, 0x55, 0x4D, 0x2B };
    int b = i + 3;
    return (S[b >> 3] >> (7 - (b & 7))) & 1;
}

uint8_t frame_crc8(const uint8_t *bits40)
{
    uint8_t c = 0;
    int i;
    for (i = 0; i < 40; i++) {
        uint8_t fb = (uint8_t)(((c >> 7) & 1) ^ bits40[i]);
        c = (uint8_t)((c << 1) ^ (fb ? 0x07 : 0));
    }
    return c;
}

/* codeword by polynomial degree: cw[0..5] parity (ECC0 high nibble = x^0),
 * cw[6..14] data (S29..S26 = x^6 ... LS,LSS,TZC,SK0 = x^14) */
static void bits_to_cw(const uint8_t *bits, uint8_t cw[15])
{
    int k, j;
    for (k = 0; k < 9; k++) {
        uint8_t v = 0;
        for (j = 0; j < 4; j++) v = (uint8_t)((v << 1) | bits[27 + 4 * k + j]);
        cw[6 + k] = v;
    }
    for (k = 0; k < 6; k++) {
        uint8_t v = 0;
        for (j = 0; j < 4; j++) v = (uint8_t)((v << 1) | bits[64 + 4 * k + j]);
        cw[k] = v;
    }
}

static void cw_to_bits(const uint8_t cw[15], uint8_t *bits)
{
    int k, j;
    for (k = 0; k < 9; k++)
        for (j = 0; j < 4; j++) bits[27 + 4 * k + j] = (cw[6 + k] >> (3 - j)) & 1;
    for (k = 0; k < 6; k++)
        for (j = 0; j < 4; j++) bits[64 + 4 * k + j] = (cw[k] >> (3 - j)) & 1;
}

void frame_rs_encode(uint8_t cw[15])
{
    uint8_t rem[15];
    int d, j;
    memset(rem, 0, 6);
    memcpy(rem + 6, cw + 6, 9);
    for (d = 14; d >= 6; d--) {
        uint8_t coef = rem[d];
        if (coef)
            for (j = 0; j < 7; j++) rem[d - 6 + j] ^= gmul(coef, RS_GEN[j]);
    }
    memcpy(cw, rem, 6);
}

int frame_rs_decode(uint8_t cw[15])
{
    uint8_t S[6], C[7], B[7], T[7], Om[6];
    uint8_t pos[3];
    int i, j, n, L = 0, m = 1, npos = 0, any = 0;
    uint8_t b = 1;

    /* syndromes S_i = c(alpha^(i+1)) */
    for (i = 0; i < 6; i++) {
        uint8_t s = 0;
        for (j = 14; j >= 0; j--) s = gmul(s, GF_EXP[i + 1]) ^ cw[j];
        S[i] = s;
        any |= s;
    }
    if (!any) return 0;

    /* Berlekamp-Massey */
    memset(C, 0, sizeof C); memset(B, 0, sizeof B);
    C[0] = B[0] = 1;
    for (n = 0; n < 6; n++) {
        uint8_t dlt = S[n], coef;
        for (i = 1; i <= L; i++) dlt ^= gmul(C[i], S[n - i]);
        if (!dlt) { m++; continue; }
        memcpy(T, C, sizeof C);
        coef = gdiv(dlt, b);
        for (i = m; i < 7; i++) C[i] ^= gmul(coef, B[i - m]);
        if (2 * L <= n) { L = n + 1 - L; memcpy(B, T, sizeof B); b = dlt; m = 1; }
        else m++;
    }
    if (L > 3) return -1;

    /* Chien search: roots of C at alpha^-p -> error at degree p */
    for (n = 0; n < 15; n++) {
        uint8_t xinv = GF_EXP[(15 - n) % 15], s = 0;
        for (i = L; i >= 0; i--) s = gmul(s, xinv) ^ C[i];
        if (!s) {
            if (npos == 3) return -1;
            pos[npos++] = (uint8_t)n;
        }
    }
    if (npos != L) return -1;

    /* Forney, first consecutive root b = 1: e = Omega(X^-1) / C'(X^-1) */
    for (i = 0; i < 6; i++) {
        uint8_t o = 0;
        for (j = 0; j <= i && j <= L; j++) o ^= gmul(S[i - j], C[j]);
        Om[i] = o;
    }
    for (n = 0; n < npos; n++) {
        uint8_t xinv = GF_EXP[(15 - pos[n]) % 15], num = 0, den = 0;
        for (i = 5; i >= 0; i--) num = gmul(num, xinv) ^ Om[i];
        for (i = 1; i <= L; i += 2)            /* formal derivative: odd terms */
            den ^= gmul(C[i], GF_EXP[(GF_LOG[xinv] * (i - 1)) % 15]);
        if (!den) return -1;
        cw[pos[n]] ^= gdiv(num, den);
    }
    return L;
}

/* Validate/correct hard bits in place. Returns 1 if RS + CRC pass. */
int frame_check_bits(uint8_t bits[FRAME_BITS], frame_info_t *out)
{
    uint8_t cw[15], rx = 0, d[37];
    int i, fixed, crc_ok = 0, sk1_fixed = 0;
    uint32_t n3 = 0;

    bits_to_cw(bits, cw);
    fixed = frame_rs_decode(cw);
    if (fixed < 0) return 0;
    cw_to_bits(cw, bits);
    for (i = 88; i < 96; i++) rx = (uint8_t)((rx << 1) | bits[i]);
    if (frame_crc8(bits + 24) == rx) {
        crc_ok = 1;
    } else {
        bits[63] ^= 1;                      /* SK1 is not RS protected */
        if (frame_crc8(bits + 24) == rx) { crc_ok = 1; sk1_fixed = 1; }
        else bits[63] ^= 1;
    }
    if (!crc_ok) return 0;
    if (out) {
        for (i = 0; i < 37; i++) d[i] = bits[27 + i] ^ scramble_bit(i);
        for (i = 0; i < 30; i++) n3 = (n3 << 1) | d[i];
        out->n3 = n3;
        out->tz = (uint8_t)(d[30] | (d[31] << 1));
        out->ls = d[32];
        out->lss = d[33];
        out->tzc = d[34];
        out->sk = (uint8_t)(d[35] | (d[36] << 1));
        out->rs_fixed = (uint8_t)fixed;
        out->sk1_fixed = (uint8_t)sk1_fixed;
    }
    return 1;
}

void frame_build(uint32_t n3, uint8_t tz, uint8_t flags, uint8_t bits[FRAME_BITS])
{
    uint8_t d[37], cw[15], c;
    int i;
    for (i = 0; i < 30; i++) d[i] = (n3 >> (29 - i)) & 1;
    d[30] = tz & 1; d[31] = (tz >> 1) & 1;
    for (i = 0; i < 5; i++) d[32 + i] = (flags >> i) & 1;   /* LS LSS TZC SK0 SK1 */
    for (i = 0; i < PREAMBLE_BITS; i++) bits[i] = PREAMBLE[i];
    for (i = 0; i < 37; i++) bits[27 + i] = d[i] ^ scramble_bit(i);
    bits_to_cw(bits, cw);
    frame_rs_encode(cw);
    cw_to_bits(cw, bits);
    c = frame_crc8(bits + 24);
    for (i = 0; i < 8; i++) bits[88 + i] = (c >> (7 - i)) & 1;
}

/* With all 96 bits known, correlate the whole frame against its model
 * waveform (the preamble detector only uses 27 bits) for a better frame
 * start estimate. Returns the offset in blocks, Q15, within +-REFINE_LAGS. */
#define REFINE_LAGS 3
static int32_t refine_timing(const int16_t *ph, const uint8_t *bits, int32_t l1, int32_t l0)
{
    int32_t c[2 * REFINE_LAGS + 1], mean = 0, best;
    static int16_t m[FRAME_BITS * SPB];   /* static: too big for the dsPIC stack */
    uint8_t prev = 1;
    int k, i, n = 0, lag, bi = 0;

    for (k = 0; k < FRAME_BITS; k++) {
        int32_t a = prev ? l1 : l0, b = bits[k] ? l1 : l0;
        for (i = 0; i < SPB; i++) {
            int32_t f = 2 * i + 1;
            if (f > 2 * RAMP_BLOCKS) f = 2 * RAMP_BLOCKS;
            m[n] = (int16_t)((a + (b - a) * f / (2 * RAMP_BLOCKS)) >> 4);
            mean += m[n++];
        }
        prev = bits[k];
    }
    mean /= n;
    for (lag = -REFINE_LAGS; lag <= REFINE_LAGS; lag++) {
        int32_t acc = 0;
        for (i = 0; i < n; i++) acc += (int32_t)(m[i] - mean) * (ph[i + lag] >> 4);
        c[lag + REFINE_LAGS] = acc;
        if (acc > c[bi]) bi = lag + REFINE_LAGS;
    }
    best = (int32_t)(bi - REFINE_LAGS) << 15;
    if (bi > 0 && bi < 2 * REFINE_LAGS) {
        int64_t cp = c[bi - 1], c0 = c[bi], cn = c[bi + 1];
        int64_t den = 2 * (cp - 2 * c0 + cn);
        if (den < 0) best += (int32_t)(((cp - cn) * 32768) / den);
    }
    return best;
}

static int popcount5(unsigned v)
{
    int n = 0;
    while (v) { n += v & 1; v >>= 1; }
    return n;
}

/* Max-log MAP (BCJR) over the 2-state trellis "value of the current bit".
 * The received phase of bit k depends on bits k-1 and k (linear slew), so
 * each branch is compared against its model waveform over all SPB samples.
 * Known preamble bits are forced. Produces an LLR per bit (positive = 1). */
#define NEG_INF  (-0x1FFFFFFFL)   /* 3 * NEG_INF must fit in int32 */
static void map_detect(const int16_t *ph, int32_t l1, int32_t l0, int32_t *llr)
{
    static int32_t alpha[FRAME_BITS + 1][2], beta[FRAME_BITS + 1][2];
    static int32_t gam[FRAME_BITS][2][2];
    int16_t model[2][2][SPB];
    int k, i, p, c;

    for (p = 0; p < 2; p++)
        for (c = 0; c < 2; c++) {
            int32_t a = p ? l1 : l0, b = c ? l1 : l0;
            for (i = 0; i < SPB; i++) {
                int32_t f = 2 * i + 1;
                if (f > 2 * RAMP_BLOCKS) f = 2 * RAMP_BLOCKS;
                model[p][c][i] = (int16_t)((a + (b - a) * f / (2 * RAMP_BLOCKS)) >> 4);
            }
        }
    for (k = 0; k < FRAME_BITS; k++) {
        const int16_t *x = ph + k * SPB;
        for (p = 0; p < 2; p++)
            for (c = 0; c < 2; c++) {
                int32_t d2 = 0;
                if (k < PREAMBLE_BITS && c != PREAMBLE[k]) { gam[k][p][c] = NEG_INF; continue; }
                for (i = 0; i < SPB; i++) {
                    int32_t e = (x[i] >> 4) - model[p][c][i];
                    d2 += e * e;
                }
                gam[k][p][c] = -(d2 >> 4);
            }
    }
    alpha[0][0] = NEG_INF; alpha[0][1] = 0;            /* idle carrier = '1' */
    for (k = 0; k < FRAME_BITS; k++) {
        int32_t m;
        for (c = 0; c < 2; c++) {
            int32_t v0 = alpha[k][0] + gam[k][0][c], v1 = alpha[k][1] + gam[k][1][c];
            alpha[k + 1][c] = v0 > v1 ? v0 : v1;
        }
        m = alpha[k + 1][0] > alpha[k + 1][1] ? alpha[k + 1][0] : alpha[k + 1][1];
        alpha[k + 1][0] -= m; alpha[k + 1][1] -= m;
        if (alpha[k + 1][0] < NEG_INF) alpha[k + 1][0] = NEG_INF;
        if (alpha[k + 1][1] < NEG_INF) alpha[k + 1][1] = NEG_INF;
    }
    beta[FRAME_BITS][0] = beta[FRAME_BITS][1] = 0;
    for (k = FRAME_BITS - 1; k >= 0; k--) {
        int32_t m;
        for (p = 0; p < 2; p++) {
            int32_t v0 = gam[k][p][0] + beta[k + 1][0], v1 = gam[k][p][1] + beta[k + 1][1];
            beta[k][p] = v0 > v1 ? v0 : v1;
        }
        m = beta[k][0] > beta[k][1] ? beta[k][0] : beta[k][1];
        beta[k][0] -= m; beta[k][1] -= m;
        if (beta[k][0] < NEG_INF) beta[k][0] = NEG_INF;
        if (beta[k][1] < NEG_INF) beta[k][1] = NEG_INF;
    }
    for (k = 0; k < FRAME_BITS; k++) {
        int32_t best[2];
        for (c = 0; c < 2; c++) {
            int32_t v0 = alpha[k][0] + gam[k][0][c] + beta[k + 1][c];
            int32_t v1 = alpha[k][1] + gam[k][1][c] + beta[k + 1][c];
            best[c] = v0 > v1 ? v0 : v1;
        }
        llr[k] = best[1] - best[0];
    }
}

frame_status_t frame_decode(const int16_t *ph, frame_info_t *out)
{
    int32_t endv[PREAMBLE_BITS];
    static int32_t llr[FRAME_BITS];
    int32_t s1 = 0, s0 = 0, n1 = 0, n0 = 0, var = 0;
    uint8_t bits[FRAME_BITS], work[FRAME_BITS];
    uint8_t weak[CHASE_BITS];
    int32_t weak_mag[CHASE_BITS];
    int k, i, err, marker_err;
    unsigned combo, ncombo;

    /* Levels: the phase settles in the last part of each bit (~16 ms slew),
     * so the last three 2 ms samples of the known preamble bits are used. */
    for (k = 0; k < PREAMBLE_BITS; k++) {
        const int16_t *p = ph + k * SPB + SPB - 3;
        endv[k] = (int32_t)p[0] + p[1] + p[2];
        if (PREAMBLE[k]) { s1 += endv[k]; n1++; } else { s0 += endv[k]; n0++; }
    }
    s1 /= n1; s0 /= n0;
    if (s1 - s0 < 3 * PH_10DEG) return FR_NO_SIGNAL;
    for (k = 0; k < PREAMBLE_BITS; k++) {
        int32_t dv = endv[k] - (PREAMBLE[k] ? s1 : s0);
        var += (dv >> 4) * (dv >> 4);
    }
    var /= PREAMBLE_BITS;

    /* The marker is judged on plain end-of-bit decisions (the MAP detector
     * forces known bits, so it cannot tell us whether they were there). */
    marker_err = 0;
    err = 0;
    for (k = 16; k < PREAMBLE_BITS; k++) {
        uint8_t b = endv[k] > (s1 + s0) / 2;
        if (k < 24) marker_err += b != PREAMBLE[k];
        else err += b != PREAMBLE[k];
    }
    if (marker_err > 2 || err > 1) return FR_NOT_TIME;

    map_detect(ph, s1 / 3, s0 / 3, llr);
    for (k = 0; k < FRAME_BITS; k++) bits[k] = llr[k] > 0;

    memset(out, 0, sizeof(*out));
    out->lvl_sep = (int16_t)((s1 - s0) / 3);
    {
        float sig = (float)(s1 - s0) / 2.0f / 16.0f;
        float noise = sqrtf((float)var + 1.0f);
        out->snr_db = (int16_t)(20.0f * log10f(sig / noise + 1e-3f));
    }
    for (i = 0; i < 12; i++) {
        uint8_t v = 0;
        for (k = 0; k < 8; k++) v = (uint8_t)((v << 1) | bits[i * 8 + k]);
        out->raw[i] = v;
    }

    memcpy(work, bits, sizeof bits);
    if (frame_check_bits(work, out)) {
        out->timing_q15 = refine_timing(ph, work, s1 / 3, s0 / 3);
        return FR_OK;
    }

    /* Other services use markers like 0x78 (2 bits away from 0x60). Such
     * frames get only the single hard-decision attempt above: trying 31
     * Chase variants of a non-time frame would give a ~2% chance of a
     * bogus RS+CRC match. */
    if (marker_err > 1) return FR_UNCORRECTABLE;

    /* Chase-II: flip combinations of the least reliable data bits */
    for (i = 0; i < CHASE_BITS; i++) { weak[i] = 0; weak_mag[i] = 0x7FFFFFFFL; }
    for (k = PREAMBLE_BITS; k < FRAME_BITS; k++) {
        int32_t a = llr[k] < 0 ? -llr[k] : llr[k];
        for (i = 0; i < CHASE_BITS; i++) {
            if (a < weak_mag[i]) {
                int j;
                for (j = CHASE_BITS - 1; j > i; j--) { weak[j] = weak[j - 1]; weak_mag[j] = weak_mag[j - 1]; }
                weak[i] = (uint8_t)k; weak_mag[i] = a;
                break;
            }
        }
    }
    ncombo = 1u << CHASE_BITS;
    for (err = 1; err <= CHASE_BITS; err++) {        /* fewest flips first */
        for (combo = 1; combo < ncombo; combo++) {
            if (popcount5(combo) != err) continue;
            memcpy(work, bits, sizeof bits);
            for (i = 0; i < CHASE_BITS; i++)
                if (combo & (1u << i)) work[weak[i]] ^= 1;
            if (frame_check_bits(work, out)) {
                out->chase_flips = (uint8_t)err;
                out->timing_q15 = refine_timing(ph, work, s1 / 3, s0 / 3);
                return FR_OK;
            }
        }
    }
    return FR_UNCORRECTABLE;
}
