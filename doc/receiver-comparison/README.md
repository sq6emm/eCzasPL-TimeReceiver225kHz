# e-CzasPL reception: three receivers compared

What three independent receivers say about the e-CzasPL signal and about the eCzas
firmware, 27 September 2026. Every 3-s slot is compared across receivers with
[`compare3.py`](compare3.py).

## Receivers

| receiver | site | antenna | decoder |
|---|---|---|---|
| eCzas board, firmware 2.0.5 | Legnica (strong local noise since 26 Sep ~16 UTC) | ferrite rod | on the board |
| ICOM IC-705, USB 224.000 kHz, audio recorded (reference) | Wrocław | 12×12 cm frame loop | firmware core in the host simulator + slot classifier |
| NUCLEO-H723ZG, direct sampling at 1 Msps | Wrocław | ferrite rod with 1.5 nF | the same 2.0.5 core, unchanged |

The Nucleo receiver is developed in its own repository,
[sq6emm/eCzas-DCF-gps-stm32](https://github.com/sq6emm/eCzas-DCF-gps-stm32)
(firmware, measurements, DCF77 and GPS plans).

## Reception

Share of the time frames that the IC-705 saw (a time frame decoded or confirmed in
the same slot):

| period (UTC) | time frames | IC-705 | Nucleo | Legnica |
|---|---|---|---|---|
| 15:10–16:10 | 301 | 99.7 % | 84.7 % | 16.6 % |
| 16:45–17:27 (Nucleo with image filter) | 110 | 100 % | **92.7 %** | 15.5 % |

- In the full hour, the Nucleo decoded 211 time frames that Legnica missed. Legnica
  decoded 6 that the Nucleo missed.
- **Wrong times: none** on any receiver. Nothing was decoded outside a real
  time-frame slot.
- The timekeeper rejected every miscorrected low-SNR frame, i.e. every frame that
  decoded to a nonsense date after "fixed 3 + soft retry": about 6 per hour on the
  Nucleo, and 50 in 19 h in Legnica.

## What it says about the signal

- **Time frames are in about a quarter of the slots:** about 300 per hour. The other
  slots carry other messages or are idle.
- **Early frames are real transmitter behaviour:** a valid frame for slot S+3 is sent
  in slot S, 2.4–2.5 s early. On 27 Sep at 16:19:12 UTC the frame for 16:19:15 was
  caught independently by the IC-705 (−2472 ms) and the Nucleo (−2479 ms). These
  are two different receivers with different antennas and decoders. See
  [GUM-issue-1](../GUM-issue-1/README.md): report v1.2 lists 20 events (to 27 Sep
  20:17 UTC); 3 of them were seen by both the IC-705 and the Nucleo.

## What it says about the eCzas receiver

- **Firmware 2.0.5 is not the limit.** The same core reaches 85–93 % in Wrocław and
  17 % in Legnica. The difference is the signal-to-noise ratio at the site: loop
  noise was 15–26° in Legnica (RSSI up from 59 to 64 dBµV since 26 Sep) against
  7–13° on the Nucleo. The Legnica site needs less local noise or input filtering
  (see [antenna-filter](../antenna-filter)), not new code.
- **2.0.5 is robust:** in 19 h in Legnica there were no reboots, no time steps and
  no wrong times. Clock error against NTP averaged 0.6–0.9 ms per hour.
- **Lessons for any receiver front end** (found on the Nucleo, where every part of the
  chain is visible):
  - **Clock phase noise matters more than its accuracy.** A local clock corrected
    only in frequency still left ~25° rms of phase noise in the 5–80 Hz data band,
    as large as the PSK itself. Frames were heard but never decoded. Locking the
    mixer phase to a crystal fixed it.
  - **Image noise costs about 8 points of reception:** 84.5 % → 92.7 % when the
    noise of the other sideband no longer folds onto the signal. The SI4735 in USB
    mode already rejects the image in its IF, so the board does not have this loss.

## Reproducing

```
python3 compare3.py FROM TO nucleo_rx.log legnica_pecz.log ic705_slots.csv
```

- `legnica_pecz.log`: the `$PECZ ... FRAME` lines of the board's NMEA output.
- `ic705_slots.csv`: the slot table of the IC-705 processor (see
  [GUM-issue-1/ic705-wroclaw](../GUM-issue-1)).
- `nucleo_rx.log`: the Nucleo logger output.

The recordings themselves are not in the repository.
