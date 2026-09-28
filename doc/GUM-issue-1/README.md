# GUM issue 1 – ramki czasu nadawane ok. 2,4 s za wcześnie / time frames sent about 2.4 s early

Raport dla GUM (Główny Urząd Miar) wraz z danymi źródłowymi.
Report for GUM (Central Office of Measures) with its source data.

**Raport / report:** [`eCzasPL_early_frames_2026-09-27.pdf`](eCzasPL_early_frames_2026-09-27.pdf)
(PL str. 1–6, EN pages 7–10), wersja / version 1.3

## W skrócie / in short

30 razy (25–28.09.2026) poprawna ramka czasu slotu S+3 została nadana w slocie S,
0,52–0,60 s po jego początku, czyli 2,40–2,48 s za wcześnie; slot S+3 był pusty.
Zarejestrowały to cztery niezależne odbiorniki w trzech miejscach (Legnica; Wrocław: IC-705
i NUCLEO/STM32; **nowe w wersji 1.3: publiczny odbiornik KiwiSDR w Řevnicach w Czechach**,
239 km od Wrocławia, z czasem GPS – zdarzenia 29 i 30 odebrane tam bez błędów bitowych),
9 zdarzeń po dwa jednocześnie. We Wrocławiu (IC-705): 20 na 8 907 ramek czasu (ok. 1 na 450).

On 30 occasions (25–28 Sep 2026) a valid time frame of slot S+3 was transmitted in
slot S, 0.52–0.60 s after its start, i.e. 2.40–2.48 s early; slot S+3 stayed empty.
Seen by four independent receivers at three locations (Legnica; Wrocław: IC-705 and
NUCLEO/STM32; **new in version 1.3: a public KiwiSDR receiver in Řevnice, Czech Republic**,
239 km from Wrocław, with GPS time – events 29 and 30 received there without bit errors),
9 events by two at once. In Wrocław (IC-705): 20 in 8,907 time frames (about 1 in 450).

| # | Slot S (UTC) | Czas w ramce / time in frame | Odchyłka / offset | Odbiornik / receiver |
|---|---|---|---|---|
| 1 | 2026-09-25 23:47:12 | 23:47:15 | −2419 ms | Legnica |
| 2 | 2026-09-26 04:44:36 | 04:44:39 | −2460 ms | Legnica |
| 3 | 2026-09-26 07:12:00 | 07:12:03 | −2459 ms | Legnica |
| 4 | 2026-09-26 11:12:21 | 11:12:24 | −2439 ms | Legnica |
| 5 | 2026-09-26 11:35:27 | 11:35:30 | −2480 ms | Legnica |
| 6 | 2026-09-26 15:49:48 | 15:49:51 | −2419 ms | Legnica |
| 7 | 2026-09-27 00:02:57 | 00:03:00 | −2476 ms | Wrocław IC-705 |
| 8 | 2026-09-27 06:37:15 | 06:37:18 | −2401 ms | Wrocław IC-705 + Legnica |
| 9 | 2026-09-27 08:27:51 | 08:27:54 | −2443 ms | Wrocław IC-705 |
| 10 | 2026-09-27 08:51:42 | 08:51:45 | −2434 ms | Wrocław IC-705 |
| 11 | 2026-09-27 11:46:00 | 11:46:03 | −2408 ms | Wrocław IC-705 |
| 12 | 2026-09-27 12:07:48 | 12:07:51 | −2451 ms | Wrocław IC-705 |
| 13 | 2026-09-27 13:04:27 | 13:04:30 | −2404 ms | Wrocław IC-705 |
| 14 | 2026-09-27 13:17:00 | 13:17:03 | −2469 ms | Wrocław IC-705 (Legnica ‡) |
| 15 | 2026-09-27 13:18:24 | 13:18:27 | −2431 ms | Wrocław IC-705 (Legnica ‡) |
| 16 | 2026-09-27 13:19:48 | 13:19:51 | −2434 ms | Wrocław IC-705 |
| 17 | 2026-09-27 13:21:39 | 13:21:42 | −2478 ms | Wrocław IC-705 |
| 18 | 2026-09-27 16:19:12 | 16:19:15 | −2472 ms | Wrocław IC-705 + NUCLEO (−2479 ms) |
| 19 | 2026-09-27 18:30:15 | 18:30:18 | −2435 ms | Wrocław IC-705 + NUCLEO (−2440 ms) |
| 20 | 2026-09-27 18:41:00 | 18:41:03 | −2416 ms | Wrocław IC-705 + NUCLEO (−2420 ms) |
| 21 | 2026-09-27 23:20:00 | 23:20:03 | −2417 ms | Wrocław IC-705 (NUCLEO ‡, Legnica ‡) |
| 22 | 2026-09-27 23:20:36 | 23:20:39 | −2439 ms | Wrocław IC-705 + NUCLEO (−2440 ms) |
| 23 | 2026-09-28 00:13:57 | 00:14:00 | −2444 ms | Wrocław IC-705 + NUCLEO (−2439 ms) |
| 24 | 2026-09-28 03:06:39 | 03:06:42 | −2460 ms | Wrocław NUCLEO (IC-705 wyłączony / off) |
| 25 | 2026-09-28 03:21:54 | 03:21:57 | −2440 ms | Wrocław NUCLEO (IC-705 wyłączony / off) |
| 26 | 2026-09-28 06:40:15 | 06:40:18 | −2454 ms | Wrocław IC-705 + NUCLEO (−2459 ms) (Legnica ‡) |
| 27 | 2026-09-28 08:32:36 | 08:32:39 | −2481 ms | Wrocław IC-705 + NUCLEO (−2480 ms) |
| 28 | 2026-09-28 08:34:00 | 08:34:03 | −2444 ms | Wrocław IC-705 + NUCLEO (−2439 ms) |
| 29 | 2026-09-28 11:02:00 | 11:02:03 | −2460 ms | **KiwiSDR Řevnice CZ** (NUCLEO ‡) |
| 30 | 2026-09-28 11:11:09 | 11:11:12 | −2440 ms | **KiwiSDR Řevnice CZ** (Legnica ‡) |

‡ Niezdekodowane wykrycie w miejscu ramki (słaba przesłanka) / undecodable detection at the frame's position (weak evidence).

## Zawartość / contents

| Ścieżka / path | Opis | Description |
|---|---|---|
| `recordings/ic705_early_frame_20260927_0002.wav` | Zdarzenie 7, 120 s od 00:02:00,000 UTC; wczesna ramka od 57,52 s | Event 7, 120 s from 00:02:00.000 UTC; early frame at 57.52 s |
| `recordings/ic705_early_frame_20260927_0636.wav` | Zdarzenie 8, 120 s od 06:36:30,000 UTC; wczesna ramka od 45,60 s | Event 8, 120 s from 06:36:30.000 UTC; early frame at 45.60 s |
| `recordings/ic705_early_frame_20260927_0827.wav` | Zdarzenie 9, 120 s od 08:27:00,000 UTC; wczesna ramka od 51,56 s | Event 9, 120 s from 08:27:00.000 UTC; early frame at 51.56 s |
| `recordings/ic705_early_frame_20260927_131630.wav` | Zdarzenia 14–17, 360 s od 13:16:30 UTC | Events 14–17, 360 s from 13:16:30 UTC |
| `recordings/ic705_early_frame_20260927_161830.wav` | Zdarzenie 18, 120 s od 16:18:30 UTC | Event 18, 120 s from 16:18:30 UTC |
| `recordings/ic705_early_frame_20260927_182930.wav` | Zdarzenie 19, 120 s od 18:29:30 UTC | Event 19, 120 s from 18:29:30 UTC |
| `recordings/ic705_early_frame_20260927_184030.wav` | Zdarzenie 20, 120 s od 18:40:30 UTC | Event 20, 120 s from 18:40:30 UTC |
| `recordings/ic705_early_frame_20260927_231930.wav` | Zdarzenia 21–22, 180 s od 23:19:30 UTC | Events 21–22, 180 s from 23:19:30 UTC |
| `recordings/ic705_early_frame_20260928_001327.wav` | Zdarzenie 23, 120 s od 00:13:27 UTC | Event 23, 120 s from 00:13:27 UTC |
| `recordings/ic705_early_frame_20260928_063945.wav` | Zdarzenie 26, 120 s od 06:39:45 UTC | Event 26, 120 s from 06:39:45 UTC |
| `recordings/ic705_early_frame_20260928_083206.wav` | Zdarzenia 27–28, 180 s od 08:32:06 UTC | Events 27–28, 180 s from 08:32:06 UTC |
| `recordings/kiwi_czechia_early_frame_20260928_110130.wav` | Zdarzenie 29, KiwiSDR Řevnice, 120 s od 11:01:30,000 UTC (oś czasu GPS) | Event 29, KiwiSDR Řevnice, 120 s from 11:01:30.000 UTC (GPS time axis) |
| `recordings/kiwi_czechia_early_frame_20260928_111039.wav` | Zdarzenie 30, KiwiSDR Řevnice, 120 s od 11:10:39,000 UTC (oś czasu GPS) | Event 30, KiwiSDR Řevnice, 120 s from 11:10:39.000 UTC (GPS time axis) |
| `legnica-log/event*.log` | Dziennik odbiornika z Legnicy, S−30 s … S+45 s, wszystkie 30 zdarzeń (od zdarzenia 9 odbiór zbyt słaby; niezdekodowane wykrycia: 14, 15, 21, 26, 30) | Legnica receiver log, S−30 s … S+45 s, all 30 events (from event 9 on, reception too weak; undecodable detections: 14, 15, 21, 26, 30) |
| `nucleo-log/event*.log` | Dziennik odbiornika NUCLEO (STM32H723, bezpośrednie próbkowanie), zdarzenia 18–30 | NUCLEO receiver log (STM32H723, direct sampling), events 18–30 |
| `ic705-wroclaw/slots.csv` | Każdy slot 3 s, 26.09 22:11 – 28.09 09:27 UTC (41 942 sloty): treść, CRC, wynik dekodera | Every 3-s slot, 26.09 22:11 – 28.09 09:27 UTC (41,942 slots): content, CRC, decoder result |
| `ic705-wroclaw/frames.csv` | Każda zdekodowana ramka i jej odchyłka od własnego slotu | Every decoded frame and its offset from its own slot |
| `kiwi-revnice/slots.csv`, `frames.csv` | To samo dla KiwiSDR w Řevnicach, 28.09 10:20–11:40 UTC (1 592 sloty) | The same for the KiwiSDR in Řevnice, 28.09 10:20–11:40 UTC (1,592 slots) |
| `figures/` | Rysunki z raportu (faza nośnej) | Report figures (carrier phase) |
| `tools/` | Skrypty: klasyfikacja slotów, rysunki, PDF | Scripts: slot classification, figures, PDF |

**Nagrania / recordings:** ICOM IC-705, antena ferrytowa / ferrite rod antenna (od / from 27.09 ~14:33 UTC: ramowa / frame loop 12×12 cm), USB,
224,000 kHz (nośna 225 kHz = ton 1 kHz / carrier = 1 kHz tone), 16 bit mono 8 kHz,
Wrocław. Czas początku plików wyznaczony z dekodowanych ramek (dokładność kilku ms) /
file start times derived from the decoded frames (a few ms accuracy).

**Nagrania KiwiSDR / KiwiSDR recordings:** publiczny odbiornik / public receiver „Central Czechia SDR”,
Řevnice CZ (KiwiSDR, antena / antenna MLA-30+), 453 km od nadajnika / from the transmitter.
Zapis IQ 225 kHz ±3 kHz, 12 kHz, każdy blok 512 próbek ze znacznikiem czasu GPS; przetworzony
(filtry zerofazowe) do tej samej postaci co nagrania IC-705 (nośna = ton 1 kHz, 8 kHz), oś czasu GPS
/ IQ recording with a GPS time stamp on every 512-sample block, converted (zero-phase filters) to
the IC-705 format (carrier = 1 kHz tone, 8 kHz), GPS time axis (`tools/gum_events_21_30.py`).

**Dziennik Legnica / Legnica log:** kolumny 1–2 to czas odbioru linii przez komputer
(NTP) / columns 1–2 are the host receive time (NTP). `FRAME` – ramka i wynik dekodowania,
`CLOCK` – porównanie z zegarem odbiornika (`REJECTED - frame differs from clock by -2419 ms`
to wczesna ramka / is the early frame), `RAW` – odebrane bajty 1–12 przed korekcją /
received bytes 1–12 before correction, `SIGNAL` – poziom, nośna, stan pętli / level,
carrier, loop state.

**slots.csv:** `content` = time (ramka czasu / time frame, preambuła 0x5555 0x60),
other (inny komunikat / other message), idle (brak modulacji / no modulation),
unclear (za słaby sygnał / too weak to tell); `crc` = crc_ok / bit_errors (prosty
demodulator / simple demodulator); `firmware_result` = decoded / confirmed / failed /
none (dekoder oprogramowania odbiornika na nagraniu / the receiver firmware's decoder on
the recording).

Pełne nagrania nocy (ok. 470 MB) – na życzenie. Full-night recordings (about 470 MB) –
on request. Kontakt / contact: Dawid SQ6EMM.
