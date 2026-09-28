"""Events 21-30 (27/28.09.2026): WAV clips and received bytes of the early frame (same simple phase
demodulator as gum_new_events.py) for
  - the IC-705 in Wroclaw (USB 224.000 kHz, 8 kHz WAV; file time from the decoded frames), events
    21-23 and 26-28;
  - the KiwiSDR "Central Czechia" (public receiver, MLA-30+ loop, GPS-disciplined), events 29-30:
    IQ 225 kHz +-3 kHz, 12 kHz, every 512-sample block with its GPS time (kiwirecorder --kiwi-wav).
    The clip is rebuilt from the blocks by their GPS time (zero-phase filters, no delay), so its
    time axis is UTC (GPS) at the receiver, i.e. including ~1.5 ms propagation from Solec Kujawski.
docker: -v <ic705 in>:/in -v <kiwi raw>:/kiwi -v <proc>:/codec -v <out>:/o"""
import sys, json, glob, struct, datetime as dt
import numpy as np, scipy.io.wavfile as wf, scipy.signal as ss
sys.path.insert(0, "/codec"); from eczas_codec import check_frame, make_frame

EPOCH = dt.datetime(2000, 1, 1)
PRE = [0, 1] * 8 + [0, 1, 1, 0, 0, 0, 0, 0] + [1, 0, 1]
byt = lambda B: " ".join(f"{int(''.join(map(str, B[8*i:8*i+8])), 2):02X}" for i in range(12))

# event, N, IC-705 file, frame start in the file [s], offset [s] (eczas-ic705 alerts.log)
IC = [(21, 281288801, "20260927_231731.wav", 149.061, -2.417), (22, 281288813, "20260927_231731.wav", 185.040, -2.439),
      (23, 281289880, "20260928_000731.wav", 385.929, -2.444), (26, 281297606, "20260928_063732.wav", 163.115, -2.454),
      (27, 281299853, "20260928_082732.wav", 303.863, -2.481), (28, 281299881, "20260928_082732.wav", 387.900, -2.444)]
IC_CLIPS = [("2026-09-27 23:19:30", 180, "20260927_231731.wav"),       # 21, 22
            ("2026-09-28 00:13:27", 120, "20260928_000731.wav"),       # 23
            ("2026-09-28 06:39:45", 120, "20260928_063732.wav"),       # 26
            ("2026-09-28 08:32:06", 180, "20260928_082732.wav")]       # 27, 28
# event, N, offset [s] (kiwi-ref proc alerts), clip start
KIWI = [(29, 281302841, -2.460, "2026-09-28 11:01:30"), (30, 281303024, -2.440, "2026-09-28 11:10:39")]
GPS0, LEAP, FSK = dt.datetime(1980, 1, 6), 18, 12000


def analyse(x, fs, tS, st, N, k):
    """x: 8 kHz 'USB audio' (carrier ~1 kHz); tS: slot S in x's time [s]; st: expected frame start after S"""
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
    S3 = EPOCH + dt.timedelta(seconds=3 * N); S = S3 - dt.timedelta(seconds=3)
    return dict(ev=k, N=N, slot=S.strftime("%Y-%m-%d %H:%M:%S"), time=S3.strftime("%H:%M:%S"),
                start=round(st + dd / 1000, 3), rx=byt(bits), clean=byt(ref),
                diff=int(sum(p != q for p, q in zip(bits, ref))), valid_simple=bool(r and r["N"] == N),
                act_before=round(float(np.std(loc[int(9.55 * 1000):int((st - 0.03 + 9.5) * 1000)])), 1),
                act_slot_s3=round(float(np.std(loc[int(12.55 * 1000):int(14.5 * 1000)])), 1), f0=round(f0, 3))


def kiwi_blocks(files, wk_ref):
    out = []
    for fn in files:
        b = open(fn, "rb").read(); i = 12; ts = None
        while i + 8 <= len(b):
            cid, sz = b[i:i + 4], struct.unpack_from("<I", b, i + 4)[0]
            if i + 8 + sz > len(b): break
            if cid == b"kiwi":
                _, _, gs, gn = struct.unpack_from("<BBII", b, i + 8)
                ts = (GPS0 + dt.timedelta(weeks=wk_ref, seconds=gs - LEAP, microseconds=gn / 1000)) if gs else None
            elif cid == b"data" and ts is not None:
                x = np.frombuffer(b[i + 8:i + 8 + sz], "<i2").astype(float).reshape(-1, 2)
                out.append((ts, x[:, 0] + 1j * x[:, 1])); ts = None
            i += 8 + sz + (sz & 1)
    return out


out = []
t0map = {}
for k, N, fn, tf, off in IC:
    fs, x = wf.read("/in/" + fn); x = x.astype(float)
    S = EPOCH + dt.timedelta(seconds=3 * N) - dt.timedelta(seconds=3)
    st = 3.0 + off; tS = tf + 0.1 - st                   # proc.py skips the first 0.1 s of a file
    t0map.setdefault(fn, S - dt.timedelta(seconds=tS))
    d = analyse(x, fs, tS, st, N, k); d.update(receiver="IC-705 Wroclaw", offset_ms=round(off * 1000)); out.append(d)
    print(d, flush=True)
for start, dur, fn in IC_CLIPS:
    fs, x = wf.read("/in/" + fn); u = dt.datetime.strptime(start, "%Y-%m-%d %H:%M:%S")
    i = int(round((u - t0map[fn]).total_seconds() * fs)); seg = x[i:i + dur * fs]
    wf.write(f"/o/ic705_early_frame_{u:%Y%m%d_%H%M%S}.wav", fs, seg.astype(np.int16)); print("clip", u, len(seg) / fs, "s")

wk = int((dt.datetime(2026, 9, 28) - GPS0).days // 7)
blocks = kiwi_blocks(sorted(glob.glob("/kiwi/20260928T1*_225000_czechia_iq.wav")), wk)
for k, N, off, start in KIWI:
    u = dt.datetime.strptime(start, "%Y-%m-%d %H:%M:%S"); dur = 120
    z = np.zeros(dur * FSK, complex); got = 0
    for ts, x in blocks:
        j = int(round((ts - u).total_seconds() * FSK))
        if j + len(x) <= 0 or j >= len(z): continue
        a, b = max(j, 0), min(j + len(x), len(z)); z[a:b] = x[a - j:b - j]; got += b - a
    # 12 kHz complex (carrier at 0 Hz) -> 8 kHz 'USB audio' with the carrier at 1 kHz, zero-phase filters
    y = ss.filtfilt(ss.firwin(301, 900, fs=FSK), 1, z)
    y = ss.resample_poly(y, 2, 3)
    y = (y * np.exp(2j * np.pi * 1000 / 8000 * np.arange(len(y)))).real
    y = (y * 12000 / np.percentile(np.abs(y), 99.9)).clip(-32767, 32767).astype(np.int16)
    fnc = f"/o/kiwi_czechia_early_frame_{u:%Y%m%d_%H%M%S}.wav"; wf.write(fnc, 8000, y)
    print("clip", fnc, f"coverage {got / len(z):.1%}")
    S = EPOCH + dt.timedelta(seconds=3 * N) - dt.timedelta(seconds=3)
    tS = (S - u).total_seconds()
    d = analyse(y.astype(float), 8000, tS, 3.0 + off, N, k)
    d.update(receiver="KiwiSDR Central Czechia", offset_ms=round(off * 1000, 1))
    out.append(d); print(d, flush=True)
json.dump(out, open("/o/events_21_30.json", "w"), indent=1)
