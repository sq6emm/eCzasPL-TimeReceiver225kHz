#ifndef SAMPLER_H
#define SAMPLER_H

#include <stdint.h>
#include "dsp.h"

extern dsp_t g_dsp;

/* 1PPS request, written by the main loop, served from the DMA interrupt. */
enum { PPS_IDLE = 0, PPS_REQUESTED, PPS_ARMED, PPS_FIRED };
typedef struct {
    volatile int64_t  tick;       /* FCY tick of the second's start */
    volatile uint32_t sec;        /* second being marked */
    volatile uint8_t  state;
    volatile uint16_t missed;
} pps_req_t;
extern pps_req_t g_pps;

/* Text sent on the NMEA UART right at the next PPS edge (from the OC2
 * interrupt). Filled by the main loop while g_pps.state != PPS_FIRED. */
#define NMEA_NEXT_SIZE 256
extern char g_nmea_next[NMEA_NEXT_SIZE];
extern volatile uint16_t g_nmea_next_len;
extern volatile uint8_t g_led1_on_pps;
extern volatile uint16_t g_pps_fired_blk16;   /* block counter at last PPS edge */

void     sampler_start(void);
uint64_t sampler_blocks(void);       /* completed DSP blocks */
uint16_t sampler_blocks16(void);

#endif
