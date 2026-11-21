# Pomiar GNSS (RTK rover) – plan implementacji

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Nowa robota „Pomiar GNSS (RTK)”: gracz stawia tyczkę z odbiornikiem, odbiornik symuluje realistyczny pomiar (typ rozwiązania, PDOP, szum, zasłonięcie nieba z promieni gry) i zapisuje dziennik punktów (tabela w grze, CSV, DOCX).

**Architecture:** Czysta symulacja w `src/calc/Gnss.hpp` i raport w `src/calc/GnssReport.hpp` (testy g++). Automat kroków `src/job/GnssRover.hpp` przez `Job`/`IWorld` (testy z fałszywym światem). Silnik (`World.hpp`, `Controller.hpp`, `Export.hpp`, model tyczki) dokłada tylko promienie do nieba, tyczkę, panel statusu i zapis raportu. Niwelacja zostaje bez zmian.

**Tech Stack:** C++20 (g++ w testach, MSVC x86 plugin Union/Gothic API), Python (`assets/make_models.py`), PowerShell (`tests/run.ps1`, `scripts/build.ps1`, `scripts/install.ps1`).

**Spec:** `docs/superpowers/specs/2026-11-21-gnss-rover-design.md`

## Global Constraints

- Teksty widoczne w grze: bez polskich znaków (kodowanie). CSV/DOCX: polskie znaki dozwolone.
- Pętla klatki tylko hook `zCWorld::Render` (`ControllerTick`); panele tylko `PanelOpen`/`PanelClose` z `Panels.hpp` (złota ramka, `LV_FONT`).
- Klawisze wyłącznie ze stałych w `src/Keys.hpp`; menu pod Home; nowych klawiszy nie dodajemy.
- Niwelacja, tryb samodzielny, wpis odczytu, eksport DOCX/CSV niwelacji i sprzęt pod stopami nie mogą się zmienić (istniejące testy muszą dalej dawać `ALL OK`).
- TDD dla `src/calc` i `src/job`: test czerwony, potem zielony; uruchamianie: `powershell -File tests/run.ps1` (wynik `ALL OK`). Build: `powershell -File scripts/build.ps1`; instalacja: `powershell -File scripts/install.ps1 -Game "C:\Program Files (x86)\Steam\steamapps\common\Gothic II"`.
- Literalne `\n` w kodzie C++ wpisuj narzędziem Edit (heredoc zamienia je na prawdziwy newline).
- Układ lokalny: N = z_gry/100 + N0, E = x_gry/100 + E0, H = y_gry/100 + H0; N0 = 5 500 000 m, E0 = 6 500 000 m, H0 = 0 m (stałe w jednym miejscu).
- Fizyczna tyczka ma 2,000 m; presety wysokości anteny: 1,500 / 1,800 / 2,000 / 2,100 / 2,500 m (domyślnie 2,000).
- Epoka co 1 s czasu gry; zegar stoi, gdy gra zatrzymana (menu, luneta, panele).
- Wypychanie na GitHub i release tylko na wyraźne polecenie użytkownika.

## Zmiany względem specyfikacji (moje decyzje wykonawcze)

- Kolejność zbieżności: najpierw DGPS/autonomiczne, po `tFloat` (losowo 5–12 s) float, po `tFix` (losowo 15–40 s) fix – spec miał niespójne progi czasu.
- Bez pomocniczego logu na żywo dla GNSS (YAGNI): raport zapisuje się przy końcu/przerwaniu, a utrata sesji przy awarii gry jest akceptowana w v1.
- Helper tabeli DOCX: wyciągamy tylko `WrapDocx(body)` (pakiet + XML) z `BuildDocx`, bez zmiany wyniku niwelacji.
- Podsumowanie w grze przez istniejące `TextPanelShow` (bez nowego panelu tabeli).

## Review Focus

- Tyczka stawiana nad wodą/bez gruntu: `PlacePole()==false`, komunikat, stan się nie zmienia.
- Zasłonięte niebo (< 4 satelitów): typ „brak”, PDOP 99, brak NaN w współrzędnych, „Zapisz punkt” nieaktywne.
- Wczytanie zapisu / duży skok zegara w trakcie pomiaru: nie więcej niż 5 epok na klatkę, ujemna różnica czasu ignorowana, częściowy raport zapisany przy zmianie świata.
- Menu otwarte (zegar stoi): epoki nie przybywają.
- Pusty raport: „Zakończ robotę” nieaktywne; CSV z uwagą zawierającą `;` i `"` poprawnie cytowany.

## Struktura plików

- Create: `src/calc/Gnss.hpp` (symulacja), `src/calc/GnssReport.hpp` (raport, CSV, DOCX, podsumowanie), `src/job/GnssRover.hpp` (robota), `tests/test_gnss.cpp`, `tests/test_gnss_report.cpp`, `tests/test_gnss_job.cpp`.
- Modify: `src/calc/Docx.hpp` (`WrapDocx`), `src/job/Job.hpp` (rozszerzenia), `src/job/Jobs.hpp` (rejestr), `src/engine/World.hpp`, `src/engine/Placement.hpp`, `src/engine/Export.hpp`, `src/engine/Controller.hpp`, `assets/make_models.py`, `README.md`, `scripts/package.py`.

---

### Task 1: Geometria satelitów, PDOP i typ rozwiązania

**Files:**
- Create: `src/calc/Gnss.hpp`
- Test: `tests/test_gnss.cpp`

**Interfaces:**
- Produces (wszystko w `src/calc/Gnss.hpp`, bez Gothic API):
  - `struct SatDir { double azDeg, elDeg; };`
  - `struct SkyMask { double horizonDeg[36] = {}; double nearestObstacleM = 100.0; };` (sektor `s` = azymut `[10s, 10s+10)`)
  - `std::vector<SatDir> GnssSatellites(double tSec);` (30 satelitów, deterministycznie z czasu)
  - `std::vector<SatDir> GnssUsable(const std::vector<SatDir>& sats, const SkyMask& m);` (elewacja > max(10°, horyzont sektora))
  - `double GnssPdop(const std::vector<SatDir>& used);` (99.0 gdy < 4 satelitów lub macierz osobliwa)
  - `double GnssMultipath(const SkyMask& m);` (0 przy `nearestObstacleM` ≥ 5, liniowo do 1 przy 0)
  - `enum class SolType { None, Autonomous, Dgps, Float, Fix };`
  - `SolType GnssClassify(int nUsed, double pdop, double multipath, double sessionSec, double tFloatSec, double tFixSec);`
  - `const char* GnssSolName(SolType t);` → `"brak"`, `"autonomiczne"`, `"DGPS"`, `"float RTK"`, `"fix RTK"`.

- [ ] **Step 1: Napisz testy** w `tests/test_gnss.cpp` (`main` wywołuje każdą funkcję, na końcu `std::puts("test_gnss OK")`):
  - `pdop_matches_analytic_geometry`: satelity {zenit (el 90), el 30° az 0/120/240} → `fabs(GnssPdop(s) - 8.0/3.0) < 1e-9`; 3 satelity → `GnssPdop == 99.0`.
  - `constellation_is_deterministic_and_open_sky_is_good`: `GnssSatellites(0)` równe dwa razy; ma 30 elementów, wszystkie `elDeg` w [0, 90]; `GnssSatellites(0) != GnssSatellites(600)`; dla `SkyMask{}` (horyzont 0) w t = 0, 1000, 5000 s: `GnssUsable(...).size() >= 8` i `GnssPdop(...) < 2.5`.
  - `mask_removes_exactly_blocked_satellites`: maska z `horizonDeg[0] = 60` (sektor 0); oczekiwana liczba użytych liczona w teście pętlą po `GnssSatellites(300)` (sektor = `(int)floor(az/10) % 36`, warunek `el > max(10, horizon)`) równa `GnssUsable(...).size()`.
  - `multipath_scales_with_obstacle_distance`: 0 przy 5 m i 100 m, 1 przy 0 m, 0,5 przy 2,5 m.
  - `classify_table`: (n, pdop, mp, sesja, tFloat 8, tFix 20) → `(3, 1, 0, 100)=None`, `(6, 1.2, 0.1, 25)=Fix`, `(6, 1.2, 0.1, 10)=Float`, `(6, 1.2, 0.1, 3)=Dgps`, `(6, 2.8, 0.1, 25)=Float` (PDOP za duży na fix), `(6, 1.2, 0.5, 25)=Float` (wielodrogowość ≥ 0,3 blokuje fix), `(6, 1.2, 0.7, 25)=Dgps` (≥ 0,6 blokuje float), `(5, 3.8, 0, 100)=Dgps`, `(4, 6, 0, 100)=Autonomous`.
- [ ] **Step 2: Uruchom** `powershell -File tests/run.ps1`, oczekiwane: `COMPILE FAIL test_gnss.cpp`.
- [ ] **Step 3: Zaimplementuj** `Gnss.hpp` według sygnatur. Progi `GnssClassify`: `None` gdy n < 4; `Fix` gdy n ≥ 6, pdop < 2,5, mp < 0,3, sesja ≥ tFix; `Float` gdy n ≥ 5, pdop < 3,5, mp < 0,6, sesja ≥ tFloat; `Dgps` gdy n ≥ 5, pdop < 4, mp < 0,6; w pozostałych `Autonomous`. PDOP: macierz geometrii wierszami `[cos(el)·sin(az), cos(el)·cos(az), sin(el), 1]`, `Q = (GᵀG)⁻¹` (4×4 przez eliminację Gaussa), `PDOP = sqrt(Q00+Q11+Q22)`. Wzór konstelacji dobierz tak, by przechodził test pogody otwartego nieba (np. `az = 12k + 0,004·t·(k%3+1)`, `el = 5 + 85·|sin(1,7k + 0,0007t)|`).
- [ ] **Step 4: Uruchom testy**, oczekiwane: `ALL OK`.
- [ ] **Step 5: Commit** `git add src/calc/Gnss.hpp tests/test_gnss.cpp && git commit -m "calc: geometria satelitow, PDOP i typ rozwiazania GNSS"`.

---

### Task 2: Sesja pomiarowa (szum, błąd systematyczny, antena, układ)

**Files:**
- Modify: `src/calc/Gnss.hpp`
- Test: `tests/test_gnss.cpp`

**Interfaces:**
- Consumes: Task 1 (`SkyMask`, `GnssSatellites`, `GnssUsable`, `GnssPdop`, `GnssMultipath`, `GnssClassify`, `SolType`).
- Produces:
  - `struct GridPos { double n, e, h; };`
  - `GridPos GnssToGrid(double gameX, double gameY, double gameZ);` oraz `void GnssFromGrid(const GridPos& g, double& gameX, double& gameY, double& gameZ);` (cm ↔ m, stałe N0/E0/H0).
  - `struct SolSigma { double hMm, vMm; };` `SolSigma GnssSigma(SolType t, double pdop, double multipath);` – bazowe σ (poziom/pion, mm): fix 8/15, float 300/450, DGPS 700/1100, autonomiczne 3000/5000, brak 0/0; mnożnik `max(0.7, pdop/2.0) · (1 + 2·multipath)`.
  - `class GnssRng { public: explicit GnssRng(uint32_t seed); double Uniform(); double Normal(); };` (własny xorshift32 + Box–Muller, wynik identyczny na każdym kompilatorze).
  - `struct GnssFix { SolType type = SolType::None; int used = 0, visible = 0, epochs = 0; double pdop = 99, multipath = 0; double n = 0, e = 0, h = 0; double hRmsMm = 0, vRmsMm = 0; };`
  - `class GnssSession { public: GnssSession(uint32_t seed, double tStartSec, const GridPos& truth, double antennaSetM, double antennaTrueM = 2.0); void Step(double tSec, const SkyMask& mask); const GnssFix& Fix() const; double StartSec() const; };` – `Step` dodaje jedną epokę: liczy satelity, PDOP, typ (`tFloat` ∈ [5,12] s i `tFix` ∈ [15,40] s losowane w konstruktorze), szum = błąd systematyczny (losowany ponownie przy zmianie typu rozwiązania, σ = 0,5·σ typu) + szum biały + pochylenie tyczki (σ 0,3°, przesunięcie poziome `antennaTrueM·tan`); `Fix()` zwraca średnią epok od ostatniej zmiany typu, `h` skorygowane o `antennaTrueM - antennaSetM`; RMS to odchylenie standardowe epok (0 przy < 2 epokach).

- [ ] **Step 1: Napisz testy** (dopisz do `tests/test_gnss.cpp`):
  - `grid_roundtrip`: `GnssFromGrid(GnssToGrid(1234.5, -678.9, 4321.0))` zwraca te same wartości (±1e-9); `GnssToGrid(0,0,0)` = `{5500000, 6500000, 0}`; `GnssToGrid(100, 200, 300)` = `{5500003, 6500001, 2}`.
  - `sigma_ordering`: `GnssSigma(Fix,2,0)` = `{8,15}`; `Float.hMm > Fix.hMm`, `Dgps > Float`, `Autonomous > Dgps`; rośnie z PDOP (3 > 2) i z wielodrogowością (0,5 > 0); `None` = `{0,0}`.
  - `rng_is_deterministic`: dwa `GnssRng(7)` dają te same 100 wartości; `Normal()` z 20000 próbek ma średnią |m| < 0,03 i odchylenie w [0,97; 1,03].
  - `session_reaches_fix_in_open_sky`: `SkyMask{}`, seed 1, `Step` co 1 s od t = 1 do 60: po 3 s `Fix().type` to `Dgps` lub `Autonomous`; po 45 s `Fix`; `Fix().epochs >= 5`.
  - `fix_error_is_centimeters_but_bias_survives_averaging`: dla seedów 1..40 sesja otwartego nieba, 200 epok po osiągnięciu fixu (pomiń pierwsze 45 s), antena ustawiona poprawnie; średni błąd poziomy (|n−truth|, |e−truth|) każdej sesji < 0,05 m; odchylenie standardowe błędu N średnich z 40 sesji w [0,3·8 mm, 0,8·8 mm]·√(1+tilt) tolerancja szeroka: ∈ [0,003 m; 0,02 m] (błąd systematyczny widoczny, nie znika).
  - `antenna_mismatch_biases_height`: antena ustawiona 1,800 przy prawdziwej 2,000 → średni błąd H w fixie ≈ +0,200 m (±0,03 m).
  - `blocked_sky_gives_no_solution`: maska z `horizonDeg[s] = 89` we wszystkich sektorach → `Fix().type == None`, `used < 4`, `pdop == 99`, a `n`, `e`, `h` są skończone (`std::isfinite`).
- [ ] **Step 2: Uruchom** `tests/run.ps1`, oczekiwane: błędy kompilacji / FAIL w `test_gnss`.
- [ ] **Step 3: Zaimplementuj** funkcje i klasy z sygnatur; punkt odniesienia błędu to `truth`; typ rozwiązania z `GnssClassify(used, pdop, GnssMultipath(mask), t − tStart, tFloat, tFix)`; gdy `None`, współrzędne zostają ostatnimi znanymi (początkowo `truth`) i `epochs` rośnie od zera przy zmianie typu.
- [ ] **Step 4: Uruchom testy**, oczekiwane: `ALL OK`.
- [ ] **Step 5: Commit** `git commit -am "calc: sesja pomiarowa GNSS (szum, blad systematyczny, antena, uklad lokalny)"`.

---

### Task 3: Raport GNSS (model, CSV, DOCX, podsumowanie)

**Files:**
- Create: `src/calc/GnssReport.hpp`
- Modify: `src/calc/Docx.hpp` (wyciągnij `docxdetail::WrapDocx`)
- Test: `tests/test_gnss_report.cpp`

**Interfaces:**
- Consumes: `SolType`, `GnssSolName` (Task 1); `fbdetail::Num`, `fbdetail::Csv` z `FieldBook.hpp`; `docxdetail::Para`, `docxdetail::Row`, `docxdetail::ZipStore` z `Docx.hpp`.
- Produces:
  - `struct GnssPoint { std::string name, timeText; double n = 0, e = 0, h = 0; std::string solution; int used = 0; double pdop = 0, hRmsMm = 0, vRmsMm = 0; int epochs = 0; double antennaM = 2.0; std::string note; bool hasTrue = false; double dNmm = 0, dEmm = 0, dHmm = 0, d2Dmm = 0; };`
  - `struct GnssReport { bool complete = true, showTrueError = false; std::string dateText; std::vector<GnssPoint> points; };`
  - `std::string GnssToCsv(const GnssReport&);`
  - `std::string GnssBuildDocx(const GnssReport&);`
  - `std::vector<std::string> GnssSummaryLines(const GnssReport&);` (do panelu w grze i DOCX: bez polskich znaków).
  - W `Docx.hpp`: `inline std::string docxdetail::WrapDocx(const std::string& bodyXml);` (sectPr landscape + `[Content_Types].xml` + rels + `ZipStore`), używane przez `BuildDocx` bez zmiany jego wyniku.

- [ ] **Step 1: Napisz testy** `tests/test_gnss_report.cpp`:
  - `csv_header_and_row`: nagłówek bez błędu to dokładnie `nr;punkt;czas;N_m;E_m;H_m;rozwiazanie;satelity;PDOP;RMS_H_mm;RMS_V_mm;epoki;antena_m;uwagi`; z `showTrueError` zakończony `;blad_N_mm;blad_E_mm;blad_H_mm;blad_2D_mm`. Punkt `{name "P1", timeText "09:15:03", n 5500123.4567, e 6500234.5, h 12.3456, solution "fix RTK", used 9, pdop 1.84, hRms 9.2, vRms 14.6, epochs 12, antenna 2.0}` daje wiersz `1;P1;09:15:03;5500123.457;6500234.500;12.346;fix RTK;9;1.8;9;15;12;2.000;` (PDOP 1 miejsce, RMS 0 miejsc, antena 3 miejsca).
  - `csv_quotes_note_and_appends_true_error`: `note = "a;b \"c\""` jest w cudzysłowie z podwojonym `"`; punkt z `hasTrue`, `dN -3.2, dE 4.1, dH 7.8, d2D 5.2` ma na końcu `;-3.2;4.1;7.8;5.2`.
  - `incomplete_report_is_flagged`: `complete=false` → pierwsza linia CSV zaczyna się od `# UWAGA: pomiar niekompletny`.
  - `docx_contains_points`: `GnssBuildDocx` zaczyna się od `PK`, zawiera `P1`, `fix RTK` i `Pomiar GNSS` (archiwum „stored”, więc tekst jest widoczny wprost).
  - `summary_lines`: 3 punkty (2 fix, 1 float, PDOP 1.5/2.0/3.0) → linie zawierają `Punkty: 3`, `w fixie: 2`, `sredni PDOP: 2.2`; przy `hasTrue` linia `Najgorszy punkt: <nazwa> (<d2D> mm)` z maksymalnym `d2Dmm`; bez prawdziwego błędu linia o najwyższym PDOP.
  - `docx_refactor_keeps_leveling_output`: istniejące `tests/test_docx.cpp` i `tests/check_docx.py` dalej przechodzą.
- [ ] **Step 2: Uruchom** `tests/run.ps1`, oczekiwane: `COMPILE FAIL test_gnss_report.cpp`.
- [ ] **Step 3: Zaimplementuj** `WrapDocx` (przenieś końcówkę `BuildDocx`) i `GnssReport.hpp`; DOCX: tytuł „Pomiar GNSS (RTK)”, data, tabela tych samych kolumn co CSV (nagłówki po polsku z polskimi znakami), potem linie `GnssSummaryLines`. Punkt zapisany bez fixu ma w `note` „zapis bez fixu”; to ustawia robota (Task 4), nie raport.
- [ ] **Step 4: Uruchom testy**, oczekiwane: `ALL OK`.
- [ ] **Step 5: Commit** `git add src/calc tests && git commit -m "calc: raport GNSS (CSV, DOCX, podsumowanie)"`.

---

### Task 4: Robota GNSS (automat, menu, rozszerzenia Job/IWorld)

**Files:**
- Create: `src/job/GnssRover.hpp`
- Modify: `src/job/Job.hpp`, `src/job/Jobs.hpp`
- Test: `tests/test_gnss_job.cpp` (fałszywy świat), `tests/test_traverse_job.cpp` (bez zmian oczekiwane)

**Interfaces:**
- Consumes: Task 1–3 (`GnssSession`, `GnssFix`, `SkyMask`, `GridPos`, `GnssReport`, `GnssPoint`, `GnssSolName`).
- Produces (w `Job.hpp`):
  - `enum class JobKind { Leveling, Gnss };`
  - `JobOptions` dostaje `bool showTrueError = false;` (inicjalizacja `JobOptions{kind, mode}` nadal działa).
  - `JobType` dostaje na końcu `bool gnss = false;`.
  - `IWorld` – nowe metody **niepure** z domyślnym ciałem (żeby istniejący `FakeWorld` w testach niwelacji się nie zmieniał): `virtual bool PlacePole() { return false; }`, `virtual void RemovePole() {}`, `virtual SkyMask SkyHorizon() const { return SkyMask{}; }` (z `nearestObstacleM`), `virtual GridPos PoleGroundGrid() const { return GridPos{0,0,0}; }`, `virtual double NowSec() const { return 0; }`, `virtual std::string TimeText() const { return ""; }`.
  - `Job` – metody z domyślnym ciałem: `virtual JobKind Kind() const { return JobKind::Leveling; }`, `virtual GnssReport FinishGnss(bool complete) { return GnssReport{}; }`, `virtual void Tick() {}`, `virtual std::vector<std::string> StatusLines() const { return {}; }`.
  - `std::unique_ptr<Job> CreateGnssRover(IWorld&, const JobOptions&);` zarejestrowane w `Jobs.hpp` jako `{"gnss", "Pomiar GNSS (RTK)", &CreateGnssRover, true}`.
- Menu robota (id → etykieta ASCII): `pole` „Postaw tyczke z odbiornikiem”, `antenna` „Wysokosc anteny: X.XXX m (zmien)”, `start` „Rozpocznij pomiar punktu”, `store` „Zapisz punkt”, `discard` „Odrzuc pomiar”, `finish` „Zakoncz robote”. Dostępność: `pole`, `antenna` – gdy nie trwa pomiar; `start` – gdy tyczka stoi i nie trwa pomiar; `store` – trwa pomiar i (`type == Fix && epochs >= 10` albo `type != None && type != Fix && epochs >= 5`); `discard` – trwa pomiar; `finish` – ≥ 1 punkt i nie trwa pomiar. `Hud()` zwraca krok w jednej linii (ASCII, z `LV_KEYNAME_MENU`). `StatusLines()` zwraca 4 linie podczas pomiaru (rozwiazanie, satelity użyte/widoczne i PDOP, epoki i RMS, N/E/H lub „brak rozwiazania”), a przy postawionej tyczce bez pomiaru jedną linię z podpowiedzią.

- [ ] **Step 1: Napisz testy** `tests/test_gnss_job.cpp` z `FakeGnssWorld : IWorld` (wszystkie pure z `IWorld` zaimplementowane pustymi ciałami jak w `test_traverse_job.cpp`, plus: `PlacePole` zwraca pole `ground`, `SkyHorizon` zwraca `mask`, `PoleGroundGrid` zwraca stałe `{5500100, 6500200, 12.5}`, `NowSec` zwraca `now`, `TimeText` zwraca `"09:15:03"`, `Random01` zwraca 0,5, `Say` zbiera komunikaty). Pomocnik `advance(seconds)` przesuwa `now` w krokach 1 s wołając `job->Tick()` po każdym kroku.
  - `menu_enablement_follows_state`: na starcie włączone tylko `pole` i `antenna`; po `pole` także `start`; po `start` wyłączone `pole`, `antenna`, `start`, włączone `discard`, `store` jeszcze nie.
  - `pole_blocked_without_ground`: `ground=false` → `pole` nie zmienia stanu (`start` dalej wyłączone) i `said` ma komunikat z „gruntu”.
  - `open_sky_fix_then_store`: po `pole`, `start`, `advance(60)` – `StatusLines()` zawiera „fix RTK”, `store` włączone; po `store`: `FinishGnss(true).points.size() == 1`, nazwa `P1`, `solution == "fix RTK"`, `note == ""`, `timeText == "09:15:03"`; `finish` włączone; po `finish` zwraca raport `complete == true`.
  - `store_without_fix_is_flagged`: maska z `horizonDeg` = 70 we wszystkich sektorach i `nearestObstacleM = 0.5` (bez fixu), `advance(20)`: jeśli `store` włączone, po zapisie `note == "zapis bez fixu"` i `solution != "fix RTK"`.
  - `blocked_sky_never_enables_store`: maska 89° we wszystkich sektorach, `advance(120)` → `store` wyłączone, `StatusLines()` zawiera „brak rozwiazania”, żadnej linii z `nan`/`inf`.
  - `clock_frozen_means_no_epochs`: po `start` i 10 wywołaniach `Tick()` bez zmiany `now` – `epochs == 0` (linia statusu pokazuje epoki 0); duży skok `now` o +1000 s powoduje najwyżej 5 nowych epok na jedno `Tick()`; skok wstecz (`now` mniejsze) nie psuje stanu (kolejne `Tick` nie zwiększa epok, po przywróceniu biegu znów rosną).
  - `antenna_presets_cycle_and_bias_height`: `antenna` przełącza 2,000 → 2,100 → 2,500 → 1,500 → 1,800 → 2,000; po ustawieniu 1,800 pomiar w fixie daje w raporcie `h` wyższe od prawdziwego (12.5) o ok. 0,2 m (±0,05) i `antennaM == 1.8`.
  - `true_error_columns_when_enabled`: `JobOptions{..., showTrueError=true}` → `points[0].hasTrue == true`, `d2Dmm == sqrt(dN²+dE²)`, błąd poziomy < 50 mm w fixie; bez opcji `hasTrue == false`.
  - `abort_returns_partial_report`: po jednym zapisanym punkcie i trwającym drugim pomiarze `FinishGnss(false)` zwraca `complete == false` z jednym punktem; z zerem punktów `points.empty()`.
  - `discard_clears_measurement`: po `discard` `start` znów włączone, punktów nadal 0.
- [ ] **Step 2: Uruchom** `tests/run.ps1`, oczekiwane: `COMPILE FAIL test_gnss_job.cpp`.
- [ ] **Step 3: Zaimplementuj** zmiany w `Job.hpp` (nowe metody, `JobKind`, pola) i `GnssRover.hpp`. `Tick()`: gdy trwa pomiar, `while (now − lastEpoch >= 1.0 && steps < 5)` wołaj `session.Step(lastEpoch + 1, w_.SkyHorizon())`; gdy `now < lastEpoch` ustaw `lastEpoch = now`. Ziarno sesji `uint32_t(Random01()·4e9)`. Zapis punktu: współrzędne z `Fix()`, `d*mm = (zapisane − PoleGroundGrid) · 1000` tylko przy `showTrueError`. Nazwy `P1..Pn`.
- [ ] **Step 4: Uruchom testy**, oczekiwane: `ALL OK` (w tym niezmienione testy niwelacji).
- [ ] **Step 5: Commit** `git add src/job tests && git commit -m "job: robota Pomiar GNSS (RTK rover) z rozszerzonym interfejsem Job/IWorld"`.

---

### Task 5: Silnik – tyczka, promienie do nieba, panel statusu, przepływ i zapis

**Files:**
- Modify: `assets/make_models.py`, `src/engine/Placement.hpp`, `src/engine/World.hpp`, `src/engine/Export.hpp`, `src/engine/Controller.hpp`
- Test: build (`powershell -File scripts/build.ps1` → `BUILD OK`), `tests/run.ps1` → `ALL OK`; zachowanie w grze sprawdza użytkownik.

**Interfaces:**
- Consumes: Task 4 (`IWorld` GNSS, `Job::Kind/FinishGnss/Tick/StatusLines`, `JobType::gnss`, `JobOptions::showTrueError`), Task 3 (`GnssToCsv`, `GnssBuildDocx`, `GnssSummaryLines`), istniejące `FeetGround`, `SpawnVob`, `RemoveVobSafe`, `DropVobRef`, `VobIsStale`, `MakeJobDir`, `WriteBytes`, `TextPanelShow`, `PanelOpen/PanelClose`.
- Produces:
  - `EngineWorld`: `PlacePole`, `RemovePole`, `SkyHorizon`, `PoleGroundGrid`, `NowSec`, `TimeText` (override), `void AdvanceClock(double dtSec)`; tyczka uwzględniona w `ForgetAll`, `Stale`, `ClearAll`.
  - `LV_VIS_POLE = "LVL_POLE.3DS"`, `LV_POLE_H = 200.0f` (cm) w `Placement.hpp`.
  - `bool ExportGnss(const GnssReport&, const std::string& dir, bool aborted)` w `Export.hpp` (`pomiar_gnss.csv` + `pomiar_gnss.docx`; aborted: tylko `pomiar_gnss_przerwany.csv`).
  - W `Controller.hpp`: `CtrlStartJob(int typeIdx, TraverseKind kind, CalcMode mode, bool showTrueError = false)`, `CtrlOpenTrueErrorMenu(int typeIdx)`, `CtrlFinishGnss()`, `CtrlSavePartialGnss(const GnssReport&)`, panel statusu `gStatusPanel` z `CtrlUpdateStatusPanel()`.

- [ ] **Step 1: Model tyczki** w `assets/make_models.py`: `build_pole()` (graniastosłup r = 1,2 cm od 0 do 200 cm w `block(2)`; antena: walec r = 9 cm, 200–206 cm w `block(1)`; pierścień r = 10 cm, 206–208 cm w `block(3)`) i wpis `("LVL_POLE", build_pole(), one)` w liście `main()`. Uruchom `python assets/make_models.py <tmp>`: powstaje `LVL_POLE.3DS`.
- [ ] **Step 2: Tyczka i zegar w `World.hpp`**: `PlacePole` – `FeetGround(p)`, usuń starą tyczkę, `SpawnVob(LV_VIS_POLE, p)`, zapamiętaj `poleGround_`; `PoleGroundGrid` = `GnssToGrid(poleGround_[VX], poleGround_[VY], poleGround_[VZ])`; `NowSec` = `clock_` (zegar własny), `AdvanceClock` dodaje `dtSec`; `TimeText` – zegar systemowy `HH:MM:SS` (`strftime`).
- [ ] **Step 3: Promienie do nieba** (`SkyHorizon`): z punktu `poleGround_ + (0, LV_POLE_H, 0)` dla każdego z 36 sektorów (azymut środka = 10·s + 5°) i elewacji 5, 20, 35, 50, 65, 80° wystrzel promień długości 8000 cm (`TraceRayNearestHit` z flagami jak w `ScopeBeginReading`: `zTRACERAY_VOB_IGNORE_NO_CD_DYN | zTRACERAY_VOB_IGNORE_CHARACTER | zTRACERAY_POLY_IGNORE_TRANSP`); `horizonDeg[s]` = najwyższa elewacja z trafieniem + 7,5° (0, gdy brak trafień); `nearestObstacleM` = najmniejsza pozioma odległość trafienia (`dystans·cos(el)/100`) po wszystkich promieniach. Kierunek wektora: `(sin az · cos el, sin el, cos az · cos el)`.
- [ ] **Step 4: Eksport** w `Export.hpp`: `ExportGnss` jak w Interfaces.
- [ ] **Step 5: Kontroler**: (a) menu startowe – po wyborze typu z `gnss == true` zamiast menu rodzaju/trybu `MenuOpen` z „Pokazywac prawdziwy blad po pomiarze?” (Nie domyślnie / Tak) i `CtrlStartJob(typeIdx, TraverseKind::Open, CalcMode::Auto, tak)`; (b) `ControllerTick`: dt z `GetTickCount()` (maks. 0,2 s), `gWorld.AdvanceClock(dt)` tylko gdy nic nie zatrzymuje gry (`!MenuIsOpen() && !ScopeActive() && !FieldBookVisible() && !TextPanelVisible() && !FieldBookEditActive()`), potem `if (gJob) gJob->Tick()`; (c) `CtrlUpdateStatusPanel`: gdy `gJob` i `StatusLines()` niepuste – panel `PanelOpen(150, 150, 3300, 1500)` z liniami, odświeżany tylko po zmianie tekstu, zamykany w `CtrlEndJob`/`CtrlForgetJob`; wołany obok `CtrlUpdateJobHud()`; (d) `CtrlOpenMenu`: pozycję „scope” i klawisz `LV_KEY_SCOPE` obsługuj tylko dla `Kind() == JobKind::Leveling`; (e) `finish`: `CtrlFinishGnss` – `FinishGnss(true)`, `MakeJobDir()`, `ExportGnss`, `TextPanelShow("Pomiar GNSS - podsumowanie", lines, ok ? "Zapisano w folderze: " + dir : "BLAD ZAPISU plikow")` z `GnssSummaryLines` i linią na punkt, `CtrlEndJob()`; (f) przerwanie i zmiana świata dla GNSS przez `CtrlSavePartialGnss` (tworzy folder i zapisuje `pomiar_gnss_przerwany.csv`, jeśli są punkty); (g) `ClearAll`/`ForgetAll`/`Stale` obsługują tyczkę.
- [ ] **Step 6: Build i testy**: `powershell -File scripts/build.ps1` → `BUILD OK`; `powershell -File tests/run.ps1` → `ALL OK`.
- [ ] **Step 7: Instalacja** (`scripts/install.ps1 -Game ...`) i porównanie `cmp` DLL; użytkownik sprawdza w grze: start robot GNSS, tyczka pod stopami, panel statusu, przejście Float→Fix pod gołym niebem, brak fixu pod dachem/drzewem, zapis punktów, folder z `pomiar_gnss.csv/.docx`, przerwanie, wczytanie zapisu w trakcie.
- [ ] **Step 8: Commit** `git add assets src && git commit -m "engine: tyczka GNSS, promienie do nieba, panel odbiornika i zapis raportu"`.

---

### Task 6: Dokumentacja, paczka i weryfikacja końcowa

**Files:**
- Modify: `README.md`, `scripts/package.py`

**Interfaces:**
- Consumes: wszystkie poprzednie zadania.

- [ ] **Step 1: README**: rozdział „Pomiar GNSS (RTK)” – cel (nauka obsługi odbiornika), przebieg (tyczka, antena, start, czekanie na fix, zapis), typy rozwiązań z orientacyjną dokładnością, wpływ zasłonięcia nieba, układ współrzędnych (N0/E0), pliki (`pomiar_gnss.csv/.docx`, przerwany), opcja prawdziwego błędu, ograniczenia v1 (brak logu na żywo, brak stacji bazowej/statyki). Zaktualizuj zdanie „Tylko jedna robota” w sekcji ograniczeń.
- [ ] **Step 2: `scripts/package.py`**: w `INSTALACJA.txt` dopisz krótko robotę GNSS i folder wyników; wersja domyślna `0.2.0`.
- [ ] **Step 3: Weryfikacja**: `powershell -File tests/run.ps1` (`ALL OK`), `powershell -File scripts/build.ps1` (`BUILD OK`), `python scripts/package.py 0.2.0` i `unzip -l release/GothicSurveying-v0.2.0-G2A.zip | grep LVL_POLE`.
- [ ] **Step 4: Commit** `git add README.md scripts && git commit -m "docs: robota Pomiar GNSS (RTK), paczka 0.2.0"`. Wypychanie i release dopiero na polecenie użytkownika.

---

## Samokontrola planu

- **Pokrycie specyfikacji:** cele i przebieg (T4, T5), symulacja (T1–T2), układ współrzędnych (T2), dziennik/CSV/DOCX/podsumowanie (T3, T5), opcjonalny prawdziwy błąd (T4–T5), promienie do nieba (T5), model tyczki (T5), przepływ przerwania/zmiany świata (T5), dokumentacja (T6). Odstępstwa wymienione na górze.
- **Spójność typów:** `SkyMask`, `SolType`, `GnssFix`, `GnssSession`, `GridPos`, `GnssPoint`, `GnssReport`, `JobKind` mają jedną definicję i te same nazwy w kolejnych zadaniach.
- **Proporcje:** plan to decyzje (pliki, sygnatury, wartości, testy), bez treści funkcji poza wzorem PDOP i progami klasyfikacji, których testy nie wyznaczają jednoznacznie.
