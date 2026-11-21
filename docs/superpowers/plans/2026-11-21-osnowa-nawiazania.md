# Osnowa GNSS i wiele nawiązań w tachimetrii – plan implementacji

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** (1) Zakładanie punktów osnowy pomiarem GNSS (trwały plik + znaki w świecie). (2) Tachimetria z wieloma nawiązaniami (do 5), orientacją ze średniej kierunków, odchyłkami nawiązań w panelu i dzienniku, stanowiskiem i nawiązaniami na punktach osnowy oraz ostrzejszym celowaniem (±0,02°).

**Architecture:** Czysty `src/calc/Control.hpp` (osnowa: CSV, id, filtr) i rozszerzenia `Tachy.hpp`/`TachyReport.hpp` (testy g++). Roboty `GnssRover` i `TachyStationJob` przez `Job`/`IWorld` (testy z fałszywym światem). Silnik: `ControlNet.hpp` (plik osnowy, znaki), `World.hpp`, `Controller.hpp`. Niwelacja bez zmian.

**Tech Stack:** C++20 (g++ w testach, MSVC x86 plugin Union/Gothic API), PowerShell, Python (`make_models.py`, bez nowych modeli).

**Spec:** `docs/superpowers/specs/2026-11-21-osnowa-nawiazania-design.md` (rozszerza specyfikacje GNSS i tachimetrii)

## Global Constraints

- Teksty widoczne w grze bez polskich znaków; CSV/DOCX z polskimi znakami. Commity **bez żadnych podpisów** (`Co-Authored-By` itp.). Praca na gałęzi `osnowa` odciętej od `tachimetria`; scalenie/wypchnięcie/release tylko na polecenie.
- Pętla klatki tylko `ControllerTick`; panele przez `PanelOpen` (złota ramka, stały odstęp wierszy `y_local = v*8192/H`); klawisze ze stałych w `src/Keys.hpp`.
- Niwelacja, wcześniejsze zachowanie GNSS i tachimetrii bez osnowy oraz ich testy nie mogą się zmienić poza jawnie wskazanymi (ścieżka „prawdziwe współrzędne” zostaje).
- TDD dla `calc` i `job`; testy: `powershell -File tests/run.ps1` (`ALL OK`); build: `powershell -File scripts/build.ps1` (`BUILD OK`); instalacja: `powershell -File scripts/install.ps1 -Game "C:\Program Files (x86)\Steam\steamapps\common\Gothic II"`.
- Literalne `\n` w kodzie C++ wpisuj narzędziem Edit/Write (heredoc w powłoce zamienia je na newline, a duże heredoce psują się — skrypty poprawek zapisuj jako pliki narzędziem Write). `std::max`/`std::min` jako `(std::max)(...)`.
- Stałe: nawiązań max 5; osnowa wymaga fixu i ≥ 20 epok; stanowisko na osnowie: znak w promieniu ≤ 2,0 m od bohatera; nawiązanie: znak 3–300 m od stanowiska; ostrzeżenie „niepewne”: |odchyłka kierunku| > 20 mgon albo |odchyłka odległości| > 20 mm + 5 ppm; tolerancja celowania ±0,02° z dolnym limitem 1,5 cm; wysokość lustra jak dotąd (presety 1,5 / 1,8 / 2,0 / 1,3 m).
- Plik osnowy: `GothicSurveying/osnowa.csv`, separator `;`, nagłówek `id;swiat;N_m;E_m;H_m;x_cm;y_cm;z_cm;rozwiazanie;PDOP;epoki;czas`.

## Review Focus

- Plik osnowy brakujący, pusty, z uszkodzonymi lub pustymi wierszami: ładowanie nie wywala się, id się nie powtarzają, znaki nie dublują się po ponownym wejściu do świata.
- Stanowisko na osnowie bez znaku w promieniu 2 m: komunikat, nic nie stawia się; nawiązanie bliżej niż 3 m, dalej niż 300 m i to samo co stanowisko nie są na liście.
- Orientacja z jednym i z wieloma nawiązaniami, średnia kołowa przez 0°/360°, powtórny pomiar tego samego nawiązania nadpisuje (nie dubluje), progi ostrzeżenia (20 mgon; 20 mm + 5 ppm) na granicy.
- „Załóż punkt osnowy” w GNSS tylko przy fixie i ≥ 20 epokach; po zakończeniu roboty, przerwaniu i zmianie świata znaki i plik osnowy zostają.
- Przerwanie roboty / zmiana świata w widoku lunety: widok zamknięty, gra odpauzowana, częściowy raport zapisany (z nawiązaniami).

## Struktura plików

- Create: `src/calc/Control.hpp`, `src/engine/ControlNet.hpp`, `tests/test_control.cpp`.
- Modify: `src/calc/Tachy.hpp`, `src/calc/TachyReport.hpp`, `src/job/Job.hpp`, `src/job/GnssRover.hpp`, `src/job/TachyStation.hpp`, `src/engine/World.hpp`, `src/engine/Controller.hpp`, `src/engine/Export.hpp`, `src/Plugin.hpp`, `README.md`, `scripts/package.py`; testy `tests/test_tachy.cpp`, `tests/test_tachy_report.cpp`, `tests/test_tachy_job.cpp`, `tests/test_gnss_job.cpp`.

---

### Task 1: Osnowa (`calc/Control.hpp`) i obliczenia nawiązań

**Files:**
- Create: `src/calc/Control.hpp`, `tests/test_control.cpp`
- Modify: `src/calc/Tachy.hpp`, `tests/test_tachy.cpp`

**Interfaces:**
- Consumes: `fbdetail::Csv` z `FieldBook.hpp`; `GridPos` z `Gnss.hpp`.
- Produces w `Control.hpp`:
  - `struct ControlPoint { std::string id, world; double n = 0, e = 0, h = 0; double x = 0, y = 0, z = 0; std::string solution; double pdop = 0; int epochs = 0; std::string timeText; };` (x,y,z = położenie w grze [cm])
  - `std::string ControlCsvHeader();` = `id;swiat;N_m;E_m;H_m;x_cm;y_cm;z_cm;rozwiazanie;PDOP;epoki;czas` (bez końca linii); `std::string ControlCsvRow(const ControlPoint&);` (N/E/H 3 miejsca, x/y/z 1 miejsce, PDOP 1 miejsce, `swiat` i `rozwiazanie` w cudzysłowie przy `;` lub `"`).
  - `std::string ControlToCsv(const std::vector<ControlPoint>&);` (nagłówek + wiersze, każdy zakończony newline).
  - `std::vector<ControlPoint> ControlFromCsv(const std::string& text);` (pomija nagłówek, puste i wadliwe wiersze, zna cudzysłowy, nie rzuca wyjątków).
  - `std::string ControlNextId(const std::vector<ControlPoint>&);` → `O` + (największy numer z id `O<liczba>` + 1), `O1` dla pustej.
  - `std::vector<ControlPoint> ControlNear(const std::vector<ControlPoint>&, const std::string& world, double xCm, double zCm, double minM, double maxM);` – punkty z tego świata, poziomo w [minM, maxM] od (xCm, zCm), posortowane rosnąco po odległości.
- W `Tachy.hpp`:
  - `TachyAimError` – nowa tolerancja: `ok` gdy |lateral| ≤ max(0,015 m, dPoz·tan 0,02°) i |vertical| ≤ max(0,015 m, slope·tan 0,02°) i dystans 3–300 m (reszta bez zmian).
  - `double AngleDiffDeg(double a, double b);` (a − b zawinięte do [−180, 180]); `double TachyCircularMeanDeg(const std::vector<double>& deg);` (średnia kołowa w [0,360), pusta → 0).

- [ ] **Step 1: Napisz testy.** `tests/test_control.cpp` (`main` drukuje `test_control OK`): `csv_roundtrip` (dwa punkty, jeden ze `swiat = "a;b \"c\""`: `ControlFromCsv(ControlToCsv(v))` zwraca te same pola z tolerancją 1e-3 m / 0,1 cm); `csv_tolerates_garbage` (pusty tekst, tylko nagłówek, wiersz z brakującymi polami, wiersz z literami zamiast liczby, pusty wiersz: nie rzuca wyjątku, zwraca tylko poprawne wiersze); `next_id` (puste → `O1`; `O1, O3` → `O4`; id `X5` ignorowane); `near_filters_and_sorts` (świat `W1`; punkty o x = 0, 100·100, 400·100 cm, z = 0 oraz jeden z `W2`: zakres 3–300 m od (0,0) zwraca tylko ten z 100 m; zakres 0–2 m zwraca ten z 0 m; wynik rosnąco). W `tests/test_tachy.cpp` dopisz: `aim_tolerance_is_angular` (100 m na wschód, `camAz=90`: 0,03 m w poprzek ok, 0,05 m nie; 10 m: 0,01 m ok, 0,02 m nie; 300 m: 0,10 m ok (granica 0,105 m), 0,12 m nie; to samo w pionie), `angle_diff_and_circular_mean` (`AngleDiffDeg(10,350)==20`, `(350,10)==-20`; średnia `{350,10}==0`, `{10,20}==15`, `{359,1}==0`, `{}==0`, `{90}==90`). Zaktualizuj istniejące `aim_error_and_limits` do nowej tolerancji (5 cm i 10 cm w poprzek na 100 m są teraz poza tolerancją; 2 cm i 3 cm w poprzek na 100 m: 2 cm ok, 3 cm ok, 3,6 cm nie).
- [ ] **Step 2: Uruchom** `tests/run.ps1` – oczekiwane `COMPILE FAIL test_control.cpp` i niepowodzenie testów celowania.
- [ ] **Step 3: Zaimplementuj** `Control.hpp` i zmiany w `Tachy.hpp` według sygnatur (parser CSV z `"` jak `fbdetail::Csv` w drugą stronę; liczby przez `strtod` ze sprawdzeniem końca; pole `epoki` przez `strtol`).
- [ ] **Step 4: Uruchom testy** – oczekiwane `ALL OK`.
- [ ] **Step 5: Commit:** `git add src/calc tests && git commit -m "calc: osnowa (CSV, id, filtr), tolerancja katowa celowania, srednia kolowa kierunkow"`.

---

### Task 2: Raport – tabela nawiązań

**Files:**
- Modify: `src/calc/TachyReport.hpp`, `tests/test_tachy_report.cpp`

**Interfaces:**
- Consumes: Task 1.
- Produces:
  - `struct TachyBacksight { std::string name; double azGon = 0, hzGon = 0, dirResidMgon = 0, distMeasuredM = 0, distCoordM = 0, distResidMm = 0; bool uncertain = false; };`
  - w `TachyReport` nowe pola: `std::string coordSource = "prawdziwe"; std::string stationName; std::vector<TachyBacksight> backsights;`
  - `std::string TachyBacksightCsvLine(const TachyBacksight&);` = `# nawiazanie;nazwa;azymut_g;Hz_g;odchylka_kierunku_mgon;D_zmierzona_m;D_ze_wspolrzednych_m;odchylka_odleglosci_mm;ocena` (wiersz z wartościami: azymut i Hz 4 miejsca, odchyłka kierunku 1, odległości 3, odchyłka odległości 1; ocena `ok`/`niepewne`).
  - `TachyToCsv` – kolejność linii: `# UWAGA...` (jeśli niekompletny), `# stanowisko;...` (bez zmian), `# zrodlo;<coordSource>;<stationName>`, po jednej linii `# nawiazanie;...` na nawiązanie, nagłówek, wiersze.
  - `TachyBuildDocx` – dodatkowa tabela „Nawiązania” (kolumny: Nawiązanie, Azymut [g], Hz [g], Odchyłka kierunku [mgon], D zmierzona [m], D ze współrzędnych [m], Odchyłka odległości [mm], Ocena; szerokość `14000 / 8` dxa) przed tabelą punktów, gdy są nawiązania; linia stanowiska rozszerzona o źródło i nazwę.
  - `TachySummaryLines` – gdy są nawiązania, dodatkowa linia `Nawiazania: <n>, najwieksza odchylka kierunku: <x.x> mgon`.

- [ ] **Step 1: Zaktualizuj i dopisz testy** w `tests/test_tachy_report.cpp`: dotychczasowe asercje przesuwają się o linię `# zrodlo;prawdziwe;` (nagłówek jest teraz w `l[2]`, pierwszy wiersz w `l[3]`; z `# UWAGA` o jedną więcej); dodaj `csv_backsight_lines` (dwa nawiązania: `{"O2", 215.5958, 0.0000, 3.14159, 40.001, 40.003, 2.0, false}` i `{"O3", 120.0, 10.0, 25.0, 55.0, 55.05, 50.0, true}` → linie `# nawiazanie;O2;215.5958;0.0000;3.1;40.001;40.003;2.0;ok` i `...;O3;120.0000;10.0000;25.0;55.000;55.050;50.0;niepewne`; kolejność: stanowisko, zrodlo, nawiązania, nagłówek), `docx_has_backsight_table` (zawiera `Nawiązania`, `O2`, `w:w="1750"`), `summary_mentions_backsights` (`Nawiazania: 2, najwieksza odchylka kierunku: 25.0 mgon`).
- [ ] **Step 2: Uruchom** – oczekiwane niepowodzenie/kompilacja nie przechodzi.
- [ ] **Step 3: Zaimplementuj** zmiany; pola nowej struktury wypełnia robota (Task 4).
- [ ] **Step 4: Uruchom testy** – `ALL OK`.
- [ ] **Step 5: Commit:** `git add src/calc tests && git commit -m "calc: tabela nawiazan w raporcie tachimetrycznym (CSV, DOCX, podsumowanie)"`.

---

### Task 3: GNSS – „Załóż punkt osnowy”

**Files:**
- Modify: `src/job/Job.hpp`, `src/job/GnssRover.hpp`, `tests/test_gnss_job.cpp`

**Interfaces:**
- Consumes: Task 1 (`ControlPoint`), `GnssFromGrid`.
- Produces:
  - `IWorld` (niepure, domyślne ciało): `virtual std::string StoreControlPoint(const ControlPoint& cp) { return ""; }` – zapisuje punkt do osnowy i stawia znak; zwraca nadane `id` (`""` = błąd zapisu).
  - Menu GNSS: nowa pozycja `control` „Zaloz punkt osnowy”, podpowiedź „fix i 20 epok”, aktywna gdy trwa pomiar, typ `Fix` i `epochs >= 20`. `OnMenu("control")`: wypełnia `ControlPoint` (`n,e,h` z `Fix()`, `x,y,z` = `GnssFromGrid(w_.PoleGroundGrid())`, `solution`, `pdop`, `epochs`, `timeText`), wywołuje `StoreControlPoint`; przy pustym id komunikat `Say("Blad zapisu osnowy")` i brak zapisu punktu; inaczej zapisuje punkt do dziennika jak `store` z uwagą `osnowa <id>`.

- [ ] **Step 1: Napisz testy** w `tests/test_gnss_job.cpp`: `FakeGnss` dostaje `std::vector<ControlPoint> stored` i `std::string nextId = "O1"`; `StoreControlPoint` dopisuje punkt i zwraca `nextId`. Testy: `control_requires_fix_and_20_epochs` (po `measure(19)` w dobrym niebie `control` wyłączone; po `advance` do ≥ 20 epok w fixie włączone; w warunkach bez fixu zawsze wyłączone: `w.mask.nearestObstacleM = 2.5`, `advance(60)`), `control_stores_point_and_report` (po `control`: `w.stored.size() == 1`, `stored[0].n/e/h` równe punktowi w raporcie, `stored[0].x/y/z` zgodne z `GnssFromGrid(PoleGroundGrid())` (±1e-6), `FinishGnss(true).points[0].note == "osnowa O1"`), `control_failure_does_not_store` (`nextId = ""` → brak punktu w raporcie, komunikat zawiera „osnowy”).
- [ ] **Step 2: Uruchom** – oczekiwane niepowodzenie.
- [ ] **Step 3: Zaimplementuj** zmiany w `Job.hpp` i `GnssRover.hpp`.
- [ ] **Step 4: Uruchom testy** – `ALL OK` (istniejące testy GNSS niezmienione poza `menu_enablement` rozszerzonym o `control`).
- [ ] **Step 5: Commit:** `git add src/job tests && git commit -m "job: GNSS - zakladanie punktu osnowy"`.

---

### Task 4: Tachimetria – wiele nawiązań, osnowa jako źródło, odchyłki

**Files:**
- Modify: `src/job/Job.hpp`, `src/job/TachyStation.hpp`, `tests/test_tachy_job.cpp`

**Interfaces:**
- Consumes: Task 1–2 (`ControlPoint`, `TachyBacksight`, `AngleDiffDeg`, `TachyCircularMeanDeg`, nowa tolerancja).
- Produces w `Job.hpp` (`IWorld`, niepure, domyślne ciała): zamiast `PlaceBacksight()` → `virtual bool PlaceBacksight(int slot) { return false; }` (znak z lustrem pod stopami jako nawiązanie `slot` 1–5) oraz `virtual void RemoveBacksight(int slot) {}`, `virtual bool PlaceBacksightAt(int slot, const ControlPoint& cp) { return false; }`, `virtual bool PlaceTachymeterAt(const ControlPoint& cp) { return false; }`, `virtual std::vector<ControlPoint> ControlNearHero(double maxM) const { return {}; }` (punkty osnowy tego świata w promieniu od bohatera), `virtual std::vector<ControlPoint> ControlNearStation(double minM, double maxM) const { return {}; }` (od stanowiska, w zakresie), `PointGrid(int slot)` i `LineClear(bool toPrism, int slot = 1)` (slot = nawiązanie). `Job` dostaje `virtual bool NeedsChoice(const std::string& menuId) const { return false; }` (kontroler wtedy pokazuje listę) i `virtual std::vector<MenuItem> TachyChoices(const std::string& menuId) const { return {}; }` (id pozycji `station:<id>` / `backsight:<id>`).
- Menu (id → etykieta ASCII): `src` „Zrodlo wspolrzednych: osnowa GNSS / prawdziwe (zmien)” (aktywne przed postawieniem stanowiska; domyślnie osnowa, gdy `ControlNearHero(1e9)` niepuste), `station` „Postaw tachimetr (stanowisko)”, `backsight` „Dodaj nawiazanie (lustro)” (max 5, tylko przed pierwszym punktem), `unback` „Usun ostatnie nawiazanie”, `meas`, `prism`, `prismh`, `view` „Patrz przez lunete (F10)” (aktywne po stanowisku i co najmniej jednym nawiązaniu; **pozycja `orient` znika** – orientacja dzieje się w lunecie), `finish`. W trybie osnowy `NeedsChoice("station")` i `NeedsChoice("backsight")` zwracają true, a `TachyChoices` listy: stanowisko – `ControlNearHero(2.0)`; nawiązanie – `ControlNearStation(3.0, 300.0)` bez już użytych punktów i bez punktu stanowiska; etykieta `O3  N <n> E <e>  (<d> m)` (ASCII).
- `TachyFire`: (1) brak stanowiska → komunikat; (2) jeśli celownik trafia (nowa tolerancja) w lustro któregoś nawiązania → odczyt nawiązania (kierunek z szumem 5″, odległość skośna z szumem 2 mm + 2 ppm, nadpisuje wcześniejszy odczyt tego nawiązania), przeliczenie orientacji `az0` jako `TachyCircularMeanDeg` z `az_i − Az_i` po wszystkich odczytanych nawiązaniach, odchyłki, komunikat `Nawiazanie <nazwa>: kierunek <x.x> mgon, odleglosc <y.y> mm (<n> z <m>)` (+ „ niepewne” gdy próg); przy zasłonięciu „Linia celowa zaslonieta”; (3) bez orientacji (0 odczytanych nawiązań) „Najpierw zmierz nawiazanie (celuj w lustro nawiazania)”; (4) dalej pomiar punktu jak dotąd (na lustro lub bezlustrowy). `Az_i` = azymut ze współrzędnych stanowiska do współrzędnych nawiązania (osnowa: zmierzone `n,e`; prawdziwe: z `StationGrid`/`PointGrid` zaokrąglone do mm); w trybie osnowy współrzędne stanowiska `st_` = `n,e,h` punktu osnowy (zmierzone GNSS); w trybie prawdziwym jak dotąd. Odległość „ze współrzędnych” = odległość skośna od oka (`st_.h + ih`) do środka lustra (`h_nawiązania + wysokość lustra`). Przestawienie stanowiska, dodanie/usunięcie nawiązania lub zmiana źródła kasuje wszystkie odczyty nawiązań (orientacja unieważniona).
- `FinishTachy` wypełnia `coordSource` („osnowa GNSS” / „prawdziwe”), `stationName` (id punktu osnowy albo `S`), `backsights`. Prawdziwy błąd punktów liczony względem prawdziwych współrzędnych w grze (w trybie osnowy zawiera błąd osnowy).

- [ ] **Step 1: Przepisz testy** `tests/test_tachy_job.cpp` (fałszywy świat: pola `controls` (wektor `ControlPoint`), `stationControl`, pozycje prawdziwe stanowiska, nawiązań 1–5 (`back[5]`), tyczki; metody z Interfaces; pomocnik `aim` jak dotąd, `fireAt(target, above, ...)`). Testy: `registry_and_menu_state` (na starcie tylko `src` i `station`; `view` wyłączone do pierwszego odczytu nawiązania; `orient` nie istnieje), `multiple_backsights_orientation_mean` (dwa nawiązania w prawdziwym trybie, dokładne celowanie, `FinishTachy` ma 2 nawiązania z |odchyłka kierunku| < 3 mgon i |odchyłka odległości| < 10 mm; orientacja = średnia: punkt 50 m na wschód zmierzony z dokładnością 0,02 m), `backsight_aiming_is_angular` (celowanie 3 cm obok lustra nawiązania na 100 m ok, 8 cm nie), `remeasure_overwrites` (dwukrotny odczyt tego samego nawiązania: wciąż 1 wpis), `uncertain_flag_thresholds` (sztucznie przesunięte współrzędne nawiązania: odchyłka kierunku 25 mgon → `uncertain`; 15 mgon i odległości 10 mm + 3 ppm → ok), `control_source_uses_gnss_coordinates` (tryb osnowy: stanowisko i dwa nawiązania z listy; współrzędne stanowiska w raporcie to zmierzone `n,e,h` punktu osnowy, nie prawdziwe; `TachyChoices("station")` zawiera tylko punkty ≤ 2 m, `TachyChoices("backsight")` pomija punkt stanowiska, punkty < 3 m i > 300 m oraz użyte), `no_control_nearby_refused` (`ControlNearHero` puste → `NeedsChoice("station")` true, `TachyChoices` puste; `OnMenu("station:O9")` z nieistniejącym id nie stawia), `limit_five_backsights` (szósty `backsight` wyłączony), `reposition_clears_readings` (po `station`/`unback`/`src` odczyty nawiązań znikają, pomiar punktu odmawia), `measure_requires_orientation_and_reflectorless` (pomiar bezlustrowy po orientacji działa jak dotąd; zachowanie testów z poprzedniej wersji: odmowy, zasięg, `extreme_aim_does_not_crash`, `true_error_...`).
- [ ] **Step 2: Uruchom** – oczekiwane niepowodzenie/kompilacja nie przechodzi.
- [ ] **Step 3: Zaimplementuj** `Job.hpp` (zmiana sygnatur, nowe metody) i `TachyStation.hpp` (nawiązania jako `std::vector` slotów, panel: `Hz/V`, `Nawiazania: k z n zmierzone, orientacja: tak/nie`, `Pomiar ...`, ostatni komunikat).
- [ ] **Step 4: Uruchom testy** – `ALL OK`.
- [ ] **Step 5: Commit:** `git add src/job tests && git commit -m "job: tachimetria - wiele nawiazan, osnowa GNSS jako zrodlo, odchylki"`.

---

### Task 5: Silnik – osnowa w świecie, listy wyboru, nawiązania

**Files:**
- Create: `src/engine/ControlNet.hpp`
- Modify: `src/engine/World.hpp`, `src/engine/Controller.hpp`, `src/engine/Export.hpp`, `src/engine/TachyView.hpp`, `src/Plugin.hpp`
- Test: build (`BUILD OK`), `tests/run.ps1` (`ALL OK`); zachowanie w grze sprawdza użytkownik.

**Interfaces:**
- Consumes: Task 3–4 (`IWorld` rozszerzenia, `Job::NeedsChoice/TachyChoices`, `StoreControlPoint`), `Control.hpp`, istniejące `SpawnVob`, `RemoveVobSafe`, `DropVobRef`, `VobIsStale`, `FeetGround`, `MakeJobDir`, `MenuOpen`, `TextPanelShow`.
- Produces:
  - `ControlNet.hpp` (dołączany po `Placement.hpp`, przed `World.hpp`): `gControl` (`std::vector<ControlPoint>`), `ControlFilePath()` = `GothicSurveying/osnowa.csv`, `ControlLoad()`, `ControlSave()`, `std::string ControlWorldName()` (nazwa bieżącego świata z API Union, np. plik świata z `ogame->GetGameWorld()`; sprawdź dostępne pole/metodę), `std::string ControlAdd(ControlPoint)` (nadaje `id` przez `ControlNextId`, `world`, zapisuje plik, stawia znak, zwraca id; `""` przy błędzie zapisu), `ControlSyncMarkers()` (stawia brakujące znaki punktów tego świata modelem `LV_VIS_REPER`, bez duplikatów), `ControlForgetMarkers()` (po zmianie świata: `DropVobRef` bez dotykania świata).
  - `EngineWorld`: implementacje `StoreControlPoint`, `PlaceTachymeterAt`, `PlaceBacksightAt`, `PlaceBacksight(int slot)`, `RemoveBacksight(int slot)`, `ControlNearHero`, `ControlNearStation`, `PointGrid(slot)`, `LineClear(bool, int slot)`; nawiązania w slotach 1–5 (znak + tyczka z lustrem `PrismModel(prismH_)`), czyszczenie w `ClearAll/ForgetAll/Stale`; znaki osnowy nie są usuwane przez `ClearAll`.
  - Kontroler: `ControlSyncMarkers()` przy pierwszym ticku w nowym świecie i po zmianie świata `ControlForgetMarkers()` + ponowna synchronizacja; menu startowe dostaje pozycję „Pokaz osnowe” (lista punktów tego świata: id, N, E, H, rozwiązanie, PDOP, epoki przez `TextPanelShow`); w menu roboty `NeedsChoice(id)` otwiera listę wyboru (`MenuOpen` z `TachyChoices(id)`, wybór wywołuje `gJob->OnMenu("<id>:<punkt>")`); `TachyView` bez zmian poza użyciem nowego panelu z liczbą nawiązań.

- [ ] **Step 1: ControlNet** (plik, znaki): wczytanie przy starcie wtyczki (brak pliku = pusta osnowa), `ControlAdd` zapisuje cały plik, `ControlSyncMarkers` pilnuje, by znak punktu nie powstał dwa razy (mapa `id → vob`).
- [ ] **Step 2: Świat** – implementacje z Interfaces; `PlaceBacksightAt` i `PlaceTachymeterAt` osadzają obiekt dokładnie na `cp.x, cp.y, cp.z` (bez śledzenia gruntu pod stopami).
- [ ] **Step 3: Kontroler** – listy wyboru, pozycja „Pokaz osnowe”, synchronizacja znaków, zapis częściowego raportu z nawiązaniami (`CtrlSavePartialTachy` bez zmian w kontrakcie).
- [ ] **Step 4: Build i testy** – `scripts/build.ps1` → `BUILD OK`; `tests/run.ps1` → `ALL OK`.
- [ ] **Step 5: Instalacja i weryfikacja przez użytkownika** – GNSS: fix, 20 epok, „Załóż punkt osnowy” ×3, plik `GothicSurveying/osnowa.csv`, znaki zostają po „Zakończ”; wczytanie zapisu i powrót znaków; tachimetria: źródło osnowa, stanowisko na znaku z listy, 2–3 nawiązania, odchyłki w panelu i dzienniku, pomiar punktów, tabela nawiązań w DOCX.
- [ ] **Step 6: Commit:** `git add src && git commit -m "engine: osnowa GNSS w swiecie, listy wyboru, wiele nawiazan"`.

---

### Task 6: Dokumentacja i paczka

**Files:**
- Modify: `README.md`, `scripts/package.py`

**Interfaces:**
- Consumes: wszystkie poprzednie zadania.

- [ ] **Step 1: README:** rozdział „Osnowa GNSS” (zakładanie punktów, plik `osnowa.csv`, znaki, „Pokaż osnowę”), aktualizacja rozdziału „Tachimetria” (źródło współrzędnych, wiele nawiązań, orientacja ze średniej, odchyłki, progi ostrzeżeń, dokładniejsze celowanie ±0,02°, tabela nawiązań w dzienniku), rozdział „Ograniczenia” (brak wcięć, brak wyrównania osnowy, jedno stanowisko).
- [ ] **Step 2: `scripts/package.py`:** krótki opis osnowy i nawiązań w `INSTALACJA.txt`; wersja domyślna `0.4.0`.
- [ ] **Step 3: Weryfikacja:** `tests/run.ps1` (`ALL OK`), `scripts/build.ps1` (`BUILD OK`), `python scripts/package.py 0.4.0`.
- [ ] **Step 4: Commit:** `git add README.md scripts && git commit -m "docs: osnowa GNSS i wiele nawiazan, paczka 0.4.0"`.

---

## Samokontrola planu

- **Pokrycie specyfikacji:** osnowa trwała i znaki (T1, T5), zakładanie z GNSS (T3), źródło współrzędnych, stanowisko na osnowie, wiele nawiązań, średnia, odchyłki i ostrzeżenia (T4), tolerancja kątowa (T1), dziennik i DOCX (T2), menu „Pokaż osnowę” i listy wyboru (T5), dokumentacja (T6). Review Focus ma testy w T1 (plik, id), T4 (granice, lista, nawiązania), T3 (fix i 20 epok); sprzątanie i widok sprawdza T5 (build i gra).
- **Spójność typów:** `ControlPoint`, `TachyBacksight`, `PlaceBacksight(int)`, `ControlNearHero/Station`, `NeedsChoice/TachyChoices` mają po jednej definicji i te same nazwy w zadaniach.
- **Proporcje:** plan to decyzje (pliki, sygnatury, wartości, asercje), bez ciał funkcji poza opisem algorytmów w nawiasach.
