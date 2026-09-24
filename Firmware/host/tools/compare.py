import numpy as np, subprocess, sys, re, os
from orig_model import run
from concurrent.futures import ProcessPoolExecutor
REC=os.environ.get("REC","224k_2102.raw")
base=np.fromfile(REC,"<i2").astype(float)
rms=np.sqrt(np.mean(base**2))
# truth from our decoder on the clean file
out=subprocess.run([os.environ.get("SIM","../sim"),REC],capture_output=True,text=True).stdout
ref=[(float(l.split()[0]),int(re.search(r"N=(\d+)",l).group(1))) for l in out.splitlines() if " N=" in l]
t0,N0=ref[0]
def truth_at_start(t): return N0+round((t-t0)/3)
def job(args):
    snr,seed=args
    rng=np.random.default_rng(seed)
    x=base+rng.normal(0,rms/10**(snr/20),len(base)) if snr<100 else base.copy()
    x=np.clip(x*0.5,-32768,32767)
    fn=f"/tmp/eczas_{snr}_{seed}.raw"; x.astype("<i2").tofile(fn)
    # original
    o=run(x)
    o_ok=o_bad=0; offs=[]
    for tt,N in o:
        # frame start = tick - (96 bits + latency); original labels the tick as 3N+2
        n_true=truth_at_start(tt-2.1)
        if N==n_true: o_ok+=1; offs.append(tt-(t0+3*(N-N0))-2.0)
        else: o_bad+=1
    # new
    s=subprocess.run([os.environ.get("SIM","../sim"),fn],capture_output=True,text=True).stdout
    n_ok=n_bad=n_rej=0; errs=[]
    for l in s.splitlines():
        if " N=" not in l: continue
        t=float(l.split()[0]); N=int(re.search(r"N=(\d+)",l).group(1))
        acc=("ACCEPT" in l) or ("SYNC" in l) or ("STEP" in l)
        good=(N==truth_at_start(t))
        if "REJECT" in l or "CAND" in l: n_rej+=1; continue
        if good: n_ok+=1; errs.append((t-(t0+3*(N-N0)))*1e3)
        else: n_bad+=1
    import os; os.remove(fn)
    return snr,seed,o_ok,o_bad,(np.mean(offs)*1e3 if offs else float('nan')),(np.std(offs)*1e3 if offs else float('nan')),n_ok,n_bad,n_rej,(np.std(errs) if errs else float('nan'))
if __name__=="__main__":
    snrs=[float(a) for a in sys.argv[1].split(",")]; seeds=range(int(sys.argv[2]))
    jobs=[(s,k) for s in snrs for k in seeds]
    with ProcessPoolExecutor(30) as ex: res=list(ex.map(job,jobs))
    print("clean-run reference frames:",len(ref))
    print(" SNR | ORIGINAL: good  WRONG-TIME  offset_ms(mean/std) | NEW: good  WRONG-TIME  held-back  jitter_ms")
    for snr in snrs:
        r=[x for x in res if x[0]==snr]
        a=np.array([x[2:] for x in r],float)
        m=np.nanmean(a,0); s_=a.sum(0)
        print(f"{snr:5.0f} | {m[0]:6.1f} {s_[1]:6.0f}/{len(r)}runs  {m[2]:7.1f}/{m[3]:5.1f} | {m[4]:6.1f} {s_[5]:6.0f}  {m[6]:5.1f}  {m[7]:6.2f}")
