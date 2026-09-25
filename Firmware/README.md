# e-CzasPL 225 kHz receiver – new firmware (2.0.3)

A from-scratch firmware for the simplified receiver (dsPIC33FJ128GP804 + SI4735)
in this repository. It is a drop-in replacement for `uproszczony_odbiornik.hex`:
same board, same connectors, same `$GPRMC` format.

* `eczas_receiver_2.0.3.hex` – ready to program (MPLAB IPE, device dsPIC33FJ128GP804)
* `eczas_receiver_2.0.3_64MC804.hex` – the same for boards fitted with a
  dsPIC33FJ64MC804 (same pinout; check the marking on the chip, the programmer
  reports an invalid device ID if the wrong device is selected)
* `uproszczony_odbiornik.hex` – the original firmware, unchanged
* `ORIGINAL_FIRMWARE_NOTES.md` – what the original does and why it shows wrong times
* `SERIAL_OUTPUT.md` – how to connect to SV1/SV2 and what every line means,
  for this firmware and for the original

## Why the original shows wrong times

Reverse engineering the original HEX found (details in `ORIGINAL_FIRMWARE_NOTES.md`):

1. **The CRC-8 in each frame is never checked.** A frame is accepted when the
   Reed-Solomon RS(15,9) decoder "succeeds" and the three `101` bits are right.
   RS(15,9) corrects at most 3 symbols; with 4 or more errors it silently
   "corrects" to a *different* valid codeword in about 5–10 % of cases.
2. **Every accepted frame overwrites the clock**, with no comparison against the
   running time and no confirmation by a second frame.
3. The bit detector is a hard ±16° threshold on a one-bit differential phase,
   sampled from the first edge it sees, with no re-timing. It misses most weak
   frames, so the bad frames are a large part of what it accepts.

Replaying the original algorithm and the new one on a real 20-minute off-air
recording with added noise (30 runs each):

| SNR  | original: frames/20 min | original: **wrong time accepted** | new: frames/20 min | new: **wrong time** |
|-----:|------------------------:|---------------------------------:|-------------------:|-------------------:|
| clean|  13  |  0 | 61 | 0 |
| 10 dB|  19.4 |  6 | 58.6 | **0** |
|  5 dB|  11.2 | 12 | 51.4 | **0** |
|  0 dB|   1.7 | 10 | 25.9 | **0** |
| −3 dB|   0.1 |  6 |  4.4 | **0** |

At 0 dB, one in five frames the original accepts carries a wrong time.

## What the new firmware does

* **Demodulator** (`src/core/dsp.c`, runs in the DMA interrupt, ~9 % CPU):
  NCO mixer + 2 ms integrate-and-dump → carrier phase → decision-directed
  PLL with frequency acquisition (the SI4735's 1 kHz tone is several Hz off
  and drifts) → normalised correlation with the 27 known preamble bits.
* **Decoder** (`src/core/frame.c`): max-log MAP bit detection that uses the
  real ~16 ms phase slew, RS(15,9) decoding, **CRC-8 check**, SK1 recovery
  through the CRC, Chase retries on the 7 least reliable bits, then
  errors-and-erasures RS decoding with the 2 or 4 least reliable symbols
  erased, and a fine timing estimate from all 96 bits. Every retry only
  proposes a frame; the CRC and the timekeeper decide.
* **Timekeeper** (`src/core/timekeeper.c`): a new time is accepted only if it
  matches the running clock (±100 ms). Before the first sync **two frames must
  agree with each other** before anything is set; to step a running clock that
  disagrees, three must.
  The crystal rate is measured over hours, so holdover stays accurate. Time
  stays "valid" for 24 h without frames (LED3, `$GPRMC` status `A`).
* **1PPS** from a hardware output compare (200 ns resolution) on SV1 pin 1 and
  LED4. The `$` of `$GPRMC` leaves the UART at the PPS edge.
* SI4735 set-up identical to the original (same SSB patch, filter, AVC, tuning).
* Robustness: watchdog, I²C timeouts (the original could hang forever),
  automatic radio re-initialisation, ADC clock within datasheet limits
  (the original ran the 12-bit ADC at a 75 ns TAD, spec minimum 117.6 ns),
  UART at 115200 −0.2 % (the original was +3.3 % off).

## Connectors and LEDs

| | |
|---|---|
| SV1 pin 3 | diagnostics, 115200 8N1 (3.3 V) |
| SV1 pin 1 | 1PPS output, 100 ms high pulse, rising edge = start of second |
| SV2 pin 1 | NMEA out, 115200 8N1 |
| LED1 | lit 0.5 s from the PPS after each verified frame |
| LED2 | a frame is being received |
| LED3 | time valid (synchronised, last good frame < 24 h ago) |
| LED4 | 1PPS |

NMEA on SV2 every second:

```
$GPRMC,192204,A,5214.5098,N,02100.0504,E,0.00,000.0,130826,,E,A*03
$PGGUM,A5,8B,...,3*hh        after each good frame: frame bytes 3..11, age of previous fix (s)
$PECZ,[  603.2] FRAME ...*hh diagnostics copy (DEBUG_TO_NMEA, see below)
```

Before the first synchronisation `$GPRMC,,V,,,,,,,,,,N*53` is sent.

## Diagnostics (SV1) and adjusting R27

R27 (200 k trimmer) is the feedback resistor of the MCP607 amplifier between
the SI4735 audio output and the ADC (gain = R27 / 18 k). For the first three
minutes after power-up a `SIGNAL` line is printed every second:

```
[    41.0] SIGNAL  level  54% [##########----------] OK, volume 48/63, centre 1% | carrier 1004.37 Hz (+4.37) locked, '0' bits at -33 deg, noise 6 deg | radio RSSI 38 dBuV SNR 21 dB
```

Since 2.0.2 the firmware sets the level itself: once a second it turns the
SI4735 audio volume (which feeds the MCP607) down while the ADC clips or the
level is above 85 %, and up while it stays below 40 %. R27 only needs turning
when the line ends with `- decrease gain R27` (still clipping at the lowest
volume) or `- increase gain R27` (still low at full volume). Other lines:

```
[   603.2] FRAME   #57 corr 0.86 snr 12 dB, fixed 1 symbol(s): 2026-08-13 19:21:48 UTC (21:21:48 local, UTC+2)
[   603.2] CLOCK   frame agrees with clock, error +0.31 ms, xtal -2100 ppb
[   606.1] FRAME   #58 corr 0.47 - not decodable (too many bit errors, snr -3 dB)
[   610.0] STATUS  2026-08-13 19:22:03 UTC (21:22:03 local, UTC+2) VALID, last good frame 7 s ago | frames heard 58, undecodable 12, accepted 44, rejected 0, steps 0
```

A `CLOCK REJECTED` line means a frame passed RS and CRC but disagreed with the
clock. On weak signals these are miscorrected frames, exactly what the
original firmware copied into its clock.

`DEBUG_TO_NMEA` (`src/core/eczas_cfg.h`) copies these lines to SV2 as
`$PECZ,...` sentences, sent 40–700 ms after each PPS so they never delay
`$GPRMC`. It is off in the ready-made hex files since 2.0.3: many lines are
longer than the 82 characters NMEA 0183 allows and overflow the line buffer
of simple NMEA readers. Build with `make NMEA_DEBUG=1` to turn it on (e.g.
for a test bench that only has SV2 connected).

## Timing accuracy and calibration

The frame start is taken as the encoded second, as the specification states.
PA3FWM measured the transmitter (September 2026) at 10–25 ms late, including
about 3 ms of propagation. Earlier it was 150–200 ms late.
To compensate, set `TX_DELAY_US` / `RX_DELAY_US` in `src/core/eczas_cfg.h`
after comparing SV1 pin 1 with a GPS PPS. The receiver's own measurement
jitter is well below 1 ms on a decent signal.

## Building

Requires MPLAB XC16 (tested with v2.10, free mode) and GNU make:

```
make                 # -> build/eczas_receiver.hex
make host-test       # codec/date self-test with the host C compiler
```

`make MCU=33FJ64MC804` builds for a dsPIC33FJ64MC804, `make NMEA_DEBUG=1`
also sends the diagnostics on SV2 (see below).
`XC16_DIR` can point at the compiler if it is not in `~/.local/microchip/xc16`
or `/opt/microchip/xc16`. MPLAB X users can create a standalone project for
dsPIC33FJ128GP804 and add all files under `src/`.

## Testing without hardware

The demodulator, decoder and timekeeper (`src/core`) are plain C and compile
for the PC. `host/sim.c` runs them on a recording (USB demodulated, carrier at
~1 kHz), e.g. the recordings in <https://github.com/sp5wwp/e-Czas/tree/main/samples>:

```
python3 host/tools/wav2raw.py 224k_2102.wav rec.raw
make -C host sim && host/sim rec.raw          # add -n <snr dB>, -f <fade Hz>, -i <impulses/s>
cd host/tools && REC=../../rec.raw SIM=../sim python3 compare.py 20,10,5,0,-3 30
```

`make -C host test` runs the self-tests: codec, errors-and-erasures RS,
frames captured off-air, timekeeper rules, and a synthetic signal with the
conditions met on the first board (+14 Hz tone offset, clipping, 0 to -8 dB).
`host/tools/bench.py` compares simulator builds on the recordings at several
noise levels and seeds, with the board's crystal error, and counts wrong
times that reach the clock (it must stay 0):

```
python3 host/tools/bench.py prep host/sim /path/to/recordings   # 224k_*.raw
python3 host/tools/bench.py run new=host/sim old=/tmp/sim_old --snr 3,0,-3,-6 --seeds 8
```

`compare.py` runs the new decoder and a model of the original firmware's
demodulator (`orig_model.py`, using its actual filter coefficients) on the
same noisy signals. `host/bench_sim30.c` + `host/run_sim30.sh` run the
XC16-compiled core in Microchip's `sim30` simulator to check results and
cycle counts on the real instruction set.

## Status

Running on a receiver since 25 Sep 2026 (a board fitted with a
dsPIC33FJ64MC804): it synchronises within a minute or two, rejects the wrong
frames the original firmware accepted, and keeps `$GPRMC` aligned to the
second. Also verified: decoding of real recordings with added noise, frequency
offset and clipping (`host/sim`, `host/tools/bench.py`), decoding and CPU load
on the dsPIC instruction set (`sim30`). Not yet measured: the absolute delay
of the 1PPS against GPS (`RX_DELAY_US`).

## License

The new firmware (`src/`, `host/`, `eczas_receiver_2.0.3*.hex`) is released
under the MIT License in `../LICENSE`, like the rest of this repository. It is
derived in part from e-CzasPL's original firmware (`uproszczony_odbiornik.hex`,
© 2024 e-CzasPL, MIT): the SI4735 set-up sequence and, for the host-side
comparison model only, the original filter coefficients
(`host/tools/orig_fir_tables.json`) were recovered from it.

**Exception: the SI4735 SSB patch** (`src/hw/si4735_patch.c`, and the copy
of it inside the `eczas_receiver_2.0.3*.hex` files) is **not** covered by the MIT
License. It is firmware for the SI4735's internal DSP and is the property of
Silicon Labs (now Skyworks Solutions). Silicon Labs has not published it or
put it under a public license. The same patch is distributed with the
[PU2CLR SI4735 Arduino library](https://github.com/pu2clr/SI4735) ecosystem
and is embedded in e-CzasPL's original firmware; our copy was extracted
byte for byte from `uproszczony_odbiornik.hex`, unchanged. It is included only
so that the receiver works as before, for non-commercial use. If you are the
rights holder and object to its distribution, please open an issue and it will
be removed.
