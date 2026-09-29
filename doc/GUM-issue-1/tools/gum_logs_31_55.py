"""Log excerpts for events 31-55 (window S-30 s .. S+45 s) and what each receiver logged at the early
frame (a FRAME line S+2.3 .. S+3.0 s after the start of slot S, host time).
  /leg/beacon-2026-09-2[89].log  Legnica receiver log, $GPRMC lines already removed
  /nuc/rx.log                     NUCLEO log
docker: -v <legnica logs>:/leg -v <nucleo log dir>:/nuc -v <doc/GUM-issue-1>:/o"""
import re, glob, datetime as dt

EV = [(31, "2026-09-28 11:54:45"), (32, "2026-09-28 12:13:03"), (33, "2026-09-28 12:26:12"), (34, "2026-09-28 12:48:03"),
      (35, "2026-09-28 13:03:30"), (36, "2026-09-28 14:50:18"), (37, "2026-09-28 16:01:03"), (38, "2026-09-28 16:32:18"),
      (39, "2026-09-28 16:41:54"), (40, "2026-09-28 18:58:00"), (41, "2026-09-28 20:06:18"), (42, "2026-09-28 20:36:42"),
      (43, "2026-09-28 22:51:27"), (44, "2026-09-29 01:58:03"), (45, "2026-09-29 02:31:15"), (46, "2026-09-29 03:37:21"),
      (47, "2026-09-29 03:43:00"), (48, "2026-09-29 04:05:03"), (49, "2026-09-29 05:14:27"), (50, "2026-09-29 06:55:03"),
      (51, "2026-09-29 10:23:30"), (52, "2026-09-29 11:11:03"), (53, "2026-09-29 11:26:39"), (54, "2026-09-29 11:43:00"),
      (55, "2026-09-29 16:13:15")]
clean = lambda b: re.sub(r"[^\t\x20-\x7e]", "", b.decode(errors="ignore"))

leg = []
for fn in sorted(glob.glob("/leg/beacon-2026-09-2*.log")):
    for raw in open(fn, "rb"):
        l = clean(raw)
        try: leg.append((dt.datetime.strptime(l[:23], "%Y-%m-%d %H:%M:%S.%f"), l))
        except ValueError: pass
nuc = []
for raw in open("/nuc/rx.log", "rb"):
    l = clean(raw)
    if not re.search(r"\] (STATUS|FRAME|CLOCK) ", l): continue
    try: nuc.append((dt.datetime.strptime(l[:23], "%Y-%m-%dT%H:%M:%S.%f"), l))
    except ValueError: pass


def nuc_fw(S):
    if S < dt.datetime(2026, 9, 28, 17, 15):
        return "carrier-coherent receiver, mixer phase-locked to true time, time base 32.768 kHz crystal (LSE)"
    return ("10 MHz OCXO time base since 28.09 17:15 UTC, steered to the received carriers since 28.09 ~17:40 UTC, "
            "carrier-coherent receiver")


for k, F in EV:
    F = dt.datetime.strptime(F, "%Y-%m-%d %H:%M:%S"); S = F - dt.timedelta(seconds=3)
    a, b = S - dt.timedelta(seconds=30), S + dt.timedelta(seconds=45)
    tag = f"event{k}_{S:%Y%m%d_%H%M%S}.log"
    L = [l for t, l in leg if a <= t < b]
    with open("/o/legnica-log/" + tag, "w") as f:
        f.write(f"# Legnica receiver log, slot S = {S:%Y-%m-%d %H:%M:%S} UTC, window S-30 s .. S+45 s ($GPRMC lines omitted)\n"
                "# column 1-2: host receive time (UTC, NTP); $PECZ = receiver diagnostics, [uptime s]\n")
        f.writelines(l + "\n" for l in L)
    n = [l for t, l in nuc if a <= t < b]
    if n:
        with open("/o/nucleo-log/" + tag, "w") as f:
            f.write(f"# NUCLEO-H723ZG receiver log (Wroclaw, ferrite rod, direct sampling; {nuc_fw(S)}; frame decoder and "
                    f"timekeeper = eCzas 2.0.5 core), slot S = {S:%Y-%m-%d %H:%M:%S} UTC, window S-30 s .. S+45 s\n"
                    "# column 1: host receive time (UTC, NTP); [uptime s]; STATUS / FRAME / CLOCK lines only "
                    "(DCF77, CARRIER, TIMEMAP, OCXO and audio dump omitted)\n")
            f.writelines(l + "\n" for l in n)
    at = lambda lst: [l for t, l in lst if S + dt.timedelta(seconds=2.3) <= t < S + dt.timedelta(seconds=3.0) and "FRAME" in l]
    sl = lambda lst: [l for t, l in lst if S + dt.timedelta(seconds=5.0) <= t < S + dt.timedelta(seconds=5.4) and "FRAME" in l]
    print(f"== {k} S={S:%m-%d %H:%M:%S} legnica {len(L)} lines, nucleo {len(n)} lines")
    for l in at(leg): print("   LEG early:", l[:150])
    for l in sl(leg): print("   LEG S+3  :", l[:150])
    for l in at(nuc): print("   NUC early:", l[:150])
    for l in sl(nuc): print("   NUC S+3  :", l[:150])
