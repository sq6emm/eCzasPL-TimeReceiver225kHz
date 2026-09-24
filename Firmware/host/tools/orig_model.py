"""Behavioural model of the ORIGINAL firmware demodulator/decoder (reverse engineered)."""
import numpy as np, json, sys
from numba import njit
import scipy.signal as ss
from eczas_codec import rs_decode, frame_bits_to_codeword, codeword_to_bits, SCR, PRE
import os
T=json.load(open(os.path.join(os.path.dirname(os.path.abspath(__file__)),"orig_fir_tables.json")))
FIR1=np.array(T["fir1"],float)/32768; FIR2=np.array(T["fir2"],float)/2**18

@njit(cache=True)
def fast_atan(A,B):   # original 0x50ce angle approximation, units pi=32768
    b=abs(B)+1e-9
    if A>=0:
        r=(A-b)/(A+b); a=0x2000-0x2000*r
    else:
        r=(A+b)/(b-A); a=0x6000-0x2000*r
    return -a if B<0 else a

@njit(cache=True)
def demod(y):
    npairs=len(y)//2
    ang=np.zeros(npairs)
    for m in range(100,npairs):
        I=y[2*m]; Q=y[2*m+1]; Io=y[2*m-200]; Qo=y[2*m-199]
        A=I*Io+Q*Qo; B=I*Qo-Q*Io
        ang[m]=fast_atan(A,B)
    return ang

@njit(cache=True)
def slicer(z):
    # DC tracker + threshold + sync state machine; returns list of (block_of_commit, 72 bits)
    hi=0.0; edge=0; bit=0; last=0; state=0; cnt=0; nmatch=0; sr=0
    PAT=0x5560
    out_blk=[]; out_bits=[]
    buf=np.zeros(72,np.int64); nb=0
    hist=np.zeros(16,np.int64)
    for k in range(len(z)):
        hi+=(z[k]-hi)/4096.0
        w=z[k]-hi
        if w>3000:
            if edge==0: bit=1
            edge=1
        elif w<-3000:
            if edge==0: bit=0
            edge=1
        else: edge=0
        if state==0:
            if bit!=last:
                cnt=5; state=1; nmatch=0; last=bit
        elif state==1:
            cnt-=1
            if cnt==0:
                cnt=10
                sr=((sr<<1)|bit)&0xffff
                exp=(PAT>>(15-nmatch))&1
                if bit==exp: nmatch+=1
                else:
                    # fall back to longest suffix of sr that is a prefix of PAT
                    nm=0
                    for L in range(min(nmatch+1,15),0,-1):
                        if (sr&((1<<L)-1))==(PAT>>(16-L)): nm=L; break
                    nmatch=nm
                if nmatch==16: state=2; nb=0
            last=bit
        else:
            cnt-=1
            if cnt==0:
                cnt=10; buf[nb]=bit; nb+=1
                if nb==72:
                    out_blk.append(k); out_bits.append(buf.copy()); state=0
            last=bit
    return out_blk,out_bits

def run(x):   # x: 10 kHz samples
    y=ss.lfilter(FIR1,1.0,x)
    ang=demod(y)                     # 5 kHz
    z=np.convolve(ang,FIR2,'full')[:len(ang)][9::10]   # one output per 2 ms block
    blks,bits=slicer(z)
    res=[]
    for k,b in zip(blks,bits):
        fb=PRE[:24]+[int(v) for v in b]
        if fb[24:27]!=[1,0,1]: continue
        r=rs_decode(frame_bits_to_codeword(fb))
        if r is None: continue
        cb=codeword_to_bits(r[0],fb)
        d=[cb[27+i]^((SCR>>(36-i))&1) for i in range(37)]
        N=0
        for v in d[:30]: N=(N<<1)|v
        tick_t=(k+50)*0.002           # 1-second tick 50 blocks after last bit
        res.append((tick_t,N))
    return res
