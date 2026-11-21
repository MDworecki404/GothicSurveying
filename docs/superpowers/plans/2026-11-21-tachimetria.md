# Tachimetria (pomiar biegunowy) – plan implementacji

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Nowa robota „Tachimetria (pomiar biegunowy)”: stanowisko o znanych współrzędnych, orientacja na punkt nawiązania, pomiar punktów z lustrem w lunecie z pochylaniem, współrzędne liczone z odczytanych kątów i odległości, dziennik (tabela w grze, CSV, DOCX).

**Architecture:** Czyste obliczenia w `src/calc/Tachy.hpp`, raport w `src/calc/TachyReport.hpp` (testy g++). Automat kroków `src/job/TachyStation.hpp` przez `Job`/`IWorld` (testy z fałszywym światem). Silnik: nowy widok `TachyView.hpp` (luneta z pochyleniem), rozszerzenia `World.hpp` i `Controller.hpp`. Niwelacja i GNSS bez zmian.

**Tech Stack:** C++20 (g++ w testach, MSVC x86 plugin Union/Gothic API), Python (`assets/make_models.py`), PowerShell.

**Spec:** `docs/superpowers/specs/2026-11-21-tachimetria-design.md`

## Global Constraints

- Teksty widoczne w grze: bez polskich znaków. CSV/DOCX: polskie znaki dozwolone.
- Pętla klatki tylko hook `zCWorld::Render` (`ControllerTick`); panele tylko `PanelOpen`/`PanelClose` (złota ramka, `LV_FONT`); odstęp wierszy w panelach stały w jednostkach ekranu (jak `CtrlUpdateStatusPanel`: `y_local = v * 8192 / H`).
- Klawisze ze stałych w `src/Keys.hpp`; menu Home, luneta F10; strzałki, Shift, Enter jak w lunecie niwelacyjnej; nowych klawiszy nie dodajemy.
- Niwelacja, GNSS, `Scope.hpp`, eksport i testy tych robót nie mogą się zmienić (istniejące testy muszą dalej dawać `ALL OK`).
- TDD dla `src/calc` i `src/job`; testy: `powershell -File tests/run.ps1` (`ALL OK`); build: `powershell -File scripts/build.ps1` (`BUILD OK`); instalacja: `powershell -File scripts/install.ps1 -Game "C:\Program Files (x86)\Steam\steamapps\common\Gothic II"`.
- Literalne `\n` w C++ wpisuj narzędziem Edit (heredoc zamienia je na newline). `std::max`/`std::min` zapisuj `(std::max)(...)` (makra `windows.h`).
- Jednostki: kąty w gradach (gon, 400 g = 360°), odległości i współrzędne w metrach; azymut od północy (+Z) zgodnie z ruchem wskazówek zegara (+X = wschód); układ lokalny z `GnssToGrid`.
- Stałe: wysokość instrumentu 1,500 m (`LV_INSTR_H` = 150 cm), tyczka z lustrem 1,500 m, presety wysokości lustra 1,300 / 1,500 / 1,800 / 2,000 m (domyślnie 1,500), tolerancja celowania 0,08 m (poziomo i pionowo), zasięg 3–300 m, szum: kąty 5″, odległość 2 mm + 2 ppm.
- Commity bez żadnych linii `Co-Authored-By` ani innych podpisów (życzenie użytkownika). Praca na gałęzi `tachimetria`, scalenie/wypchnięcie/release tylko na polecenie.

## Review Focus

- Pomiar przed orientacją: odmowa z komunikatem „Najpierw orientacja”, żadne dane się nie zapisują.
- Celowanie obok lustra (> 8 cm), poza zasięgiem 3–300 m, zasłonięta linia: komunikat, brak punktu w dzienniku.
- Pochylenie lunety do skrajnych kątów (zenit/nadir) i obrót przez 0°/360°: Hz zawsze w [0, 400) g, V w [0, 200] g, brak NaN.
- Przestawienie stanowiska albo punktu nawiązania po orientacji: orientacja unieważniona (ponowna orientacja wymagana).
- Przerwanie robota/zmiana świata w widoku lunety: widok zamknięty, gra odpauzowana, częściowy raport zapisany.

## Struktura plików

- Create: `src/calc/Tachy.hpp`, `src/calc/TachyReport.hpp`, `src/job/TachyStation.hpp`, `src/engine/TachyView.hpp`, `tests/test_tachy.cpp`, `tests/test_tachy_report.cpp`, `tests/test_tachy_job.cpp`.
- Modify: `src/job/Job.hpp`, `src/job/Jobs.hpp`, `src/engine/World.hpp`, `src/engine/Placement.hpp`, `src/engine/Export.hpp`, `src/engine/Controller.hpp`, `src/Plugin.hpp`, `assets/make_models.py`, `README.md`, `scripts/package.py`.

---

### Task 1: Obliczenia tachimetryczne (`calc/Tachy.hpp`)

**Files:**
- Create: `src/calc/Tachy.hpp`
- Test: `tests/test_tachy.cpp`

**Interfaces:**
- Consumes: `GnssRng` z `src/calc/Gnss.hpp` (`Normal()`).
- Produces (wszystko bez Gothic API):
  - `double GonToDeg(double g); double DegToGon(double d); double NormDeg(double d);` (do [0,360)) `double NormGon(double g);` (do [0,400)).
  - `double TachyAzimuthDeg(double n1, double e1, double n2, double e2);` azymut punkt 1 → punkt 2, `atan2(dE, dN)`, [0,360).
  - `struct TachyNoise { double angleArcsec = 5.0, distMm = 2.0, distPpm = 2.0; };`
  - `struct TachyReading { double hzGon, vGon, slopeM; };` (V = kąt zenitalny)
  - `struct TachyStation { double n, e, h; double instrH = 1.5; double azBackDeg; double az0Deg; };` (`az0Deg` = azymut kamery na nawiązanie z orientacji)
  - `double TachyOrient(double camAzDeg, const TachyNoise&, GnssRng&);` → `az0` (azymut kamery z szumem kątowym).
  - `TachyReading TachyMeasure(double camAzDeg, double camPitchDeg, double az0Deg, double trueSlopeM, const TachyNoise&, GnssRng&);` (`Hz = NormGon(DegToGon(camAz - az0 + noise))`, `V = DegToGon(90 - pitch + noise)` obcięte do [0,200], `slope = true + noise`).
  - `struct TachyResult { double n, e, h, hDistM, azDeg; };` `TachyResult TachyPolar(const TachyStation&, const TachyReading&, double prismSetM);` (`Az = azBack + Hz[deg]`, `dPoz = D·sin V`, `N = Ns + dPoz·cos Az`, `E = Es + dPoz·sin Az`, `H = Hs + instrH + D·cos V − prismSetM`).
  - `struct TachyAim { double lateralM, verticalM, distM; bool ok; };` `TachyAim TachyAimError(double dN, double dE, double dH, double camAzDeg, double camPitchDeg);` – (dN, dE, dH) od oka lunety do środka celu; `ok` gdy |lateral| ≤ 0,08, |vertical| ≤ 0,08 i dystans skośny w [3, 300] m.

- [ ] **Step 1: Napisz testy** `tests/test_tachy.cpp` (`main` na końcu drukuje `test_tachy OK`), z `TachyNoise{0,0,0}` tam, gdzie wynik ma być dokładny:
  - `conversions`: `GonToDeg(100) == 90`, `DegToGon(90) == 100`, `NormDeg(-90) == 270`, `NormGon(-100) == 300`, `NormGon(400) == 0`.
  - `azimuth_directions`: z (0,0): na północ (1,0) 0°, wschód (0,1) 90°, południe (−1,0) 180°, zachód (0,−1) 270° (±1e-9).
  - `polar_east_level`: stacja (5500000, 6500000, 100), `azBackDeg = 0`, `instrH = 1.5`; odczyt `{100, 100, 50}` i lustro 1,5 → `n == 5500000`, `e == 6500050`, `h == 100` (±1e-9); lustro ustawione 1,8 → `h == 99.7`.
  - `polar_sloped_north`: cel 30 m na północ i 10 m wyżej oka: `slope = sqrt(1000)`, `pitch = atan(10/30)` w stopniach → `TachyMeasure` (zero szumu, `az0 = 0`, `camAz = 0`) daje `vGon ≈ 79.5167` (±1e-3); `TachyPolar` daje `n = Ns + 30`, `e = Es`, `h = Hs + 1.5 + 10 − 1.5` (±1e-6).
  - `orientation_zeroes_hz`: `az0 = TachyOrient(37.5, {0,0,0}, rng) == 37.5`; `TachyMeasure(37.5, 0, 37.5, 10, ...)` ma `hzGon == 0`; `camAz = 37.5 + 90` daje `100`; `camAz` przez 0°/360° (np. `az0 = 350`, `camAz = 10` → `hz = DegToGon(20)`).
  - `extreme_pitch_stays_finite`: pitch ±90° i `NaN`-odporny: `vGon` w [0,200], `hzGon` w [0,400), wszystkie `isfinite`.
  - `aim_error_and_limits`: oko → cel 100 m na wschód (`dE=100`), `camAz = 90`, `camPitch = 0`: `ok`, `lateral ≈ 0`; cel przesunięty o 0,05 m na południe: `ok`; o 0,10 m: `!ok`; 0,10 m wyżej (`dH = 0.1`): `!ok`; dystans 2 m i 301 m: `!ok`.
  - `noise_statistics`: 20000 pomiarów `TachyMeasure` przy domyślnym szumie, cel 100 m: odchylenie standardowe `hz` w [0,9; 1,1]·(5″ w gradach ≈ 0,001543 g), odległości w [0,9; 1,1]·(0,002 + 2e-6·100 = 0,0022 m).
- [ ] **Step 2: Uruchom** `tests/run.ps1` – oczekiwane `COMPILE FAIL test_tachy.cpp`.
- [ ] **Step 3: Zaimplementuj** `Tachy.hpp` według sygnatur; `TachyAimError`: `dPoz = hypot(dN,dE)`, `lateral = dPoz·sin(angdiff(azCel, camAz))`, `vertical = slope·sin(elCel − pitch)`; kąty w stopniach, szum kątowy `angleArcsec/3600` stopnia.
- [ ] **Step 4: Uruchom testy** – oczekiwane `ALL OK`.
- [ ] **Step 5: Commit** (bez podpisów): `git add src/calc/Tachy.hpp tests/test_tachy.cpp && git commit -m "calc: obliczenia tachimetryczne (katy, orientacja, wynik biegunowy, celowanie)"`.

---

### Task 2: Raport tachimetryczny (`calc/TachyReport.hpp`)

**Files:**
- Create: `src/calc/TachyReport.hpp`
- Test: `tests/test_tachy_report.cpp`

**Interfaces:**
- Consumes: Task 1 (`TachyStation`), `fbdetail::Num`/`Csv` z `FieldBook.hpp`, `docxdetail::Para`/`Row`/`WrapDocx` z `Docx.hpp`.
- Produces:
  - `struct TachyPoint { std::string name, timeText; double hzGon = 0, vGon = 0, slopeM = 0, hDistM = 0, dHM = 0, n = 0, e = 0, h = 0, prismM = 1.5; std::string note; bool hasTrue = false; double dNm = 0, dEm = 0, dHm = 0, d2Dm = 0; };`
  - `struct TachyReport { bool complete = true, showTrueError = false; std::string dateText; double stationN = 0, stationE = 0, stationH = 0, instrH = 1.5, azBackGon = 0; std::vector<TachyPoint> points; };`
  - `std::string TachyCsvHeader(bool trueErr); std::string TachyCsvRow(const TachyPoint&, int nr, bool trueErr);` (bez końca linii)
  - `std::string TachyToCsv(const TachyReport&);` (pierwsza linia `# UWAGA: pomiar niekompletny (robota przerwana)` gdy niekompletny; potem linia `# stanowisko;N;E;H;wys_instrumentu;azymut_nawiazania_g` z wartościami; potem nagłówek i wiersze)
  - `std::string TachyBuildDocx(const TachyReport&);` (tabela o szerokości mieszczącej się na stronie: `14000 / liczba_kolumn` dxa)
  - `std::vector<std::string> TachySummaryLines(const TachyReport&);`

- [ ] **Step 1: Napisz testy** `tests/test_tachy_report.cpp`:
  - `csv_header_and_row`: nagłówek bez błędu to dokładnie `nr;punkt;czas;Hz_g;V_g;D_skosna_m;d_pozioma_m;dh_m;N_m;E_m;H_m;wys_lustra_m;uwagi`, z błędem dodatkowo `;blad_N_m;blad_E_m;blad_H_m;blad_2D_m`. Punkt `{T1, 09:15:03, hz 123.45678, v 98.76543, slope 55.1234, hDist 55.0, dH 3.4567, n 5500012.3456, e 6500034.5, h 12.3456, prism 1.5}` daje wiersz `1;T1;09:15:03;123.4568;98.7654;55.123;55.000;3.457;5500012.346;6500034.500;12.346;1.500;` (kąty 4 miejsca, odległości i współrzędne 3).
  - `csv_station_line_and_quoting`: linia `# stanowisko;5500000.000;6500000.000;100.000;1.500;0.0000`; uwaga `a;b "c"` cytowana; z `hasTrue` na końcu `;-0.003;0.004;0.008;0.005` (`dN = -0.0032`, `dE = 0.0041`, `dH = 0.0078`, `d2D = 0.0052` m).
  - `incomplete_flag`: `complete=false` → pierwsza linia zaczyna się od `# UWAGA: pomiar niekompletny`.
  - `docx_contains_points`: archiwum zaczyna się od `PK`, zawiera `T1`, `Tachimetria` i `w:w="1000"` przy 14 kolumnach (z błędem 18 kolumn: `w:w="777"`).
  - `summary_lines`: 3 punkty o `slope` 20/40/60 → linie zawierają `Punkty: 3`, `srednia odleglosc: 40.0 m`, `najdluzsza celowa: 60.0 m`; przy `hasTrue` linia `Najgorszy punkt: <nazwa> (<d2D> m)` z maksymalnym `d2Dm`.
- [ ] **Step 2: Uruchom** `tests/run.ps1` – oczekiwane `COMPILE FAIL test_tachy_report.cpp`.
- [ ] **Step 3: Zaimplementuj** `TachyReport.hpp` (wzorem `GnssReport.hpp`; w DOCX tytuł „Tachimetria (pomiar biegunowy)”, data, linia stanowiska, tabela, podsumowanie).
- [ ] **Step 4: Uruchom testy** – oczekiwane `ALL OK`.
- [ ] **Step 5: Commit**: `git add src/calc tests && git commit -m "calc: raport tachimetryczny (CSV, DOCX, podsumowanie)"`.

---

### Task 3: Robota tachimetryczna (`job/TachyStation.hpp`)

**Files:**
- Create: `src/job/TachyStation.hpp`
- Modify: `src/job/Job.hpp`, `src/job/Jobs.hpp`
- Test: `tests/test_tachy_job.cpp`

**Interfaces:**
- Consumes: Task 1–2; `GnssRng` (`Gnss.hpp`), `GridPos`.
- Produces (w `Job.hpp`):
  - `JobKind` dostaje `Tachy`.
  - `IWorld` – nowe metody niepure z domyślnym ciałem: `virtual GridPos StationGrid() const { return {0,0,0}; }` (prawdziwe współrzędne instrumentu), `virtual GridPos PointGrid(int pointId) const { return {0,0,0}; }`, `virtual bool PlacePrism() { return false; }` (tyczka z lustrem w miejscu `pole_`), `virtual bool LineClear(bool toPrism) const { return true; }`.
  - `Job` – metody z domyślnym ciałem: `virtual TachyReport FinishTachy(bool complete) { return TachyReport{}; }`, `virtual std::string TachyFire(double camAzDeg, double camPitchDeg) { return ""; }` (Enter w lunecie; zwraca komunikat ASCII), `virtual std::vector<std::string> TachyPanel(double camAzDeg, double camPitchDeg) const { return {}; }` (linie panelu lunety).
  - `JobType.tachy` (bool, domyślnie false), rejestr: `{"tachy", "Tachimetria (pomiar biegunowy)", &CreateTachyStation, false, true}` (kolejność pól: `gnss`, `tachy`).
  - `std::unique_ptr<Job> CreateTachyStation(IWorld&, const JobOptions&);`
- Menu (id → etykieta ASCII): `station` „Postaw tachimetr (stanowisko)”, `backsight` „Postaw punkt nawiazania”, `orient` „Orientacja na nawiazanie”, `prism` „Postaw tyczke z lustrem”, `prismh` „Wysokosc lustra: X.XXX m (zmien)”, `view` „Patrz przez lunete (F10)”, `finish` „Zakoncz robote”, `abort` dodaje kontroler. Dostępność: `station` – dopóki nie zmierzono punktu; `backsight` – po stanowisku i dopóki nie zmierzono punktu; `orient` – po stanowisku i nawiązaniu; `prism`, `prismh` – zawsze po stanowisku; `view` – po orientacji i tyczce; `finish` – ≥ 1 punkt. Przestawienie stanowiska lub nawiązania kasuje orientację (`oriented_ = false`).
- `OnMenu("orient")` ustawia tryb Orient, `OnMenu("view")` tryb Measure (kontroler potem otwiera widok). `TachyFire` w trybie Orient: sprawdza celowanie w nawiązanie (`TachyAimError` z `StationGrid()`/`PointGrid(1)` i 1,500 m oka; cel = znak na gruncie + 0,1 m; `LineClear(false)`), zapisuje `az0 = TachyOrient(camAz, ...)`, `oriented_ = true`, zwraca „Orientacja ustawiona”. W trybie Measure: odmawia „Najpierw orientacja” bez orientacji, „Nie celujesz w lustro” przy `!ok`, „Linia celowa zaslonieta” przy `!LineClear(true)`; inaczej liczy `TachyMeasure`/`TachyPolar` (prawdziwa odległość skośna z `StationGrid()` + oko i środka lustra = grunt tyczki + `kPrismTrueM`), dopisuje punkt `T<n>` i zwraca „T<n>: N ... E ... H ...” (ASCII).

- [ ] **Step 1: Napisz testy** `tests/test_tachy_job.cpp` z `FakeTachyWorld : IWorld` (pure metody z pustymi ciałami jak w `test_gnss_job.cpp`; `StationGrid` zwraca `{5500000, 6500000, 100}`, `PointGrid(1)` zwraca `{5500100, 6500000, 100}` (nawiązanie 100 m na północ), prawdziwy grunt tyczki ustawiany w polu `prism` (`PoleGroundGrid()` zwraca to pole), `LineClear` zwraca pole `clear`, `Random01` 0,5, `TimeText` „09:15:03”). Pomocnik `aimAt(Fx&, GridPos target, double targetAboveGroundM)` liczy azymut i pochylenie kamery na cel z oka (stanowisko + 1,5 m).
  - `menu_enablement_follows_state`: na starcie tylko `station` (i `prismh` wyłączone); po `station` włączone `backsight`, `prism`, `prismh`; po `backsight` włączone `orient`; `view` wyłączone do orientacji i tyczki; `finish` wyłączone.
  - `measure_before_orient_refused`: tryb Measure bez orientacji → `TachyFire` zwraca tekst zawierający „orientacja”, brak punktów.
  - `orient_then_measure_gives_coordinates`: orientacja celowana dokładnie w nawiązanie (celowanie w znak: cel = `PointGrid(1)` + 0,1 m), tyczka z lustrem 50 m na wschód (grunt `{5500000, 6500050, 100}`), celowanie w lustro na wysokości 1,5 m: po `TachyFire` `FinishTachy(true).points[0]`: `n ≈ 5500000`, `e ≈ 6500050`, `h ≈ 100` (tolerancje 0,02 m poziomo, 0,03 m pionowo – to szum 5″ i 2 mm); `hzGon ≈ 100` (±0,01).
  - `aim_off_target_refused_and_blocked_line`: celowanie 0,3 m obok lustra → „Nie celujesz w lustro”; `clear=false` → „Linia celowa zaslonieta”; w obu przypadkach 0 punktów.
  - `prism_height_mismatch_biases_h`: preset 1,8 (trzy przełączenia `prismh` z 1,5: 1,8 po 2 → sprawdź kolejność presetów 1,3 / 1,5 / 1,8 / 2,0 zaczynając od domyślnego 1,5) → `h` o ≈ −0,3 m względem prawdziwego.
  - `reposition_invalidates_orientation`: po orientacji `station` (przestawienie) → `view` wyłączone, `orient` włączone.
  - `true_error_and_abort`: opcja prawdziwego błędu → `hasTrue`, `d2Dm < 0.05`; `FinishTachy(false)` po 1 punkcie → `complete == false`, 1 punkt; bez punktów pusty.
  - `extreme_aim_does_not_crash`: `TachyFire` z `camPitch = ±90`, `camAz = -720` → komunikat odmowy, brak NaN w panelu (`TachyPanel` bez „nan”/„inf”).
- [ ] **Step 2: Uruchom** `tests/run.ps1` – oczekiwane `COMPILE FAIL test_tachy_job.cpp`.
- [ ] **Step 3: Zaimplementuj** rozszerzenia `Job.hpp`, `TachyStation.hpp` (`TachyPanel`: linie `Tryb: orientacja|pomiar`, `Hz: 123.4567 g   V: 98.7654 g` (Hz względem orientacji; przed orientacją Hz bezwzględne w `[0,400)`), `Punkty: n`, ostatni wynik albo podpowiedź „Enter - orientuj na nawiazanie”), `Jobs.hpp` (rejestr).
- [ ] **Step 4: Uruchom testy** – oczekiwane `ALL OK` (w tym niezmienione testy niwelacji i GNSS).
- [ ] **Step 5: Commit**: `git add src/job tests && git commit -m "job: robota Tachimetria z rozszerzonym interfejsem Job/IWorld"`.

---

### Task 4: Silnik – widok lunety z pochyleniem, tyczka z lustrem, przepływ

**Files:**
- Modify: `assets/make_models.py`, `src/engine/Placement.hpp`, `src/engine/World.hpp`, `src/engine/Export.hpp`, `src/engine/Controller.hpp`, `src/Plugin.hpp`
- Create: `src/engine/TachyView.hpp`
- Test: build (`BUILD OK`), `tests/run.ps1` (`ALL OK`); zachowanie w grze sprawdza użytkownik.

**Interfaces:**
- Consumes: Task 3 (`IWorld` rozszerzenia, `Job::TachyFire/TachyPanel/FinishTachy`, `JobType::tachy`), istniejące `PoleGround`, `PlacePole`, `SpawnVob`, `FeetGround`, `ScopeApplyZoom` (wzorzec), `LV_RETICLE`, `ViewOpenNow`, `TextPanelShow`, `MakeJobDir`.
- Produces:
  - `LV_VIS_PRISM = "LVL_PRISM.3DS"` w `Placement.hpp`; `build_prism()` w `make_models.py` (pręt 1,5 m, tarcza lustra r = 8 cm na szczycie) z wpisem w `main()`.
  - `EngineWorld`: `StationGrid()` (z `instrGround_`), `PointGrid(int)` (z `pts_`), `PlacePrism()` (jak `PlacePole`, ale model `LV_VIS_PRISM`), `LineClear(bool toPrism)` (promień od oka stanowiska do środka celu z flagami jak w `ScopeBeginReading`, bez trafienia = true).
  - `TachyView.hpp`: `bool TachyViewActive(); void TachyViewEnter(const TachyCallbacks&); void TachyViewLeave(); void TachyViewTick();` oraz `struct TachyCallbacks { std::function<std::string(double camAzDeg, double camPitchDeg)> fire; std::function<std::vector<std::string>(double, double)> panel; std::function<void()> onEnter, onLeave; };`.
  - `ExportTachy(TachyReport, dir, aborted)` w `Export.hpp` (`pomiar_tachimetryczny.csv/.docx`, przerwany `pomiar_tachimetryczny_przerwany.csv`), `AppendTachyLog(file, point, nr, trueErr)`.
  - W `Controller.hpp`: `CtrlSavePartialTachy`, `CtrlFinishTachy`, `CtrlEnterTachyView()`; menu startowe dla typu `tachy` pyta o prawdziwy błąd (jak `gnss`); pozycje `orient`/`view` wołają `gJob->OnMenu(id)` i otwierają widok; pozycja `scope` i klawisz F10 działają tylko dla niwelacji, dla tachimetrii F10 otwiera widok (tryb Measure po orientacji, inaczej komunikat).

- [ ] **Step 1: Model** `build_prism()` w `make_models.py` (prism r = 1,2 cm 0–150 cm w `block(2)`, tarcza r = 8 cm 150–152 cm w `block(1)`, pierścień r = 9 cm 152–153 cm w `block(3)`); sprawdź `python assets/make_models.py <tmp>`: powstaje `LVL_PRISM.3DS`.
- [ ] **Step 2: Świat**: `PlacePrism` w `World.hpp` obok `PlacePole`, `StationGrid`, `PointGrid`, `LineClear` (cel: `poleGround_ + (0, kPrismTrueCm, 0)` lub środek znaku nawiązania); `RemovePole` czyści obie tyczki (jeden slot `pole_`).
- [ ] **Step 3: Widok** `TachyView.hpp`: stan `gTachyOn`, `gTachyAz`, `gTachyPitch` (rad, ograniczony do ±89°), obrót ←→ i ↑↓ z prędkościami `LV_TURN_FAST`/`LV_TURN_SLOW` (Shift) jak w `Scope.hpp`, zbliżenie `LV_ZOOM` własną funkcją `TachyApplyZoom()` korzystającą z `gFovX/gFovY/gFovSaved`, krzyż `LV_RETICLE`, kamera w `StationGrid` + `LV_INSTR_H`, `SetHeadingAtWorld(sin az·cos p, sin p, cos az·cos p)`, bohater i ubiór ukryci, gra spauzowana, panel w lewym górnym rogu (`callbacks.panel`, stały odstęp wierszy, odświeżany po zmianie tekstu), Enter → `callbacks.fire` i komunikat przez `HudMessage`, F10 (`LV_KEY_SCOPE`) wyjście; `TachyViewLeave` przywraca FOV, widoczność bohatera i ubioru, `ogame->Unpause()`.
- [ ] **Step 4: Kontroler i eksport** według listy w Interfaces; `CtrlForgetJob` woła `TachyViewLeave()`; `gamePaused` w `ControllerTick` obejmuje `TachyViewActive()`; zakończenie pokazuje `TextPanelShow` z `TachySummaryLines` i linią na punkt; po każdym punkcie wpis do logu `odczyty_biezace.csv`.
- [ ] **Step 5: Build i testy**: `scripts/build.ps1` → `BUILD OK`; `tests/run.ps1` → `ALL OK`.
- [ ] **Step 6: Instalacja i weryfikacja przez użytkownika**: `scripts/install.ps1 ...`, porównanie `cmp` DLL; użytkownik sprawdza: postawienie stanowiska i nawiązania, orientacja (Enter na znaku), pochylanie lunety strzałkami ↑↓, pomiar lustra na różnych odległościach i wysokościach, komunikaty odmowy, tabela po zakończeniu, pliki w folderze roboty, przerwanie z widoku.
- [ ] **Step 7: Commit**: `git add assets src && git commit -m "engine: tachimetria - widok lunety z pochyleniem, tyczka z lustrem, przeplyw i zapis"`.

---

### Task 5: Dokumentacja i paczka

**Files:**
- Modify: `README.md`, `scripts/package.py`

**Interfaces:**
- Consumes: wszystkie poprzednie zadania.

- [ ] **Step 1: README**: rozdział „Tachimetria (pomiar biegunowy)” – cel, przebieg (stanowisko, nawiązanie, orientacja, tyczka z lustrem, pomiar), sterowanie lunetą (← → ↑ ↓, Shift, Enter, F10), jednostki (gon), wzory (biegunowe), szum, opcja prawdziwego błędu, pliki, ograniczenia v1; zaktualizuj zdanie o liczbie robót.
- [ ] **Step 2: `scripts/package.py`**: krótki opis tachimetrii w `INSTALACJA.txt`; wersja domyślna `0.3.0`.
- [ ] **Step 3: Weryfikacja**: `tests/run.ps1` (`ALL OK`), `scripts/build.ps1` (`BUILD OK`), `python scripts/package.py 0.3.0` i `unzip -l release/GothicSurveying-v0.3.0-G2A.zip | grep LVL_PRISM`.
- [ ] **Step 4: Commit**: `git add README.md scripts && git commit -m "docs: robota Tachimetria, paczka 0.3.0"`.

---

## Samokontrola planu

- **Pokrycie specyfikacji:** obliczenia i szum (T1), dziennik/CSV/DOCX/podsumowanie (T2), przebieg robót i menu, orientacja, odmowy celowania, wysokość lustra, prawdziwy błąd, przerwanie (T3), widok z pochyleniem, tyczka z lustrem, zapis plików, log na bieżąco (T4), dokumentacja i paczka (T5). Wszystkie punkty Review Focus mają testy w T1/T3 (pomiar bez orientacji, celowanie/zasięg/linia, skrajne kąty i przejście przez 0°, unieważnienie orientacji) albo opis weryfikacji w T4 (przerwanie z widoku).
- **Spójność typów:** `TachyStation`, `TachyReading`, `TachyResult`, `TachyAim`, `TachyPoint`, `TachyReport`, `JobKind::Tachy`, `JobType::tachy` mają jedną definicję i te same nazwy w kolejnych zadaniach.
- **Proporcje:** plan to decyzje (pliki, sygnatury, wartości, testy); ciała funkcji tylko tam, gdzie wzory wyznaczają algorytm (`TachyPolar`, `TachyAimError`).
