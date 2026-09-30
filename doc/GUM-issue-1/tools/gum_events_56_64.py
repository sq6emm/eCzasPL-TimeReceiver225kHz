"""Events 56-64 (29/30.09.2026): WAV clips and received bytes of the early frame, as gum_events_31_55.py
(KiwiSDR "Central Czechia", GPS time axis, and IC-7610 Wroclaw; all nine decoded by both).
docker: -v <ic7610 in>:/ic -v <kiwi raw>:/kiwi -v <proc>:/codec -v <out>:/o -v <tools>:/t"""
import sys
sys.path.insert(0, "/t")
import gum_events_31_55 as g

# event, time in frame (UTC), offset KiwiSDR [ms], offset IC-7610 [ms] (kiwi-ref / eczas-ic7610 alerts.log)
EV = [(56, "2026-09-29 17:40:24", -2440, -2440), (57, "2026-09-29 21:09:03", -2440, -2440),
      (58, "2026-09-29 22:49:39", -2420, -2420), (59, "2026-09-29 23:29:45", -2441, -2439),
      (60, "2026-09-30 01:01:42", -2440, -2440), (61, "2026-09-30 02:52:24", -2420, -2420),
      (62, "2026-09-30 06:24:03", -2440, -2440), (63, "2026-09-30 07:37:03", -2460, -2460),
      (64, "2026-09-30 07:41:48", -2440, -2440)]
# 20260929_232347.wav (recorder restart, no previous-file tail): frame at 354.638 s in alerts.log
# = 354.738 s in the file (proc skips 0.1 s) against 355.62 s by the name (+60 ms) -> began 0.88 s late
g.IC_LATE["20260929_232347.wav"] = 0.88

if __name__ == "__main__":
    g.run(EV, "events_56_64.json")
