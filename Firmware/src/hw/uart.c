/*
 * UART1 (diagnostics) and UART2 (NMEA), 115200 8N1.
 *
 * BRGH = 1 gives 114943 baud (-0.2 %). The original used BRGH = 0 with
 * BRG = 20, i.e. 119048 baud (+3.3 %), which is at the edge of what many
 * receivers tolerate.
 */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "board.h"
#include "uart.h"

#define BRG_VALUE   ((FCY_HZ / (4L * UART_BAUD)) - 1)
#define DBG_SIZE    1024
#define NMEA_SIZE   1024
#define MIR_SIZE    1024

static volatile char dbg_buf[DBG_SIZE];
static volatile uint16_t dbg_head, dbg_tail, dbg_drop;
static volatile char nmea_buf[NMEA_SIZE];
static volatile uint16_t nmea_head, nmea_tail;
#if DEBUG_TO_NMEA
static char mir_buf[MIR_SIZE];              /* debug text waiting for SV2 */
static uint16_t mir_head, mir_tail, mir_lines;
#endif

void uart_init(void)
{
    U1MODE = 0; U1MODEbits.BRGH = 1; U1BRG = BRG_VALUE;
    U2MODE = 0; U2MODEbits.BRGH = 1; U2BRG = BRG_VALUE;
    U1STAbits.UTXISEL0 = 0; U1STAbits.UTXISEL1 = 0;   /* irq when a char moves to TSR */
    U2STAbits.UTXISEL0 = 0; U2STAbits.UTXISEL1 = 0;
    IPC3bits.U1TXIP = 3;
    IPC7bits.U2TXIP = 3;
    U1MODEbits.UARTEN = 1; U1STAbits.UTXEN = 1;
    U2MODEbits.UARTEN = 1; U2STAbits.UTXEN = 1;
    IFS0bits.U1TXIF = 0; IEC0bits.U1TXIE = 1;
    IFS1bits.U2TXIF = 0; IEC1bits.U2TXIE = 1;
    IEC0bits.U1RXIE = 0; IEC1bits.U2RXIE = 0;
}

void __attribute__((interrupt, no_auto_psv)) _U1TXInterrupt(void)
{
    IFS0bits.U1TXIF = 0;
    while (!U1STAbits.UTXBF && dbg_tail != dbg_head) {
        U1TXREG = dbg_buf[dbg_tail];
        dbg_tail = (dbg_tail + 1) & (DBG_SIZE - 1);
    }
}

void __attribute__((interrupt, no_auto_psv)) _U2TXInterrupt(void)
{
    IFS1bits.U2TXIF = 0;
    while (!U2STAbits.UTXBF && nmea_tail != nmea_head) {
        U2TXREG = nmea_buf[nmea_tail];
        nmea_tail = (nmea_tail + 1) & (NMEA_SIZE - 1);
    }
}

static void mirror(const char *s, uint16_t n)
{
#if DEBUG_TO_NMEA
    while (n--) {
        char c = *s++;
        uint16_t next = (mir_head + 1) & (MIR_SIZE - 1);
        if (c == '\r') continue;
        if (c == '*' || c == '$') c = '#';  /* would break the NMEA framing */
        if (next == mir_tail) return;       /* full: drop */
        mir_buf[mir_head] = c;
        mir_head = next;
        if (c == '\n') mir_lines++;
    }
#else
    (void)s; (void)n;
#endif
}

void dbg_write(const char *s, uint16_t n)
{
    mirror(s, n);
    while (n--) {
        uint16_t next = (dbg_head + 1) & (DBG_SIZE - 1);
        if (next == dbg_tail) { dbg_drop++; continue; }     /* never block */
        dbg_buf[dbg_head] = *s++;
        dbg_head = next;
    }
    IFS0bits.U1TXIF = 1;                                    /* kick */
}

void dbg_puts(const char *s)
{
    dbg_write(s, (uint16_t)strlen(s));
}

void dbg_printf(const char *fmt, ...)
{
    char line[160];
    va_list ap;
    int n;
    va_start(ap, fmt);
    n = vsnprintf(line, sizeof line, fmt, ap);
    va_end(ap);
    if (n > (int)sizeof line - 1) n = sizeof line - 1;
    if (n > 0) dbg_write(line, (uint16_t)n);
}

uint16_t dbg_dropped(void)
{
    return dbg_drop;
}

uint16_t nmea_free(void)
{
    return (uint16_t)((nmea_tail - nmea_head - 1) & (NMEA_SIZE - 1));
}

void dbg_mirror_flush(void)
{
#if DEBUG_TO_NMEA
    char line[200];
    while (mir_lines) {
        uint16_t n = 6, i = mir_tail;
        uint8_t cs = 0;
        const char *q;
        memcpy(line, "$PECZ,", 6);
        while (mir_buf[i] != '\n') {
            if (n < sizeof line - 8) line[n++] = mir_buf[i];
            i = (i + 1) & (MIR_SIZE - 1);
        }
        for (q = line + 1; q < line + n; q++) cs ^= (uint8_t)*q;
        n += (uint16_t)sprintf(line + n, "*%02X\r\n", cs);
        if (nmea_free() < n) return;        /* try again later */
        mir_tail = (i + 1) & (MIR_SIZE - 1);
        mir_lines--;
        __builtin_disi(0x3FFF);             /* atomic w.r.t. the PPS interrupt */
        nmea_write(line, n);
        __builtin_disi(0);
    }
#endif
}

void nmea_write(const char *s, uint16_t n)
{
    while (n--) {
        uint16_t next = (nmea_head + 1) & (NMEA_SIZE - 1);
        if (next == nmea_tail) break;
        nmea_buf[nmea_head] = *s++;
        nmea_head = next;
    }
    IFS1bits.U2TXIF = 1;
}
