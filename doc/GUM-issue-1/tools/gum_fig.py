import sys, json, datetime as dt
import numpy as np, scipy.io.wavfile as wf, scipy.signal as ss
import matplotlib; matplotlib.use("Agg"); import matplotlib.pyplot as plt
sys.path.insert(0,"/codec"); from eczas_codec import check_frame, make_frame
plt.rcParams.update({"font.family":"DejaVu Sans","font.size":9})
EV=[dict(name="ev1",file="20260927_000058.wav",med=-0.638,slot="2026-09-27 00:02:57",early=2.4763,N=281260860),
    dict(name="ev2",file="20260927_063059.wav",med=-0.096,slot="2026-09-27 06:37:15",early=2.4008,N=281268746)]
out={}
for e in EV:
    fs,x=wf.read("/in/"+e["file"]); x=x.astype(float)
    name=dt.datetime.strptime(e["file"][:15],"%Y%m%d_%H%M%S")
    S=dt.datetime.strptime(e["slot"],"%Y-%m-%d %H:%M:%S")
    ft=lambda t:(t-name).total_seconds()+e["med"]            # UTC -> file time
    a=ft(S)-9.5; b=ft(S)+15.5
    seg=x[int(a*fs):int(b*fs)]; t=np.arange(len(seg))/fs
    # carrier frequency from this segment
    z0=seg*np.exp(-2j*np.pi*1000*t); bb,aa=ss.butter(4,40/(fs/2)); zz=ss.filtfilt(bb,aa,z0)[::8]
    N=1<<18; Z=np.abs(np.fft.fft(zz*np.hanning(len(zz)),N)); f=np.fft.fftfreq(N,1/1000); f0=1000+f[np.argmax(np.where(abs(f)<5,Z,0))]
    z=seg*np.exp(-2j*np.pi*f0*t); bb,aa=ss.butter(4,100/(fs/2)); z=ss.filtfilt(bb,aa,z)[::fs//1000]
    ph=np.degrees(np.unwrap(np.angle(z)))
    # idle reference: 85th percentile per 0.5 s, interpolated
    k=500; ref=np.array([np.percentile(ph[i:i+k],85) for i in range(0,len(ph)-k+1,k)])
    ph=ph-np.interp(np.arange(len(ph)),np.arange(len(ref))*k+k/2,ref)
    ts=np.arange(len(ph))/1000.0-9.5                           # seconds relative to slot S
    st=3.0-e["early"]                                          # early frame start rel. to S
    i0=int(round((st+9.5)*1000))
    # local idle reference just before and after the early frame, linear between
    r1=np.median(ph[i0-450:i0-50]); r2=np.median(ph[i0+1950:i0+2350])
    loc=ph-(r1+(r2-r1)*(np.arange(len(ph))-(i0-250))/2400.0)
    PRE=[0,1]*8+[0,1,1,0,0,0,0,0]+[1,0,1]
    best=None
    for d in range(-15,16):
        v=np.array([np.mean(loc[i0+d+20*j+6:i0+d+20*j+18]) for j in range(96)])
        l0=np.median(v[:27][np.array(PRE)==0])
        b=[1 if q>l0/2 else 0 for q in v]
        sc=sum(x==y for x,y in zip(b[:27],PRE))
        if best is None or sc>best[0]: best=(sc,d,b,l0)
    sc,dd,bits,l0=best
    i0+=dd; st+=dd/1000.0; ph=loc
    r=check_frame(bits); ref_bits=make_frame(e["N"],2)
    byt=lambda B:" ".join(f"{int(''.join(map(str,B[8*i:8*i+8])),2):02X}" for i in range(12))
    pre_act=float(np.std(ph[int((0.05+9.5)*1000):int((st-0.03+9.5)*1000)]))
    idle_act=float(np.std(ph[int((3.0+9.5)*1000):int((5.0+9.5)*1000)]))
    out[e["name"]]=dict(l0=round(float(l0),1), tshift_ms=dd, start=(S+dt.timedelta(seconds=st)).strftime("%H:%M:%S.%f")[:-3], slot=e["slot"],
        rx=byt(bits), clean=byt(ref_bits), diff=sum(a!=b for a,b in zip(bits,ref_bits)), valid=bool(r),
        Nrx=r["N"] if r else None, f0=round(f0,3), act_before=round(pre_act,1), act_idle=round(idle_act,1))
    # figure: 25 s timeline
    fig,ax=plt.subplots(figsize=(7.2,2.6))
    ax.plot(ts,ph,lw=0.5,color="#1f4e79")
    for s in range(-9,16,3): ax.axvline(s,color="#999",lw=0.6,ls="--")
    for s in range(-9,16,3):
        lab=(S+dt.timedelta(seconds=s)).strftime(":%S")
        ax.text(s+0.05,22,lab,fontsize=7,color="#555")
    ax.axvspan(st,st+1.92,color="#d62728",alpha=0.15)
    ax.axvspan(3,4.92,color="#999",alpha=0.12,hatch="//",fill=False)
    ax.annotate("early frame (content = slot S+3)",xy=(st+0.9,-45),xytext=(-8.8,-70),fontsize=7,color="#d62728",
                arrowprops=dict(arrowstyle="->",color="#d62728",lw=0.7))
    ax.annotate("slot S+3: empty",xy=(4.0,-10),xytext=(5.6,-70),fontsize=7,color="#555",
                arrowprops=dict(arrowstyle="->",color="#777",lw=0.7))
    ax.set_ylim(-85,30); ax.set_xlim(-9.5,15.5)
    ax.set_xlabel(f"time [s] relative to slot S = {e['slot'][11:]} UTC"); ax.set_ylabel("carrier phase [deg]")
    fig.tight_layout(); fig.savefig(f"/o/{e['name']}_timeline.png",dpi=220); plt.close(fig)
    # zoom: received phase vs ideal frame N
    fig,ax=plt.subplots(figsize=(7.2,2.2))
    zt=ts[i0-150:i0+2100]; ax.plot(zt,ph[i0-150:i0+2100],lw=0.6,color="#1f4e79",label="received (IC-705, Wroclaw)")
    lvl=np.median(ph[i0:i0+1920][np.array([ref_bits[int(j/20)] for j in range(1920)])==0])
    ideal=np.concatenate([np.zeros(150),np.repeat([0 if b else lvl for b in ref_bits],20),np.zeros(180)])[:len(zt)]
    ax.plot(zt,ideal,lw=1.0,color="#d62728",alpha=0.8,label=f"ideal time frame N={e['N']}")
    ax.axvline(0,color="#999",ls="--",lw=0.6); ax.axvline(st,color="#d62728",ls=":",lw=0.8)
    ax.text(st+0.02,20,f"frame start {st:.3f} s after slot start",fontsize=7,color="#d62728")
    ax.set_ylim(-80,30); ax.set_xlabel("time [s] relative to slot S"); ax.set_ylabel("phase [deg]"); ax.legend(fontsize=7,loc="lower right")
    fig.tight_layout(); fig.savefig(f"/o/{e['name']}_zoom.png",dpi=220); plt.close(fig)
json.dump(out,open("/o/events.json","w"),indent=1); print(json.dumps(out,indent=1))
