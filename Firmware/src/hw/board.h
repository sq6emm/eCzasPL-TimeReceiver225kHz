/*
 * Board support for the simplified e-CzasPL receiver (dsPIC33FJ128GP804 + SI4735).
 *
 * Pin map (from KiCAD/1.0 and the original firmware):
 *   RA0/AN0   MOD_IN     audio from SI4735 via MCP607
 *   RB8/RB9   SCL1/SDA1  SI4735 I2C (address 0x11)
 *   RC3       RRST       SI4735 reset (active low)
 *   RB12/RP12 SV1-3      diagnostic UART1 TX (115200 8N1)
 *   RB13/RP13 SV1-1      1PPS output (OC2)
 *   RB3/RP3   SV2-1      NMEA UART2 TX (115200 8N1)
 *   RB2/RP2   SV2-3      UART2 RX (unused, internal pull-up)
 *   RB11      LED1       frame received and verified
 *   RA10      LED2       DCD - frame being received
 *   RA8       LED3       time valid (synchronised within 24 h)
 *   RC0/RP16  LED4       1PPS (OC2)
 *   RC4, RC5  DIR, STEP  unused on this board, driven high like the original
 */
#ifndef BOARD_H
#define BOARD_H

#include <xc.h>
#include <stdint.h>
#include "eczas_cfg.h"

#define FCY FCY_HZ
#include <libpic30.h>

#define LED1(on)   (LATBbits.LATB11 = (on))
#define LED2(on)   (LATAbits.LATA10 = (on))
#define LED3(on)   (LATAbits.LATA8 = (on))
#define RADIO_RST(level) (LATCbits.LATC3 = (level))

void board_init(void);
void delay_ms(uint16_t ms);

#endif
