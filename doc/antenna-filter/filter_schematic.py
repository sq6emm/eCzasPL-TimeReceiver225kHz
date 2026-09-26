"""Draws antenna-filter-225kHz.pdf: ferrite antenna tank + VHF/UHF low-pass in
front of the SI4735 AMI input. Run: python3 filter_schematic.py (needs schemdraw,
matplotlib)."""
import math
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.backends.backend_pdf import PdfPages
import schemdraw
import schemdraw.elements as elm

L_ANT = 360e-6
def f_res(c): return 1 / (2 * math.pi * math.sqrt(L_ANT * c))
C225 = 1 / ((2 * math.pi * 225e3) ** 2 * L_ANT)

A4 = (11.69, 8.27)
GREY = "#777777"
out = PdfPages("antenna-filter-225kHz.pdf")

# ---------------------------------------------------------------- page 1
fig = plt.figure(figsize=A4)
fig.text(0.04, 0.95, "e-CzasPL receiver: 225 kHz ferrite antenna tank and VHF/UHF filter for the SI4735 AMI input",
         fontsize=13, weight="bold")
fig.text(0.04, 0.925, "For firmware 2.0.4+ (automatic ANTCAP trim at power-up). All capacitors in the tank are C0G/NP0.",
         fontsize=9, color=GREY)
ax = fig.add_axes([0.02, 0.40, 0.96, 0.50]); ax.axis("off")
schemdraw.config(fontsize=11)
d = schemdraw.Drawing(show=False, unit=2.4)
if True:
    # ferrite antenna
    lant = d.add(elm.Inductor2(loops=5).down())
    d.add(elm.Label().at((lant.center[0] - 0.5, lant.center[1])).label(
        "L_ANT\n360 µH\nferrite rod\n120 × Ø10 mm", halign="right"))
    core = elm.Line().at((lant.start[0] + 0.55, lant.start[1] - 0.2)).to((lant.end[0] + 0.55, lant.end[1] + 0.2))
    d.add(core); d.add(elm.Line().at((core.start[0] + 0.15, core.start[1])).to((core.end[0] + 0.15, core.end[1])))
    d.add(elm.Line().at(lant.start).right().length(1.5))
    hot = d.here
    # coax: centre = hot, shield = cold end of the coil
    cx = d.add(elm.Coax(length=5).at(hot).right().label("W1  RG58, keep ≤ 1 m\n(≈100 pF/m counts in the tank)", ofst=0.5))
    d.add(elm.Line().at(lant.end).right().tox(cx.shieldstart))
    d.add(elm.Line().up().toy(cx.shieldstart))
    # box wall
    d.add(elm.Line().at(cx.end).right().length(1.4))
    j1 = d.here
    wall_x = j1[0]
    d.add(elm.Line(ls="--", color=GREY).at((wall_x, hot[1] + 2.6)).to((wall_x, hot[1] - 5.2)))
    d.add(elm.Label().at((wall_x + 0.2, hot[1] + 2.4)).label("metal box wall (chassis)", loc="right", color=GREY, halign="left"))
    d.add(elm.Dot(open=True).at(j1))
    d.add(elm.Label().at((wall_x - 0.2, hot[1] - 1.9)).label("J1 BNC\n(body bonded\nto the wall)", halign="right"))
    d.add(elm.Line().at(cx.shieldend).down().length(0.6))
    d.add(elm.GroundChassis())
    # C1 at the wall
    d.add(elm.Line().at(j1).right().length(1.2))
    n1 = d.add(elm.Dot())
    d.add(elm.Capacitor().down().at(n1.center).label("C1\n820 pF C0G\n(680 pF with\n1 m RG58)", loc="bottom", ofst=0.25))
    d.add(elm.GroundChassis())
    d.add(elm.Line().at(n1.center).right().length(0.8))
    d.add(elm.RBox(w=1.2, h=0.5).right().length(2.4).label("FB1\n600 Ω @ 100 MHz", ofst=0.25))
    d.add(elm.Line().right().length(0.8))
    d.add(elm.Inductor2(loops=3).right().length(2.4).label("L1  10 µH", ofst=0.25))
    d.add(elm.Line().right().length(0.6))
    n2 = d.add(elm.Dot())
    d.add(elm.Capacitor().down().at(n2.center).label("C2\n220 pF C0G", loc="bottom", ofst=0.25))
    d.add(elm.Ground())
    d.add(elm.Line().at(n2.center).right().length(1.8))
    d.add(elm.Dot(open=True).label("board\nANT input", loc="top", ofst=0.2))
    d.add(elm.Capacitor(color=GREY).right().length(2.6).label("C18 470 nF\n(on the board)", loc="bottom", color=GREY, ofst=0.25))
    n3 = d.add(elm.Dot(color=GREY))
    d.add(elm.Line(color=GREY).right().length(1.2))
    d.add(elm.Label().label("SI4735\nAMI", loc="right", color=GREY, halign="left"))
    d.add(elm.CapacitorVar(color=GREY).down().at(n3.center).label("ANTCAP\n0–584 pF\n(inside SI4735,\nset by firmware)", loc="bottom", color=GREY, ofst=0.25))
    d.add(elm.Ground(color=GREY))

d.save("schematic.png", dpi=250)
ax.imshow(plt.imread("schematic.png"), interpolation="lanczos")

# notes
cA = 820e-12 + 220e-12 + 30e-12
cB = 680e-12 + 220e-12 + 100e-12 + 30e-12
rows = [["", "fixed C (incl. ~30 pF stray)", "ANTCAP for 225 kHz", "tuning range with ANTCAP 0–584 pF"],
        ["A: antenna on the box, short leads (C1 = 820 pF)", f"{cA*1e12:.0f} pF", f"≈ {(C225-cA)*1e12:.0f} pF",
         f"{f_res(cA+584e-12)/1e3:.0f}–{f_res(cA)/1e3:.0f} kHz"],
        ["B: ~1 m RG58 to the antenna (C1 = 680 pF)", f"{cB*1e12:.0f} pF", f"≈ {(C225-cB)*1e12:.0f} pF",
         f"{f_res(cB+584e-12)/1e3:.0f}–{f_res(cB)/1e3:.0f} kHz"]]
tax = fig.add_axes([0.04, 0.265, 0.92, 0.10]); tax.axis("off")
t = tax.table(cellText=rows, loc="center", cellLoc="left", colWidths=[0.36, 0.24, 0.14, 0.26])
t.auto_set_font_size(False); t.set_fontsize(8.5); t.scale(1, 1.35)
for k in range(4): t[0, k].set_text_props(weight="bold")
fig.text(0.04, 0.375, f"Resonance budget: 360 µH needs {C225*1e12:.0f} pF total at 225 kHz. "
         "±10 % on L_ANT needs 1.26–1.54 nF, still inside the ANTCAP range in both variants.", fontsize=9)
notes = [
 "1. Tank capacitors C1, C2 must be C0G/NP0: X7R changes by up to ±15 % with temperature and would detune the antenna by more than its 3–6 kHz bandwidth.",
 "2. C1 goes directly at J1, to the wall, with the shortest possible leads: it carries the VHF/UHF current to the chassis. Bond the coax shield / cold end 360° at the wall.",
 "3. FB1 and L1 are practically a short at 225 kHz (L1 = 14 Ω); with C1/C2 they form a π low-pass: roughly 40–60 dB at FM/DAB/TV, limited by layout.",
 "4. C2 sits next to the board's ANT input; its ground is the board ground, joined to the chassis at one point near J1. C18 (470 nF, on the board) stays.",
 "5. At power-up, firmware 2.0.4 sweeps ANTCAP and logs e.g. 'SI4735: antenna tuned, ANTCAP 345.2 pF'. If it logs 'no antenna resonance', read the RSSI list:",
 "    rising towards the right end = too little fixed C, add 100–150 pF to C1; rising towards the left end = too much, remove 100–150 pF.",
 "6. Orientation: rod axis at right angles to the direction of Solec Kujawski. Keep the rod away from switching supplies and digital wiring.",
 "7. Protection diodes, if ever needed, go after L1 (never before the low-pass), so they cannot rectify the strong VHF signals.",
]
for i, n in enumerate(notes):
    fig.text(0.04, 0.225 - i * 0.026, n, fontsize=8.3)
out.savefig(fig); plt.close(fig)

# ---------------------------------------------------------------- page 2: BOM
fig = plt.figure(figsize=A4)
fig.text(0.04, 0.94, "Parts list: TME (tme.eu) symbols", fontsize=13, weight="bold")
bom = [
 ["Ref", "Qty", "Part", "TME symbol (manufacturer part)", "Alternatives at TME"],
 ["C1", "1", "820 pF C0G 50 V ±5 %, 1206", "C1206C821J5GACTU (KEMET)", "GRM1885C1H821JA01D (Murata, 0603)"],
 ["C1 (var. B)", "1", "680 pF C0G 50 V ±5 %", "CL10C681JB8NNNC (Samsung, 0603)", "RCE5C1H681J0A2H03B (Murata, THT 2.5 mm)"],
 ["C2", "1", "220 pF C0G 50 V ±5 %, 1206", "C1206C221J5GAC (KEMET)", "CC1206JRNPO9221 (Yageo),\nCL31C221JBCNNNC (Samsung)"],
 ["FB1", "1", "Ferrite bead 600 Ω @ 100 MHz, 1206", "BLM31PG601SN1L (Murata)", "LCBC-601 (Ferrocore)"],
 ["L1", "1", "Inductor 10 µH, 1210", "LQH32CN100K53L (Murata)", "LQH32PN100MNCL (Murata)"],
 ["J1", "1", "BNC jack, bulkhead front mount, solder cup,\nnon-isolated (body to the wall)", "112575 (Amphenol RF)", "B6351B1-ND3G-50 (Amphenol RF)"],
 ["P1", "1", "BNC plug for RG58, crimp", "BNC-203", "R141-082-000 (Radiall)"],
 ["W1", "≤1 m", "Coax RG58 50 Ω", "TAS-RG58CU (Tasker RG58C/U)", "SIVA-RG58CU (Siva Cavi)"],
 ["L_ANT", "1", "Ferrite rod antenna 360 µH, 120 × Ø10 mm", "(existing)", ""],
]
ax = fig.add_axes([0.04, 0.35, 0.92, 0.55]); ax.axis("off")
t = ax.table(cellText=bom, loc="upper center", cellLoc="left", colWidths=[0.08, 0.05, 0.30, 0.28, 0.29])
t.auto_set_font_size(False); t.set_fontsize(8.5); t.scale(1, 2.4)
for k in range(5): t[0, k].set_text_props(weight="bold")
fig.text(0.04, 0.30, "Stock was not verified: TME's site does not allow automated stock checks, and some 1206 C0G values have large minimum",
         fontsize=9)
fig.text(0.04, 0.28, "order quantities. Check the symbols in the TME basket; any C0G/NP0 50 V part of the same value and size is equivalent.",
         fontsize=9)
fig.text(0.04, 0.24, "Mounting: C1 at J1 on the inside of the wall; FB1, L1, C2 on a small piece of copper-clad board between J1 and the receiver's",
         fontsize=9)
fig.text(0.04, 0.22, "ANT input, with the copper as ground bonded to the wall at J1.", fontsize=9)
out.savefig(fig); plt.close(fig)
out.close()
print("ok")
