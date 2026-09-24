#ifndef NMEA_H
#define NMEA_H

#include "timekeeper.h"

#define NMEA_MAX 96

/* Formats $GPRMC for the second described by d into buf (>= NMEA_MAX bytes).
 * Returns the length. have_time = 0 produces an empty "no fix" sentence. */
int nmea_rmc(char *buf, const tk_date_t *d, int have_time, int valid);

#endif
