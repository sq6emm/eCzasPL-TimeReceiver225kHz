#!/usr/bin/env python3
"""
The same check as check_start_sequence.py (is 0x680C transmitted before each e-CzasPL time frame?)
on the NUCLEO-H723ZG receiver: its own e-CzasPL mixer output (ecz_*.c64: 2 kHz complex, sample n = sum
over [n, n+1) x 0.5 ms of true time, n on the firmware's n2k scale; .idx.csv gives the first n) and its
log (rx.log: FRAME lines with the decoded time, TIMEMAP lines mapping n2k to the e-CzasPL second).
Each frame is placed where the FIRMWARE puts its second (ramp start, firmware 6bd5276 and later); no fit.

  check_start_sequence_nucleo.py LOGDIR [--since 'YYYY-MM-DDTHH:MM:SS'] [--inject] [-q]
  (needs eczas_codec in /codec for make_frame)
"""
import sys, re, glob, os, argparse, datetime as dt, numpy as np
sys.path.insert(0, "/codec"); from eczas_codec import make_frame

FS = 2000; BIT = 0.020; RAMP = 0.0186; SEQ = "0110100000001100"
EPOCH = dt.datetime(2000, 1, 1, tzinfo=dt.timezone.utc).timestamp()
ap = argparse.ArgumentParser(); ap.add_argument("logdir"); ap.add_argument("--since", default="2026-10-01T14:51:00")
ap.add_argument("--inject", action="store_true"); ap.add_argument("-q", action="store_true")
a = ap.parse_args()
tm, fr = [], []
for raw in open(f"{a.logdir}/rx.log", "rb"):
    l = re.sub(rb"[^\x20-\x7e]", b"", raw).decode(errors="ignore")
    if l[:19] < a.since: continue
    m = re.search(r"TIMEMAP audio sample (\d+) = eCzas ([\d.]+) \(unix\)", l)
    if m: tm.append((int(m.group(1)), float(m.group(2)))); continue
    m = re.search(r"FRAME +#\d+ corr \d+% snr \d+ dB.*: (\d{4}-\d\d-\d\d \d\d:\d\d:\d\d) UTC", l)
    if m: fr.append(dt.datetime.strptime(m.group(1), "%Y-%m-%d %H:%M:%S").replace(tzinfo=dt.timezone.utc).timestamp())
tm = np.array(tm); fr = sorted(set(fr)); res = []
dumps = sorted(f for f in glob.glob(f"{a.logdir}/ecz_*.c64") if os.path.basename(f)[4:19] >= a.since.replace("-", "").replace("T", "_").replace(":", "")[:15])
for f in dumps:
    z = np.fromfile(f, np.complex64).astype(complex)
    n0 = int(open(f.replace(".c64", ".idx.csv")).readlines()[1].split(",")[0])
    for T in fr:
        j = np.searchsorted(tm[:, 1], T) - 1
        if j < 0: continue
        S, U = tm[j]
        x0 = S + (T - U) * FS - n0                      # frame start (ramp start) in samples of this dump
        i0, i1 = int(x0 - 0.6 * FS), int(x0 + 2.0 * FS)
        if i0 < 0 or i1 > len(z): continue
        seg = z[i0:i1].copy(); tc = (np.arange(i0, i1) + 0.5) / FS    # complex, interval centres
        if a.inject:
            st = x0 / FS - 16 * BIT; lev = [0.0 if c == "1" else -np.radians(36) for c in SEQ]
            kx, ky = [st - 0.001], [0.0]
            for k in range(16): kx += [st + k * BIT, st + k * BIT + RAMP]; ky += [ky[-1], lev[k]]
            kx += [x0 / FS - 0.0001]; ky += [ky[-1]]; seg = seg * np.exp(1j * np.interp(tc, kx, ky, left=0.0, right=0.0))
        # no unwrapping / detrending: the mixer is locked to the OCXO, the carrier moves ~1 ppb (0.0005
        # cycle in 2.6 s); complex means per window, angle against the idle carrier before the frame
        idle = seg[: int(0.25 * FS)].mean()
        def lvl(k):                                      # samples fully inside 18.8..19.8 ms of bit k
            t_a, t_b = x0 / FS + k * BIT + 0.0188, x0 / FS + k * BIT + 0.0198
            s_a, s_b = int(np.ceil(t_a * FS)), int(np.floor(t_b * FS))
            return np.angle(seg[s_a - i0: max(s_b, s_a + 1) - i0].mean() * np.conj(idle))
        ref = np.array(make_frame(int(round((T - EPOCH) / 3)), 2)) > 0
        fb = np.array([lvl(k) for k in range(96)]); l1, l0 = fb[ref].mean(), fb[~ref].mean()
        step = np.degrees(l1 - l0)
        pre = np.array([(lvl(k) - l0) / (l1 - l0) for k in range(-16, 0)])
        stamp = dt.datetime.fromtimestamp(T, dt.timezone.utc).strftime("%Y-%m-%d %H:%M:%S")
        if not 15 <= abs(step) <= 45:
            res.append((stamp, "nofit", pre, step)); continue
        rd = "".join("1" if v > 0.5 else "0" for v in pre)
        kind = "0x680C" if rd == SEQ else ("none" if np.ptp(pre) < 0.6 else "other")
        res.append((stamp, kind, pre, step))
        if not a.q: print(f"{stamp}  {kind:6s}  " + " ".join(f"{v:+.2f}" for v in pre) + f"  (bit step {step:.1f} deg)")
nofit = sum(x[1] == "nofit" for x in res); res = [x for x in res if x[1] != "nofit"]
if not res: sys.exit("no frames")
k = [x[1] for x in res]; m = np.mean([x[2] for x in res], axis=0)
print(("INJECTED TEST - " if a.inject else "") + f"NUCLEO SUMMARY {len(res)} time frames {res[0][0]} .. {res[-1][0]} UTC: 0x680C {k.count('0x680C')}, "
      f"no start sequence {k.count('none')}, other {k.count('other')}; mean of the 16 slots " + " ".join(f"{v:+.2f}" for v in m) +
      f"; read as bits {''.join('1' if v > 0.5 else '0' for v in m)} (0x680C = {SEQ}); bit step {np.mean([x[3] for x in res]):.1f} deg; not located {nofit}")
