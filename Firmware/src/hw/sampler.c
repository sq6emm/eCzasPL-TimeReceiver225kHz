/*
 * ADC sampling (Timer3 -> ADC1 -> DMA0 ping-pong), DSP scheduling and the
 * hardware-timed 1PPS output.
 *
 * Timebase: ADC sample s is converted at the s-th Timer3 period match; its
 * tick is defined as s * TICKS_PER_SAMPLE. Within the DMA interrupt of
 * block b (samples 20(b-1) .. 20b-1) the current tick is therefore
 * (20b - 1) * TICKS_PER_SAMPLE + TMR3.
 *
 * 1PPS: Timer2 free-runs at FCY/8. When the requested edge is less than
 * three blocks away, OC2 is armed to drive RP13 (SV1 pin 1) and RP16 (LED4)
 * high at exactly that Timer2 count (200 ns resolution); PPS_WIDTH_MS later
 * it is switched to drive low again.
 */
#include <string.h>
#include "board.h"
#include "sampler.h"
#include "nmea.h"
#include "uart.h"

dsp_t g_dsp;
pps_req_t g_pps;
char g_nmea_next[NMEA_NEXT_SIZE];
volatile uint16_t g_nmea_next_len;
volatile uint8_t g_led1_on_pps;
volatile uint16_t g_pps_fired_blk16;

static int16_t adc_a[DSP_BLOCK] __attribute__((space(dma)));
static int16_t adc_b[DSP_BLOCK] __attribute__((space(dma)));
static volatile uint64_t blocks;
static volatile uint16_t blocks16;
static volatile uint16_t pps_off_at, led1_off_at;
static volatile uint8_t pps_high, led1_lit;

uint64_t sampler_blocks(void)
{
    uint64_t b;
    __builtin_disi(0x3FFF);
    b = blocks;
    __builtin_disi(0);
    return b;
}

uint16_t sampler_blocks16(void)
{
    return blocks16;
}

void sampler_start(void)
{
    dsp_init(&g_dsp);

    /* Timer2: free running FCY/8 timebase for OC2 */
    T2CON = 0;
    T2CONbits.TCKPS = 1;
    PR2 = 0xFFFF;
    TMR2 = 0;

    /* Timer3: ADC trigger at ADC_FS_HZ */
    T3CON = 0;
    TMR3 = 0;
    PR3 = TICKS_PER_SAMPLE - 1;

    /* ADC1: AN0, 12-bit, signed fractional, Timer3 trigger, auto sample */
    AD1CON1 = 0;
    AD1CON1bits.AD12B = 1;
    AD1CON1bits.FORM = 3;
    AD1CON1bits.SSRC = 2;
    AD1CON1bits.ASAM = 1;
    AD1CON1bits.ADDMABM = 1;
    AD1CON2 = 0;
    AD1CON3 = 0;
    AD1CON3bits.ADCS = 4;       /* TAD = 5 Tcy = 125 ns (>= 117.6 ns for 12-bit);
                                   the original used 75 ns, out of spec */
    AD1CON4 = 0;
    AD1CHS0 = 0;                /* CH0+ = AN0, CH0- = VREFL */

    /* DMA0: ADC1 -> adc_a/adc_b, continuous ping-pong, DSP_BLOCK words each */
    DMA0CON = 0;
    DMA0CONbits.MODE = 2;
    DMA0REQ = 13;               /* ADC1 */
    DMA0PAD = (volatile unsigned int)&ADC1BUF0;
    DMA0STA = __builtin_dmaoffset(adc_a);
    DMA0STB = __builtin_dmaoffset(adc_b);
    DMA0CNT = DSP_BLOCK - 1;
    IPC1bits.DMA0IP = 6;
    IFS0bits.DMA0IF = 0;
    IEC0bits.DMA0IE = 1;
    DMA0CONbits.CHEN = 1;

    IPC1bits.OC2IP = 5;
    OC2CON = 0;

    AD1CON1bits.ADON = 1;
    T2CONbits.TON = 1;
    T3CONbits.TON = 1;
}

void __attribute__((interrupt, auto_psv)) _DMA0Interrupt(void)
{
    uint16_t tmr2 = TMR2, tmr3 = TMR3;
    const int16_t *buf;
    uint64_t b;

    IFS0bits.DMA0IF = 0;
    /* PPST0 = 1: channel now uses STB, so buffer A has just been filled */
    buf = DMACS1bits.PPST0 ? adc_a : adc_b;
    b = ++blocks;
    ++blocks16;

    if (g_pps.state == PPS_REQUESTED) {
        int64_t now = ((int64_t)b * DSP_BLOCK - 1) * TICKS_PER_SAMPLE + tmr3;
        int64_t dt = g_pps.tick - now;
        if (dt <= 0) {
            g_pps.missed++;
            g_pps.state = PPS_IDLE;
        } else if (dt < 3L * TICKS_PER_BLOCK) {
            OC2CON = 0;
            OC2R = (uint16_t)(tmr2 + (uint16_t)(dt >> 3));
            IFS0bits.OC2IF = 0;
            IEC0bits.OC2IE = 1;
            OC2CONbits.OCM = 1;             /* init low, compare forces high */
            g_pps.state = PPS_ARMED;
        }
    }
    if (pps_high && (int16_t)(blocks16 - pps_off_at) >= 0) {
        OC2R = (uint16_t)(TMR2 + 8);
        OC2CONbits.OCM = 2;                 /* stays high, compare forces low */
        pps_high = 0;
    }
    if (led1_lit && (int16_t)(blocks16 - led1_off_at) >= 0) {
        LED1(0);
        led1_lit = 0;
    }

    dsp_block(&g_dsp, buf);
    LED2(g_dsp.det_state != 0);
}

void __attribute__((interrupt, no_auto_psv)) _OC2Interrupt(void)
{
    IFS0bits.OC2IF = 0;
    IEC0bits.OC2IE = 0;
    if (g_nmea_next_len) {
        nmea_write(g_nmea_next, g_nmea_next_len);
        g_nmea_next_len = 0;
    }
    g_pps_fired_blk16 = blocks16;
    pps_off_at = (uint16_t)(blocks16 + PPS_WIDTH_MS / 2);
    pps_high = 1;
    if (g_led1_on_pps) {
        LED1(1);
        led1_off_at = (uint16_t)(blocks16 + 250);
        led1_lit = 1;
        g_led1_on_pps = 0;
    }
    g_pps.state = PPS_FIRED;
}
