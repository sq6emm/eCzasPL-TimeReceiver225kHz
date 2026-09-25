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
 */
#include "board.h"
#include "i2c.h"
#include "si4735.h"
#include "uart.h"

#define ADDR 0x11

static uint8_t volume = VOL_START;

#define CMD_POWER_UP      0x01
#define CMD_GET_REV       0x10
#define CMD_POWER_DOWN    0x11
#define CMD_SET_PROPERTY  0x12
#define CMD_SSB_TUNE_FREQ 0x40
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

static int tune(uint8_t cap_h, uint8_t cap_l)
{
    /* ARG1 = USB (2 << 6) | 1, as sent by the original */
    uint8_t c[6] = { CMD_SSB_TUNE_FREQ, 0x81, 0x00, 224, cap_h, cap_l };
    int r = cmd(c, 6);
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
    tune(0x00, 0x01);
    delay_ms(550);
    set_property(PROP_SSB_MODE, 0x9015);
    set_property(PROP_SSB_BFO, 0);
    set_property(PROP_AM_AVC_MAX_GAIN, 89 * 340);
    if (tune(0x82, 0xB8)) return -5;

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
