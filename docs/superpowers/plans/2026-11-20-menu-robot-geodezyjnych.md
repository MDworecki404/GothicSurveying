# Menu robót geodezyjnych i ciąg niwelacyjny – plan implementacji

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Zastąpić klawisze F7–F11 menu robót (F12) z modułowym ciągiem niwelacyjnym (dowiązany/zamknięty/otwarty), dziennikiem w trybie automatycznym i samodzielnym oraz eksportem CSV i DOCX.

**Architecture:** Czyste warstwy `calc/` (obliczenia, dziennik, CSV, DOCX) i `job/` (interfejs `Job`, automat ciągu przez `IWorld`) nie includują Gothic API i są testowane `g++`. Warstwa `engine/` (panele, luneta, menu, adapter `EngineWorld : IWorld`, kontroler) działa w `namespace GOTHIC_NAMESPACE` i jest weryfikowana kompilacją i testem w grze.

**Tech Stack:** C++ (MSVC x86 przez szablon CMake, standard 23; testy `g++ -std=c++20`), Gothic API/Union API, Python 3 (tylko `tests/check_docx.py`, bez zewnętrznych bibliotek).

**Spec:** `docs/superpowers/specs/2026-11-20-menu-robot-geodezyjnych-design.md`

## Global Constraints

- Teksty widoczne w grze (menu, HUD, komunikaty, panele) **bez polskich znaków** (kodowanie); CSV i DOCX mogą mieć polskie znaki (UTF-8).
- Pętla klatki: wyłącznie hook `zCWorld::Render` (`this == ogame->GetWorld()`); `oCGame::Render` nie jest wołany.
- Tekst w grze tylko przez widoki `zCView` z tłem `LVL_PANEL.TGA`; nie używać `screen->Print`.
- F5/F6 zarezerwowane przez grę; brak klawiatury numerycznej. Klawisz menu: stała `LV_KEY_MENU = KEY_F12`; luneta: `KEY_F10`. F7/F8/F9/F11 usunięte w Tasku 6.
- `calc/` i `job/` nie includują nic z Gothic API; `engine/` jest dołączany z `src/Plugin.hpp` wewnątrz `namespace GOTHIC_NAMESPACE`, **bez** strażników include i `#pragma once`.
- Jednostki: odczyty i przewyższenia w mm, wysokości w m, odległości w cm. Δh = wstecz − wprzód. Poprawki v = −fh / n (równomiernie na stanowisko). fh = ΣΔh − (H_koniec − H_start); zamknięty: H_koniec = H_A. `fh_dop = 20 mm·√L` (L w km; L = suma wszystkich celowych, prawdziwe odległości). Tolerancja sprawdzania trybu samodzielnego: 1 mm.
- Wysokość reperu = prawdziwa wysokość terenu w miejscu postawienia (m, do mm).
- Łata wprzód: „punkt obrotowy” (z żabką) albo „punkt końcowy” (dowiązany: reper B; zamknięty: łata < 30 cm od reperu A, inaczej komunikat; otwarty: dowolny).
- Eksport: `dziennik_N.docx` (A4 pozioma, tytuł „Dziennik niwelacji”, rzeczywista data systemowa, ZIP „stored” z CRC32, bez bibliotek) i `dziennik_N.csv` (średniki, kropka dziesiętna) w katalogu głównym gry; przy zmianie świata w trakcie roboty `dziennik_N_przerwany.csv`.
- Nie psuć istniejących zachowań: stawianie pod stopami, luneta z krzyżem i kreskami dalmierczymi, wpis 3 odczytów w mm, panele.

## Review Focus

- Ciąg bez kompletnego stanowiska lub bez domknięcia: `Zakończ robotę` ma być nieaktywne, `Finish` nie może się wywalić (Task 4).
- Zerowa długość ciągu i `fh = 0`; pusta lista stanowisk (Task 1).
- Odczyty poza 0–3000 mm, górna < dolna, dolna bez górnej, odczyt na łacie, której nie ma (Task 4).
- Pola z `;`, `"`, `&`, `<` i polskimi znakami w CSV i DOCX (Tasks 2, 3).
- Zmiana świata (wczytanie zapisu) przy otwartym menu lub w widoku lunety: sprzątanie paneli i zapis przerwanego dziennika (Task 7, test w grze).

---

### Task 1: Obliczenia ciągu, układ plików i uruchamianie testów

**Files:**
- Move: `src/LevelMath.hpp` → `src/calc/LevelMath.hpp` (`git mv`), poprawić include w `src/Level.hpp` i `tests/test_level_math.cpp`
- Create: `src/calc/Traverse.hpp`, `tests/test_traverse.cpp`, `tests/run.ps1`
- Modify: `CMakeLists.txt` (dodać `${CMAKE_SOURCE_DIR}/src` do `target_include_directories`, linia 12)

**Interfaces:**
- Produces (`src/calc/Traverse.hpp`, bez Gothic API, `#pragma once`):
```cpp
enum class TraverseKind { Anchored, Closed, Open };
struct Stand { int backMm; int foreMm; double sightBackCm; double sightForeCm; };
struct TraverseResult {
  std::vector<double> dhMm, vMm, dhAdjMm;          // po jednym na stanowisko
  std::vector<double> heightRawM, heightAdjM;      // n+1 punktów, [0] = start
  double sumBackMm, sumForeMm, sumDhMm, fhMm, lengthM, fhDopMm;
  bool hasClosure, withinTolerance;
};
TraverseResult ComputeTraverse(TraverseKind kind, const std::vector<Stand>& stands,
                               double hStartM, double hEndM, double fhDopCoeffMm = 20.0);
```
- `run.ps1`: kompiluje każde `tests/test_*.cpp` (`g++ -std=c++20 -Wall -Isrc`, dopisuje `C:\msys64\ucrt64\bin` do PATH, gdy brak `g++`), uruchamia i na końcu wypisuje `ALL OK` albo kończy kodem ≠ 0; po testach uruchamia `python tests/check_docx.py tests/out/sample.docx`, jeśli plik istnieje.

- [ ] **Step 1: Failing test** `tests/test_traverse.cpp` (helper `near(a,b,eps)`), stanowiska S3 = `{1500,1200,3000,3000}, {1400,1650,3000,3000}, {1300,1100,3000,3000}`:
  - `anchored_example`: `ComputeTraverse(Anchored, S3, 100.000, 100.240)` → `dhMm = {300,-250,200}`, `sumBackMm=4200`, `sumForeMm=3950`, `sumDhMm=250`, `fhMm=10`, `lengthM=180`, `fhDopMm≈8.485` (±0.001), `withinTolerance=false`, `vMm` każde ≈ −3.3333, `dhAdjMm = {296.667,-253.333,196.667}`, `heightAdjM = {100.000,100.296667,100.043333,100.240}`, `heightRawM = {100.000,100.300,100.050,100.250}`.
  - `closed_example`: stanowisko 3 z `foreMm=1340`, `hEndM` przekazane jako 0 (ignorowane): `fhMm=10`, `heightAdjM.back() ≈ 100.000`.
  - `open_example`: `hasClosure=false`, `fhMm=0`, `fhDopMm=0`, `withinTolerance=true`, `heightAdjM == heightRawM`, `vMm` same zera.
  - `zero_length_has_no_tolerance`: celowe 0 cm, fh=10 → `withinTolerance=false`; fh=0 → `true`.
  - `empty_stands_returns_empty_result`: puste wektory, brak wyjątku, `heightAdjM = {hStartM}`.
- [ ] **Step 2: Run** `powershell -File tests/run.ps1` → Expected: FAIL (brak pliku `calc/Traverse.hpp`).
- [ ] **Step 3: Create `tests/run.ps1`, move `LevelMath.hpp`, update includes, add include dir in `CMakeLists.txt`, implement `ComputeTraverse`** (poprawki tylko gdy `hasClosure`; zamknięty ignoruje `hEndM` i używa `hStartM`).
- [ ] **Step 4: Run** `powershell -File tests/run.ps1` → Expected: `ALL OK` (w tym `test_level_math`).
- [ ] **Step 5: Commit** `git add -A && git commit -m "calc: ComputeTraverse, uklad src/calc, run.ps1"`

### Task 2: Model dziennika i CSV

**Files:**
- Create: `src/calc/FieldBook.hpp`, `tests/test_fieldbook.cpp`

**Interfaces:**
- Consumes: `TraverseKind`, `Stand`, `TraverseResult`, `ComputeTraverse` (Task 1).
- Produces:
```cpp
enum class CalcMode { Auto, Manual };
struct FbRow { int standNo; std::string pointName; int backMm, foreMm;
               double dhMm, vMm, dhAdjMm, heightM; std::string note; double sightBackCm, sightForeCm; };
struct FieldBook { TraverseKind kind; CalcMode mode; bool complete;
                   std::string startName; double startHeightM; std::vector<FbRow> rows; TraverseResult sum; };
FieldBook BuildFieldBook(TraverseKind, CalcMode, const std::vector<Stand>&, const std::vector<std::string>& forePointNames,
                         double hStartM, double hEndM, bool complete);
std::string ToCsv(const FieldBook&);
std::string KindName(TraverseKind);   // "ciąg dowiązany" | "ciąg zamknięty" | "ciąg otwarty" (UTF-8)
std::string ModeName(CalcMode);       // "automatyczne" | "samodzielne"
```
- CSV: nagłówek dokładnie `nr_stanowiska;punkt;odczyt_wstecz_mm;odczyt_wprzod_mm;dh_mm;poprawka_mm;dh_wyrown_mm;H_m;uwagi`; pierwsza linia danych to punkt startowy `0;A;;;;;;100.000;reper`; po pustej linii podsumowanie `klucz;wartosc`: `suma_wstecz_mm`, `suma_wprzod_mm`, `suma_dh_mm`, `fh_mm`, `fh_dop_mm`, `dlugosc_m`, `wynik` (`w tolerancji`/`przekroczony`/`brak kontroli`). mm z 1 miejscem, H z 3.

- [ ] **Step 1: Failing test** `tests/test_fieldbook.cpp` (dane S3, nazwy `{"1","2","B"}`, hStart 100.000, hEnd 100.240):
  - `rows_match_traverse`: 3 wiersze, `rows[2].pointName=="B"`, `rows[2].heightM≈100.240`, `rows[0].dhAdjMm≈296.667`.
  - `csv_has_header_start_row_and_summary`: nagłówek jak wyżej w pierwszej linii, druga linia `0;A;;;;;;100.000;reper`, zawiera `fh_mm;10.0` i `wynik;przekroczony`.
  - `csv_escapes_special_fields`: `pointName = "x;\"y\""` → w CSV `"x;""y"""`.
  - `incomplete_book_is_flagged`: `complete=false` → pierwsza linia `# UWAGA: dziennik niekompletny (robota przerwana)`.
  - `open_book_has_no_closure_rows`: Open → `wynik;brak kontroli`, kolumna poprawek puste.
- [ ] **Step 2: Run** `powershell -File tests/run.ps1` → Expected: FAIL (brak `FieldBook.hpp`).
- [ ] **Step 3: Implement** w `src/calc/FieldBook.hpp`; punkt startowy ma stałą nazwę `A`, `forePointNames` ma długość = liczba stanowisk.
- [ ] **Step 4: Run** → Expected: `ALL OK`.
- [ ] **Step 5: Commit** `git commit -m "calc: FieldBook i eksport CSV"`

### Task 3: Zapis DOCX

**Files:**
- Create: `src/calc/Docx.hpp`, `tests/test_docx.cpp`, `tests/check_docx.py`

**Interfaces:**
- Consumes: `FieldBook`, `KindName`, `ModeName` (Task 2).
- Produces:
```cpp
uint32_t Crc32(const std::string& bytes);
std::string ZipStore(const std::vector<std::pair<std::string, std::string>>& files);  // nazwa, zawartość; metoda "stored"
std::string BuildDocx(const FieldBook& fb, const std::string& dateText);               // bajty pliku .docx
```
- Dokument: `[Content_Types].xml`, `_rels/.rels`, `word/document.xml`; `w:pgSz w:w="16838" w:h="11906" w:orient="landscape"`; tytuł „Dziennik niwelacji”, linia z `KindName`, `ModeName` i datą, **jedna tabela** z nagłówkiem + wierszem startowym + wierszem na stanowisko (kolumny jak CSV), po tabeli akapity podsumowania (Σ, fh, fh_dop, wynik). Wszystkie teksty XML-escapowane (`&`, `<`, `>`, `"`).

- [ ] **Step 1: Failing tests.** `tests/test_docx.cpp`: `Crc32("123456789")==0xCBF43926u`; `ZipStore` zaczyna się od `PK\x03\x04`, kończy rekordem EOCD `PK\x05\x06` z liczbą wpisów równą liczbie plików; `BuildDocx` zawiera nazwy trzech plików; zapisuje `tests/out/sample.docx` dla `FieldBook` z `pointName = "Zabka & <1> żółć"` (`std::filesystem::create_directories`). `tests/check_docx.py <plik>` (tylko `zipfile` i `xml.dom.minidom`): `testzip()` zwraca `None`; każdy `.xml` się parsuje; w `document.xml` jest dokładnie jedna tabela `w:tbl`, `w:tr` = liczba wierszy + 2; tekst zawiera `Dziennik niwelacji`, `ciąg dowiązany` i po sparsowaniu dokładnie `Zabka & <1> żółć`; jest `w:orient="landscape"`.
- [ ] **Step 2: Run** `powershell -File tests/run.ps1` → Expected: FAIL (brak `Docx.hpp`).
- [ ] **Step 3: Implement** `Crc32` (tablicowy), `ZipStore` (local file header + central directory + EOCD, rozmiary i CRC wpisane wprost, brak kompresji), `BuildDocx` (tabela z ramkami `w:tblBorders`, czcionka domyślna; bez stylów i bez numeracji stron).
- [ ] **Step 4: Run** → Expected: `ALL OK` (łącznie z `check_docx.py`). Dopisać do `.gitignore`: `tests/out/`.
- [ ] **Step 5: Commit** `git commit -m "calc: zapis DOCX (ZIP stored)"`

### Task 4: Interfejs `Job` i automat ciągu niwelacyjnego

**Files:**
- Create: `src/job/Job.hpp`, `src/job/LevelingTraverse.hpp`, `tests/test_traverse_job.cpp`

**Interfaces:**
- Consumes: `TraverseKind`, `CalcMode`, `Stand`, `FieldBook`, `BuildFieldBook` (Tasks 1–2).
- Produces (`src/job/Job.hpp`):
```cpp
enum class RodRole { Back, Fore };
enum class MarkerKind { Reper, Turning };
struct MenuItem { std::string id, label, hint; bool enabled; };
struct JobOptions { TraverseKind kind; CalcMode mode; };
class IWorld {
 public:
  virtual ~IWorld() = default;
  virtual bool   PlaceMarker(int pointId, MarkerKind kind) = 0;      // pod stopami bohatera; false = brak gruntu
  virtual bool   FeetWithinCm(int pointId, double cm) const = 0;
  virtual void   PlaceRod(RodRole role, int pointId) = 0;           // przenosi obiekt łaty na punkt
  virtual void   RemoveRod(RodRole role) = 0;
  virtual bool   PlaceInstrument() = 0;                              // pod stopami; nowe stanowisko
  virtual void   RemoveInstrument() = 0;
  virtual double PointHeightM(int pointId) const = 0;                // prawdziwa wysokość terenu punktu
  virtual double SightDistCm(int pointId) const = 0;                 // pozioma, od instrumentu
  virtual void   Say(const std::string& textAscii) = 0;
};
class Job {
 public:
  virtual ~Job() = default;
  virtual std::string Name() const = 0;
  virtual std::vector<MenuItem> Menu() const = 0;
  virtual void OnMenu(const std::string& id) = 0;
  virtual std::vector<int> ActiveRodPoints() const = 0;             // punkty z łatą, do wyboru pod kreską
  virtual bool OnReading(int pointId, int midMm, int upMm, int loMm) = 0;  // up/lo < 0 = brak dalmierza
  virtual std::string Hud() const = 0;                               // ASCII
  virtual bool CanFinish() const = 0;
  virtual FieldBook Finish(bool complete) = 0;                       // complete=false przy przerwaniu
};
struct JobType { std::string id, name; std::unique_ptr<Job> (*create)(IWorld&, const JobOptions&); };
const std::vector<JobType>& JobTypes();   // v1: {"traverse", "Ciag niwelacyjny", ...}
```
- Id pozycji menu ciągu: `reper_a`, `instrument`, `rod_back`, `rod_fore_turn`, `rod_fore_end`, `move_next`, `finish`. Punkty: 1 = A, kolejne 2, 3, …; nazwy w dzienniku: A, 1, 2, …, a ostatni punkt dowiązanego to `B`.
- Aktywność pozycji z flag (nie z enumu stanów): `reper_a` – brak punktów; `instrument` – A istnieje, brak instrumentu, ciąg nie domknięty; `rod_back` – instrument, brak łaty wstecz; `rod_fore_*` – instrument + łata wstecz, brak łaty wprzód; `move_next` – oba odczyty zrobione i punkt wprzód obrotowy; `finish` – punkt końcowy odczytany i ≥ 1 kompletne stanowisko. Łata wstecz staje na bieżącym punkcie automatycznie (pozycja punktu, nie bohatera).
- `move_next`: `RemoveInstrument`, `RemoveRod(Back)`, `PlaceRod(Back, forePoint)`, `RemoveRod(Fore)`, nowy punkt wstecz = stary wprzód.

- [ ] **Step 1: Failing tests** w `tests/test_traverse_job.cpp` z `FakeWorld : IWorld` (zapisuje wywołania w `std::vector<std::string> calls`, `feetNear` sterowalne, `PointHeightM`: 1→100.000, B→100.240, `SightDistCm` = 3000):
  - `menu_at_start_only_allows_reper_a`.
  - `rod_fore_blocked_until_instrument_and_back_rod`.
  - `full_anchored_run_produces_expected_fieldbook`: 3 stanowiska z odczytami S3 (Task 1), ostatnie przez `rod_fore_end`; `Finish(true)` → 3 wiersze, `sum.fhMm≈10`, `rows[2].pointName=="B"`.
  - `move_next_swaps_roles_and_removes_instrument`: kolejność wywołań jak wyżej.
  - `closed_end_requires_feet_on_reper_a`: `feetNear=false` → `rod_fore_end` nie zmienia stanu, `Say` dostaje komunikat zawierający „reperze A”.
  - `open_end_finishes_without_closure`.
  - `finish_disabled_until_end_point_read`; `CanFinish()` false przy 0 stanowisk i `Finish(false)` zwraca `complete=false` bez wyjątku.
  - `reading_rejects_out_of_range`: `OnReading(p, 3001, -1, -1)` false; `up=1600, lo=-1` false; `up=1400, lo=1600` true (kolejność dowolna).
  - `reading_on_unplaced_rod_is_rejected`: punkt bez łaty → false.
  - `readings_order_is_free`: wprzód przed wstecz daje ten sam Δh.
- [ ] **Step 2: Run** `powershell -File tests/run.ps1` → Expected: FAIL (brak `job/Job.hpp`).
- [ ] **Step 3: Implement** `Job.hpp` (interfejsy i `JobTypes()`; rejestr zwraca jedną robotę) oraz `LevelingTraverse.hpp` (klasa `LevelingTraverse : Job`, fabryka `CreateLevelingTraverse`). Odległość celowej do `Stand` bierze z `IWorld::SightDistCm` w chwili odczytu.
- [ ] **Step 4: Run** → Expected: `ALL OK`.
- [ ] **Step 5: Commit** `git commit -m "job: interfejs Job, automat ciagu niwelacyjnego"`

### Task 5: Rozbicie `Level.hpp` na moduły silnika (bez zmiany zachowania)

**Files:**
- Create: `scripts/build.ps1`, `src/engine/Panels.hpp`, `src/engine/Scope.hpp`, `src/engine/Placement.hpp`
- Modify: `src/Level.hpp` (zostaje cienka klejka starego zachowania F7–F11, korzysta z nowych modułów), `src/Plugin.hpp`

**Interfaces:**
- `scripts/build.ps1`: ustawia środowisko `vcvarsall.bat x86` (ścieżka VS 2022 Community, `C:\msys64\ucrt64\bin` w PATH), uruchamia `cmake --preset x86-release`, `cmake --build out/build/x86-release`, `cmake --install out/build/x86-release --prefix out/install/x86-release`; kończy kodem błędu kompilatora.
- Produces (`engine/Panels.hpp`, namespace `GOTHIC_NAMESPACE`): `zCView* PanelOpen(int x0,int y0,int x1,int y1)`, `void PanelClose(zCView*&)`, `void HudMessage(const char*)`, `void HudTick()` (wygaszanie komunikatu po 7 s).
- Produces (`engine/Placement.hpp`): `bool FeetGround(zVEC3& hit)`, `zCVob* SpawnVob(const char* visual, const zVEC3& p)`, `void RemoveVobSafe(zCVob*&)`.
- Produces (`engine/Scope.hpp`):
```cpp
struct RodTarget { int pointId; zVEC3 foot; };
struct ScopeCallbacks {
  std::function<std::vector<RodTarget>()> targets;                         // łaty do wyboru pod pionową kreską
  std::function<void(int pointId,int midMm,int upMm,int loMm)> onReading;  // po wpisaniu kompletu odczytów
};
void ScopeEnter(const zVEC3& instrumentGround, const ScopeCallbacks& cb);
void ScopeLeave();
bool ScopeActive();
void ScopeTick();      // strzałki/Shift, krzyż, kreski dalmiercze, zbliżenie, wpisywanie 3 odczytów, F10 anuluj/wyjdź
```

- [ ] **Step 1: Create `scripts/build.ps1`** i potwierdzić, że obecne źródła budują się nim: `powershell -File scripts/build.ps1` → Expected: brak `error`, powstaje `out/install/x86-release/bin/GothicSurveying.dll`.
- [ ] **Step 2: Wydzielić panele i placement** z `Level.hpp` (`LvPanelOpen/Close`, `LvMsg`, `LvFeet`, `LvSpawn`) do `Panels.hpp`/`Placement.hpp`; stare nazwy zastąpić nowymi w `Level.hpp`. Build jak w kroku 1 → Expected: bez błędów.
- [ ] **Step 3: Wydzielić lunetę** (`LvEnterView`, `LvLeaveView`, `LvViewTick`, `LvApplyZoom`, `LvOpenHairs`, wpisywanie, wybór łaty pod kreską, sprawdzanie linii celowej) do `Scope.hpp` według powyższego interfejsu; `Level.hpp` podpina `ScopeCallbacks` do starej logiki BS/FS (`LvFinishReading`, CSV `niwelacja_N.csv`). Linia celowa i walidacja zakresu `0..300 cm` zostają w `Scope`.
- [ ] **Step 4: Verify** `powershell -File scripts/build.ps1` bez błędów, potem `scripts/install.ps1 -Game C:\GothicII`. Test w grze (autor): F7/F8/F9 stawiają sprzęt, F10 wchodzi do lunety z krzyżem i kreskami dalmierczymi, strzałki obracają, Enter + 3 odczyty zapisują wiersz w `niwelacja_N.csv` jak przed refaktorem; `GothicSurveying.log` bez nowych błędów.
- [ ] **Step 5: Commit** `git commit -m "engine: panele, placement i luneta wydzielone z Level.hpp"`

### Task 6: Menu, kontroler robót i adapter `EngineWorld`

**Files:**
- Create: `src/engine/Menu.hpp`, `src/engine/World.hpp`, `src/engine/Controller.hpp`
- Modify: `src/Plugin.hpp` (hook woła `ControllerTick()` zamiast `LevelLoop()`), `README.md` (sterowanie)
- Delete: `src/Level.hpp` (stare F7–F11 i klejka)

**Interfaces:**
- Consumes: `Job`, `IWorld`, `JobTypes()` (Task 4); `Scope*`, `Panel*`, `HudMessage`, placement (Task 5).
- Produces:
```cpp
// engine/Menu.hpp
struct MenuView { std::string title; std::vector<MenuItem> items; };
void MenuOpen(const MenuView&, std::function<void(const std::string& id)> onChoose);
void MenuClose();
bool MenuIsOpen();
void MenuTick();     // ↑/↓ (pomija nieaktywne), Enter wybiera, LV_KEY_MENU zamyka; gra zatrzymana (ogame->Pause) gdy otwarte
// engine/World.hpp
class EngineWorld : public IWorld {            // implementacja IWorld; trzyma markery, 2 łaty (role) i niwelator
 public:
  zVEC3 PointFoot(int pointId) const;          // pozycja punktu (dla ScopeCallbacks::targets i SightDistCm)
  zVEC3 InstrumentGround() const;              // grunt pod instrumentem (dla ScopeEnter)
  void  ForgetAll();                           // po zmianie świata: zapomina vob-y bez ich usuwania
  /* + wszystkie metody IWorld */ };
// engine/Controller.hpp
void ControllerTick();   // F12: poza robotą menu startowe, w robocie menu z job->Menu(); F10: ScopeEnter z targets z job->ActiveRodPoints()
```
- Menu startowe: `Rozpocznij robote` → typ roboty z `JobTypes()` → rodzaj ciągu (`dowiazany`, `zamkniety`, `otwarty`) → tryb liczenia (`automatyczne`, `samodzielne`) → `create(world, options)`. W robocie: panel HUD z `job->Hud()` stale widoczny; wybór pozycji → `job->OnMenu(id)`, menu się zamyka. `Zakończ robotę` w tym tasku tylko woła `Finish(true)` i wypisuje podsumowanie w HUD (eksport w Tasku 7). Reper: HUD podaje `PointHeightM` z dokładnością do mm.

- [ ] **Step 1: Implement `Menu.hpp`** (panel `zCView` z tłem, lista z kursorem „>”, pozycje nieaktywne wyszarzone tekstowo „(niedostepne: hint)”) i `World.hpp` według interfejsu (łaty: dwa obiekty wielokrotnego użytku; markery: reper i żabka jako vob-y `LVL_PLATE.3DS` / osobny znacznik reperu z modelu istniejącego; po zmianie świata `ForgetAll()`).
- [ ] **Step 2: Implement `Controller.hpp`** (maszyna: Idle → wybór typu → rodzaj → tryb → Running; w Running F12 otwiera menu roboty; F10 uruchamia lunetę, rola łaty wynika z `Job::OnReading`); podmienić wywołanie w `Plugin.hpp`; usunąć `Level.hpp`.
- [ ] **Step 3: Build** `powershell -File scripts/build.ps1` → Expected: bez `error`; `powershell -File tests/run.ps1` → `ALL OK`.
- [ ] **Step 4: Update README** (sterowanie: F12 menu, F10 luneta, przebieg ciągu) i commit po teście w grze.
- [ ] **Step 5: Test w grze (autor)**: F12 → Rozpocznij robotę → Ciąg niwelacyjny → dowiązany → automatyczne; postaw reper A, niwelator, łatę wstecz, odczyt w lunecie, łatę wprzód (punkt obrotowy), odczyt, „Przełóż”, drugie stanowisko, punkt końcowy; HUD pokazuje kolejne kroki; pozycje niedozwolone są nieaktywne. **Commit** `git commit -m "engine: menu robot i kontroler, adapter EngineWorld"`.

### Task 7: Zakończenie roboty – podgląd dziennika, CSV i DOCX, przerwanie

**Files:**
- Create: `src/engine/FieldBookView.hpp`, `src/engine/Export.hpp`
- Modify: `src/engine/Controller.hpp`, `README.md`

**Interfaces:**
- Consumes: `FieldBook`, `ToCsv` (Task 2), `BuildDocx` (Task 3), `Job::Finish` (Task 4).
- Produces:
```cpp
// engine/FieldBookView.hpp
void FieldBookShow(const FieldBook&);     // panel tabeli (przewijanie ↑/↓, Esc/F12 zamyka)
bool FieldBookVisible();
void FieldBookTick();
// engine/Export.hpp
std::string DateTodayText();                                           // np. "2026-11-20"
bool ExportFieldBook(const FieldBook&, int seq, bool aborted);         // dziennik_N.docx + .csv  /  dziennik_N_przerwany.csv
```
- Numer `N`: pierwszy wolny w katalogu głównym gry (sprawdzenie istnienia plików). Przy `aborted` tylko CSV z flagą `complete=false`.

- [ ] **Step 1: Implement `Export.hpp`** (zapis binarny `std::ofstream`) i `FieldBookView.hpp` (kolumny dopasowane do szerokości panelu; przy braku miejsca na ekranie pokazuje ograniczony zestaw: punkt, odczyty, Δh, H; pełny jest w pliku).
- [ ] **Step 2: Wire `Zakończ robotę`** w kontrolerze: `Finish(true)` → `FieldBookShow` → `ExportFieldBook(..., false)` → HUD „Dziennik zapisany: dziennik_N.docx”. Przy zmianie świata w trakcie roboty (`ogame->GetWorld()` inny): `Finish(false)`, `ExportFieldBook(..., true)`, zamknięcie menu, paneli i lunety.
- [ ] **Step 3: Build** `powershell -File scripts/build.ps1` → Expected: bez błędów; `tests/run.ps1` → `ALL OK`.
- [ ] **Step 4: Test w grze (autor)**: pełny ciąg dowiązany, zamknięty i otwarty do końca; po `Zakończ robotę` pojawia się tabela, w katalogu głównym gry są `dziennik_N.docx` i `.csv`; **autor otwiera `.docx` w Wordzie** (ma się otworzyć bez komunikatu o uszkodzeniu, tabela i podsumowanie zgodne z CSV). Wczytanie zapisu w trakcie roboty → `dziennik_N_przerwany.csv`, brak crashu przy otwartym menu i w lunecie.
- [ ] **Step 5: Commit** `git commit -m "engine: zakonczenie roboty, dziennik w grze, eksport CSV i DOCX"`

### Task 8: Tryb samodzielny – edytor dziennika i sprawdzanie

**Files:**
- Create: `src/calc/ManualCheck.hpp`, `tests/test_manual_check.cpp`, `src/engine/FieldBookEdit.hpp`
- Modify: `src/calc/FieldBook.hpp` (kolumny „twoje” w CSV), `src/calc/Docx.hpp` (druga tabela „Sprawdzenie”), `tests/test_fieldbook.cpp`, `tests/test_docx.cpp`, `src/engine/Controller.hpp`, `README.md`

**Interfaces:**
- Consumes: `FieldBook`, `ToCsv`, `BuildDocx` (Tasks 2–3).
- Produces (`src/calc/ManualCheck.hpp`):
```cpp
struct PlayerEntries {                    // wartości wpisane przez gracza; NaN = nie wpisano
  std::vector<double> dhMm, vMm, heightM;  // per stanowisko; heightM = wysokość punktu wprzód
  double sumDhMm, fhMm;
};
struct CellVerdict { double correct, player, diff; bool entered, ok; };
struct ManualVerdict { std::vector<CellVerdict> dh, v, height; CellVerdict sumDh, fh; int ok, total; };
ManualVerdict CheckManual(const FieldBook&, const PlayerEntries&, double tolMm = 1.0);   // wysokości porównywane w mm
std::string ToCsvWithVerdict(const FieldBook&, const ManualVerdict&);                      // dodaje kolumny twoje/roznica/ok
```
- Edytor (`engine/FieldBookEdit.hpp`): `void FieldBookEditStart(const FieldBook&)`, `PlayerEntries FieldBookEditResult()`, `void FieldBookEditTick()` — kursor po komórkach ←/→/↑/↓, cyfry 0–9, `-` (liczby ujemne, `KEY_MINUS`), Backspace, Enter zatwierdza komórkę, `F12` kończy edycję i przechodzi do sprawdzenia.

- [ ] **Step 1: Failing tests** `tests/test_manual_check.cpp` (FieldBook z Taska 2):
  - `all_correct_entries_pass`: wpisy równe poprawnym → `ok == total`.
  - `within_tolerance`: Δh różni się o 0.9 mm → ok; o 1.1 mm → nie.
  - `missing_cells_count_as_not_entered`: NaN → `entered=false`, `ok=false`, `total` bez zmian.
  - `height_compared_in_mm`: `heightM` różni się o 0.0005 m → ok; o 0.002 m → nie.
  - `csv_with_verdict_has_player_and_diff_columns`; dla `Open`/`Closed` bez fh i v kolumny z `v` są puste.
  Rozszerzyć `test_docx.cpp`/`check_docx.py`: dla `Manual` dokument ma drugą tabelę `Sprawdzenie` (2 × `w:tbl`).
- [ ] **Step 2: Run** `powershell -File tests/run.ps1` → Expected: FAIL.
- [ ] **Step 3: Implement** `ManualCheck.hpp`, kolumny w CSV i druga tabela w `BuildDocx(const FieldBook&, const std::string&, const ManualVerdict* verdict = nullptr)`.
- [ ] **Step 4: Run** → Expected: `ALL OK`.
- [ ] **Step 5: Implement edytor i podpiąć w kontrolerze**: w trybie `Manual` `Zakończ robotę` pokazuje tabelę z pustymi komórkami (Δh, Σ, fh, poprawki v, wysokości wyrównane); po `F12` → `CheckManual` → panel wyniku (trafione/chybione, poprawne wartości) → eksport CSV i DOCX z werdyktem. Build `scripts/build.ps1` → bez błędów.
- [ ] **Step 6: Test w grze (autor)**: robota z trybem samodzielnym, wpis celowo jednego błędnego pola; oznaczenie chybionej komórki w grze i w DOCX; `README.md` opisuje tryb. **Commit** `git commit -m "tryb samodzielny: edytor dziennika i sprawdzanie"`
