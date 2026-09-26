# Reference recordings

`ic705_ferrite_20260926_214746.wav` – 144 s received with an ICOM IC-705 and a
ferrite rod antenna, USB at 224.000 kHz (carrier at 1000.1 Hz, 52 dB above the
noise floor), mono 16-bit 8 kHz, start 2026-09-26 21:47:46 UTC (first time-frame
slot at 1.94 s = 21:47:48 UTC). Of its 47 slots, 12 carry time frames, 16 other
messages (IDs 0x47, 0x43, 0x03) and 19 are idle: not every 3-s slot carries a
time frame. The firmware decodes all 12.

Convert for the simulator / benchmark with
`python3 tools/wav2raw.py data/ic705_ferrite_20260926_214746.wav ic705_0926.raw`.

The two e-CzasPL recordings `224k_1836` and `224k_2102` used by
`tools/bench.py` come from SP5WWP's project (github.com/sp5wwp/e-Czas) and are
not stored here.
