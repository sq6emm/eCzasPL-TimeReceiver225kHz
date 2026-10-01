#!/usr/bin/env python3
"""
Is the start sequence 0x680C transmitted before each e-CzasPL time frame?

GUM, "Opis ramki czasu e-Czas Radio": "Przed każdą ramką transmitowane są ponadto 2 B (0x680C –
0b0110 0b1000 0b0000 0b1100) będące sekwencją startu. Nie są one częścią depeszy czasowej."
If so, the 16 bits 0110 1000 0000 1100 occupy the 320 ms (16 x 20 ms) right before the first 0x55.

Method (IC-7610 MAIN 226.000 USB, mirrored IF at 13 kHz, 48 kHz stereo WAV): carrier phase at 4 kHz;
the frame start is fitted with the decoded frame (eczas_codec.make_frame) as 16.6 ms linear phase ramps
starting at the bit edges; the phase at the end of each 20 ms bit (17.5-19.5 ms into the bit, after
the ramp) is read for the 96 frame bits and for the 16 bit slots before the frame; each slot is
expressed on the frame's own scale: 0 = mean level of its '0' bits, 1 = mean level of its '1' bits.

  check_start_sequence.py FRAMES.csv [--since 'YYYY-MM-DD HH:MM:SS'] [--last N] [-q]
  (run in the eczas-ic705-proc image: /codec = eczas_codec, recordings in the current directory)
FRAMES.csv: eczas-ic7610 out/frames.csv (time, N, offset_ms, ..., file). Regular frames only.
Output per frame: the 16 slot values; summary: how many frames show 0x680C, no sequence (all slots
at one level), or something else.
"""
import sys, csv, argparse, datetime as dt, numpy as np, scipy.io.wavfile as wf, scipy.signal as ss
sys.path.insert(0, "/codec"); from eczas_codec import make_frame

FS2 = 4000; RAMP = 0.0166; BIT = 0.020
EPOCH = dt.datetime(2000, 1, 1, tzinfo=dt.timezone.utc)
SEQ = "0110100000001100"


def model(tt, start, bits):                  # +-1 levels, linear ramps starting at the bit edges
    lev = np.concatenate([[-bits[0]], bits]); kx, ky = [start - 0.04], [lev[0]]
    for k in range(len(bits)):
        kx += [start + k * BIT, start + k * BIT + RAMP]; ky += [lev[k], lev[k + 1]]
    m = np.interp(tt, kx, ky); mk = (tt >= start - 0.04) & (tt < start + len(bits) * BIT)
    return (m - m[mk].mean()) * mk


def refine(s, grid):
    s = np.abs(np.asarray(s)); j = int(np.argmax(s))
    if j in (0, len(s) - 1): return grid[j]
    d = s[j - 1] - 2 * s[j] + s[j + 1]
    return grid[j] + (0.5 * (s[j - 1] - s[j + 1]) / d if d < 0 else 0) * (grid[1] - grid[0])


ap = argparse.ArgumentParser(); ap.add_argument("frames"); ap.add_argument("--since", default="")
ap.add_argument("--last", type=int, default=0); ap.add_argument("-q", action="store_true")
ap.add_argument("--inject", action="store_true", help="TEST: add a 0x680C phase pattern (36 deg, 16.6 ms ramps) before each frame")
a = ap.parse_args()
rows = [r for r in csv.reader(open(a.frames)) if not r[0].startswith("time") and abs(float(r[2])) < 50 and r[0] >= a.since]
if a.last: rows = rows[-a.last:]
cache, res = {}, []
for r in rows:
    N, fn = int(r[1]), r[7]
    try:
        if fn not in cache:
            fs, x = wf.read(fn); x = x[:, 0].astype(float); n = np.arange(len(x))
            z = np.conj(x * np.exp(-2j * np.pi * 13000.0 / fs * n))
            cache = {fn: np.unwrap(np.angle(ss.resample_poly(z, 1, fs // FS2)))}
    except (FileNotFoundError, ValueError):
        continue
    ph = cache[fn]
    t_name = dt.datetime.strptime(fn[:15], "%Y%m%d_%H%M%S").replace(tzinfo=dt.timezone.utc)
    t0 = (EPOCH + dt.timedelta(seconds=3 * N) - t_name).total_seconds() + 0.05
    i0, i1 = int((t0 - 0.6) * FS2), int((t0 + 2.2) * FS2)
    if i0 < 0 or i1 > len(ph): continue
    seg = ph[i0:i1].copy(); tt = np.arange(i0, i1) / FS2
    ref = np.array(make_frame(N, 2), float) * 2 - 1
    seg = seg - np.polyval(np.polyfit(tt, seg, 1), tt)
    sc = lambda T: float(np.dot(seg, model(tt, T, ref)))
    g = np.arange(-0.1, 0.1001, 0.0005); j = int(np.argmax(np.abs([sc(t0 + c) for c in g])))
    f = np.arange(g[j] - 0.0006, g[j] + 0.00061, 0.00005); T = t0 + refine([sc(t0 + c) for c in f], f)
    if a.inject:                             # self-test: the 16 bits right before the FITTED frame start, '0' = -36 deg
        st = T - 16 * BIT; lev = [0.0 if c == "1" else -np.radians(36) for c in SEQ]
        kx, ky = [st - 0.001], [0.0]
        for k in range(16): kx += [st + k * BIT, st + k * BIT + RAMP]; ky += [ky[-1], lev[k]]
        kx += [T - 0.0001]; ky += [ky[-1]]
        inj = np.interp(tt, kx, ky, left=0.0, right=0.0); seg = seg + inj - np.polyval(np.polyfit(tt, inj, 1), tt)
    lvl = lambda k: seg[int((T + k * BIT + 0.0175) * FS2) - i0: int((T + k * BIT + 0.0195) * FS2) - i0].mean()
    fb = np.array([lvl(k) for k in range(96)]); b1 = ref > 0
    l1, l0 = fb[b1].mean(), fb[~b1].mean()
    pre = np.array([(lvl(k) - l0) / (l1 - l0) for k in range(-16, 0)])
    rd = "".join("1" if v > 0.5 else "0" for v in pre)
    step = np.degrees(l1 - l0)
    if not 25 <= abs(step) <= 45:            # the frame was not found where expected (recorder gap etc.)
        res.append((r[0], "nofit", pre, step))
        if not a.q: print(f"{r[0]}  frame not located (bit step {step:.1f} deg) - skipped")
        continue
    kind = "0x680C" if rd == SEQ else ("none" if np.ptp(pre) < 0.3 else "other")
    res.append((r[0], kind, pre, np.degrees(l1 - l0)))
    if not a.q:
        print(f"{r[0]}  {kind:6s}  " + " ".join(f"{v:+.2f}" for v in pre) + f"  (bit step {np.degrees(l1 - l0):.1f} deg)")
nofit = sum(x[1] == "nofit" for x in res); res = [x for x in res if x[1] != "nofit"]
if not res: sys.exit("no frames")
k = [x[1] for x in res]; m = np.mean([x[2] for x in res], axis=0)
print(("INJECTED TEST - " if a.inject else "") + f"SUMMARY {len(res)} time frames {res[0][0]} .. {res[-1][0]} UTC: 0x680C {k.count('0x680C')}, no start sequence "
      f"{k.count('none')}, other {k.count('other')}; mean of the 16 slots {m.mean():+.2f} (range {m.min():+.2f}..{m.max():+.2f}; "
      f"0 = '0' level, 1 = '1' level); bit step {np.mean([x[3] for x in res]):.1f} deg; frames not located {nofit}")
