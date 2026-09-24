/*
 * Clock, configuration bits, GPIO and peripheral pin select.
 */
#include "board.h"

/* Start on FRC, switch to 10 MHz HS crystal + PLL in board_init() (same
 * approach as the original firmware). Watchdog is enabled from software
 * once the radio is initialised: LPRC/128/512 = ~2 s. */
#pragma config FNOSC = FRC, IESO = OFF
#pragma config POSCMD = HS, OSCIOFNC = OFF, IOL1WAY = OFF, FCKSM = CSECMD
#pragma config FWDTEN = OFF, WINDIS = OFF, WDTPRE = PR128, WDTPOST = PS512
#pragma config FPWRT = PWR128, ALTI2C = OFF
#pragma config ICS = PGD1, JTAGEN = OFF
#pragma config GWRP = OFF, GSS = OFF

/* peripheral output function codes (dsPIC33FJ128GP804 datasheet, table 11-2) */
#define RPOUT_U1TX  3
#define RPOUT_U2TX  5
#define RPOUT_OC2   19

void delay_ms(uint16_t ms)
{
    while (ms--) {
        __delay32(FCY / 1000);
        ClrWdt();
    }
}

static void clock_init(void)
{
    /* Fosc = 10 MHz / N1(2) * M(32) / N2(2) = 80 MHz, Fcy = 40 MHz */
    PLLFBD = 30;                    /* M = 32 */
    CLKDIVbits.PLLPOST = 0;         /* N2 = 2 */
    CLKDIVbits.PLLPRE = 0;          /* N1 = 2 */
    __builtin_write_OSCCONH(0x03);  /* primary oscillator with PLL */
    __builtin_write_OSCCONL(OSCCON | 0x01);
    while (OSCCONbits.COSC != 0x3) ;
    while (!OSCCONbits.LOCK) ;
}

void board_init(void)
{
    RCONbits.SWDTEN = 0;
    clock_init();

    AD1PCFGL = 0xFFFE;              /* only AN0 analog; RB2/RB3 (AN4/5) are UART */

    LATA = 0; LATB = 0; LATC = 0;
    LATCbits.LATC3 = 1;             /* radio out of reset */
    LATCbits.LATC4 = 1;
    LATCbits.LATC5 = 1;
    TRISAbits.TRISA0 = 1;           /* MOD_IN */
    TRISAbits.TRISA8 = 0;           /* LED3 */
    TRISAbits.TRISA10 = 0;          /* LED2 */
    TRISBbits.TRISB11 = 0;          /* LED1 */
    TRISBbits.TRISB12 = 0;          /* U1TX */
    TRISBbits.TRISB13 = 0;          /* PPS */
    TRISBbits.TRISB3 = 0;           /* U2TX */
    TRISBbits.TRISB2 = 1;           /* U2RX */
    TRISCbits.TRISC0 = 0;           /* LED4 / PPS */
    TRISCbits.TRISC3 = 0;           /* RRST */
    TRISCbits.TRISC4 = 0;
    TRISCbits.TRISC5 = 0;
    CNPU1bits.CN6PUE = 1;           /* pull-up on the unused NMEA RX line (R2 fix) */

    __builtin_write_OSCCONL(OSCCON & ~(1 << 6));     /* unlock PPS */
    RPOR6bits.RP12R = RPOUT_U1TX;
    RPOR6bits.RP13R = RPOUT_OC2;
    RPOR1bits.RP3R = RPOUT_U2TX;
    RPOR8bits.RP16R = RPOUT_OC2;
    RPINR19bits.U2RXR = 2;
    __builtin_write_OSCCONL(OSCCON | (1 << 6));      /* lock PPS */
}
