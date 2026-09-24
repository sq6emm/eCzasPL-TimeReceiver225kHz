"""Convert a USB-demodulated recording (carrier at ~1 kHz) to the harness input:
raw signed 16-bit little-endian mono at 10 kHz.   python3 wav2raw.py in.wav out.raw"""
import sys
from math import gcd
import numpy as np, scipy.io.wavfile as wf, scipy.signal as ss
fs, x = wf.read(sys.argv[1])
x = x.astype(float) if x.ndim == 1 else x[:, 0].astype(float)
g = gcd(10000, fs)
y = ss.resample_poly(x, 10000 // g, fs // g)
(y / np.max(np.abs(y)) * 20000).astype("<i2").tofile(sys.argv[2])
