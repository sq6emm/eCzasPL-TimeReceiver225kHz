# GUM issue 1 – ramki czasu nadawane ok. 2,4 s za wcześnie / time frames sent about 2.4 s early

Raport dla GUM (Główny Urząd Miar) wraz z danymi źródłowymi.
Report for GUM (Central Office of Measures) with its source data.

**Raport / report:** [`eCzasPL_early_frames_2026-09-27.pdf`](eCzasPL_early_frames_2026-09-27.pdf)
(PL str. 1–4, EN pages 5–6), wersja / version 1.1

## W skrócie / in short

9 razy (25–27.09.2026) poprawna ramka czasu slotu S+3 została nadana w slocie S,
0,52–0,60 s po jego początku, czyli 2,40–2,48 s za wcześnie; slot S+3 był pusty.
Zarejestrowały to dwa niezależne odbiorniki (Legnica, Wrocław), zdarzenie 8 oba jednocześnie.
We Wrocławiu: 3 na 3 195 ramek czasu (ok. 1 na 1 070).

On 9 occasions (25–27 Sep 2026) a valid time frame of slot S+3 was transmitted in
slot S, 0.52–0.60 s after its start, i.e. 2.40–2.48 s early; slot S+3 stayed empty.
Seen by two independent receivers (Legnica, Wrocław), event 8 by both at once.
In Wrocław: 3 in 3,195 time frames (about 1 in 1,070).

| # | Slot S (UTC) | Czas w ramce / time in frame | Odchyłka / offset | Odbiornik / receiver |
|---|---|---|---|---|
| 1 | 2026-09-25 23:47:12 | 23:47:15 | −2419 ms | Legnica |
| 2 | 2026-09-26 04:44:36 | 04:44:39 | −2460 ms | Legnica |
| 3 | 2026-09-26 07:12:00 | 07:12:03 | −2459 ms | Legnica |
| 4 | 2026-09-26 11:12:21 | 11:12:24 | −2439 ms | Legnica |
| 5 | 2026-09-26 11:35:27 | 11:35:30 | −2480 ms | Legnica |
| 6 | 2026-09-26 15:49:48 | 15:49:51 | −2419 ms | Legnica |
| 7 | 2026-09-27 00:02:57 | 00:03:00 | −2476 ms | Wrocław |
| 8 | 2026-09-27 06:37:15 | 06:37:18 | −2401 ms | Wrocław + Legnica |
| 9 | 2026-09-27 08:27:51 | 08:27:54 | −2443 ms | Wrocław |

## Zawartość / contents

| Ścieżka / path | Opis | Description |
|---|---|---|
| `recordings/ic705_early_frame_20260927_0002.wav` | Zdarzenie 7, 120 s od 00:02:00,000 UTC; wczesna ramka od 57,52 s | Event 7, 120 s from 00:02:00.000 UTC; early frame at 57.52 s |
| `recordings/ic705_early_frame_20260927_0636.wav` | Zdarzenie 8, 120 s od 06:36:30,000 UTC; wczesna ramka od 45,60 s | Event 8, 120 s from 06:36:30.000 UTC; early frame at 45.60 s |
| `recordings/ic705_early_frame_20260927_0827.wav` | Zdarzenie 9, 120 s od 08:27:00,000 UTC; wczesna ramka od 51,56 s | Event 9, 120 s from 08:27:00.000 UTC; early frame at 51.56 s |
| `legnica-log/event*.log` | Dziennik odbiornika z Legnicy, S−30 s … S+45 s (dla zdarzenia 9 bez ramek – zbyt słaby odbiór) | Legnica receiver log, S−30 s … S+45 s (no frames for event 9 – reception too weak) |
| `ic705-wroclaw/slots.csv` | Każdy slot 3 s, 26.09 22:11 – 27.09 09:00 UTC (12 885 slotów): treść, CRC, wynik dekodera | Every 3-s slot, 26.09 22:11 – 27.09 09:00 UTC (12,885 slots): content, CRC, decoder result |
| `ic705-wroclaw/frames.csv` | Każda zdekodowana ramka i jej odchyłka od własnego slotu | Every decoded frame and its offset from its own slot |
| `figures/` | Rysunki z raportu (faza nośnej) | Report figures (carrier phase) |
| `tools/` | Skrypty: klasyfikacja slotów, rysunki, PDF | Scripts: slot classification, figures, PDF |

**Nagrania / recordings:** ICOM IC-705, antena ferrytowa / ferrite rod antenna, USB,
224,000 kHz (nośna 225 kHz = ton 1 kHz / carrier = 1 kHz tone), 16 bit mono 8 kHz,
Wrocław. Czas początku plików wyznaczony z dekodowanych ramek (dokładność kilku ms) /
file start times derived from the decoded frames (a few ms accuracy).

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
