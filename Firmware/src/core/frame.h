/*
 * e-CzasPL time frame decoding.
 *
 * Frame (96 bits, MSB first):
 *   0..15  0x5555 sync        16..23 0x60 time frame marker   24..26 101
 *   27..56 S29..S0 (3-second periods since 2000-01-01 00:00:00 UTC, MSB first)
 *   57..58 TZ0 TZ1   59 LS   60 LSS   61 TZC   62..63 SK0 SK1
 *   64..87 Reed-Solomon RS(15,9) parity over bits 27..62 (4-bit symbols)
 *   88..95 CRC-8 (poly 0x07, init 0) over the scrambled bits 24..63
 * Bits 27..63 are XORed with the 37 LSBs of 0x0A47554D2B ("\nGUM+").
 */
#ifndef FRAME_H
#define FRAME_H

#include <stdint.h>
#include "eczas_cfg.h"

typedef enum {
    FR_OK = 0,
    FR_NOT_TIME,      /* marker is not 0x60 / 101 (e.g. Enea lighting frame) */
    FR_NO_SIGNAL,     /* preamble levels not separable */
    FR_UNCORRECTABLE  /* RS + CRC failed, also after Chase retries */
} frame_status_t;

typedef struct {
    uint32_t n3;            /* 3-second periods since 2000-01-01 */
    uint8_t  tz;            /* local time offset in hours (0..3) */
    uint8_t  ls, lss;       /* leap second announced / sign (1 = subtract) */
    uint8_t  tzc;           /* local time change announced */
    uint8_t  sk;            /* transmitter state 0..3 */
    uint8_t  rs_fixed;      /* RS symbols corrected */
    uint8_t  chase_flips;   /* extra bits flipped by Chase decoding */
    uint8_t  sk1_fixed;     /* SK1 recovered through CRC */
    uint8_t  erasures;      /* symbols erased by the GMD retry (0 = not used) */
    uint8_t  raw[12];       /* hard-decision frame as received */
    int16_t  lvl_sep;       /* measured '1'-'0' phase separation */
    int16_t  snr_db;        /* estimated from preamble (dB) */
    int32_t  timing_q15;    /* refined frame start relative to the candidate
                               start, in blocks (Q15) */
} frame_info_t;

frame_status_t frame_decode(const int16_t *ph, frame_info_t *out);

/* Exposed for tests / encoder */
uint8_t frame_crc8(const uint8_t *bits40);
int     frame_rs_decode(uint8_t cw[15]);           /* returns #fixed or -1 */
int     frame_rs_decode_erasures(uint8_t cw[15], const uint8_t *erase, int f); /* erase: degrees */
void    frame_rs_encode(uint8_t cw[15]);           /* fills cw[0..5] from cw[6..14] */
void    frame_build(uint32_t n3, uint8_t tz, uint8_t flags, uint8_t bits[FRAME_BITS]);
int     frame_check_bits(uint8_t bits[FRAME_BITS], frame_info_t *out);

#endif
