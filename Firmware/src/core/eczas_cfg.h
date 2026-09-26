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
#ifndef RAMP_BLOCKS
#define RAMP_BLOCKS       8           /* measured phase slew ~16 ms */
#endif

/* Frame detector: normalised correlation threshold against preamble (0..1),
 * expressed in 1/1024 units. Real frames score 0.35..0.95; everything that
 * passes is still subject to RS + CRC + time consistency checks. */
#ifndef DET_THRESHOLD_Q10
#define DET_THRESHOLD_Q10 300
#endif

/* Chase decoding: number of least-reliable bits tried when a frame fails
 * (2^n - 1 variants). 7 on the recordings: +9..+64 % frames over 5 (the most
 * at -3..-6 dB) with no wrong time reaching the clock; about 2.4x as many
 * miscorrected frames reach the timekeeper (which rejects them). Worst case
 * 1.1 M cycles (28 ms) per candidate. 8 would add 5-10 % more frames for
 * ~65 % more miscorrections and 55 ms. */
#ifndef CHASE_BITS
#define CHASE_BITS        7
#endif

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
 * Off by default: many lines are longer than the 82 characters NMEA 0183
 * allows, which overflows the line buffer of simple NMEA readers (seen with
 * the MGMBeacon, 90 bytes). Build with "make NMEA_DEBUG=1" to enable. */
#ifndef DEBUG_TO_NMEA
#define DEBUG_TO_NMEA     0
#endif
/* Report the audio level every second for this long after power-up (to
 * adjust the gain trimmer R27), then every STATUS_PERIOD_S. */
#define LEVEL_FAST_S      180

/* Audio level control: once a second the SI4735 volume (0..63) is stepped
 * down while the ADC clips or the level is above LEVEL_HIGH_PCT, and up while
 * it stays below LEVEL_LOW_PCT for LEVEL_UP_S seconds. The demodulator uses
 * only the carrier phase, so volume changes do not disturb it. R27 needs
 * adjusting only when the volume sits at VOL_MIN or 63. */
#define VOL_MIN           10
#define VOL_START         58          /* 63 clipped for the first second on the bench board */
#define LEVEL_HIGH_PCT    85
#define LEVEL_LOW_PCT     40
#define LEVEL_UP_S        5
#define LEVEL_CLIP_MAX    2           /* clipped samples per second tolerated (atmospherics) */
#define STATUS_PERIOD_S   10

/* SI4735 antenna capacitor (ANTCAP, 95 fF steps). The original firmware sends
 * 0x82B8; at power-up the receiver logs what the chip does with it. With
 * ANTCAP_SWEEP it then sweeps 0..~584 pF (about 10 s) and keeps the value with
 * the highest RSSI, but only if that is a real resonance: a peak inside the
 * range that drops by ANTCAP_MIN_GAIN_DB on both sides (a tank the capacitor
 * can tune, e.g. ferrite + external C0G capacitor). Otherwise it keeps 0x82B8,
 * which the chip clamps to its maximum, 6143 (584 pF). Without a tank the
 * RSSI (mostly noise) still varies by ~20 dB, with maxima at the ends. */
#define ANTCAP_ORIGINAL   0x82B8
#define ANTCAP_MAX        6144
#ifndef ANTCAP_SWEEP
#define ANTCAP_SWEEP      1
#endif
#define ANTCAP_MIN_GAIN_DB 3

/* Position reported in $GPRMC (GUM Time and Frequency Laboratory, Warsaw). */
#define NMEA_LAT          "5214.5098,N"
#define NMEA_LON          "02100.0504,E"

/* Last-resort decoding of frames that RS and Chase could not fix: erase up
 * to this many least reliable RS symbols (2, 4, 6; 0 = off). */
#ifndef GMD_MAX_ERASURES
#define GMD_MAX_ERASURES  4
#endif

/* Once synchronised, a frame that cannot be decoded is compared with the
 * frame the clock expects at that moment (number, time zone and flags from
 * the last good frame). If at least this share of its soft bits agrees
 * (1/1024), it counts as a timing update; it can never set a new time.
 * Undecodable true frames score ~0.85-0.95, a wrong frame number <= 0.69. */
#ifndef CONFIRM_MIN_Q10
#define CONFIRM_MIN_Q10   820
#endif

#define FW_VERSION        "2.0.4"

#endif
