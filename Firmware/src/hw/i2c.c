/*
 * Blocking I2C1 master, 100 kHz, every wait bounded so a stuck bus cannot
 * hang the firmware (the original busy-waited forever).
 */
#include "board.h"
#include "i2c.h"

#define I2C_TIMEOUT 20000u      /* polling iterations, a few ms */

static int wait_clear(volatile unsigned int *reg, unsigned int mask)
{
    uint16_t t = I2C_TIMEOUT;
    while ((*reg & mask) && --t) ;
    return t ? 0 : -1;
}

static int idle(void)
{
    /* SEN, RSEN, PEN, RCEN, ACKEN clear and not transmitting */
    if (wait_clear(&I2C1CON, 0x001F)) return -1;
    return wait_clear(&I2C1STAT, 0x4000);
}

static void bus_recover(void)
{
    I2C1CONbits.I2CEN = 0;
    __delay32(400);
    I2C1CONbits.I2CEN = 1;
}

void i2c_init(void)
{
    I2C1CON = 0;
    I2C1BRG = (uint16_t)(FCY_HZ / 100000L - FCY_HZ / 10000000L - 1);  /* 395 */
    I2C1CONbits.DISSLW = 1;
    I2C1CONbits.I2CEN = 1;
}

static int start(void)
{
    if (idle()) return -1;
    I2C1CONbits.SEN = 1;
    return wait_clear(&I2C1CON, 0x0001);
}

static void stop(void)
{
    idle();
    I2C1CONbits.PEN = 1;
    wait_clear(&I2C1CON, 0x0004);
}

static int send(uint8_t b)
{
    if (idle()) return -1;
    I2C1TRN = b;
    if (wait_clear(&I2C1STAT, 0x4000)) return -1;   /* TRSTAT */
    if (idle()) return -1;
    return I2C1STATbits.ACKSTAT ? -2 : 0;
}

int i2c_write(uint8_t addr, const uint8_t *data, uint8_t n)
{
    int r = start();
    if (!r) r = send((uint8_t)(addr << 1));
    while (!r && n--) r = send(*data++);
    stop();
    if (r == -1) bus_recover();
    return r;
}

int i2c_read(uint8_t addr, uint8_t *data, uint8_t n)
{
    int r = start();
    if (!r) r = send((uint8_t)((addr << 1) | 1));
    while (!r && n) {
        if (idle()) { r = -1; break; }
        I2C1CONbits.RCEN = 1;
        if (wait_clear(&I2C1CON, 0x0008)) { r = -1; break; }
        *data++ = (uint8_t)I2C1RCV;
        n--;
        I2C1CONbits.ACKDT = n ? 0 : 1;              /* NACK the last byte */
        I2C1CONbits.ACKEN = 1;
        if (wait_clear(&I2C1CON, 0x0010)) { r = -1; break; }
    }
    stop();
    if (r == -1) bus_recover();
    return r;
}
