#ifndef SI4735_H
#define SI4735_H

#include <stdint.h>

typedef struct {
    uint8_t rssi;      /* dBuV */
    uint8_t snr;       /* dB */
    uint8_t status;
} si4735_rsq_t;

/* Reset, load SSB patch and tune 224 kHz USB. Returns 0 on success. */
int si4735_init(void);
int si4735_rsq(si4735_rsq_t *q);
/* Audio output volume 0..63 (RX_VOLUME); kept across si4735_init(). */
int si4735_set_volume(uint8_t v);
uint8_t si4735_volume(void);

extern const uint16_t SI4735_PATCH_LINES;
extern const uint16_t SI4735_PATCH_CMD15_COUNT;
extern const uint16_t SI4735_PATCH_CMD15[];
extern const uint8_t  SI4735_PATCH[];

#endif
