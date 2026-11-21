# Robota „Pomiar GNSS (RTK rover)” – projekt

Data: 2026-11-21. Status: do akceptacji.

## Po co to jest

Gracz ma **nauczyć się obsługi odbiornika GNSS** (gdzie ustawić tyczkę, kiedy czekać na fix, kiedy wynik jest wiarygodny)
i **zebrać współrzędne punktów do porównania** (z innymi pomiarami, z prawdziwymi współrzędnymi punktu w grze).
Odbiornik nie zna „prawdziwej” wysokości: podaje wynik z szumem, typem rozwiązania i wskaźnikami jakości, tak jak w terenie.
Dziennik zapisuje całą sesję punktu (kiedy złapał fix, ile epok uśredniono, jakość), a nie same X, Y, H.

## Cele

- Nowa robota w rejestrze `JobTypes()`: „Pomiar GNSS (RTK)”, obok ciągu niwelacyjnego.
- Realistyczna symulacja jakości zależna od otoczenia: widoczność nieba liczona promieniami z gry (drzewa, dachy, ściany).
- Odbiornik na żywo pokazuje stan (typ rozwiązania, satelity, PDOP, RMS, epoki, bieżące N/E/H).
- Dziennik GNSS: tabela w grze, CSV i DOCX w tym samym układzie folderów co niwelacja (`GothicSurveying/<data_godzina>/`).
- Opcjonalny **prawdziwy błąd** każdego punktu względem jego położenia w grze (wybierany przy starcie roboty, domyślnie wyłączony).

## Poza zakresem (v1)

- Tachimetria, niwelacja trygonometryczna, pomiar statyczny, stacja bazowa, wytyczanie, transformacje układów.
- Wizualizacja satelitów na niebie, wykres skyplot.
- Uogólnianie dziennika niwelacji (wariant B z rozmowy): niwelacja zostaje bez zmian.

## Czego ma nie być (anty-cele)

- Odbiornik, który „po prostu podaje wysokość z gry”.
- Zmiany psujące niwelację, tryb samodzielny, eksport DOCX/CSV niwelacji, wpis odczytu, sprzęt pod stopami.
- Teksty widoczne w grze z polskimi znakami (kodowanie); w CSV/DOCX polskie znaki są dozwolone.

## Ograniczenia (z projektu)

- Gothic API: pętla klatki to tylko hook `zCWorld::Render`; panele tekstowe tylko przez `PanelOpen` (zCView) z czcionką `LV_FONT`.
- Klawisze wyłącznie ze stałych w `src/Keys.hpp`; menu pod Home. Nie F11 (GD3D11), nie F12 (Steam), nie F5/F6.
- TDD dla `src/calc` i `src/job` (test czerwony, potem zielony); część silnikowa buduje się i testuje w grze przez użytkownika.
- Grę uruchamia się przez junction bez polskich znaków w ścieżce; literalne `\n` w kodzie C++ wpisuje się narzędziem Edit, nie heredocem.
- Czas symulacji = czas gry (gra zatrzymana w menu = epoki nie płyną).

## Podejścia

**Dziennik (zdecydowane):** A – osobny `GnssReport` z własnym eksportem i podglądem, niwelacja nietknięta. Wspólne funkcje
pisania tabeli CSV/DOCX wyciągane do małego helpera (bez zmiany wyników niwelacji). Odrzucone B (uniwersalny `Report` dla
wszystkich robót): większe ryzyko regresji w trybie samodzielnym; wrócimy do niego przy trzeciej robocie.

**Model jakości (moja decyzja):**
1. *Ułamek nieba* (jedna liczba 0–1 z promieni): najprostszy, ale nie pozwala policzyć PDOP ani zobaczyć, że jedna ściana psuje geometrię.
2. *Konstelacja + maska horyzontu* (wybrane): ok. 30 pseudo-satelitów o azymucie i elewacji zależnych od czasu; promienie dają maskę
   horyzontu (elewacja zasłonięcia na sektor azymutu); satelita jest używany, gdy leży nad maską; z używanych liczymy prawdziwy PDOP
   (macierz geometrii 4×4). Daje realistyczne efekty (kanion ulicy, ściana po jednej stronie) i testowalną matematykę.
3. *Pełna symulacja sygnałów (SNR, pseudoodległości)*: za duży koszt, nieczytelny dla gracza.

## Projekt

### Przebieg roboty

Start: Rozpocznij robotę → typ „Pomiar GNSS (RTK)” → pytanie „Pokazywać prawdziwy błąd po pomiarze?” (Tak/Nie). Dla tej roboty nie
ma menu rodzaju ciągu ani trybu liczenia (kontroler pyta o opcje zależnie od typu).

Menu roboty (Home):
- **Postaw tyczkę z odbiornikiem** – pod stopami bohatera, model tyczki z anteną; możliwe tylko po przestawieniu poprzedniego punktu.
- **Wysokość anteny: X m (zmień)** – przełącza presety 1,500 / 1,800 / 2,000 / 2,100 / 2,500 m. Fizyczna tyczka ma 2,000 m;
  niezgodność ustawienia z rzeczywistością daje błąd systematyczny w H (uczy kontroli tego parametru).
- **Rozpocznij pomiar punktu** – odbiornik zaczyna zbierać epoki, po zapisie punkt dostaje nazwę P1, P2, …
- **Zapisz punkt** – aktywne po ≥ 10 epokach w fixie (przy float/DGPS/autonomicznym punkt można zapisać wcześniej, ale oznaczony
  jakością słabszą niż fix, ostrzeżenie w dzienniku).
- **Odrzuć pomiar** – kasuje bieżącą sesję punktu.
- **Zakończ robotę** (≥ 1 punkt), **Przerwij robotę** (zapisuje to, co zmierzono, jak w niwelacji).

HUD odbiornika (panel w złotej ramce, odświeżany co 1 s czasu gry): typ rozwiązania, satelity użyte/widoczne, PDOP, RMS poziomy/
pionowy, numer epoki, bieżące N, E, H (po uśrednieniu).

### Symulacja (`src/calc/Gnss.hpp`, czysta, bez Gothic API)

- **Konstelacja:** `Satellites(t)` zwraca ok. 30 wpisów (azymut, elewacja) deterministycznie z czasu; stałe ziarno generatora.
- **Maska horyzontu:** 36 sektorów po 10° azymutu, wartość = elewacja najniższego niezasłoniętego kierunku w sektorze (silnik liczy
  promieniami ku niebu co 15° elewacji od 5° do 80°; wynik przekazuje do `calc`).
- **Użyte satelity:** elewacja > max(10°, maska sektora). PDOP z macierzy kierunków (inwersja 4×4, `PDOP = sqrt(tr(Q))`).
- **Wielodrogowość:** wskaźnik z bliskości przeszkód (najkrótszy promień boczny < 5 m ⇒ rośnie szum i spada szansa na fix).
- **Typ rozwiązania:** brak (< 4 sat.), autonomiczne (4+), DGPS (≥ 5, PDOP < 4), float RTK (≥ 5, PDOP < 3,5, ≥ 15 s), fix RTK
  (≥ 6, PDOP < 2,5, niska wielodrogowość, ≥ 8–15 s po starcie). Czas zbieżności losowy w zakresach dobranych w grze.
- **Szum epoki** = błąd systematyczny sesji (losowany raz, stały ~60 s) + szum biały: fix ≈ 8 mm + 1 ppm poziomo / 15 mm pionowo,
  float 20–40 cm, DGPS 0,5–1 m, autonomiczne 2–5 m, mnożniki z PDOP i wielodrogowości; pochylenie tyczki σ ≈ 0,3° (błąd poziomy rośnie
  z wysokością anteny). Uśrednianie zmniejsza szum biały, ale nie błąd systematyczny (to jest sedno nauki).
- **Układ współrzędnych:** lokalny, N = z_gry/100 + N0, E = x_gry/100 + E0, H = y_gry/100 + H0 (stałe fikcyjne w jednym miejscu, np.
  N0 = 5 500 000 m, E0 = 6 500 000 m); odbiornik zwraca zaokrąglenie do mm.

### Dziennik GNSS

Wiersz punktu: nr, nazwa, data/czas (czas rzeczywisty zapisu), N, E, H [m], typ rozwiązania, satelity użyte, PDOP, RMS H/V [mm],
liczba epok, wysokość anteny, uwagi (np. „brak fixu”). Przy opcji prawdziwego błędu dodatkowo: ΔN, ΔE, ΔH, Δ2D [mm] względem prawdziwego
punktu w grze. Podsumowanie: liczba punktów, ile w fixie, średni PDOP, najgorszy punkt.

Pliki w folderze roboty: `pomiar_gnss.csv`, `pomiar_gnss.docx`; przerwana: `pomiar_gnss_przerwany.csv`; log zapisanych punktów na bieżąco
w `odczyty_biezace.csv` analogicznie do niwelacji.

### Architektura

- `calc/Gnss.hpp`: konstelacja, maska, PDOP, typ rozwiązania, szum, konwersja układu. Wszystko z deterministycznym RNG (testowalne).
- `calc/GnssReport.hpp`: model raportu + CSV; DOCX przez wspólny helper tabeli (wyciągnięty z `Docx.hpp` bez zmiany niwelacji).
- `job/Gnss*.hpp`: automat kroków i menu (`Job`), rozszerzenie `IWorld` o `SkyHorizonMask()` (silnik: promienie), `PlacePole()`, `GroundPoint()`;
  `JobOptions` rozszerzone o opcję prawdziwego błędu; `JobType` informuje kontroler, jakie menu opcji pokazać.
- `engine/`: model tyczki z anteną (`LVL_POLE.3DS` z `make_models.py`), panel odbiornika, kontroler (osobny przebieg raportu), podgląd tabeli.
- Reuse: menu, ramka, ubiór bohatera, foldery wyników, klawisze, `FeetGround`, `SpawnVob`.

### Testy

- `calc`: PDOP dla znanej geometrii (zenit + 3 sat. co 120° przy 30° ⇒ wartość policzona analitycznie), maska zmniejsza liczbę użytych
  satelitów, progi typu rozwiązania, statystyka szumu (średnia i σ z tysięcy epok z ziarnem), błąd systematyczny nie znika przy uśrednianiu,
  konwersja układu tam i z powrotem, format CSV raportu.
- `job`: kolejność kroków i dostępność menu z fałszywym światem (kontrola rozmiaru próbki, zapis punktu w fixie, odrzucenie, przerwanie).
- Silnik: sprawdza użytkownik w grze (tyczka, HUD, promienie, zapis plików).

## Zostawione budowniczemu

Dokładne progi i czasy zbieżności (strojone w grze), układ paneli, wygląd modelu tyczki, liczba i rozkład pseudo-satelitów,
nazwy pól CSV poza wymienionymi, sposób wyciągnięcia helpera tabeli.

## Ryzyka

- Promienie do nieba: ~216 promieni co 1 s czasu gry jest tanie, ale zależy od geometrii sceny (drzewa w Gothicu bywają bez kolizji –
  wtedy zasłonięcie będzie niedoszacowane; ocena w grze).
- Strojenie realizmu (progi fix/float) wymaga prób w grze i może potrwać dłużej niż kod.
