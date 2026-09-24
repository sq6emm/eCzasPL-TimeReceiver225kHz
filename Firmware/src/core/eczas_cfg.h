/*
 * e-CzasPL 225 kHz time receiver - compile-time configuration.
 *
 * Everything here is shared by the dsPIC firmware and the host test harness.
 */
#ifndef ECZAS_CFG_H
#define ECZAS_CFG_H

/* ---- Sampling ------------------------------------------------------------ */
#define FCY_HZ            40000000L   /* instruction clock (10 MHz xtal, PLL x32/2/2) */
#define ADC_FS_HZ         10000       /* ADC sample rate (Timer3 period = FCY/ADC_FS) */
#define TICKS_PER_SAMPLE  (FCY_HZ / ADC_FS_HZ)            /* 4000 */
#define DSP_BLOCK         20          /* ADC samples per DSP block -> 500 Hz */
#define DSP_RATE_HZ       (ADC_FS_HZ / DSP_BLOCK)         /* 500 */
#define TICKS_PER_BLOCK   (TICKS_PER_SAMPLE * DSP_BLOCK)  /* 80000 */

/* ---- Signal -------------------------------------------------------------- */
#define AUDIO_CARRIER_HZ  1000        /* SI4735 USB at 224 kHz -> carrier at ~1 kHz */
#define BIT_RATE          50
#define SPB               (DSP_RATE_HZ / BIT_RATE)        /* 10 blocks per bit */
#define FRAME_BITS        96
#define PREAMBLE_BITS     27          /* 0x5555, 0x60, 101 */
#define RAMP_BLOCKS       8           /* measured phase slew ~16 ms */

/* Frame detector: normalised correlation threshold against preamble (0..1),
 * expressed in 1/1024 units. Real frames score 0.35..0.95; everything that
 * passes is still subject to RS + CRC + time consistency checks. */
#ifndef DET_THRESHOLD_Q10
#define DET_THRESHOLD_Q10 360
#endif

/* Chase decoding: number of least-reliable bits tried when a frame fails. */
#define CHASE_BITS        5

/* ---- Timekeeping --------------------------------------------------------- */
/* Delay between the moment encoded in a frame and the moment the frame start
 * is seen by our detector (radio + transmitter). The spec says the frame starts
 * exactly at the encoded second. PA3FWM measured (Sep 2026) the transmitter to
 * be 10..25 ms late incl. ~3 ms propagation; adjust here after calibrating
 * against GPS if you need better than a few tens of milliseconds. */
#define TX_DELAY_US       0L
#define RX_DELAY_US       0L          /* SI4735 audio path; not measured */

#define HOLDOVER_VALID_S  (24L * 3600L)  /* LED3/'A' status kept this long w/o frames */
#define SYNC_MAX_ERR_MS   100         /* frame accepted if within this of our clock */

/* ---- Outputs ------------------------------------------------------------- */
#define UART_BAUD         115200L
#define PPS_WIDTH_MS      100

/* Copy the human readable diagnostics (SV1) to the NMEA port (SV2) as
 * proprietary $PECZ,<text>*hh sentences, sent between the $GPRMC sentences.
 * NMEA/GPS software ignores unknown sentences. Set to 0 for a clean NMEA
 * stream. */
#define DEBUG_TO_NMEA     1
/* Report the audio level every second for this long after power-up (to
 * adjust the gain trimmer R27), then every STATUS_PERIOD_S. */
#define LEVEL_FAST_S      180
#define STATUS_PERIOD_S   10

/* Position reported in $GPRMC (GUM Time and Frequency Laboratory, Warsaw). */
#define NMEA_LAT          "5214.5098,N"
#define NMEA_LON          "02100.0504,E"

#define FW_VERSION        "2.0.0"

#endif
