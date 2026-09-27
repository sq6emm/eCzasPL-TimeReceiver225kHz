"""Events 10-20 (27.09.2026, IC-705 Wroclaw): received bytes before correction (same simple
phase demodulator as gum_fig.py), the correct frame for N, and 8 kHz WAV clips.
Input: the IC-705 10-min recordings in /in and the processor's alert lines (frame start in file).
docker: -v <ic705 in>:/in -v <proc>:/codec -v <out>:/o"""
import sys, json, datetime as dt
import numpy as np, scipy.io.wavfile as wf, scipy.signal as ss
sys.path.insert(0, "/codec"); from eczas_codec import check_frame, make_frame

# N, file, frame start in the file [s], offset [s] (from /data/claude/eczas-ic705/out/alerts.log)
EV = [(281271435, "20260927_085059.wav", 43.189, -2.434), (281274921, "20260927_113730.wav", 510.509, -2.408),
      (281275357, "20260927_120730.wav", 18.404, -2.451), (281276490, "20260927_125730.wav", 417.349, -2.404),
      (281276741, "20260927_130730.wav", 570.263, -2.469), (281276769, "20260927_131730.wav", 54.280, -2.431),
      (281276797, "20260927_131730.wav", 138.277, -2.434), (281276834, "20260927_131730.wav", 249.233, -2.478),
      (281280385, "20260927_161730.wav", 101.868, -2.472), (281283006, "20260927_182730.wav", 164.639, -2.435),
      (281283221, "20260927_183730.wav", 209.637, -2.416)]
CLIPS = [("2026-09-27 13:16:30", 360, "20260927_130730.wav", 281276741),     # events 14-17
         ("2026-09-27 16:18:30", 120, "20260927_161730.wav", 281280385),     # 18
         ("2026-09-27 18:29:30", 120, "20260927_182730.wav", 281283006),     # 19
         ("2026-09-27 18:40:30", 120, "20260927_183730.wav", 281283221)]     # 20
EPOCH = dt.datetime(2000, 1, 1)
PRE = [0, 1] * 8 + [0, 1, 1, 0, 0, 0, 0, 0] + [1, 0, 1]
byt = lambda B: " ".join(f"{int(''.join(map(str, B[8*i:8*i+8])), 2):02X}" for i in range(12))
t0map = {}                                               # file -> UTC of file time 0

out = []
for k, (N, fn, tf, off) in enumerate(EV, 10):
    fs, x = wf.read("/in/" + fn); x = x.astype(float)
    S3 = EPOCH + dt.timedelta(seconds=3 * N)             # the slot the frame belongs to
    S = S3 - dt.timedelta(seconds=3)
    st = 3.0 + off                                       # frame start after the start of slot S
    tS = tf + 0.1 - st                                   # slot S in file time (proc.py skips the first 0.1 s)
    t0map.setdefault(fn, S - dt.timedelta(seconds=tS))
    a = tS - 9.5; seg = x[int(a * fs):int((tS + 15.5) * fs)]; t = np.arange(len(seg)) / fs
    z0 = seg * np.exp(-2j * np.pi * 1000 * t); bb, aa = ss.butter(4, 40 / (fs / 2)); zz = ss.filtfilt(bb, aa, z0)[::8]
    NN = 1 << 18; Z = np.abs(np.fft.fft(zz * np.hanning(len(zz)), NN)); f = np.fft.fftfreq(NN, 1 / 1000)
    f0 = 1000 + f[np.argmax(np.where(abs(f) < 5, Z, 0))]
    z = seg * np.exp(-2j * np.pi * f0 * t); bb, aa = ss.butter(4, 100 / (fs / 2)); z = ss.filtfilt(bb, aa, z)[::fs // 1000]
    ph = np.degrees(np.unwrap(np.angle(z)))
    i0 = int(round((st + 9.5) * 1000))
    r1 = np.median(ph[i0 - 450:i0 - 50]); r2 = np.median(ph[i0 + 1950:i0 + 2350])
    loc = ph - (r1 + (r2 - r1) * (np.arange(len(ph)) - (i0 - 250)) / 2400.0)
    best = None
    for d in range(-150, 151):
        v = np.array([np.mean(loc[i0 + d + 20 * j + 6:i0 + d + 20 * j + 18]) for j in range(96)])
        l0 = np.median(v[:27][np.array(PRE) == 0])
        b = [1 if q > l0 / 2 else 0 for q in v]
        sc = sum(p == q for p, q in zip(b[:27], PRE))
        if best is None or sc > best[0]: best = (sc, d, b)
    sc, dd, bits = best
    r = check_frame(list(bits)); ref = make_frame(N, 2)
    pre_act = float(np.std(loc[int((0.05 + 9.5) * 1000):int((st - 0.03 + 9.5) * 1000)]))
    idle_act = float(np.std(loc[int((3.05 + 9.5) * 1000):int((5.0 + 9.5) * 1000)]))
    out.append(dict(ev=k, N=N, slot=S.strftime("%Y-%m-%d %H:%M:%S"), time=S3.strftime("%H:%M:%S"),
                    offset_ms=round(off * 1000), start=round(st + dd / 1000, 3), rx=byt(bits), clean=byt(ref),
                    diff=int(sum(p != q for p, q in zip(bits, ref))), valid_simple=bool(r and r["N"] == N),
                    act_before=round(pre_act, 1), act_slot_s3=round(idle_act, 1), f0=round(f0, 3)))
    print(out[-1], flush=True)

for start, dur, fn, N in CLIPS:
    fs, x = wf.read("/in/" + fn)
    u = dt.datetime.strptime(start, "%Y-%m-%d %H:%M:%S")
    if fn not in t0map: continue
    i = int(round((u - t0map[fn]).total_seconds() * fs))
    seg = x[i:i + dur * fs]
    if len(seg) < dur * fs:                              # continue into the next 10-min file
        nxt = (dt.datetime.strptime(fn[:15], "%Y%m%d_%H%M%S") + dt.timedelta(minutes=10)).strftime("%Y%m%d_%H%M%S")
        import glob
        g = sorted(glob.glob(f"/in/{nxt[:12]}*.wav"))
        if g:
            _, y = wf.read(g[0]); seg = np.concatenate([seg, y[:dur * fs - len(seg)]])
    name = f"/o/ic705_early_frame_{u:%Y%m%d_%H%M%S}.wav"
    wf.write(name, fs, seg.astype(np.int16)); print("clip", name, len(seg) / fs, "s")
json.dump(out, open("/o/events_10_20.json", "w"), indent=1)
