# Reference recordings

`ic705_ferrite_20260926_214746.wav` – 144 s received with an ICOM IC-705 and a
ferrite rod antenna, USB at 224.000 kHz (carrier at 1000.1 Hz, 52 dB above the
noise floor), mono 16-bit 8 kHz, start 2026-09-26 21:47:46 UTC (first time-frame
slot at 1.94 s = 21:47:48 UTC). Of its 47 slots, 12 carry time frames, 16 other
messages (IDs 0x47, 0x43, 0x03) and 19 are idle: not every 3-s slot carries a
time frame. The firmware decodes all 12.

Convert for the simulator / benchmark with
`python3 tools/wav2raw.py data/ic705_ferrite_20260926_214746.wav ic705_0926.raw`.

The following clips come from a night of recording with the same IC-705 and
ferrite rod in Wroclaw (26/27 Sep 2026, preamp off, AGC slow, USB 224.000 kHz,
8 kHz mono). Start times are exact to a few ms (from the decoded frames).

* `ic705_early_frame_20260927_0002.wav` – 120 s from 00:02:00.00 UTC. The
  frame for 00:03:00 (N=281260860) starts at 57.52 s = 00:02:57.52, 2.476 s
  early, 0.52 s into the previous slot; its own slot is empty. The same
  transmitter behaviour was seen six times by the bench receiver in Legnica, so
  it is not a receiver artefact. The firmware decodes it (snr 9 dB, 1 RS
  correction) and the timekeeper rejects it; 8 frames decoded in total, no
  wrong time. Used by `tools/bench.py` ("early").
* `ic705_selective_fade_20260926_2224.wav` – 180 s from 22:24:30 UTC: night
  sky-wave fading in which the carrier drops ~8 dB more than the programme
  sidebands. 8 time frames sent; the firmware gets 1 decoded + 3 confirmed.
  This is the condition that made the frequency search run to +40 Hz before
  firmware 2.0.5.
* `ic705_busy_20260927_0428.wav` – 170 s from 04:28:09 UTC, the busiest period
  of other messages of the night (31 other, 24 idle, 3 time frames: only the
  one at hh:mm:03 each minute). 1 decoded + 1 confirmed.

Over the whole night (22:11-06:00 UTC, 330 fully classified minutes) 24 % of
the slots carried a time frame, 4.8 per minute on average (1..10), about 290
per hour. Only the slot at hh:mm:03 carried one every minute (100 %); the slot
at hh:mm:33 always carried another message; hh:mm:00 had one in only 22 % of
the minutes.

The two e-CzasPL recordings `224k_1836` and `224k_2102` used by
`tools/bench.py` come from SP5WWP's project (github.com/sp5wwp/e-Czas) and are
not stored here.
