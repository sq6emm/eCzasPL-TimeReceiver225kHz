# Reference implementation of e-CzasPL frame coding (RS(15,9) GF16 x^4+x+1, b=1; CRC8 0x07; scrambling)
import numpy as np
EXP=[0]*30; LOG=[0]*16
v=1
for i in range(15):
    EXP[i]=v; LOG[v]=i; v<<=1
    if v&16: v^=0x13
for i in range(15,30): EXP[i]=EXP[i-15]
def gmul(a,b): return 0 if a==0 or b==0 else EXP[LOG[a]+LOG[b]]
def gdiv(a,b): return 0 if a==0 else EXP[(LOG[a]-LOG[b])%15]
SCR=0x0A47554D2B & ((1<<37)-1)    # 37 LSBs, applied MSB-first to bits 27..63
def crc8(bits40):     # bits list (40 bits: frame bits 24..63)
    c=0
    for b in bits40:
        fb=((c>>7)&1)^b; c=((c<<1)&0xff)^(0x07 if fb else 0)
    return c
def rs_generator():
    g=[1]
    for i in range(1,7):
        # multiply by (x - a^i)
        ng=[0]*(len(g)+1)
        for j,c in enumerate(g):
            ng[j]^=gmul(c,EXP[i]); ng[j+1]^=c
        g=ng
    return g  # low->high degree
G=rs_generator()
def rs_encode(data9):  # data9[0]=coef x^6 (S0-S3) ... data9[8]=x^14
    # c(x)=d(x)x^6 + (d(x)x^6 mod g)
    msg=[0]*6+list(data9)
    rem=msg[:]
    for d in range(14,5,-1):
        coef=rem[d]
        if coef:
            for j in range(7):
                rem[d-6+j]^=gmul(coef,G[j])
    return rem[:6]+list(data9)   # c[0..14] by degree
def rs_decode(c):    # c by degree 0..14; returns corrected list or None
    S=[0]*6
    for i in range(6):
        s=0
        for d in range(14,-1,-1): s=gmul(s,EXP[i+1])^c[d]
        S[i]=s
    if not any(S): return list(c),0
    # Berlekamp-Massey
    C=[1]+[0]*6; B=[1]+[0]*6; L=0; m=1; b=1
    for n in range(6):
        d=S[n]
        for i in range(1,L+1): d^=gmul(C[i],S[n-i])
        if d==0: m+=1; continue
        T=C[:]; coef=gdiv(d,b)
        for i in range(m,7): C[i]^=gmul(coef,B[i-m])
        if 2*L<=n: L=n+1-L; B=T; b=d; m=1
        else: m+=1
    if L>3: return None
    # Chien
    pos=[]
    for d in range(15):
        xinv=EXP[(15-d)%15]; s=0
        for i in range(L,-1,-1): s=gmul(s,xinv)^C[i]
        if s==0: pos.append(d)
    if len(pos)!=L: return None
    # Forney: Omega = S(x)C(x) mod x^6
    Om=[0]*6
    for i in range(6):
        for j in range(i+1):
            if j<=6: Om[i]^=gmul(S[i-j],C[j]) if j<len(C) else 0
    out=list(c)
    for d in range(len(pos)):
        p=pos[d]; X=EXP[p]; Xinv=EXP[(15-p)%15]
        num=0
        for i in range(5,-1,-1): num=gmul(num,Xinv)^Om[i]
        den=0
        for i in range(1,L+1,2): den^=gmul(C[i],EXP[(LOG[Xinv]*(i-1))%15]) if Xinv else 0
        if den==0: return None
        # b=1 -> e = X^(1-b) * Om/C' = Om/C'
        out[p]^=gdiv(num,den)
    return out,L
def frame_bits_to_codeword(bits):   # bits: 96 list
    data=[]
    for k in range(9):   # S0-S3 at bits 27..30 -> degree 6
        v=0
        for b in bits[27+4*k:31+4*k]: v=(v<<1)|b
        data.append(v)
    par=[]
    for k in range(6):
        v=0
        for b in bits[64+4*k:68+4*k]: v=(v<<1)|b
        par.append(v)
    return par+data     # degree 0..5 = ECC0hi..ECC2lo, 6..14 = S0..SK0
def codeword_to_bits(cw,bits):
    b=list(bits)
    for k in range(9):
        for j in range(4): b[27+4*k+j]=(cw[6+k]>>(3-j))&1
    for k in range(6):
        for j in range(4): b[64+4*k+j]=(cw[k]>>(3-j))&1
    return b
PRE=[0,1]*8+[0,1,1,0,0,0,0,0]+[1,0,1]
def check_frame(bits):
    """bits: 96 hard bits. Returns dict or None. Strict: RS<=3 + CRC must hold."""
    if bits[24:27]!=[1,0,1]: return None
    r=rs_decode(frame_bits_to_codeword(bits))
    if r is None: return None
    cw,nerr=r
    b=codeword_to_bits(cw,bits)
    rx=0
    for x in b[88:96]: rx=(rx<<1)|x
    ok=False
    for sk1 in (b[63],1-b[63]):
        b[63]=sk1
        if crc8(b[24:64])==rx: ok=True; break
    if not ok: return None
    d=[b[27+i]^((SCR>>(36-i))&1) for i in range(37)]
    N=0
    for x in d[:30]: N=(N<<1)|x
    return dict(N=N, tz=d[30]+2*d[31], ls=d[32], lss=d[33], tzc=d[34], sk=d[35]+2*d[36], nerr=nerr, bits=b)
def make_frame(N,tz=0,ls=0,lss=0,tzc=0,sk=0):
    d=[(N>>(29-i))&1 for i in range(30)]+[tz&1,tz>>1,ls,lss,tzc,sk&1,sk>>1]
    s=[d[i]^((SCR>>(36-i))&1) for i in range(37)]
    bits=PRE+s+[0]*32
    cw=frame_bits_to_codeword(bits)
    cw=rs_encode(cw[6:])
    bits=codeword_to_bits(cw,bits)
    c=crc8(bits[24:64])
    bits[88:96]=[(c>>(7-i))&1 for i in range(8)]
    return bits
if __name__=="__main__":
    import random
    # self-test with sp6hfe's example frame from decoded_data (1st line, v2 data is differential so craft)
    f=make_frame(258787930,tz=2)
    r=check_frame(f); assert r and r['N']==258787930 and r['tz']==2
    for t in range(2000):
        N=random.randrange(1<<30); f=make_frame(N,random.randrange(4))
        e=list(f); 
        for s in random.sample(range(15),3):   # corrupt 3 symbols
            pass
        r=check_frame(e); assert r and r['N']==N
    print("codec self-test ok; generator",G)
