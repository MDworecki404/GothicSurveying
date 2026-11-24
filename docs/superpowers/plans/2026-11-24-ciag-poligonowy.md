# Pomiar „Ciąg poligonowy" - plan wdrożenia

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ósmy rodzaj pomiaru odtwarzający prawdziwą procedurę ciągu poligonowego (kąty lewe w KL i KP, boki w obu kierunkach, rachunek z odchyłkami i poprawkami) w trzech rodzajach: nawiązany obustronnie, zamknięty, wiszący.

**Architecture:** Czysty rachunek w `calc/Polygon.hpp`, raport w `calc/PolygonReport.hpp`, automat stanowisk w `job/PolygonJob.hpp` (wzór: `job/TrigLevel.hpp`, widok lunety `TachyView` i szum z `calc/Tachy.hpp`), cienka integracja w kontrolerze. Limity odchyłek wynikają z dokładności symulowanego instrumentu.

**Tech Stack:** C++20 (testy g++ `tests/run.ps1`), MSVC x86 + Union/Gothic API w `engine/` (`scripts/build.ps1`).

**Spec:** `docs/superpowers/specs/2026-11-24-ciag-poligonowy-design.md` (wzory i przykład liczbowy: `docs/przewodnik-pomiary-geodezyjne.pdf`, sekcja 5)

## Global Constraints

- Teksty w grze bez polskich znaków (ASCII); README/dokumenty mogą je mieć.
- Zgodność wsteczna: zapisy stanu `ver=1`, id pozycji menu innych robót, testy `tests/test_*_job.cpp` zielone. `test_workflow.cpp` iteruje po `JobTypes()`: nowe id pozycji muszą trafić do tabeli sekcji (`job/Workflow.hpp`).
- `powershell -File tests/run.ps1` musi dać `ALL OK`; build `powershell -File scripts/build.ps1` → `BUILD OK` (od Task 5).
- Makra windows.h: `(std::min)(...)`, nie nazywać zmiennych `near`, `rad2`. Pliki łatające pisać narzędziem Write/Edit i uruchamiać z pliku (nie heredoc z `\n`).
- Niepewne wywołania silnika z `SvTrace`; nie zgadywać.
- Kąty w gradach (`g`), współrzędne N (=X) i E (=Y) w metrach; kąt poziomy lewy `β = K_przód − K_tył (mod 400g)`.
- Limity odchyłek to **założenie symulatora** (nie wartości z instrukcji) i tak mają być opisane w raporcie.
- Commity: małe, lokalne, na gałęzi `ciag-poligonowy`, każdy po zielonych testach, **bez linii Co-Authored-By i bez dopisków atrybucji**. Bez push, wydań i merge'a bez polecenia.

## Review Focus

1. Zawinięcie kątów przez 0g/400g (β bliskie 0 i 400, f_β o znaku przeciwnym po sprowadzeniu do (−200g, +200g]) – test w Task 1.
2. Poligon zamknięty obchodzony zgodnie i przeciwnie do wskazówek zegara (kąty zewnętrzne vs wewnętrzne) – test w Task 1.
3. Dane brzegowe: poligon z n < 3, brak nawiązań, ten sam punkt jako tył i przód, lustro poza 3–300 m, zasłonięta linia celowa, brak kompletu obserwacji – odmowa z komunikatem, bez wywrotki (Task 3, 4).
4. Zapis gry w trakcie stanowiska i wznowienie z częściowymi obserwacjami; „Powtórz stanowisko" nie dubluje obserwacji (Task 4).
5. Bok mierzony tylko z jednego końca (pierwszy i ostatni punkt) liczy się z jednej wartości, a nie zero ani NaN (Task 3).

---

## File Structure

- Create `src/calc/Polygon.hpp`: rachunek ciągu, limity z instrumentu.
- Create `src/calc/PolygonReport.hpp`: model raportu, CSV, DOCX, linie podsumowania.
- Create `src/job/PolygonJob.hpp`: robota (automat, menu, pomiar, zapis stanu).
- Modify `src/job/Job.hpp` (`JobKind::Polygon`, `FinishPolygon`), `src/job/Jobs.hpp` (rejestr), `src/job/Workflow.hpp`, `src/job/Help.hpp`, `src/engine/Controller.hpp`, `README.md`, `scripts/package.py`.
- Tests: `tests/test_polygon.cpp`, `tests/test_polygon_report.cpp`, `tests/test_polygon_job.cpp`.

---

### Task 1: Rachunek ciągu poligonowego (`calc/Polygon.hpp`)

**Files:** Create `src/calc/Polygon.hpp`; Test `tests/test_polygon.cpp`

**Interfaces:**
- Consumes: `NormGon`, `TachyNoise` z `calc/Tachy.hpp`; `TraverseKind` z `calc/Traverse.hpp`.
- Produces:
```cpp
double AzimuthGon(double n1, double e1, double n2, double e2);        // 0..400g, N=X, E=Y
double PolyAngleGon(double klBack, double klFore, double kpBack, double kpFore, double* twoCGon = nullptr);   // beta = (K_fore-K_back) usredniony z KL i KP (kierunki KP w ukladzie KL: Hz-200g); twoC = rozbieznosc
struct PolyInput {
  TraverseKind kind = TraverseKind::Anchored;            // Anchored = nawiazany obustronnie, Closed = zamkniety, Open = wiszacy
  double nN1 = 0, eN1 = 0;                               // N1: nawiazanie poczatkowe (Anchored, Open) albo punkt orientacji (Closed)
  double nA = 0, eA = 0;                                 // A: pierwszy punkt ciagu (znany)
  double nB = 0, eB = 0, nN2 = 0, eN2 = 0;               // Anchored: ostatni punkt ciagu B i nawiazanie koncowe N2
  std::vector<double> betaGon;                           // katy lewe: Anchored n (A..B), Closed n (kolejno od A), Open m (A..przedostatni)
  double orientBetaGon = 0;                              // Closed: kat przy A od N1 do P1 (K_P1 - K_N1)
  std::vector<double> sideM;                             // boki poziome: Anchored n-1, Closed n (ostatni = P_{n-1}->A), Open m
  double sigmaArcsec = 5.0, distMm = 2.0, distPpm = 2.0; // szum instrumentu (limity)
};
struct PolyResult {
  bool valid = false; std::string error; TraverseKind kind = TraverseKind::Anchored; int n = 0; bool clockwise = false;   // clockwise: Closed
  double aStartGon = 0, aEndGon = 0;                     // azymuty nawiazania (Anchored)
  bool hasAngleCheck = false, hasLinearCheck = false, angleOk = true, linearOk = true;
  double fBetaGon = 0, vBetaGon = 0, fBetaLimGon = 0;
  std::vector<double> betaCorrGon, azimuthGon, dX, dY, vx, vy, x, y;   // x,y: wspolrzedne punktow P0..Pn (P0 = A)
  double fx = 0, fy = 0, fd = 0, fdLim = 0, sumD = 0;
};
PolyResult PolygonCompute(const PolyInput& in);
```
`PolygonCompute` ma też zwracać `valid=false` z opisem w `error` dla: za mało kątów lub boków (Anchored n ≥ 2, Closed n ≥ 3, Open m ≥ 1), niezgodnej liczby boków, długości boku ≤ 0.

- [ ] **Step 1: Test** `tests/test_polygon.cpp` (`#include "calc/Polygon.hpp"`, asercje na liczbach z przewodnika; tolerancje podane):
  - `azimuth_gon_quadrants`: `AzimuthGon(0,0,1,0)==0`, `(0,0,0,1)==100`, `(0,0,-1,0)==200`, `(0,0,0,-1)==300` (±1e-9); `AzimuthGon(1000,1000,1100,1200)≈70.4833` (±1e-4).
  - `angle_mean_removes_collimation`: dla `klBack=10, klFore=110` oraz `kpBack=10.1+200, kpFore=110.1+200` (kolimacja 0.1g w obu) `PolyAngleGon==100.0` (±1e-9), a z `twoCGon` ≈ rozbieżność; test zawinięcia: `klBack=399.9, klFore=0.1` → `0.2` (±1e-9).
  - `guide_example_anchored`: N1(1000,1000), A(1100,1200), B(1460,1560), N2(1600,1640); `betaGon={184.8109,203.7398,172.4127,201.6052}`; `sideM={170.306,199.991,147.662}`. Oczekiwane: `valid`, `aStartGon≈70.4833`, `aEndGon≈33.0499` (±1e-4), `fBetaGon≈+0.0020` (±2e-4), `vBetaGon≈-0.0005` (±1e-4), `azimuthGon[0..2]≈{55.2937,59.0330,31.4452}` (±3e-4), `fx≈0.014`, `fy≈0.009`, `fd≈0.017` (±0.002), `x[3]==1460.000` i `y[3]==1560.000` (±1e-6; poprawki domykają ciąg), `x[1]≈1210.001`, `y[1]≈1330.008` (±0.003), `angleOk && linearOk`, `fBetaLimGon≈0.00617` (±1e-5), `fdLim > fd`.
  - `anchored_bad_angle_detected`: ten sam przykład z `betaGon[1] += 0.05` → `angleOk==false`.
  - `angle_residual_wraps`: dane tak dobrane (np. `betaGon` z sumą dającą `fβ ≈ -399.998` przed sprowadzeniem), że `fBetaGon` po sprowadzeniu to `+0.002` (±1e-4), nie `-399.998`.
  - `hanging_has_no_checks`: Open, N1(1000,1000), A(1100,1200), `betaGon={184.8109,203.7398}`, `sideM={170.306,199.991}` → `hasAngleCheck==false && hasLinearCheck==false`, `x[1]≈1210.005`, `y[1]≈1330.011`, `x[2]≈1329.999`, `y[2]≈1490.005` (±0.001).
  - `closed_square_both_directions`: A=(1000,1000), orientacja N1=(1000,900), orientBeta=100; obchód zgodny ze wskazówkami (A→(1100,1000)→(1100,1100)→(1000,1100)): `betaGon={300,300,300,300}`, `sideM={100,100,100,100}` → `clockwise==true`, `fBetaGon≈0`, `x/y` wracają do A (±1e-6); obchód przeciwny: A→(1000,1100)→(1100,1100)→(1100,1000), orientacja N1=(900,1000) (azymut A→N1 = 200g), orientBeta=… (wyprowadź: K_P1−K_N1 dla P1 na wschód = 300g), `betaGon={100,100,100,100}` → `clockwise==false`, `fBetaGon≈0`. Dodaj `betaGon[1] += 0.01` → `fBetaGon≈+0.01`, `vBetaGon≈-0.0025`.
  - `invalid_inputs`: Anchored z 1 kątem, Closed z 2 kątami, niezgodna liczba boków, bok `0` → `valid==false && !error.empty()`.
- [ ] **Step 2: Run** `powershell -File tests/run.ps1` → FAIL (brak `Polygon.hpp`).
- [ ] **Step 3: Implement** wg wzorów ze specyfikacji: azymut przez `atan2`; azymuty `A_{i,i+1} = A_{i−1,i} + β_i − 200g`; `f_β` sprowadzony do (−200g, +200g]; Closed: kierunek obchodu ze znaku pola (wzór na pole z surowych przyrostów po wstępnych azymutach) → `Σβ_teor = (n∓2)·200g`; azymut pierwszego boku Closed `= AzimuthGon(A→N1) + orientBetaGon`; limity: `m_β = sigmaArcsec/3600·10/9` gon, `fBetaLim = 2·m_β·√n`; `m_d,i = (distMm/1000 + distPpm·1e-6·d_i)/√2`, `fdLim = 2·√(Σm_d,i² + m_β²[rad]·Σr_i²)` z `r_i` = odległość stanowiska od punktu końcowego (Anchored) albo od środka ciężkości (Closed) po poprawkach.
- [ ] **Step 4: Run** `tests/run.ps1` → `ALL OK`.
- [ ] **Step 5: Commit** `git add src/calc/Polygon.hpp tests/test_polygon.cpp && git commit -m "poligon: rachunek ciagu (azymuty, odchylki, poprawki, limity z instrumentu)"`

---

### Task 2: Raport ciągu poligonowego (`calc/PolygonReport.hpp`)

**Files:** Create `src/calc/PolygonReport.hpp`; Test `tests/test_polygon_report.cpp`

**Interfaces:**
- Consumes: `PolyInput`, `PolyResult` (Task 1), `Para/Row/WrapDocx` z `docxdetail` w `calc/Docx.hpp`, `fbdetail::Num`.
- Produces:
```cpp
struct PolyStationRec { std::string name, back, fore; double klBack = 0, klFore = 0, kpBack = 0, kpFore = 0, betaGon = 0, twoCGon = 0; };
struct PolySideRec { std::string from, to; double dFwdM = 0, dBackM = 0, dMeanM = 0; bool hasFwd = false, hasBack = false; };   // dFwd z poczatku boku, dBack z konca
struct PolygonReport {
  bool complete = true, showTrueError = false; std::string dateText; TraverseKind kind = TraverseKind::Anchored;
  std::vector<std::string> names;                         // nazwy punktow P0..Pn
  std::vector<PolyStationRec> stations; std::vector<PolySideRec> sides;
  PolyInput in; PolyResult res;
  std::vector<double> trueN, trueE;                       // prawdziwe pozycje znakow (gdy showTrueError), ta sama kolejnosc co names
};
std::string PolygonCsv(const PolygonReport&);             // CSV ';' z sekcjami: katy, boki, rachunek, wspolrzedne
std::string PolygonDocx(const PolygonReport&);            // pakiet .docx (ZIP) jak ProjectReportDocx
std::vector<std::string> PolygonSummaryLines(const PolygonReport&);
```
- [ ] **Step 1: Test** `tests/test_polygon_report.cpp`: raport zbudowany z przykładu z Task 1 (nazwy `A,1,2,B`): `PolygonCsv` zawiera `"Ciag poligonowy"`, nagłówki sekcji kątów/boków/rachunku, `"184.8109"` albo średnie β, `"fbeta"`, wiersz współrzędnych `"1460.000"`, oraz zdanie `"zalozenie symulatora"` przy limitach; `PolygonDocx` zaczyna się `PK`, zawiera `word/document.xml`, `Ciag poligonowy`, `zalozenie symulatora`; `PolygonSummaryLines` zawiera linię z `f_beta` i `f_d` i słowo `"w granicach"` albo `"PRZEKROCZONE"` zależnie od `angleOk/linearOk`; z `showTrueError` w CSV pojawiają się kolumny błędów (`blad_N_m`). Wiszący: w CSV `"brak kontroli"` zamiast odchyłek.
- [ ] **Step 2: Run** → FAIL. **Step 3: Implement** (wzór: `calc/TachyReport.hpp`, `calc/ProjectReport.hpp`); liczby `%.4f` dla kątów, `%.3f` dla metrów. **Step 4: Run** `ALL OK`.
- [ ] **Step 5: Commit** `git commit -m "poligon: raport (CSV, DOCX, podsumowanie)"` (z `git add` obu plików).

---

### Task 3: Robota - ustawianie nawiązań i punktów, stanowiska i pomiar (`job/PolygonJob.hpp`, część 1)

**Files:** Modify `src/job/Job.hpp` (`JobKind::Polygon`; `virtual PolygonReport FinishPolygon(bool complete) { (void)complete; return PolygonReport{}; }`, include `calc/PolygonReport.hpp`); Create `src/job/PolygonJob.hpp`; Test `tests/test_polygon_job.cpp`

**Interfaces:**
- Consumes: Task 1, 2; `IWorld` (`ControlNearHero`, `PlaceTachymeterAt`, `PlaceTachymeterOnPoint`, `PlaceBacksightAt`, `PlacePrismOnPoint`, `PlaceMarker`, `HeroGround`, `StationGrid`, `PointGrid`, `PoleGroundGrid`, `LineClear`, `RemoveBacksight`, `TwoFaceByDefault`, `InstrumentLevelingEnabled`), `TachyMeasureFace`, `TachyAimError`, `TachyNoise`, `GnssRng`, `Libella`.
- Produces: `class PolygonJob : public Job`; `std::unique_ptr<Job> CreatePolygonJob(IWorld&, const JobOptions&)`; `JobKind::Polygon`; id pozycji menu: `ref_n1`, `ref_a`, `ref_b`, `ref_n2` (wybór z listy osnowy przez `NeedsChoice/TachyChoices`, id `"ref_n1:<idPunktuOsnowy>"` itd.), `addpt` (punkt ciągu pod stopami), `inst`, `prism`, `prismh`, `view`, `redo`, `calc`, `finish`; `TachyFire/TachyFlip/TachyPanel` jak w `TrigLevel`.
- Zasady (z decyzji spec): kind z `JobOptions.kind`; wymagane nawiązania wg rodzaju (Anchored: N1, A, B, N2 i ≥ 0 punktów pośrednich; Closed: A, N1 i ≥ 2 punkty nowe; Open: N1, A i ≥ 1 punkt nowy); kolejność stanowisk: Anchored A,1..,B (tył N1 na A, przód N2 na B), Open A..P_{m−1} (tył N1 na A), Closed A..P_{n−1} cyklicznie (na A: dodatkowa obserwacja orientacji na N1 w KL i KP). Stanowisko gotowe, gdy ma po jednej obserwacji tyłu i przodu w KL i w KP (kolejność dowolna, jedno lustro przestawiane przez menu); stanowisko jest „aktualne", dopóki nie komplet, potem automatycznie następne. Kolimacja i indeks tachimetru oraz zero koła poziomego losowane przy postawieniu tachimetru (jak `NewSetup` w `TachyStationJob`).

- [ ] **Step 1: Test** `tests/test_polygon_job.cpp` ze sztucznym światem `FakePoly : IWorld` (wzór: `tests/test_tachy_job.cpp`): lista punktów osnowy (`ControlNearHero`), prawdziwe pozycje znaków, `PlaceTachymeterAt/OnPoint` ustawia `StationGrid()`, `PlaceBacksightAt(1, cp)`/`PlacePrismOnPoint(id)` ustawia pozycję lustra, `PoleGroundGrid()`/`PointGrid(1)` zwracają ją, `HeroGround` zwraca ustawialną pozycję. Pomocnik `Aim(job, cel)` liczy azymut i pochylenie z prawdziwych pozycji i woła `TachyFire`. Szum zerowy (`TachyNoise` przez hak testowy `SetNoise(const TachyNoise&)` w klasie).
  - `registry_and_kind`: `JobTypes()` zawiera `polygon` (po Task 5; w tym zadaniu tworzenie przez `CreatePolygonJob`), `Kind()==JobKind::Polygon`.
  - `setup_refs_by_kind`: Anchored bez `ref_*` → `view` niedostępne, `NextStep()=="ref_n1"`; po wybraniu N1, A, B, N2 → `NextStep()=="addpt"` albo `"inst"`; Open nie wymaga `ref_b/ref_n2` (pozycje niewidoczne albo niedostępne); Closed wymaga A i N1.
  - `addpt_places_marker_and_names_points`: `addpt` ustawia `HeroGround`, wywołuje `PlaceMarker`, kolejne punkty `1,2,...`; brak gruntu → komunikat, bez punktu; nie można dodać punktu przed A.
  - `station_collects_four_observations`: dla Anchored A–1–B: na stanowisku A (tył N1, przód 1) cztery `Aim` (KL tył, KL przód, `TachyFlip`, KP tył, KP przód) → stanowisko kompletne, `NextStep` przechodzi na stanowisko 1; kąt β w raporcie (`FinishPolygon(false)`) równa się prawdziwemu kątowi lewemu z geometrii (±1e-6 g) mimo zera koła poziomego i kolimacji wylosowanych w `NewSetup` (bez szumu).
  - `refuses_bad_shots`: lustro poza 3-300 m, `LineClear==false`, brak trafienia w lustro → komunikat, brak obserwacji; to samo lustro dwa razy w tym samym położeniu → komunikat „juz zmierzone".
  - `sides_measured_from_both_ends`: po stanowiskach A i 1 bok A–1 ma `hasFwd && hasBack` i `dMeanM` równe prawdziwej odległości poziomej; bok N1–A nie występuje w `sides`; bok zmierzony tylko z jednego końca (ostatni punkt przed końcem) ma `hasFwd==true`, `hasBack==false` i `dMeanM==dFwdM`.
  - `redo_station_replaces_observations`: `redo` czyści obserwacje bieżącego (albo wskazanego jako ostatni ukończony) stanowiska; ponowny pomiar nie dubluje wpisów.
- [ ] **Step 2: Run** → FAIL. **Step 3: Implement** `PolygonJob` (menu + `OnMenu` z blokadą pozycji niedostępnych jak w `TrigLevel`, `TachyFire` wg opisu, `FinishPolygon(false)` zwraca dotychczasowe stacje i boki w `PolygonReport` bez rachunku); `JobKind::Polygon` + domyślne `FinishPolygon` w `Job.hpp`. **Step 4: Run** `tests/run.ps1` → `ALL OK` (istniejące testy bez zmian; `test_workflow` jeszcze nie widzi nowej robot, bo nie jest w rejestrze).
- [ ] **Step 5: Commit** `git commit -m "poligon: robota - nawiazania, punkty ciagu, stanowiska i pomiar"`

---

### Task 4: Robota - podgląd rachunku, zakończenie, zapis stanu i prowadzenie (część 2)

**Files:** Modify `src/job/PolygonJob.hpp`; Test `tests/test_polygon_job.cpp` (dopisz)

**Interfaces:**
- Consumes: Task 1-3.
- Produces: `calc` (podgląd odchyłek w panelu i komunikacie), `FinishPolygon(true)` z pełnym rachunkiem (`PolygonCompute`) i raportem; `CanFinish()` (komplet wymaganych stanowisk), `CanSaveState()==true`, `SaveState/LoadState` (kind, nawiązania, punkty z prawdziwymi pozycjami, obserwacje, bieżące stanowisko, położenie lunety, wysokość lustra, libella), `NextStep()` (nawiązania → punkty → tachimetr → lustro → `view` → następne stanowisko → `calc` → `finish`), `Hud()`, `StatusLines()` bez zmian domyślnych, `SummaryLines()`.
- [ ] **Step 1: Test** (dopisz):
  - `full_anchored_matches_truth`: przykład z przewodnika odtworzony w świecie sztucznym (punkty N1, A, B, N2 w osnowie; punkty 1 i 2 zakładane przez `addpt` z `HeroGround` na prawdziwych pozycjach wynikających z przewodnika), pełny pomiar wszystkich stanowisk bez szumu → `FinishPolygon(true).res.valid`, `fBetaGon≈0` (±1e-6; brak szumu), współrzędne punktów 1, 2 równe prawdziwym (±1e-6), `angleOk && linearOk`.
  - `calc_preview_before_finish`: `calc` przed kompletem → niedostępne z powodem; po komplecie panel/komunikat zawiera `f_beta` i `f_d`.
  - `bad_angle_flagged_then_redo_fixes`: ze zepsutym pomiarem jednego kąta (hak `SetNoise` z dużym szumem kątowym albo ręcznie przesunięte `Aim` o 0.05g) `calc` zgłasza przekroczenie; po `redo` tego stanowiska i poprawnym pomiarze `angleOk==true`.
  - `closed_and_hanging_flows`: kwadrat (Closed, obchód zgodny i przeciwny) oraz wiszący (Open) przechodzą cały przebieg; Closed wymaga obserwacji orientacji na N1 przy A; Open: `finish` bez `calc`.
  - `save_load_midstation`: po 2 z 4 obserwacji stanowiska 1 `SaveState` → nowa robota `LoadState` → dokończenie daje ten sam wynik co bez zapisu; `LoadState` z niepasującym stanem (inny `kind`) zwraca false.
  - `finish_requires_all_stations`: `CanFinish()==false` do kompletu; `finish` niedostępne z powodem w `why`.
  - `nextstep_always_enabled_or_special`: dla kilku faz `NextStep()` wskazuje pozycję `Menu()` aktywną albo `view`.
- [ ] **Step 2: Run** → FAIL. **Step 3: Implement**; `NextStep()` zwraca id pozycji aktywnej (`view` jest w `Menu()`). **Step 4: Run** `ALL OK`.
- [ ] **Step 5: Commit** `git commit -m "poligon: podglad rachunku, zakonczenie, zapis stanu, prowadzenie"`

---

### Task 5: Rejestr, sekcje menu, pomoc i integracja z kontrolerem

**Files:** Modify `src/job/Jobs.hpp`, `src/job/Job.hpp` (flaga `polygon` w `JobType`), `src/job/Workflow.hpp`, `src/job/Help.hpp`, `src/engine/Controller.hpp`, `src/engine/ProjectState.hpp` (jeśli trzeba), `tests/test_workflow.cpp`, `tests/test_polygon_job.cpp`

**Interfaces:**
- Consumes: Task 3, 4.
- Produces: rejestr `{"polygon", "Ciag poligonowy", &CreatePolygonJob, ..., polygon = true}`; wpisy tabeli sekcji: `ref_n1, ref_a, ref_b, ref_n2, addpt` w „Przygotowanie" (przed `inst`), `redo` w „Pomiar", `calc` w „Wyniki" (przed `finish`); `HelpContextFor(JobKind::Polygon)=="job:polygon"` i linie pomocy `job:polygon`; w kontrolerze: start (wybór rodzaju: nawiązany obustronnie / zamknięty / wiszący, potem pytanie o prawdziwy błąd), `CtrlFinishPolygon` (rachunek, eksport `pomiar_poligonowy.csv/.docx` do `MakeJobDir()`, panel z `PolygonSummaryLines`, `CtrlMarkEnd`, `CtrlEndJob`), zapis częściowy przy przerwaniu i zmianie świata (`pomiar_poligonowy_przerwany.csv`), klawisz F10 jak dla tachimetrii, `CtrlOpenChoice` dla `ref_*` (komunikaty braku punktów osnowy).
- [ ] **Step 1: Test:** `tests/test_workflow.cpp::every_menu_id_has_a_section` ma objąć nową robotę automatycznie (iteruje `JobTypes()`); dodaj `help_has_lines_for_every_context` o `"job:polygon"` i `HelpContextFor(JobKind::Polygon)`; w `test_polygon_job.cpp` `registry_and_kind` sprawdza wpis `polygon` w `JobTypes()`.
- [ ] **Step 2: Run** → FAIL (nowe id bez sekcji, brak kontekstu pomocy). **Step 3: Implement** zmiany z listy powyżej (kontroler cienko, wzorem tachimetrii i niwelacji trygonometrycznej). **Step 4: Run** `tests/run.ps1` → `ALL OK` oraz `powershell -File scripts/build.ps1` → `BUILD OK`.
- [ ] **Step 5: Commit** `git commit -m "poligon: rejestr, sekcje menu, pomoc i integracja z kontrolerem"`

---

### Task 6: Dokumentacja

**Files:** Modify `README.md`, `scripts/package.py`

- [ ] **Step 1:** README: sekcja „Ciąg poligonowy" (procedura, trzy rodzaje, przebieg w menu, kąty lewe, boki w obu kierunkach, rachunek i oznaczenia, limity z instrumentu jako założenie symulatora, pliki wyników, ograniczenia: tylko sytuacja, bez wysokości i wyrównania ścisłego, nawiązania z osnowy); lista robót w README i „Znanych ograniczeniach" uzupełniona o 8. rodzaj; wzmianka w sekcji „Struktura".
- [ ] **Step 2:** `scripts/package.py` (tekst INSTALACJA): jedna pozycja o ciągu poligonowym (nowy pomiar → ręczny wybór → „Ciag poligonowy"). Bez zmiany wersji i bez budowania paczki.
- [ ] **Step 3: Run** `tests/run.ps1` `ALL OK`, `scripts/build.ps1` `BUILD OK`; `python -c "import ast; ast.parse(open('scripts/package.py',encoding='utf-8').read())"`.
- [ ] **Step 4: Commit** `git commit -m "dokumentacja: ciag poligonowy"`
