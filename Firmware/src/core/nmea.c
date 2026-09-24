/*
 * NMEA 0183 output. Same $GPRMC layout as the original firmware so existing
 * consumers keep working; the status field is now honest ('V' when the time
 * is not synchronised or older than HOLDOVER_VALID_S).
 */
#include "nmea.h"

static char *put2(char *p, uint8_t v)
{
    *p++ = (char)('0' + v / 10);
    *p++ = (char)('0' + v % 10);
    return p;
}

static char *puts_(char *p, const char *s)
{
    while (*s) *p++ = *s++;
    return p;
}

static char *finish(char *start, char *p)
{
    static const char HEX[] = "0123456789ABCDEF";
    uint8_t cs = 0;
    const char *q;
    for (q = start + 1; q < p; q++) cs ^= (uint8_t)*q;
    *p++ = '*';
    *p++ = HEX[cs >> 4];
    *p++ = HEX[cs & 15];
    *p++ = '\r';
    *p++ = '\n';
    *p = 0;
    return p;
}

int nmea_rmc(char *buf, const tk_date_t *d, int have_time, int valid)
{
    char *p = buf;
    if (!have_time)
        return (int)(finish(buf, puts_(p, "$GPRMC,,V,,,,,,,,,,N")) - buf);
    p = puts_(p, "$GPRMC,");
    p = put2(p, d->hour); p = put2(p, d->min); p = put2(p, d->sec);
    *p++ = ',';
    *p++ = valid ? 'A' : 'V';
    p = puts_(p, "," NMEA_LAT "," NMEA_LON ",0.00,000.0,");
    p = put2(p, d->day); p = put2(p, d->mon); p = put2(p, (uint8_t)(d->year % 100));
    p = puts_(p, ",,E,");
    *p++ = valid ? 'A' : 'N';
    return (int)(finish(buf, p) - buf);
}
