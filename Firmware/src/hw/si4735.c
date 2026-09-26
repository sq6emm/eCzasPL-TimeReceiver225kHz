/*
 * SI4735 set-up: SSB (USB) receiver tuned to 224 kHz, so the 225 kHz carrier
 * appears as a ~1 kHz tone at the audio output.
 *
 * The command sequence and property values are the ones used by the original
 * firmware (a port of the PU2CLR SI4735 library), recovered from its
 * disassembly, so the radio is configured exactly as before:
 *   SSB_MODE = 0x9015: 1 kHz band-pass audio filter, AVC on, SSB AFC off
 *   SSB_BFO = 0, AVC max gain = 89 dB, volume = 63 (the original; this
 *   firmware starts at VOL_START and the level control in main.c adjusts it)
 *   SSB_TUNE_FREQ 224 kHz, USB, antenna cap bytes 0x82 0xB8
 *
 * si4735_antenna_tune() can then sweep the internal antenna capacitor
 * (ANTCAP, 95 fF steps up to ~584 pF) for the highest RSSI. That only helps
 * with an external tank (e.g. 360 uH ferrite + ~1.07 nF C0G): the SI4735
 * cannot resonate a ferrite antenna at 225 kHz on its own.
 */
#include "board.h"
#include "i2c.h"
#include "si4735.h"
#include "uart.h"

#define ADDR 0x11

static uint8_t volume = VOL_START;
static uint16_t antcap = ANTCAP_ORIGINAL;   /* sent with every tune */

#define CMD_POWER_UP      0x01
#define CMD_GET_REV       0x10
#define CMD_POWER_DOWN    0x11
#define CMD_SET_PROPERTY  0x12
#define CMD_SSB_TUNE_FREQ 0x40
#define CMD_SSB_TUNE_STATUS 0x42
#define CMD_AM_RSQ_STATUS 0x43

#define PROP_SSB_BFO           0x0100
#define PROP_SSB_MODE          0x0101
#define PROP_AM_AVC_MAX_GAIN   0x3103
#define PROP_RX_VOLUME         0x4000

static int wait_cts(void)
{
    uint8_t st = 0;
    uint16_t n;
    for (n = 0; n < 1000; n++) {             /* up to ~300 ms */
        if (i2c_read(ADDR, &st, 1) == 0 && (st & 0x80))
            return (st & 0x40) ? 1 : 0;      /* 1 = ERR bit set (still usable) */
        __delay32(FCY / 3333);               /* 0.3 ms */
        ClrWdt();
    }
    return -1;
}

static int cmd(const uint8_t *c, uint8_t n)
{
    if (wait_cts() < 0) return -1;
    if (i2c_write(ADDR, c, n)) return -1;
    return wait_cts() < 0 ? -1 : 0;
}

static int set_property(uint16_t prop, uint16_t val)
{
    uint8_t c[6] = { CMD_SET_PROPERTY, 0, (uint8_t)(prop >> 8), (uint8_t)prop,
                     (uint8_t)(val >> 8), (uint8_t)val };
    int r = cmd(c, 6);
    delay_ms(10);
    return r;
}

/* Returns 0, 1 if the chip flagged the command with ERR, -1 if it didn't answer. */
static int tune(uint16_t cap)
{
    /* ARG1 = USB (2 << 6) | 1, as sent by the original */
    uint8_t c[6] = { CMD_SSB_TUNE_FREQ, 0x81, 0x00, 224, (uint8_t)(cap >> 8), (uint8_t)cap };
    int r;
    if (wait_cts() < 0 || i2c_write(ADDR, c, 6)) return -1;
    r = wait_cts();
    delay_ms(100);
    return r;
}

static int load_patch(void)
{
    uint16_t line, k = 0;
    for (line = 0; line < SI4735_PATCH_LINES; line++) {
        uint8_t c[8], i;
        c[0] = 0x16;
        if (k < SI4735_PATCH_CMD15_COUNT && SI4735_PATCH_CMD15[k] == line) {
            c[0] = 0x15;
            k++;
        }
        for (i = 0; i < 7; i++) c[1 + i] = SI4735_PATCH[line * 7u + i];
        if (i2c_write(ADDR, c, 8)) return -1;
        if (wait_cts() < 0) return -1;
    }
    return 0;
}

int si4735_init(void)
{
    uint8_t c[3], rev[8];
    int i;

    RADIO_RST(1); delay_ms(100);
    RADIO_RST(0); delay_ms(100);
    RADIO_RST(1); delay_ms(100);

    /* query library id (FUNC = 15), as the original does before patching */
    c[0] = CMD_POWER_UP; c[1] = 0x1F; c[2] = 0x05;
    if (cmd(c, 3)) { dbg_puts("SI4735: no response\r\n"); return -1; }
    if (i2c_read(ADDR, rev, 8) == 0)
        dbg_printf("SI4735: part %02X fw %c%c chip %c lib %u\r\n",
                   rev[1], rev[2], rev[3], rev[6], rev[7]);

    /* power up AM with patch enable, crystal oscillator, analog audio */
    c[0] = CMD_POWER_UP; c[1] = 0x31; c[2] = 0x05;
    if (cmd(c, 3)) return -2;
    delay_ms(550);                            /* crystal start-up */

    dbg_puts("SI4735: loading SSB patch...");
    if (load_patch()) { dbg_puts(" FAILED\r\n"); return -3; }
    dbg_puts(" done\r\n");

    c[0] = CMD_POWER_UP; c[1] = 0x11; c[2] = 0x05;   /* like PU2CLR setSSB() */
    cmd(c, 3);
    delay_ms(200);

    if (set_property(PROP_SSB_MODE, 0x9012)) return -4;
    set_property(PROP_RX_VOLUME, volume);
    tune(1);
    delay_ms(550);
    set_property(PROP_SSB_MODE, 0x9015);
    set_property(PROP_SSB_BFO, 0);
    set_property(PROP_AM_AVC_MAX_GAIN, 89 * 340);
    if (tune(antcap) < 0) return -5;

    c[0] = CMD_GET_REV;
    if (cmd(c, 1) == 0 && i2c_read(ADDR, rev, 8) == 0) {
        dbg_puts("SI4735: rev");
        for (i = 1; i < 8; i++) dbg_printf(" %02X", rev[i]);
        dbg_puts("\r\n");
    }
    return 0;
}

int si4735_set_volume(uint8_t v)
{
    if (v > 63) v = 63;
    volume = v;
    return set_property(PROP_RX_VOLUME, v);
}

uint8_t si4735_volume(void)
{
    return volume;
}

int si4735_rsq(si4735_rsq_t *q)
{
    uint8_t c[2] = { CMD_AM_RSQ_STATUS, 0x01 }, r[6];
    if (cmd(c, 2)) return -1;
    if (i2c_read(ADDR, r, 6)) return -1;
    q->status = r[0];
    q->rssi = r[4];
    q->snr = r[5];
    return 0;
}

/* ANTCAP the chip is using now (95 fF units), or -1 */
static int32_t read_antcap(void)
{
    uint8_t c[2] = { CMD_SSB_TUNE_STATUS, 0x01 }, r[8];
    if (cmd(c, 2) || i2c_read(ADDR, r, 8)) return -1;
    return ((uint16_t)r[6] << 8) | r[7];
}

/* sum of 4 RSSI readings (dBuV x 4) after a tune */
static int16_t rssi4(void)
{
    si4735_rsq_t q;
    int16_t s = 0;
    uint8_t i;
    delay_ms(50);
    for (i = 0; i < 4; i++) {
        if (si4735_rsq(&q)) return -1;
        s += q.rssi;
        delay_ms(25);
    }
    return s;
}

static void put_pf(uint16_t cap)          /* 95 fF steps -> "12.3 pF" */
{
    uint32_t ff = (uint32_t)cap * 95;
    dbg_printf("%lu.%lu pF", (unsigned long)(ff / 1000), (unsigned long)(ff / 100 % 10));
}

/* A tank shows as one peak inside the range that falls by ANTCAP_MIN_GAIN_DB
 * on both sides within 3 coarse steps (about 70 pF). */
static int is_peak(const int16_t *v, uint8_t n, uint8_t k)
{
    int16_t lo = v[k] - ANTCAP_MIN_GAIN_DB * 4;
    uint8_t i, l = 0, r = 0;
    if (k < 3 || k + 3 >= n) return 0;
    for (i = 1; i <= 3; i++) {
        if (v[k - i] <= lo) l = 1;
        if (v[k + i] <= lo) r = 1;
    }
    return l && r;
}

void si4735_antenna_tune(void)
{
    int32_t now = read_antcap();
    int err = tune(antcap);

    dbg_printf("SI4735: standard antenna setting 0x%04X %s, chip uses ANTCAP %ld",
               ANTCAP_ORIGINAL, err > 0 ? "REFUSED (ERR)" : "accepted", (long)read_antcap());
    dbg_printf(" (before: %ld)\r\n", (long)now);
#if ANTCAP_SWEEP
    {
        int16_t v[24], vmax = -1;
        uint16_t cap, best = 0, first = 0, last = 0;
        uint8_t k, kb = 0;

        dbg_puts("SI4735: antenna sweep, RSSI dBuV:");
        for (k = 0; k < 24; k++) {                        /* coarse, 256 steps (24 pF) */
            tune(128 + 256u * k);
            v[k] = rssi4();
            if (v[k] < 0) { dbg_puts(" radio error\r\n"); tune(antcap); return; }
            dbg_printf(" %d", v[k] / 4);
            if (v[k] > v[kb]) kb = k;
        }
        dbg_puts("\r\n");
        if (!is_peak(v, 24, kb)) {
            dbg_puts("SI4735: no antenna resonance in range - keeping the standard setting\r\n");
            tune(antcap);
            return;
        }
        best = 128 + 256u * kb;
        for (cap = best - 256; cap <= best + 256; cap += 32) {   /* fine, 3 pF */
            int16_t f;
            tune(cap);
            f = rssi4();
            if (f > vmax) { vmax = f; first = last = cap; }
            else if (f == vmax) last = cap;
        }
        antcap = (uint16_t)((first + last) / 2);          /* middle of the peak */
        tune(antcap);
        dbg_puts("SI4735: antenna tuned, ANTCAP ");
        put_pf(antcap);
        dbg_printf(", RSSI %d dBuV\r\n", vmax / 4);
    }
#endif
}
