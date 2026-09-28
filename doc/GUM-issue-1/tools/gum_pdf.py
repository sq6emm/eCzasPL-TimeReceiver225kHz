from reportlab.lib.pagesizes import A4
from reportlab.lib.units import mm
from reportlab.lib import colors
from reportlab.lib.styles import ParagraphStyle
from reportlab.platypus import (SimpleDocTemplate, Paragraph, Spacer, Table, TableStyle,
                                Image, PageBreak, KeepTogether)
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont

F = "/usr/share/fonts/truetype/dejavu/"
pdfmetrics.registerFont(TTFont("DV", F + "DejaVuSans.ttf"))
pdfmetrics.registerFont(TTFont("DVB", F + "DejaVuSans-Bold.ttf"))
pdfmetrics.registerFont(TTFont("DVM", F + "DejaVuSansMono.ttf"))
pdfmetrics.registerFontFamily("DV", normal="DV", bold="DVB", italic="DV", boldItalic="DVB")

BLUE = colors.HexColor("#1f4e79")
st = {
    "title": ParagraphStyle("t", fontName="DVB", fontSize=15, leading=19, textColor=BLUE, spaceAfter=4),
    "sub": ParagraphStyle("s", fontName="DV", fontSize=10.5, leading=14, textColor=colors.HexColor("#444"), spaceAfter=2),
    "h1": ParagraphStyle("h1", fontName="DVB", fontSize=12.5, leading=16, textColor=BLUE, spaceBefore=8, spaceAfter=4),
    "h2": ParagraphStyle("h2", fontName="DVB", fontSize=10.5, leading=14, textColor=BLUE, spaceBefore=6, spaceAfter=2),
    "p": ParagraphStyle("p", fontName="DV", fontSize=9.2, leading=12.8, spaceAfter=4, alignment=4),
    "pl": ParagraphStyle("pl", fontName="DV", fontSize=9.2, leading=12.8, spaceAfter=4),
    "li": ParagraphStyle("li", fontName="DV", fontSize=9.2, leading=12.8, leftIndent=10, bulletIndent=0, spaceAfter=2),
    "cap": ParagraphStyle("c", fontName="DV", fontSize=8, leading=10.5, textColor=colors.HexColor("#333"), spaceAfter=8),
    "cell": ParagraphStyle("cl", fontName="DV", fontSize=7.4, leading=9.2),
    "cellb": ParagraphStyle("cb", fontName="DVB", fontSize=7.4, leading=9.2),
    "mono": ParagraphStyle("m", fontName="DVM", fontSize=7.2, leading=9.2),
    "box": ParagraphStyle("b", fontName="DV", fontSize=9.2, leading=12.8, alignment=4),
}
P = lambda t, s="p": Paragraph(t, st[s])
LI = lambda t: Paragraph(t, st["li"], bulletText="•")

# ---------------------------------------------------------------- data
EV = [  # slot S, content, offset ms, start in slot, where, snr, rs, raw, clean
    ("2026-09-25 23:47:12", "23:47:15", -2419, "0.581", "Legnica", "5 dB", "0", "55 55 60 A2 26 F5 8D 8B A3 2A 5F C6", "55 55 60 A2 26 F5 8D 8B A3 2A 5F C6"),
    ("2026-09-26 04:44:36", "04:44:39", -2460, "0.540", "Legnica", "0 dB", "2", "55 55 60 A2 26 F9 13 8B 98 58 7A 0B", "55 55 60 A2 26 F9 13 8B D8 18 7A 0B"),
    ("2026-09-26 07:12:00", "07:12:03", -2459, "0.541", "Legnica", "6 dB", "0", "55 55 60 A2 26 E7 6D 8B 12 C6 EE F1", "55 55 60 A2 26 E7 6D 8B 12 C6 EE F1"),
    ("2026-09-26 11:12:21", "11:12:24", -2439, "0.561", "Legnica", "−1 dB", "3*", "55 55 60 A2 26 E6 40 0B 98 C4 8A 2E", "55 55 60 A2 26 EE C9 0B 9A C4 8A 0E"),
    ("2026-09-26 11:35:27", "11:35:30", -2480, "0.520", "Legnica", "2 dB", "1", "55 55 60 A3 26 E9 26 0B EC 3C 48 98", "55 55 60 A2 26 E9 26 0B EC 3C 48 98"),
    ("2026-09-26 15:49:48", "15:49:51", -2419, "0.581", "Legnica", "3 dB", "3*", "55 55 60 B7 6F 97 17 8A 4A 56 8F 1D", "55 55 60 A2 26 93 17 8B 6B 56 8F 1D"),
    ("2026-09-27 00:02:57", "00:03:00", -2476, "0.524", "Wrocław IC-705", "9 dB", "1", "55 55 60 A2 26 8C D1 0B BC 13 A0 C8", "55 55 60 A2 26 8C D3 0B BC 13 A0 C8"),
    ("2026-09-27 06:37:15", "06:37:18", -2401, "0.599", "Wrocław IC-705 + Legnica†", "7 dB", "0", "55 55 60 A2 26 BC 48 0B C4 ED 85 5F", "55 55 60 A2 26 BC 48 0B C4 ED 85 5F"),
    ("2026-09-27 08:27:51", "08:27:54", -2443, "0.557", "Wrocław IC-705", "3 dB", "3", "55 55 60 A2 26 B8 1A 0B 94 13 79 D2", "55 55 60 A2 26 B8 1A 0B 94 93 79 D2"),
    ("2026-09-27 08:51:42", "08:51:45", -2434, "0.566", "Wrocław IC-705", "4 dB", "1", "55 55 60 A2 26 BB 08 8B 6F 4C 64 9B", "55 55 60 A2 26 BB 08 8B 6F 4C 64 9B"),
    ("2026-09-27 11:46:00", "11:46:03", -2408, "0.592", "Wrocław IC-705", "15 dB", "0", "55 55 60 A2 26 A0 59 8B 57 5A 5B CC", "55 55 60 A2 26 A0 59 8B 57 5A 5B CC"),
    ("2026-09-27 12:07:48", "12:07:51", -2451, "0.549", "Wrocław IC-705", "16 dB", "0", "55 55 60 A2 26 A0 A3 8B 63 CD D4 5A", "55 55 60 A2 26 A0 A3 8B 63 CD D4 5A"),
    ("2026-09-27 13:04:27", "13:04:30", -2404, "0.596", "Wrocław IC-705", "16 dB", "0", "55 55 60 A2 26 AD 68 0B 27 FA 66 38", "55 55 60 A2 26 AD 68 0B 27 FA 66 38"),
    ("2026-09-27 13:17:00", "13:17:03", -2469, "0.531", "Wrocław IC-705 (Legnica‡)", "16 dB", "0", "55 55 60 A2 26 AD EF 8B 70 E1 CC 6C", "55 55 60 A2 26 AD EF 8B 70 E1 CC 6C"),
    ("2026-09-27 13:18:24", "13:18:27", -2431, "0.569", "Wrocław IC-705 (Legnica‡)", "14 dB", "0", "55 55 60 A2 26 AD FD 8B D5 6E 00 11", "55 55 60 A2 26 AD FD 8B D5 6E 00 11"),
    ("2026-09-27 13:19:48", "13:19:51", -2434, "0.566", "Wrocław IC-705", "17 dB", "0", "55 55 60 A2 26 AD F3 8B B6 FB 44 C7", "55 55 60 A2 26 AD F3 8B B6 FB 44 C7"),
    ("2026-09-27 13:21:39", "13:21:42", -2478, "0.522", "Wrocław IC-705", "15 dB", "0", "55 55 60 A2 26 AD 9C 0B 70 F8 AD 78", "55 55 60 A2 26 AD 9C 0B 70 F8 AD 78"),
    ("2026-09-27 16:19:12", "16:19:15", -2472, "0.528", "Wrocław IC-705 + NUCLEO", "9 dB", "1", "55 55 60 A2 26 AA 8D 8B C2 0F FC A5", "55 55 60 A2 26 AA 8D 8B C2 0F FC A5"),
    ("2026-09-27 18:30:15", "18:30:18", -2435, "0.565", "Wrocław IC-705 + NUCLEO", "11 dB", "0", "55 55 60 A2 25 51 92 0B D2 5E 0C 02", "55 55 60 A2 25 51 92 0B D2 5E 0C 02"),
    ("2026-09-27 18:41:00", "18:41:03", -2416, "0.584", "Wrocław IC-705 + NUCLEO", "12 dB", "0", "55 55 60 A2 25 50 07 8B F6 01 51 40", "55 55 60 A2 25 50 07 8B F6 01 51 40"),
    # v1.3: events 21-30 (tools/gum_events_21_30.py, events_21_30.json)
    ("2026-09-27 23:20:00", "23:20:03", -2417, "0.583", "Wrocław IC-705 (NUCLEO‡, Legnica‡)", "12 dB", "0", "55 55 60 A2 25 45 7D 8B BB A9 E4 02", "55 55 60 A2 25 45 7D 8B BB A9 E4 02"),
    ("2026-09-27 23:20:36", "23:20:39", -2439, "0.561", "Wrocław IC-705 + NUCLEO", "12 dB", "0", "55 55 60 A2 25 45 7B 8B 26 FD 0A 7C", "55 55 60 A2 25 45 7B 8B 26 FD 0A 7C"),
    ("2026-09-28 00:13:57", "00:14:00", -2444, "0.556", "Wrocław IC-705 + NUCLEO", "14 dB", "0", "55 55 60 A2 05 47 00 0B 69 9D E3 03", "55 55 60 A2 25 47 01 0B 69 9D E3 03"),
    ("2026-09-28 03:06:39", "03:06:42", -2460, "0.540", "Wrocław NUCLEO", "5 dB", "1", "–", "55 55 60 A2 25 4C 46 0B 52 32 BE DF"),
    ("2026-09-28 03:21:54", "03:21:57", -2440, "0.560", "Wrocław NUCLEO", "6 dB", "0", "–", "55 55 60 A2 25 4C EE 8B 3C ED F2 E6"),
    ("2026-09-28 06:40:15", "06:40:18", -2454, "0.546", "Wrocław IC-705 + NUCLEO (Legnica‡)", "13 dB", "0", "55 55 60 A2 25 74 2E 0B E7 F6 D3 32", "55 55 60 A2 25 74 2E 0B E7 F6 D3 32"),
    ("2026-09-28 08:32:36", "08:32:39", -2481, "0.519", "Wrocław IC-705 + NUCLEO", "12 dB", "0", "55 55 60 A2 25 70 8B 8B A3 84 17 49", "55 55 60 A2 25 70 8B 8B A3 94 97 49"),
    ("2026-09-28 08:34:00", "08:34:03", -2444, "0.556", "Wrocław IC-705 + NUCLEO", "15 dB", "0", "55 55 60 A2 25 70 99 8B 06 1B 5B 34", "55 55 60 A2 25 70 99 8B 06 1B 5B 34"),
    ("2026-09-28 11:02:00", "11:02:03", -2460, "0.540", "KiwiSDR Řevnice CZ (NUCLEO‡)", "16 dB", "0", "55 55 60 A2 25 7E D1 8B BB FF 8E EB", "55 55 60 A2 25 7E D1 8B BB FF 8E EB"),
    ("2026-09-28 11:11:09", "11:11:12", -2440, "0.560", "KiwiSDR Řevnice CZ (Legnica‡)", "16 dB", "0", "55 55 60 A2 25 7E B5 0B D7 D2 F7 C3", "55 55 60 A2 25 7E B5 0B D7 D2 F7 C3"),
]
N = {"23:47:15": 281231745, "04:44:39": 281237693, "07:12:03": 281240641, "11:12:24": 281245448,
     "11:35:30": 281245910, "15:49:51": 281250997, "00:03:00": 281260860, "06:37:18": 281268746, "08:27:54": 281270958,
     "08:51:45": 281271435, "11:46:03": 281274921, "12:07:51": 281275357, "13:04:30": 281276490, "13:17:03": 281276741, "13:18:27": 281276769, "13:19:51": 281276797, "13:21:42": 281276834, "16:19:15": 281280385, "18:30:18": 281283006, "18:41:03": 281283221,
     "23:20:03": 281288801, "23:20:39": 281288813, "00:14:00": 281289880, "03:06:42": 281293334, "03:21:57": 281293639,
     "06:40:18": 281297606, "08:32:39": 281299853, "08:34:03": 281299881, "11:02:03": 281302841, "11:11:12": 281303024}


def ev_table(lang):
    if lang == "pl":
        hdr = ["#", "Slot S (początek, UTC)", "Czas w ramce (UTC)", "N", "Odchyłka [ms]", "Start ramki po początku slotu S [s]", "Odbiornik", "SNR", "Popr. RS"]
    else:
        hdr = ["#", "Slot S (start, UTC)", "Time in frame (UTC)", "N", "Offset [ms]", "Frame start after start of slot S [s]", "Receiver", "SNR", "RS fixes"]
    rows = [[Paragraph(h, st["cellb"]) for h in hdr]]
    for i, e in enumerate(EV, 1):
        rows.append([Paragraph(x, st["cell"]) for x in
                     [str(i), e[0], e[1], str(N[e[1]]), f"{e[2]:+d}".replace("-", "−"), e[3], e[4], e[5], e[6]]])
    t = Table(rows, colWidths=[8*mm, 28*mm, 18*mm, 22*mm, 15*mm, 23*mm, 24*mm, 14*mm, 13*mm], repeatRows=1)
    t.setStyle(TableStyle([
        ("BACKGROUND", (0, 0), (-1, 0), colors.HexColor("#dde6f0")),
        ("GRID", (0, 0), (-1, -1), 0.3, colors.HexColor("#999")),
        ("VALIGN", (0, 0), (-1, -1), "MIDDLE"),
        ("ROWBACKGROUNDS", (0, 1), (-1, -1), [colors.white, colors.HexColor("#f5f7fa")]),
    ]))
    return t


def raw_table(lang):
    if lang == "pl":
        hdr = ["#", "Odebrane bajty (przed korekcją)", "Poprawna ramka dla czasu N (obliczona)"]
    else:
        hdr = ["#", "Received bytes (before correction)", "Correct frame for time N (computed)"]
    rows = [[Paragraph(h, st["cellb"]) for h in hdr]]
    for i, e in enumerate(EV, 1):
        rows.append([Paragraph(str(i), st["cell"]), Paragraph(e[7], st["mono"]), Paragraph(e[8], st["mono"])])
    t = Table(rows, colWidths=[8*mm, 77*mm, 77*mm], repeatRows=1)
    t.setStyle(TableStyle([
        ("BACKGROUND", (0, 0), (-1, 0), colors.HexColor("#dde6f0")),
        ("GRID", (0, 0), (-1, -1), 0.3, colors.HexColor("#999")),
        ("VALIGN", (0, 0), (-1, -1), "MIDDLE"),
    ]))
    return t


def slot_table(lang):
    data = [(":00", 22, 24, 54), (":03", 100, 0, 0), (":06", 0, 30, 70), (":09", 8, 58, 34),
            (":30", 24, 26, 49), (":33", 0, 100, 0), (":39", 31, 38, 32), (":57", 18, 16, 66)]
    hdr = (["Sekunda slotu", "Ramka czasu", "Inny komunikat", "Brak modulacji"] if lang == "pl"
           else ["Slot second", "Time frame", "Other message", "No modulation"])
    rows = [[Paragraph(h, st["cellb"]) for h in hdr]] + [[Paragraph(x, st["cell"]) for x in
            [s, f"{a} %", f"{b} %", f"{c} %"]] for s, a, b, c in data]
    t = Table(rows, colWidths=[24*mm, 24*mm, 26*mm, 26*mm])
    t.setStyle(TableStyle([
        ("BACKGROUND", (0, 0), (-1, 0), colors.HexColor("#dde6f0")),
        ("GRID", (0, 0), (-1, -1), 0.3, colors.HexColor("#999")),
        ("BACKGROUND", (0, 2), (-1, 2), colors.HexColor("#e8f4e8")),
    ]))
    return t


def fig(path, w=172):
    im = Image(path)
    r = im.imageHeight / im.imageWidth
    im.drawWidth = w * mm; im.drawHeight = w * mm * r
    return im


def summary_box(text):
    t = Table([[Paragraph(text, st["box"])]], colWidths=[172*mm])
    t.setStyle(TableStyle([("BOX", (0, 0), (-1, -1), 0.8, BLUE),
                           ("BACKGROUND", (0, 0), (-1, -1), colors.HexColor("#f2f6fa")),
                           ("LEFTPADDING", (0, 0), (-1, -1), 7), ("RIGHTPADDING", (0, 0), (-1, -1), 7),
                           ("TOPPADDING", (0, 0), (-1, -1), 5), ("BOTTOMPADDING", (0, 0), (-1, -1), 6)]))
    return t


s = []
# ================================================================ title page
s += [P("Ramki czasu e-CzasPL Radio nadawane przedwcześnie o ok. 2,4 s", "title"),
      P("Time frames of e-CzasPL Radio transmitted about 2.4 s early", "title"),
      P("Obserwacje z czterech niezależnych odbiorników 225 kHz w trzech miejscach, 25–28 września 2026 r.<br/>"
        "Observations with four independent 225 kHz receivers at three locations, 25–28 September 2026", "sub"),
      P("Dawid SQ6EMM · 28.09.2026 · wersja 1.3 (przypadki 21–30, w tym odbiornik KiwiSDR w Czechach / "
        "events 21–30 added, including a KiwiSDR receiver in the Czech Republic)", "sub"), Spacer(1, 6)]
s.append(summary_box(
    "<b>Streszczenie.</b> Zaobserwowaliśmy 30 przypadków (25–28.09), w których poprawna ramka czasu (CRC i Reed-Solomon "
    "zgodne) przeznaczona dla slotu S+3 została nadana w poprzednim slocie S, z opóźnieniem 0,52–0,60 s "
    "względem początku slotu S – czyli o 2,40–2,48 s za wcześnie. Właściwy slot S+3 pozostawał pusty. "
    "Zjawisko zarejestrowały cztery niezależne odbiorniki w trzech miejscach (Legnica; we Wrocławiu, 65 km "
    "dalej: ICOM IC-705 oraz odbiornik z bezpośrednim próbkowaniem na STM32; publiczny odbiornik KiwiSDR "
    "w Řevnicach w Czechach, 239 km od Wrocławia, z czasem GPS – różny sprzęt, anteny i demodulatory), "
    "w 9 przypadkach dwa jednocześnie – jest to więc cecha nadawanego sygnału, a nie odbiornika ani "
    "miejsca odbioru. Częstość we Wrocławiu (IC-705): 20 na 8 907 ramek czasu (ok. 1 na 450); "
    "w Řevnicach 2 na 354 ramki czasu w ciągu 80 minut.<br/><br/>"
    "<b>Summary.</b> We observed 30 cases (25–28 Sep) in which a valid time frame (CRC and Reed-Solomon consistent) "
    "belonging to slot S+3 was transmitted in the preceding slot S, starting 0.52–0.60 s after the "
    "beginning of slot S – i.e. 2.40–2.48 s early. The proper slot S+3 stayed empty. The effect was "
    "recorded by four independent receivers at three locations (Legnica; in Wrocław, 65 km away, an ICOM "
    "IC-705 and a direct-sampling STM32 receiver; a public KiwiSDR receiver in Řevnice, Czech Republic, "
    "239 km from Wrocław, with GPS time – different hardware, antennas and demodulators), in 9 cases by "
    "two at the same time, so it is a property of the transmitted signal, not of a receiver or a "
    "receiving site. Rate in Wrocław (IC-705): 20 in 8,907 time frames (about 1 in 450); in Řevnice 2 in "
    "354 time frames within 80 minutes."))
s.append(Spacer(1, 8))
s.append(P("Polski: strony 1–6 · English: pages 7–10 · Rysunki / figures 1–3 w części polskiej, "
           "referenced from the English part.", "cap"))

# ================================================================ POLISH
s += [P("1. Opis zjawiska", "h1"),
      P("System e-CzasPL Radio nadaje komunikaty w slotach 3-sekundowych; każda ramka rozpoczyna się na "
        "początku swojego slotu (pełnej sekundy), a zawarty w niej czas (liczba N okresów 3-sekundowych od "
        "1.01.2000) jest czasem początku tego slotu. W obserwowanych przypadkach ramka z czasem slotu S+3 "
        "zaczynała się 0,52–0,60 s po początku slotu S. Przed nią, w slocie S, nie było żadnej innej "
        "transmisji (faza nośnej niemodulowana: 4,3° wobec 4,6° dla pustego slotu), a w slocie S+3 nie "
        "nadano już żadnej ramki. Treść ramek jest w pełni poprawna: preambuła 0x5555 0x60, znacznik 101, "
        "strefa czasowa UTC+2, flagi zerowe, zgodne CRC-8 i kod RS(15,9)."),
      P("2. Stanowiska odbiorcze", "h1"),
      LI("<b>Legnica</b> – uproszczony odbiornik e-CzasPL (SI4735 w trybie USB 224 kHz, dsPIC33F) z nowym "
         "oprogramowaniem dekodującym (miękkie decyzje, RS, CRC-8). Zegar odbiornika jest utrzymywany "
         "na podstawie poprawnych ramek (średni błąd dopasowania 0,3–0,5 ms); odchyłka ramki jest mierzona "
         "względem tego zegara. Obserwacja ciągła od 25.09 17:45 UTC."),
      LI("<b>Wrocław, IC-705</b> – ICOM IC-705, USB 224,000 kHz, zapis audio 8 kHz (komputer "
         "synchronizowany NTP); antena ferrytowa, od 27.09 ok. 14:33 UTC ramowa 12×12 cm. Nagrania "
         "analizowane tym samym algorytmem dekodowania oraz niezależnym prostym demodulatorem fazy. "
         "Obserwacja 26.09 22:11 – 28.09 09:27 UTC (41 942 sloty, 8 907 ramek czasu; przerwy 27.09 "
         "11:01–11:17 UTC przy przeniesieniu rejestratora i 28.09 01:05–04:51 UTC – rozładowany akumulator)."),
      LI("<b>Wrocław, NUCLEO</b> – odbiornik z bezpośrednim próbkowaniem (STM32H723, przetwornik ADC "
         "1 MS/s, antena ferrytowa strojona na ok. 217 kHz, podstawa czasu z kwarcu 32,768 kHz), z tym "
         "samym algorytmem dekodowania co w Legnicy. Niezależny od IC-705 (własna antena, tor i zegar). "
         "Od 27.09 14:47 UTC; od 27.09 ok. 23 UTC odbiornik koherentny (mieszacz synchronizowany z "
         "kwarcem 32,768 kHz), dekoder ramek i zegar bez zmian."),
      LI("<b>Řevnice (Czechy), KiwiSDR</b> – publiczny odbiornik „Central Czechia SDR” (KiwiSDR, antena "
         "ramowa MLA-30+), 453 km od nadajnika w Solcu Kujawskim, 239 km od Wrocławia. Zapis IQ 225 kHz "
         "±3 kHz, 12 kHz; każdy blok 512 próbek ma znacznik czasu GPS odbiornika, więc oś czasu nagrania "
         "jest czasem GPS. Nagrania analizowane tym samym algorytmem co nagrania IC-705. Od 28.09 10:22 UTC."),
      P("3. Zaobserwowane przypadki", "h1"),
      ev_table("pl"),
      P("Odchyłka = czas początku ramki minus początek slotu, którego czas ramka zawiera (S+3). "
        "* dekodowanie z poprawką miękką (Chase). † w Legnicy ramka została wykryta ok. 0,5 s później niż "
        "zwykłe ramki, ale przy SNR 1 dB nie dała się zdekodować. ‡ niezdekodowane wykrycie ramki "
        "w miejscu przedwczesnej ramki (słaba przesłanka – podobne wykrycia zdarzają się też bez niej). "
        "Przypadki 1–6 zdekodowane w Legnicy, 7–23 i 26–28 we Wrocławiu przez IC-705; NUCLEO: 18–20, 22, 23, "
        "26–28 jednocześnie z IC-705 (odchyłki −2479, −2440, −2420, −2440, −2439, −2459, −2480, −2439 ms), "
        "24–25 sam (IC-705 wyłączony); 29–30 zdekodowane w Řevnicach (KiwiSDR). Od 26.09 16 UTC odbiór "
        "w Legnicy jest słaby (lokalne zakłócenia).", "cap"),
      P("4. Przykładowe dane", "h1"),
      P("Tabela 2 zawiera bajty odebrane (przed korekcją błędów) i poprawną ramkę obliczoną dla tego samego "
        "N. Różnice wynikają wyłącznie z błędów transmisji przy niskim SNR i są usuwane przez kod RS – "
        "ramki są prawidłowymi ramkami czasu."),
      raw_table("pl"), P("Tabela 2 / Table 2. Bajty 1–12 ramki (szesnastkowo) / frame bytes 1–12 (hex). "
                         "10–23, 26–28: prosty demodulator fazy na nagraniach IC-705 (23 i 27: po 2 błędy bitowe, "
                         "pozostałe bez błędów); 29–30: to samo na nagraniach KiwiSDR, bez błędów; 24–25: tylko "
                         "NUCLEO, odebrane bajty nie są zapisywane / simple phase demodulator on the IC-705 "
                         "recordings (23 and 27: 2 bit errors each, the others none); 29–30: the same on the "
                         "KiwiSDR recordings, no bit errors; 24–25: NUCLEO only, received bytes not logged.", "cap"),
      KeepTogether([fig("/o/ev1_timeline.png"),
                    P("Rys. 1 / Fig. 1. Faza nośnej (IC-705, Wrocław), 27.09.2026 00:02:48–00:03:12 UTC. "
                      "Zwykłe ramki zaczynają się na granicach slotów (:48, :51, :03, :09). W slocie :57 "
                      "przez 0,524 s nie ma modulacji, po czym nadana zostaje ramka z czasem 00:03:00; "
                      "slot :00 jest pusty. / Carrier phase; regular frames start at slot boundaries, the "
                      "frame for 00:03:00 starts 0.524 s into slot :57, slot :00 is empty.", "cap")]),
      KeepTogether([fig("/o/ev1_zoom.png"),
                    P("Rys. 2 / Fig. 2. Powiększenie: odebrana faza i idealna ramka dla N = 281260860 "
                      "(27.09.2026 00:03:00 UTC). / Zoom: received phase and the ideal frame for "
                      "N = 281260860.", "cap")]),
      KeepTogether([fig("/o/ev2_timeline.png"),
                    P("Rys. 3 / Fig. 3. Przypadek 8, 27.09.2026 06:37:06–06:37:30 UTC: ramka z czasem "
                      "06:37:18 zaczyna się 0,599 s po początku slotu :15; slot :18 jest pusty. / Event 8: "
                      "the frame for 06:37:18 starts 0.599 s into slot :15; slot :18 is empty.", "cap")]),
      P("5. Charakterystyka", "h1"),
      LI("Treść: zawsze poprawna ramka czasu slotu S+3 (nie powtórzenie ani uszkodzenie ramki slotu S)."),
      LI("Położenie: start 0,52–0,60 s po początku slotu S (rozrzut ok. 80 ms, wszystkie 30 przypadków), podczas gdy zwykłe ramki "
         "zaczynają się na granicy slotu z rozrzutem poniżej kilku milisekund."),
      LI("Slot S+3 pozostaje pusty – ramka jest przesunięta, a nie zdublowana. Slot S przed ramką jest pusty."),
      LI("Częstość: we Wrocławiu (IC-705) 20 przypadków na 8 907 ramek czasu (ok. 1 na 450): w nocy 26/27.09 "
         "3 na 3 195, 27.09 od 9 UTC 11 na 3 098 (ok. 1 na 280), 27.09 20:17 – 28.09 09:27 6 na 2 616. "
         "W Legnicy 6 zdekodowanych w ciągu ok. 16 godzin, w Řevnicach 2 na 354 w ciągu 80 minut. "
         "Występują o różnych porach doby, także seriami: 13:17, 13:18, 13:19 i 13:21 UTC 27.09; "
         "23:20:00 i 23:20:36 UTC 27.09; 08:32:36 i 08:34:00 UTC 28.09."),
      LI("Ten sam przebieg w odległym miejscu: w Řevnicach (KiwiSDR, oś czasu GPS) ramki 29 i 30 odebrano bez "
         "błędów bitowych; zaczynały się 0,555 i 0,575 s po sekundzie GPS rozpoczynającej slot S "
         "(z propagacją ok. 1,5 ms), a slot S+3 był pusty."),
      P("6. Skutki dla odbiorników", "h1"),
      P("Odbiornik, który przyjmuje czas z pojedynczej poprawnej ramki i odnosi go do chwili jej odbioru, "
        "ustawi zegar o ok. 2,4–2,5 s za wcześnie (lub z błędem o jeden slot). Ramka ma poprawne CRC i RS, "
        "więc sama kontrola integralności jej nie odrzuci. Nasz odbiornik odrzuca takie ramki, ponieważ "
        "wymaga zgodności z bieżącym zegarem (±100 ms) i potwierdzenia przez kolejne ramki."),
      P("7. Możliwa przyczyna (hipoteza)", "h1"),
      P("Obserwacje są zgodne z sytuacją, w której ramka przeznaczona dla slotu S+3 zostaje zwolniona do "
        "nadania, gdy slot S jest wolny, i wysłana z opóźnieniem ok. 0,5–0,6 s zamiast na początku slotu "
        "S+3. Nie mamy wglądu w układ nadawczy – jest to wyłącznie hipoteza."),
      P("8. Dodatkowa obserwacja: wykorzystanie slotów", "h1"),
      P("W 330 w pełni sklasyfikowanych minutach (26/27.09, 22:11–06:00 UTC) ramkę czasu zawierało 24 % "
        "slotów (średnio 4,8 na minutę, ok. 290 na godzinę). Jedynie slot rozpoczynający się w sekundzie "
        ":03 zawierał ramkę czasu w każdej minucie. Slot :00, opisany w dokumentacji jako „Komunikat "
        "zegarowy 0”, zawierał ją tylko w 22 % minut; slot :33 zawsze zawierał inny komunikat."),
      slot_table("pl"), P("Tabela 3 / Table 3. Wybrane pozycje slotów / selected slot positions.", "cap"),
      P("9. Dostępne dane", "h1"),
      P("Wszystkie dane źródłowe raportu znajdują się w repozytorium:<br/>"
        "<b>github.com/sq6emm/eCzasPL-TimeReceiver225kHz</b>, katalog <b>doc/GUM-issue-1</b>:", "pl"),
      LI("recordings/ – nagrania IC-705 (WAV 8 kHz mono, USB 224,000 kHz): przypadki 7, 8, 9, 18, 19, 20 "
         "(po 120 s od 00:02:00, 06:36:30, 08:27:00, 16:18:30, 18:29:30, 18:40:30 UTC) oraz 14–17 "
         "(360 s od 13:16:30 UTC), 21–22 (180 s od 23:19:30), 23 (120 s od 00:13:27), 26 (120 s od "
         "06:39:45), 27–28 (180 s od 08:32:06); nagrania KiwiSDR z Řevnic przetworzone do tej samej postaci, "
         "oś czasu GPS: 29 (120 s od 11:01:30,000), 30 (120 s od 11:10:39,000);"),
      LI("legnica-log/ – fragmenty dziennika odbiornika z Legnicy dla wszystkich 30 przypadków "
         "(linie FRAME, CLOCK, RAW, SIGNAL); nucleo-log/ – dziennik NUCLEO dla przypadków 18–30;"),
      LI("kiwi-revnice/ – to samo dla odbiornika KiwiSDR w Řevnicach, 28.09 10:20–11:40 UTC (1 592 sloty);"),
      LI("ic705-wroclaw/ – klasyfikacja wszystkich 41 942 slotów 26.09 22:11 – 28.09 09:27 (slots.csv) "
         "oraz wszystkie zdekodowane ramki z odchyłką czasu (frames.csv);"),
      LI("figures/, tools/ – rysunki i skrypty, którymi je wykonano."),
      P("Pełne nagrania nocy 26/27.09 (ok. 470 MB) udostępnimy na życzenie.", "pl"),
      PageBreak()]

# ================================================================ ENGLISH
s += [P("1. Description", "h1"),
      P("e-CzasPL Radio transmits messages in 3-second slots; each frame starts at the beginning of its "
        "slot (a full second), and the time it carries (N, the number of 3-second periods since "
        "2000-01-01) is the start of that slot. In the observed cases a frame carrying the time of slot "
        "S+3 started 0.52–0.60 s after the beginning of slot S. Before it, slot S carried no other "
        "transmission (unmodulated carrier phase: 4.3° vs 4.6° in an empty slot), and slot S+3 then "
        "carried no frame. The frame content is fully valid: preamble 0x5555 0x60, marker 101, time zone "
        "UTC+2, zero flags, consistent CRC-8 and RS(15,9)."),
      P("2. Receiving stations", "h1"),
      LI("<b>Legnica</b> – the simplified e-CzasPL receiver (SI4735 in USB at 224 kHz, dsPIC33F) with new "
         "decoding firmware (soft decisions, RS, CRC-8). Its clock is kept by valid frames (mean fit error "
         "0.3–0.5 ms); frame offsets are measured against that clock. Continuous since 25.09 17:45 UTC."),
      LI("<b>Wrocław, IC-705</b> – ICOM IC-705, USB 224.000 kHz, 8 kHz audio recording (NTP-synchronised "
         "computer); ferrite rod, from about 14:33 UTC on 27.09 a 12×12 cm frame loop. Recordings analysed "
         "with the same decoding algorithm and with an independent simple phase demodulator. 26.09 22:11 – "
         "28.09 09:27 UTC (41,942 slots, 8,907 time frames; gaps 11:01–11:17 UTC on 27.09 when the recorder "
         "moved and 01:05–04:51 UTC on 28.09, flat battery)."),
      LI("<b>Wrocław, NUCLEO</b> – a direct-sampling receiver (STM32H723, 1 MS/s ADC, ferrite rod tuned "
         "to about 217 kHz, time base from a 32.768 kHz crystal) running the same decoding algorithm as "
         "Legnica. Independent of the IC-705 (own antenna, front end and clock). From 27.09 14:47 UTC; "
         "from about 23 UTC on 27.09 a carrier-coherent receiver (mixer locked to the 32.768 kHz crystal), "
         "frame decoder and clock unchanged."),
      LI("<b>Řevnice (Czech Republic), KiwiSDR</b> – the public receiver “Central Czechia SDR” (KiwiSDR, "
         "MLA-30+ loop), 453 km from the transmitter in Solec Kujawski, 239 km from Wrocław. IQ recording "
         "225 kHz ±3 kHz at 12 kHz; every block of 512 samples carries the receiver's GPS time, so the "
         "recording's time axis is GPS time. Analysed with the same algorithm as the IC-705 recordings. "
         "From 28.09 10:22 UTC."),
      P("3. Observed events", "h1"),
      ev_table("en"),
      P("Offset = frame start minus the start of the slot whose time the frame carries (S+3). "
        "* decoded with a soft retry (Chase). † in Legnica the frame was detected about 0.5 s later than "
        "regular frames but could not be decoded at 1 dB SNR. ‡ an undecodable detection at the position "
        "of the early frame (weak evidence – such detections also occur without one). Events 1–6 were "
        "decoded in Legnica, 7–23 and 26–28 in Wrocław by the IC-705; the NUCLEO decoded 18–20, 22, 23 and "
        "26–28 together with the IC-705 (offsets −2479, −2440, −2420, −2440, −2439, −2459, −2480, −2439 ms) "
        "and 24–25 alone (IC-705 off); 29–30 were decoded in Řevnice (KiwiSDR). Since 26.09 16 UTC "
        "reception in Legnica is poor (local interference).", "cap"),
      P("4. Example data", "h1"),
      P("Table 2 lists the received bytes (before error correction) and the correct "
        "frame computed for the same N. The differences are transmission errors at low SNR, removed by "
        "the RS code – the frames are proper time frames. Figures 1–3 (pages 4–5) show the carrier phase "
        "around events 7 and 8."),
      raw_table("en"),
      P("5. Characteristics", "h1"),
      LI("Content: always a valid time frame of slot S+3 (not a repeated or corrupted frame of slot S)."),
      LI("Position: starts 0.52–0.60 s after the start of slot S (spread about 80 ms, all 30 events), while regular "
         "frames start at the slot boundary with a spread below a few milliseconds."),
      LI("Slot S+3 stays empty – the frame is moved, not duplicated. Slot S is empty before the frame."),
      LI("Rate: in Wrocław (IC-705) 20 events in 8,907 time frames (about 1 in 450): 3 in 3,195 in the night "
         "26/27.09, 11 in 3,098 from 09 UTC on 27.09 (about 1 in 280), 6 in 2,616 from 27.09 20:17 to "
         "28.09 09:27. 6 decoded in Legnica within about 16 hours, 2 in 354 in Řevnice within 80 minutes. "
         "They occur at different times of day, also in bursts: 13:17, 13:18, 13:19 and 13:21 UTC on 27.09; "
         "23:20:00 and 23:20:36 UTC on 27.09; 08:32:36 and 08:34:00 UTC on 28.09."),
      LI("The same at a distant site: in Řevnice (KiwiSDR, GPS time axis) events 29 and 30 were received "
         "without bit errors; they started 0.555 and 0.575 s after the GPS second starting slot S "
         "(including about 1.5 ms propagation), and slot S+3 stayed empty."),
      P("6. Impact on receivers", "h1"),
      P("A receiver that takes the time from a single valid frame and refers it to the moment of "
        "reception sets its clock about 2.4–2.5 s early (or one slot off). The frame has a valid CRC and "
        "RS, so integrity checks alone do not reject it. Our receiver rejects such frames because it "
        "requires agreement with its running clock (±100 ms) and confirmation by further frames."),
      P("7. Possible cause (hypothesis)", "h1"),
      P("The observations are consistent with the frame intended for slot S+3 being released for "
        "transmission while slot S is free and sent with a latency of about 0.5–0.6 s instead of at the "
        "start of slot S+3. We have no insight into the transmitting system – this is only a hypothesis."),
      P("8. Additional observation: slot usage", "h1"),
      P("In 330 fully classified minutes (26/27.09, 22:11–06:00 UTC) 24 % of the slots carried a time "
        "frame (4.8 per minute on average, about 290 per hour). Only the slot starting at second :03 "
        "carried a time frame in every minute. Slot :00, described in the documentation as the clock "
        "message (“Komunikat zegarowy 0”), carried one in only 22 % of the minutes; slot :33 always "
        "carried another message (Table 3)."),
      slot_table("en"),
      P("9. Available data", "h1"),
      P("All source data of this report is in the repository<br/>"
        "<b>github.com/sq6emm/eCzasPL-TimeReceiver225kHz</b>, folder <b>doc/GUM-issue-1</b>:", "pl"),
      LI("recordings/ – IC-705 recordings (8 kHz mono WAV, USB 224.000 kHz): events 7, 8, 9, 18, 19, 20 "
         "(120 s each from 00:02:00, 06:36:30, 08:27:00, 16:18:30, 18:29:30, 18:40:30 UTC) and 14–17 "
         "(360 s from 13:16:30 UTC), 21–22 (180 s from 23:19:30), 23 (120 s from 00:13:27), 26 (120 s from "
         "06:39:45), 27–28 (180 s from 08:32:06); KiwiSDR recordings from Řevnice converted to the same "
         "format, GPS time axis: 29 (120 s from 11:01:30.000), 30 (120 s from 11:10:39.000);"),
      LI("legnica-log/ – Legnica receiver log excerpts for all 30 events (FRAME, CLOCK, RAW, SIGNAL lines); "
         "nucleo-log/ – NUCLEO log for events 18–30;"),
      LI("kiwi-revnice/ – the same for the KiwiSDR receiver in Řevnice, 28.09 10:20–11:40 UTC (1,592 slots);"),
      LI("ic705-wroclaw/ – classification of all 41,942 slots 26.09 22:11 – 28.09 09:27 (slots.csv) and "
         "all decoded frames with their time offset (frames.csv);"),
      LI("figures/, tools/ – the figures and the scripts that made them."),
      P("The full recordings of the night of 26/27.09 (about 470 MB) are available on request.", "pl"),
      Spacer(1, 10),
      P("Kontakt / contact: Dawid SQ6EMM", "sub")]


def footer(c, d):
    c.saveState(); c.setFont("DV", 7.5); c.setFillColor(colors.HexColor("#777"))
    c.drawString(19*mm, 10*mm, "e-CzasPL Radio 225 kHz – przedwczesne ramki / early frames – SQ6EMM, 28.09.2026 (v1.3)")
    c.drawRightString(A4[0]-19*mm, 10*mm, str(d.page)); c.restoreState()


doc = SimpleDocTemplate("/o/eCzasPL_early_frames_2026-09-27.pdf", pagesize=A4, leftMargin=19*mm,
                        rightMargin=19*mm, topMargin=16*mm, bottomMargin=17*mm,
                        title="e-CzasPL Radio: early time frames / przedwczesne ramki czasu",
                        author="Dawid SQ6EMM", subject="Observation report for GUM")
doc.build(s, onFirstPage=footer, onLaterPages=footer)
print("ok")
