# Notes on the original firmware (`uproszczony_odbiornik.hex`)

Recovered by disassembling the HEX with `xc16-objdump` (addresses are program
counter values). The image identifies itself as `GUMIAK ver. v0.1-73-g669641d-dirty`.
It is built with XC16 and uses a port of the PU2CLR SI4735 library.

## Hardware set-up

| Item | Setting |
|---|---|
| Clock | FRC at reset, then 10 MHz HS × 32 / 2 / 2 → Fcy 40 MHz (`main` 0x4918) |
| ADC | AN0, 12-bit signed fractional, Timer3 trigger, PR3 = 3999 → 10 kHz, ADCS = 2 (TAD 75 ns, below the 117.6 ns minimum) |
| DMA0 | ADC → ping-pong 2 × 20 words, interrupt every 2 ms; the only interrupt used |
| UART1 | TX on RP12 (SV1), BRG 20, BRGH 0 → 119 kbaud (+3.3 %) – diagnostics |
| UART2 | TX RP3 / RX RP2 (SV2), same rate – `$GPRMC` and `$PGGUM` |
| OC2 | RP11 (LED1) and RP13 (SV1 pin 1), Timer2 clocked from OC1 on RP7 |
| I²C1 | 100 kHz, SI4735 at 0x11, reset on RC3 |
| LEDs | RA8 blinks on the 1 s tick, RA10 = fix < 24 h, RC0 = frame being received |

SI4735 sequence (0x42AC): POWER_UP FM, GET_REV, POWER_DOWN, POWER_UP 0x1F
(library id), POWER_UP 0x31 0x05, SSB patch (7735 bytes at PSV 0x19C0, 1105
lines, commands 0x15/0x16), POWER_UP 0x11, SSB_MODE 0x9012, volume 63,
SSB_TUNE_FREQ 224 kHz USB, SSB_MODE 0x9015, SSB_BFO 0, AM_AVC_MAX_GAIN 30260,
SSB_TUNE_FREQ with antenna cap bytes 0x82 0xB8.

## Demodulator (DMA ISR 0x02D4 → 0x4BC0)

1. 200-tap FIR band-pass around 1 kHz (0x501C, coefficients at PSV 0x3A04).
2. Samples taken in pairs (x[2m], x[2m+1]) and multiplied by the conjugate of
   the pair 20 ms (one bit) earlier; fast atan2 approximation (0x50CE).
   This is a differential detector, not a Costas loop.
3. 100-tap FIR over the last 20 ms of those phases (0x507C, PSV 0x3B94).
4. DC removal with an ~8 s leaky integrator (0x838/0x83A).
5. Bit slicer: > +3000 (≈ +16.5°) sets bit 1, < −3000 sets bit 0.
6. State machine: the first edge starts a 10-block bit clock (sampling in
   the middle of the assumed bit, never re-timed). The shift register is
   matched against `0x5560` (last sync byte + marker). Then 72 bits are
   collected.

## Decoding and time handling (0x4CC8 … 0x4E9C, 0x4898, main loop)

* Symbols unpacked, then syndromes (0x4412), Berlekamp-Massey (0x4486),
  Chien (0x45EE), Forney (0x4678, 0x46D6) over GF(16), x⁴+x+1.
  Bounded-distance RS(15,9).
* Descrambling, then the time N is assembled.
* The frame is "valid" if RS succeeded and the first three bits are `101`.
  **The CRC byte is never used.**
* 0x4898 copies the time into 0x814 and sets the 1 s tick counter to 50
  blocks (100 ms after the end of the frame). On the next tick `main`
  **overwrites the clock** with 3N + 946684801 + 1 (Unix time) without
  comparing it to the running clock. On an invalid frame Timer2 (1PPS) is
  stopped.
* `$GPRMC` status is `A` when the last "valid" frame is less than 86 299 s
  old, else `V`.

## Consequences

* RS(15,9) with ≥ 4 symbol errors decodes to a wrong codeword 5–10 % of
  the time (measured with random errors). The `101` check does not help:
  those bits are outside the RS code and are usually received correctly. So
  wrong times get through, and each one is copied into the clock.
* The original needs about 10 dB more signal than the new decoder for the
  same frame rate. On weak signals a large share of what it accepts is
  wrong (`host/tools/compare.py`).
* The output time is about 25 ms late, with 6–7 ms jitter, relative to the
  frame start (model estimate).
