"""Log excerpts for events 56-64 (as gum_logs_31_55.py).
docker: -v <legnica logs>:/leg -v <nucleo log dir>:/nuc -v <doc/GUM-issue-1>:/o -v <tools>:/t"""
import sys
sys.path.insert(0, "/t")
import gum_logs_31_55 as g

EV = [(56, "2026-09-29 17:40:24"), (57, "2026-09-29 21:09:03"), (58, "2026-09-29 22:49:39"), (59, "2026-09-29 23:29:45"),
      (60, "2026-09-30 01:01:42"), (61, "2026-09-30 02:52:24"), (62, "2026-09-30 06:24:03"), (63, "2026-09-30 07:37:03"),
      (64, "2026-09-30 07:41:48")]

if __name__ == "__main__":
    g.run(EV)
