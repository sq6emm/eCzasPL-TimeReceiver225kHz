"""Events 31-55 (28/29.09.2026): WAV clips and received bytes of the early frame (the simple phase
demodulator of gum_events_21_30.py) for
  - the KiwiSDR "Central Czechia" (Revnice, GPS time axis), all events except 43;
  - the IC-7610 in Wroclaw (MAIN USB 226.000 kHz, LAN IF converted to 8 kHz 'USB audio' with the
    carrier at 1 kHz, NTP file time), events 41-55 (from 28.09 20:00 UTC).
Clips of 60 s from S-30 s: KiwiSDR for every event it has, IC-7610 for event 43 (not in the KiwiSDR
recording's alerts).
docker: -v <ic7610 in>:/ic -v <kiwi raw>:/kiwi -v <proc>:/codec -v <out>:/o -v <tools>:/t"""
import sys, os, json, glob, datetime as dt
import numpy as np, scipy.io.wavfile as wf, scipy.signal as ss
sys.path.insert(0, "/t")
from gum_events_21_30 import analyse, kiwi_blocks, EPOCH, GPS0, FSK

# event, time in frame (UTC), offset KiwiSDR [ms] (None = not decoded there), offset IC-7610 [ms] (None = not recording)
EV = [(31, "2026-09-28 11:54:45", -2440, None), (32, "2026-09-28 12:13:03", -2460, None),
      (33, "2026-09-28 12:26:12", -2460, None), (34, "2026-09-28 12:48:03", -2500, None),
      (35, "2026-09-28 13:03:30", -2440, None), (36, "2026-09-28 14:50:18", -2460, None),
      (37, "2026-09-28 16:01:03", -2400, None), (38, "2026-09-28 16:32:18", -2460, None),
      (39, "2026-09-28 16:41:54", -2420, None), (40, "2026-09-28 18:58:00", -2420, None),
      (41, "2026-09-28 20:06:18", -2440, -2440), (42, "2026-09-28 20:36:42", -2420, -2420),
      (43, "2026-09-28 22:51:27", None, -2460), (44, "2026-09-29 01:58:03", -2400, -2402),
      (45, "2026-09-29 02:31:15", -2420, -2420), (46, "2026-09-29 03:37:21", -2460, -2460),
      (47, "2026-09-29 03:43:00", -2480, -2480), (48, "2026-09-29 04:05:03", -2440, -2440),
      (49, "2026-09-29 05:14:27", -2460, -2460), (50, "2026-09-29 06:55:03", -2460, -2460),
      (51, "2026-09-29 10:23:30", -2460, -2460), (52, "2026-09-29 11:11:03", -2460, -2460),
      (53, "2026-09-29 11:26:39", -2400, -2400), (54, "2026-09-29 11:43:00", -2420, -2420),
      (55, "2026-09-29 16:13:15", -2480, -2480)]
PRE_S, DUR = 30, 60
# IC-7610 files whose recording began later than the name says (recorder restart), from the frame
# positions in eczas-ic7610-proc's alerts.log: 20260929_102015.wav starts 0.49 s late (event 51)
IC_LATE = {"20260929_102015.wav": 0.49}
p = lambda s: dt.datetime.strptime(s, "%Y-%m-%d %H:%M:%S")


def ic_audio(u, dur):
    """8 kHz IC-7610 audio from u (file-name time; the frames in these files come ~+60 ms late)"""
    fs = 8000; out = np.zeros(dur * fs); got = 0
    for fn in sorted(glob.glob("/ic/2026*.wav")):
        t0 = dt.datetime.strptime(os.path.basename(fn)[:15], "%Y%m%d_%H%M%S") + \
            dt.timedelta(seconds=IC_LATE.get(os.path.basename(fn), 0))
        if t0 > u + dt.timedelta(seconds=dur) or t0 < u - dt.timedelta(minutes=15): continue
        r, x = wf.read(fn); assert r == fs
        j = int(round(((t0 - u).total_seconds() - 0.06) * fs))
        if j + len(x) <= 0 or j >= len(out): continue
        a, b = max(j, 0), min(j + len(x), len(out)); out[a:b] = x[a - j:b - j]; got += b - a
    return out, got / len(out)


def kiwi_audio(u, dur, blocks):
    z = np.zeros(dur * FSK, complex); got = 0
    for ts, x in blocks:
        j = int(round((ts - u).total_seconds() * FSK))
        if j + len(x) <= 0 or j >= len(z): continue
        a, b = max(j, 0), min(j + len(x), len(z)); z[a:b] = x[a - j:b - j]; got += b - a
    y = ss.filtfilt(ss.firwin(301, 900, fs=FSK), 1, z)
    y = ss.resample_poly(y, 2, 3)
    y = (y * np.exp(2j * np.pi * 1000 / 8000 * np.arange(len(y)))).real
    return y, got / len(z)


def to16(y):
    return (y * 12000 / np.percentile(np.abs(y), 99.9)).clip(-32767, 32767).astype(np.int16)


def run(EV, name):
    """EV: (event, time in frame, KiwiSDR offset ms or None, IC-7610 offset ms or None); clips and
    received bytes to /o, the analysis to /o/<name>"""
    kf = sorted(glob.glob("/kiwi/2026*_225000_czechia_iq.wav"))
    kt = [dt.datetime.strptime(os.path.basename(f)[:16], "%Y%m%dT%H%M%SZ") for f in kf]
    cache = {}
    out = []
    for k, F, ok, oi in EV:
        F = p(F); S = F - dt.timedelta(seconds=3); u = S - dt.timedelta(seconds=PRE_S)
        N = int((F - EPOCH).total_seconds() // 3)
        if ok is not None:
            use = [f for f, t, tn in zip(kf, kt, kt[1:] + [dt.datetime(2100, 1, 1)])
                   if t <= u + dt.timedelta(seconds=DUR) and tn > u]
            blocks = []
            for f in use:
                if f not in cache:
                    wk = int((dt.datetime.strptime(os.path.basename(f)[:8], "%Y%m%d") - GPS0).days // 7)
                    cache.clear(); cache[f] = kiwi_blocks([f], wk)
                blocks += cache[f]
            y, cov = kiwi_audio(u, DUR, blocks)
            y16 = to16(y); wf.write(f"/o/kiwi_czechia_early_frame_{u:%Y%m%d_%H%M%S}.wav", 8000, y16)
            d = analyse(y16.astype(float), 8000, PRE_S, 3.0 + ok / 1000, N, k)
            d.update(receiver="KiwiSDR Central Czechia", offset_ms=ok, coverage=round(cov, 3)); out.append(d); print(d, flush=True)
        if oi is not None:
            y, cov = ic_audio(u, DUR)
            if ok is None:                           # no KiwiSDR decode: the clip from the IC-7610
                wf.write(f"/o/ic7610_early_frame_{u:%Y%m%d_%H%M%S}.wav", 8000, to16(y))
            d = analyse(y, 8000, PRE_S, 3.0 + oi / 1000, N, k)
            d.update(receiver="IC-7610 Wroclaw", offset_ms=oi, coverage=round(cov, 3)); out.append(d); print(d, flush=True)
    json.dump(out, open(f"/o/{name}", "w"), indent=1)


if __name__ == "__main__":
    run(EV, "events_31_55.json")
