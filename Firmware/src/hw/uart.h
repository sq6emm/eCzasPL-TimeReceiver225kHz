#ifndef UART_H
#define UART_H

#include <stdint.h>

/* UART1 = diagnostics (SV1), UART2 = NMEA (SV2). Interrupt-driven TX. */
void uart_init(void);
void dbg_write(const char *s, uint16_t n);
void dbg_puts(const char *s);
void dbg_printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void nmea_write(const char *s, uint16_t n);   /* safe to call from an ISR */
uint16_t dbg_dropped(void);
uint16_t nmea_free(void);
/* Send queued diagnostics lines to the NMEA port as $PECZ sentences. Only
 * call when no $GPRMC is due within the next few hundred ms. */
void dbg_mirror_flush(void);

#endif
