# Reading the serial output

Both connectors carry 3.3 V CMOS UART signals. Use a 3.3 V USB-UART adapter.
Do not connect RS-232 levels directly.

| Connector | Pin 1 | Pin 2 | Pin 3 |
|---|---|---|---|
| SV1 (diagnostics) | 1PPS output | GND | UART1 TX → adapter RX |
| SV2 (NMEA) | UART2 TX → adapter RX | GND | UART2 RX (unused) |

Settings: **115200 baud, 8N1, no flow control**. The original firmware
actually sends at about 119 000 baud (3.3 % fast). Most adapters still read it
at 115200.

```sh
picocom -b 115200 /dev/ttyUSB0             # or: screen /dev/ttyUSB0 115200
# log with PC timestamps (useful next to a GPSDO):
stdbuf -o0 cat /dev/ttyUSB0 | ts '%H:%M:%.S' | tee receiver.log
# new firmware, one cable on SV2: show only the diagnostics, as plain text
grep --line-buffered '^\$PECZ,' receiver.log | sed -E 's/^\$PECZ,//; s/\*[0-9A-F]{2}\r?$//'
```

(`ts` is in the `moreutils` package.)

---

## Part 1 – new firmware (2.0.0)

### SV1: diagnostics

One event per line:

```
[   603.2] FRAME   ...
 ^^^^^^^^^ ^^^^^^^
 uptime s  line type
```

The uptime is counted by the ADC sample clock since power-up, in seconds with
0.1 s resolution. It is not the time of day.

#### BOOT / RADIO – start-up

```
[     0.0] BOOT    e-CzasPL 225 kHz receiver, firmware 2.0.0 (Sep 25 2026)
[     0.0] BOOT    SV1: diagnostics, SV2: NMEA ... LED4 1PPS
SI4735: part 23 fw 60 chip D lib 7
SI4735: loading SSB patch... done
SI4735: rev 23 36 30 00 00 32 30
[     0.0] RADIO   tuned to 224 kHz USB, waiting for the 225 kHz carrier (1 kHz tone)
```

The `SI4735:` lines have no timestamp. They come from the radio driver:

* `part 23 fw 60 chip D lib 7` is the library-ID query before the patch.
  `part` is the last two digits of the part number in hex (0x23 = 35 → Si4735),
  `fw` is the firmware version as two ASCII characters, `chip` is the chip
  revision letter and `lib` is the library ID. These values are only examples.
* `rev ...` shows bytes 1–7 of GET_REV: part number, firmware major/minor (ASCII),
  patch ID (2 bytes), component firmware major/minor (ASCII).
* `SI4735: no response` means the radio did not answer on I²C. Check
  the 3.3 V supply, the 32.768 kHz crystal Y1 and the SDA/SCL solder joints.
* `SI4735: loading SSB patch... FAILED` means the I²C transfer broke off
  during the patch download.
* `RADIO   SI4735 init failed (N), retrying`: N = −1 no response,
  −2 power-up failed, −3 patch failed, −4 setting the SSB mode failed,
  −5 tuning failed. Three attempts are made.
* `RADIO   SI4735 not responding - reinitialising` means three periodic status
  reads in a row failed, so the radio is reset and set up again.

#### SIGNAL – audio level, carrier and radio (use this to set R27)

Printed every second for the first 3 minutes after power-up, then every 10 s.

```
[    41.0] SIGNAL  level  54% [##########----------] OK, centre 1% | carrier 1004.37 Hz (+4.37) locked, '0' bits at -33 deg, noise 6 deg | radio RSSI 38 dBuV SNR 21 dB
```

| Field | Meaning |
|---|---|
| `level 54%` | peak-to-peak ADC signal since the previous SIGNAL line, as % of the full ADC range. The bar shows the same value, one `#` per 5 %. |
| advice | `CLIPPING (n samples) - decrease gain R27`: n samples were within 1 % of full scale. `NO/VERY LOW AUDIO` < 10 %. `low` 10–30 %. `OK` 30–85 %. `high` > 85 %. |
| `centre 1%` | midpoint between the highest and lowest sample, in % of half range. This is the DC bias of the op-amp stage (R17/R19 divider). It should be within about ±5 %. A large value means one side clips first. |
| `carrier 1004.37 Hz (+4.37)` | frequency of the 225 kHz carrier as seen in the audio, and its offset from the nominal 1000 Hz. The offset is the SI4735 crystal error. A few Hz is normal. It should drift only slowly with temperature. |
| PLL state | `searching carrier` – measuring the frequency (1 s steps). `locking (fast)`, `locking` – the carrier loop is settling, about 3 s each. `locked` – normal operation. |
| `'0' bits at -33 deg` | learned phase of a `0` bit relative to the idle carrier (shown only when locked). The transmitter uses −45°. After the SI4735 filter −30…−40° is normal. Values near −18 or −60 (the limits) mean something is wrong. |
| `noise 6 deg` | average absolute phase error of the carrier loop. Below 10° is a clean signal. 15–25° is weak. Above 25° for 10 s makes the loop start searching again. |
| `radio RSSI 38 dBuV SNR 21 dB` | the SI4735's own signal strength and SNR at 224 kHz (AM_RSQ_STATUS). `radio NOT RESPONDING` means the I²C read failed. |

**Adjusting R27:** R27 is the feedback resistor of the MCP607 inverting
amplifier (gain = R27 / 18 kΩ). Turn it until `level` sits in the `OK` range,
ideally 40–70 %, and `CLIPPING` never appears. The broadcast audio makes the
level move a bit. The SI4735 AGC keeps it roughly constant, so one adjustment
is enough.

#### FRAME – every time frame heard

Only frames with the time marker (0x60) are reported. Frames of the other
services on the carrier (Enea lighting control etc.) are ignored silently.

```
[   603.2] FRAME   #57 corr 0.86 snr 12 dB, fixed 1 symbol(s): 2026-08-13 19:21:48 UTC (21:21:48 local, UTC+2)
[   606.1] FRAME   #58 corr 0.47 - not decodable (too many bit errors, snr -3 dB)
```

| Field | Meaning |
|---|---|
| `#57` | count of time frames heard since power-up |
| `corr 0.86` | how well the 27 known preamble bits (0x5555, 0x60, 101) match, 0…1. Strong frames score 0.8–1.0. The detector triggers at 0.36. |
| `snr 12 dB` | signal-to-noise estimated from the preamble: separation of the `1`/`0` phase levels against their scatter. Frames decode reliably above ~3 dB. |
| `fixed 1 symbol(s)` | Reed-Solomon corrected this many 4-bit symbols (0–3) |
| `+ soft retry` | the frame only decoded after flipping some of the least reliable bits (Chase decoding) |
| `+ SK1 via CRC` | the transmitter-state bit SK1, which has no RS protection, was repaired using the CRC |
| date / time | the time encoded in the frame, which is the moment this frame started. `local` adds the offset transmitted in the frame (UTC+1 winter, UTC+2 summer). |
| `leap second insertion/removal announced`, `DST change announced`, `transmitter maintenance planned (1/2/3)` | the announcement bits of the frame (maintenance: 1 = one day, 2 = a week, 3 = more than a week) |
| `not decodable (no modulation)` | the preamble matched but the phase levels were not separable |
| `not decodable (too many bit errors)` | RS + CRC failed, also after the soft retries |

Every decoded frame is followed by a CLOCK line and a RAW line.

#### CLOCK – what the timekeeper did with the frame

| Line | Meaning |
|---|---|
| `frame agrees with clock, error +0.31 ms, xtal -2100 ppb` | normal case. `error` is the frame start measured by the demodulator minus the start predicted by the running clock. A quarter of it is corrected. Expect ±1 ms on good signals and a few ms on weak ones. `xtal` is the measured error of the board's 10 MHz crystal in parts per billion (−2100 ppb = 2.1 ppm slow). It is 0 for the first 10 minutes and then comes from a baseline of 10 min to 4 h. |
| `not synchronised yet - waiting for a second frame to confirm` | no time yet. This frame is kept as a candidate. |
| `SYNCHRONISED - two frames agree, time set to ...` | two frames agreed with each other (their time difference matched the elapsed time within 20 ms + 200 ppm). The clock is now set. |
| `REJECTED - frame differs from clock by N ms (kept as candidate)` | the frame passed RS and CRC but does not match the clock (more than 100 ms off). On weak signals these are miscorrected frames, typically with absurd dates. This is exactly what the original firmware copied into its clock. |
| `STEPPED - clock was off by N ms, confirmed by two frames` | two frames that agree with each other both disagreed with the clock, so the clock was reset to them. This should practically never happen. If it does, please send the log. |

After an accepted, synchronising or stepping frame, LED1 lights for 0.5 s
from the next PPS and a `$PGGUM` sentence is sent on SV2.

#### RAW – the frame bits as received

```
[   603.2] RAW     55 55 60 A5 8B 3C 91 2E 7D 44 E0 5B
```

All 12 frame bytes (96 bits, MSB first) as hard decisions, **before** error
correction and **still scrambled**. Byte by byte:

| Byte | Content |
|---|---|
| 0–1 | sync `55 55` |
| 2 | marker `60` (time frame) |
| 3 | `101` + S29…S25 |
| 3–7 | 30-bit count of 3-second periods since 2000-01-01, then TZ0 TZ1 LS LSS TZC SK0 SK1. Bits 27–63 are XORed with 0x0A47554D2B. |
| 8–10 | Reed-Solomon parity |
| 11 | CRC-8 (poly 0x07) over bytes 3–7 as transmitted (scrambled) |

The meaning of every field is documented in `src/core/frame.h`.

#### STATUS – every 10 s

```
[   610.0] STATUS  2026-08-13 19:22:03 UTC (21:22:03 local, UTC+2) VALID, last good frame 7 s ago | frames heard 58, undecodable 12, accepted 44, rejected 0, steps 0
```

| Field | Meaning |
|---|---|
| time | current time of the receiver's clock, or `no time yet` |
| `VALID` / `HOLDOVER (>24 h, not valid)` | VALID while the last good frame is less than 24 h old (LED3 on, `$GPRMC` status `A`). After that the clock keeps running on the measured crystal rate but is marked not valid. |
| `last good frame` | seconds since the last accepted frame |
| counters | since power-up. `frames heard` = time frames detected. `undecodable` = RS/CRC failed. `accepted` = agreed with the clock. `rejected` = decoded but disagreed. `steps` = clock corrections by two agreeing frames. The first frames (candidate, synchronising) are in neither accepted nor rejected. |
| `PPS missed N` | (shown only if non-zero) a PPS edge could not be scheduled in time |
| `log dropped N` | (shown only if non-zero) characters dropped because the SV1 buffer was full |

### SV2: NMEA

```
$GPRMC,192204,A,5214.5098,N,02100.0504,E,0.00,000.0,130826,,E,A*03
$PGGUM,A5,8B,3C,91,2E,7D,44,E0,5B,3*hh
$PECZ,[   603.2] FRAME   #57 corr 0.86 ...*hh
```

* **`$GPRMC`** is sent every second. The `$` leaves the UART at the 1PPS
  rising edge, and the time (`hhmmss`, UTC) and date (`ddmmyy`) are those of
  that edge. Status `A` means valid (synchronised, last good frame < 24 h).
  `V` means holdover. The mode letter at the end is `A` or `N`. The position
  is fixed: the GUM laboratory in Warsaw. Before the first synchronisation
  the sentence is `$GPRMC,,V,,,,,,,,,,N*53`.
* **`$PGGUM`** is sent after each accepted frame, as in the original firmware.
  The fields are frame bytes 3–11 as received (hex, scrambled, before
  correction) and the time in seconds since the previous good frame.
* **`$PECZ`** is a copy of every SV1 line (`DEBUG_TO_NMEA` in
  `src/core/eczas_cfg.h`, on by default). `*` and `$` inside the text are
  replaced by `#`. These sentences are only sent 40–700 ms after a PPS, so
  they never delay `$GPRMC`. NMEA/GPS software (gpsd, chrony's refclock,
  GPS clocks) ignores unknown sentences. Set `DEBUG_TO_NMEA` to 0 if yours
  does not.

### SV1 pin 1 and LEDs

* **SV1 pin 1:** 1PPS, a 100 ms high pulse every second. The rising edge is
  the start of the second. It is generated in hardware from the disciplined
  clock with 200 ns resolution. It runs from the first synchronisation on,
  also in holdover (check LED3 / status `A`).
* **LED1:** 0.5 s flash at the PPS after a verified frame.
* **LED2:** a frame is being received (from preamble detection until the
  frame has been captured).
* **LED3:** time valid.
* **LED4:** 1PPS (same signal as SV1 pin 1).
* **LED5:** power.

---

## Part 2 – original firmware (`uproszczony_odbiornik.hex`)

Reconstructed from the disassembly. Addresses refer to
`ORIGINAL_FIRMWARE_NOTES.md`. All text below goes to **SV1 (UART1)**. SV2
(UART2) only carries `$GPRMC` and `$PGGUM`.

### Start-up

```
 GUMIAK ver. v0.1-73-g669641d-dirty
i2ctest - START
i2cstart ... OK
i2c wrie chip addr ... OK
i2cstop ... OK
radioPowerUp XOSCEN = 10
-------------------------------------------------------
0000 -> 80 23 36 30 00 00 32 30 44 xx xx xx xx xx xx xx  .#60..20D.......
-------------------------------------------------------
radioPowerUp XOSCEN = 10
Patching DSP...done!
radioPowerUp XOSCEN = 10
si4735_setSSB
... OK
```

(The hex values above are illustrative.)

* `GUMIAK ver.` is the firmware name and git version.
* `i2ctest - START` … `i2cstop ... OK` is an I²C bus test at boot. It sends a
  start, the SI4735 address (0x22) and a stop. If the radio does not
  acknowledge, `NACK ` is printed before the `... OK`. The test does not stop
  on errors.
* `radioPowerUp XOSCEN = 10` is printed after every POWER_UP command. It is
  the crystal-enable bit of the command (0x10 = external 32.768 kHz crystal
  used, 00 = not).
* The dash-framed hex dump is the SI4735 GET_REV reply: status, part number
  (0x23 → Si4735), firmware major/minor, patch ID, component firmware, chip
  revision, with ASCII on the right. It always prints 16 bytes, so the last
  7 are unrelated RAM.
* `Patching DSP...done!` means the 7.7 KB SSB patch was downloaded.
* `si4735_setSSB` / `... OK` means SSB mode was configured and tuned to 224 kHz USB.
* `Timed out` means the SI4735 did not become ready (CTS) after about 2046 polls.
* ` i2c read timeout` means an I²C read did not complete.

### Every second

```
*
RAW: a5 8b 3c 91 2e 7d 44 e0 5b          (only in seconds when a frame was received)
UTCPL: 2026-08-13T19:22:05
LAST FIX = 2 sec                          (or: no FIX yet)
STS: MAX: 18234 MIN -17820
STT: CYCLE: 10000, USED: 1650, USED_MAX 2140
STB: TIM3_BUDGET 212 890
STB: TIM2_BUDGET 7112
IIR: 4210
```

| Line | Meaning |
|---|---|
| `*` | heartbeat, printed at every 1-second tick |
| `RAW:` | a frame was received since the last tick. The 9 bytes are frame bytes 3–11 (lowercase hex), hard decisions before correction, still scrambled. They are printed for every frame the bit detector completed, **including ones that failed RS**. Frames that failed RS leave the clock unchanged. |
| `UTCPL:` | the receiver's clock, UTC. It starts at 1970-01-01T00:00:00 at power-up and counts seconds until the first frame is accepted. After a frame it is set to (frame time + 2 s), because the tick falls ~2.02 s after the frame start. Any frame that passes RS and the `101` check overwrites it, which is how wrong times appear. |
| `LAST FIX = N sec` | seconds since the last frame that passed RS. It is printed while N ≤ 86 299 (24 h), and then LED2 (RA10) is on. |
| `no FIX yet` | no frame accepted yet, or the last one is older than 24 h. LED2 is off. |
| `STS: MAX: a MIN b` | peak-hold of the ADC input (signed, ±32 767 = full scale; one sample per 2 ms is looked at). Both decay by 10 counts every 2 ms. This is the original's level indicator: values near ±32 767 mean clipping, so reduce R27. Values of a few thousand are very low. The two numbers should be about symmetric. |
| `STT: CYCLE: c, USED: u, USED_MAX m` | DMA interrupt timing in Timer1 counts of 200 ns. `CYCLE` = time between the last two interrupts (should be 10 000 = 2 ms). `USED` = duration of the last interrupt (the whole demodulator runs in it). `USED_MAX` = longest so far. Values approaching 10 000 would mean samples are lost. |
| `STB: TIM3_BUDGET a b` | minimum and maximum of Timer3 at the entry of the DMA interrupt, in CPU cycles (25 ns, range 0–3999 = one ADC sample period). This is the interrupt latency after the last conversion. Both are hold values since power-up. |
| `STB: TIM2_BUDGET t` | maximum Timer2 value seen when a frame finished decoding. Timer2 counts 200 µs steps and is restarted when the `0` of the frame's `101` marker is detected. The SV1 pin 1 pulse is due at count 7350 (1.47 s), so this value must stay below 7350 or the pulse is missed. |
| `IIR: d` | output of the slow (~8 s) averaging filter on the demodulator's one-bit phase difference, in units where 32 768 = 180°. It is dominated by the SI4735 tuning offset. A steady value means a steady carrier. Very roughly, offset in Hz ≈ d × 25 / 32 768. The pair-based detector compresses large angles, so this is an indicator, not a measurement. |

### SV2 (UART2)

* `$GPRMC,hhmmss,S,5214.5098,N,02100.0504,E,0.00,000.0,ddmmyy,,E,A*hh`
  is sent every second right after the tick. S is `A` if the fix check of the
  previous second found a frame less than 24 h old, else `V`. The time is the
  receiver clock described under `UTCPL`. Before the first frame it counts up
  from 00:00:00 on 1 January 1970, printed as date `010100` (year shown as 00).
* `$PGGUM,b3,b4,...,b11,age*hh` is sent in each second a frame was received,
  whether or not it passed RS. The fields are the same 9 bytes as `RAW:`
  (uppercase), plus `age` = the `LAST FIX` counter before this frame.

### SV1 pin 1 and LEDs (original)

The original does **not** produce a continuous 1PPS:

* **SV1 pin 1 and LED1 (RB11)**: output compare OC2 on Timer2. Timer2 is
  restarted at the `0` of the `101` marker of a frame (≈ 0.5 s into the frame).
  The pulse goes high 1.47 s later and stays high 0.5 s, so its rising edge
  is near the second 3N+2, where N is the encoded time. The pulse only comes
  after frames that pass RS. On a failed frame Timer2 is stopped. So it is
  one pulse per good frame, not one per second.
* **LED3 (RA8)**: 28 ms blink on every 1-second tick.
* **LED2 (RA10)**: on while `LAST FIX` < 24 h.
* **LED4 (RC0)**: on while a frame is being received (after the sync
  pattern, for 72 bits).

This differs from the LED list in the original documentation. The new
firmware follows the documentation (see Part 1).
