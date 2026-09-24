#!/bin/sh
# Run the XC16-compiled core benchmark in the sim30 simulator and print simlog[].
#   ./run_sim30.sh ../build/sim/bench.elf
X=${XC16_DIR:-$HOME/.local/microchip/xc16/v2.10}/bin
ELF=$1
DONE=$($X/xc16-objdump -t "$ELF" | awk '$NF=="_bench_done"{print $1}')
LOG=$($X/xc16-objdump -t "$ELF" | awk '$NF=="_simlog"{print $1}')
END=$(printf '%x' $((0x$LOG + 1023)))
printf 'LD dspic30super\nLC %s\nRP\nBS 0x%s\nE\nDF 0x%s 0x%s\nQ\n' "$ELF" "$DONE" "$LOG" "$END" |
    timeout ${TIMEOUT:-900} $X/sim30 2>&1 | python3 -c '
import sys,re
b=bytearray()
for l in sys.stdin:
    m=re.search(r"Loc <\s*[0-9a-fA-F]+>\s+((?:[0-9a-fA-F]{4}\s*)+)$",l.rstrip())
    if m:
        for w in m.group(1).split(): b+=int(w,16).to_bytes(2,"little")
    elif "rror" in l or "arning" in l: sys.stderr.write(l)
print(b.split(b"\0")[0].decode(errors="replace"))'
