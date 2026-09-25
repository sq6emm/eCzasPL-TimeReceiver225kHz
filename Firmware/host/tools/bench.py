"""Offline benchmark of simulator builds on the e-CzasPL recordings.
   python bench.py prep SIM [RECDIR]       -> rec/*.raw variants (+14 Hz too) + truth.json
                                              (RECDIR holds 224k_1836.raw, 224k_2102.raw; needs numpy, scipy)
   python bench.py run NAME=SIM [NAME=SIM ...] [--snr 3,0,-3,-6] [--seeds 4]
Every run uses the bench board's crystal error (-23.7 ppm)."""
import json, os, re, subprocess, sys, statistics as st
from concurrent.futures import ThreadPoolExecutor
D = os.path.dirname(os.path.abspath(__file__))
SRC = {"1836": "224k_1836.raw", "2102": "224k_2102.raw"}
XTAL = "-23.7"

def prep(sim, recdir="/data"):
    import numpy as np, scipy.signal as ss
    os.makedirs(f"{D}/rec", exist_ok=True)
    for k, p in SRC.items():
        x = np.fromfile(os.path.join(recdir, p), "<i2").astype(float)
        a = ss.hilbert(x); n = np.arange(len(x))
        for sh in (0, 14):
            y = np.real(a * np.exp(2j * np.pi * sh * n / 10000))
            np.clip(y, -32768, 32767).astype("<i2").tofile(f"{D}/rec/{k}_sh{sh}.raw")
    truth = {}
    for f in sorted(os.listdir(f"{D}/rec")):
        out = subprocess.run([sim, f"{D}/rec/{f}"], capture_output=True, text=True).stdout
        m = [l for l in out.splitlines() if " N=" in l]
        t0, n0 = float(m[0].split()[0]), int(re.search(r"N=(\d+)", m[0]).group(1))
        truth[f] = (t0, n0, len(m))
        print(f, "truth t0=%.3f N0=%d, %d frames decoded clean" % truth[f])
    json.dump(truth, open(f"{D}/truth.json", "w"))

def one(args):
    sim, rec, snr, seed, t0, n0 = args
    cmd = [sim, "-q", "-x", XTAL, "-s", str(seed), "-r", str(t0), str(n0)]
    if snr < 99: cmd += ["-n", str(snr)]
    out = subprocess.run(cmd + [f"{D}/rec/{rec}"], capture_output=True, text=True).stdout
    g = lambda k: float(re.search(k + r"=(-?[\d.e+]+)", out).group(1))
    return dict(ok=g("time_ok"), step=g("step"), reject=g("reject"), sync=g("first_sync"),
                wrong=g("clock_wrong"), err=g("err_mean"),
                conf=g("confirmed") if "confirmed=" in out else 0, cwrong=g("confirm_wrong") if "confirm_wrong=" in out else 0)

def run(sims, snrs, seeds):
    truth = json.load(open(f"{D}/truth.json"))
    jobs = {(name, snr): [(sim, r, snr, s, t[0], t[1]) for r, t in truth.items() for s in range(1, seeds + 1)]
            for name, sim in sims for snr in snrs}
    with ThreadPoolExecutor(30) as ex:
        res = {k: list(ex.map(one, v)) for k, v in jobs.items()}
    print(f"{'build':14}{'SNR':>5}{'fixes':>7}{'wrong':>7}{'steps':>7}{'reject':>8}{'sync med s':>12}{'no sync':>9}{'|err| ms':>10}{'confirmed':>11}{'c.wrong':>9}")
    for snr in snrs:
        for name, _ in sims:
            r = res[(name, snr)]
            syncs = [x["sync"] for x in r if x["sync"] >= 0]
            errs = [abs(x["err"]) / 1000 for x in r if x["ok"] > 2]
            print(f"{name:14}{snr:>5}{sum(x['ok'] for x in r):>7.0f}{sum(x['wrong'] for x in r):>7.0f}"
                  f"{sum(x['step'] for x in r):>7.0f}{sum(x['reject'] for x in r):>8.0f}"
                  f"{(st.median(syncs) if syncs else float('nan')):>12.0f}{len(r) - len(syncs):>9}"
                  f"{(st.median(errs) if errs else float('nan')):>10.2f}"
                  f"{sum(x['conf'] for x in r):>11.0f}{sum(x['cwrong'] for x in r):>9.0f}")

if __name__ == "__main__":
    if sys.argv[1] == "prep": prep(sys.argv[2], *sys.argv[3:4])
    else:
        a = sys.argv[2:]; snrs = [3, 0, -3, -6]; seeds = 4
        if "--snr" in a: i = a.index("--snr"); snrs = [float(x) for x in a[i + 1].split(",")]; del a[i:i + 2]
        if "--seeds" in a: i = a.index("--seeds"); seeds = int(a[i + 1]); del a[i:i + 2]
        run([x.split("=", 1) for x in a], snrs, seeds)
