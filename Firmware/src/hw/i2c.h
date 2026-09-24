#ifndef I2C_H
#define I2C_H

#include <stdint.h>

void i2c_init(void);
/* 7-bit address. Return 0 on success, negative on NACK / timeout. */
int  i2c_write(uint8_t addr, const uint8_t *data, uint8_t n);
int  i2c_read(uint8_t addr, uint8_t *data, uint8_t n);

#endif
