# Menu prowadzące: projekty, pomiary, kreatory - plan wdrożenia

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Menu, które prowadzi (projekt › pomiar › sekcje, rekomendowany krok „DALEJ", powody niedostępności, 6 kreatorów), z projektami trwającymi między sesjami i zbiorczym raportem przy zamknięciu projektu.

**Architecture:** Nakładka na istniejące roboty. `Job::Menu()/OnMenu()` bez zmian; `job/Workflow.hpp` nadaje pozycjom sekcje, kolejność i powody; `Job::NextStep()` (nowy, domyślnie pusty) mówi, jaki jest następny krok; `calc/Project.hpp` trzyma model projektu (format `StateKV`); `job/MenuLayout.hpp` liczy układ wierszy (nagłówki sekcji, okno przewijania, zawijanie); `engine/` tylko rysuje i przekazuje klawisze.

**Tech Stack:** C++20 (testy g++ `tests/run.ps1`), MSVC x86 + Union/Gothic API w `engine/` (build `scripts/build.ps1`).

**Spec:** `docs/superpowers/specs/2026-11-24-menu-projekty-design.md`

## Global Constraints

- Teksty w grze bez polskich znaków (ASCII); README/dokumenty mogą je mieć.
- Zgodność wsteczna: `stan_slot<N>.txt` (`ver=1`) dalej się wczytuje (nowe klucze opcjonalne), nazwy folderów wyników i pliki wynikowe bez zmian, id pozycji menu robót bez zmian (testy `tests/test_*_job.cpp` zielone).
- Logika w `calc/` i `job/` z testami `tests/test_*.cpp`; `powershell -File tests/run.ps1` musi dać `ALL OK`; build `powershell -File scripts/build.ps1` → `BUILD OK`.
- Makra windows.h: `(std::min)(...)`, nie nazywać zmiennych `near`, `rad2`. Funkcje `__try/__except` bez obiektów z destruktorami.
- Klawisze tylko w `src/Keys.hpp`; F5/F6/F11/F12 zajęte; Home = menu, F10 = luneta.
- 32-bit: bez setek `zCView` na klatkę, odświeżanie tylko po zmianie. Gra zatrzymana przy menu; `Tick()` roboty tylko gdy gra chodzi.
- Niepewne wywołania silnika: `SvTrace(...)` do `GothicSurveying.log`, a nie zgadywanie.
- Commity: małe, lokalne, na gałęzi `menu-projekty`, każdy po zielonych testach i buildzie, **bez linii Co-Authored-By i bez dopisków atrybucji**. Bez push, wydań i merge'a.
- Nazwy projektów: `[A-Za-z0-9_-]{1,32}` (bez polskich znaków i separatorów ścieżki); nazwy pomiarów nadawane automatycznie („Tachimetria 2"), bez wpisywania tekstu w grze.

## Review Focus

1. Zapis stanu sprzed projektów (bez kluczy `project`/`pomiar`) wczytuje się jak dotąd; pomiar trafia do „Pomiary luzem".
2. Nazwa projektu `..`, `a/b`, pusta, z polskimi znakami albo za długa jest odrzucana (test w Task 3).
3. Uszkodzony / obcięty `projekt.txt` (brak `ver`, brak pomiarów, nieznany `type`) nie wywala programu: `LoadProject` zwraca false lub pomija pozycję.
4. Pomiar „w toku" w manifeście bez aktywnej roboty (wczytano starszy zapis) nie blokuje nowych pomiarów: `Reconcile` oznacza go jako przerwany (Task 3).
5. Menu z 0 pozycjami, z samymi niedostępnymi i z 40 pozycjami (kursor na ostatniej) daje poprawny układ i okno przewijania (Task 6); zamknięcie projektu z pomiarem w toku jest odmówione (Task 3).

---

## File Structure

- Modify `src/job/Job.hpp`: `MenuItem` + `section`, `why`; `Job::NextStep()`.
- Create `src/job/Workflow.hpp`: sekcje/kolejność/powody (`Arrange`), `NextLine`, `ProjectNext`.
- Create `src/job/MenuLayout.hpp`: wiersze, nagłówki, okno przewijania, zawijanie.
- Create `src/job/Wizards.hpp`: 6 presetów.
- Create `src/calc/Project.hpp`: model projektu, zapis/odczyt, raport, porównanie.
- Create `src/calc/UiSettings.hpp`: `ustawienia.txt` (ostatni projekt, kursory).
- Modify `src/job/{LevelingTraverse,TrigLevel,GnssRover,TachyStation,ScanJob,AerialJob,PhotoJob}.hpp`: tylko `NextStep()`.
- Modify `src/engine/Menu.hpp`, `Controller.hpp`, `src/Keys.hpp`; Create `src/engine/ProjectUi.hpp` (ekran główny, kreatory, projekty).
- Tests: `tests/test_workflow.cpp`, `test_menulayout.cpp`, `test_project.cpp`, `test_wizards.cpp`, `test_projectreport.cpp`, `test_nextstep.cpp`.

---

### Task 1: Sekcje, kolejność i powody w menu robót (Workflow::Arrange)

**Files:** Modify `src/job/Job.hpp:18`; Create `src/job/Workflow.hpp`; Test `tests/test_workflow.cpp`

**Interfaces:**
- Produces: `struct MenuItem { std::string id, label, hint; bool enabled; std::string section = "", why = ""; };`
- Produces: `enum class Section { Prep, Params, Measure, Results, Tools, Advanced, Exit }; const char* SectionName(Section);` (nazwy ASCII: "Przygotowanie", "Parametry", "Pomiar", "Wyniki", "Narzedzia", "Zaawansowane", "")
- Produces: `std::vector<MenuItem> Arrange(JobKind kind, std::vector<MenuItem> items);` (nadaje `section`, sortuje stabilnie sekcja → kolejność z tabeli, wypełnia `why` dla `!enabled`: najpierw `hint`, jeśli pusty - tabela powodów; „abort" zawsze ostatnie).

- [ ] **Step 1: Test.** `tests/test_workflow.cpp` z minimalnym `FakeWorld : IWorld` (skopiuj implementację czystych wirtualnych z `tests/test_traverse_job.cpp:12-`). Testy: `every_menu_id_has_a_section` (dla każdego `JobTypes()[i].create(world, opts)` pobierz `Menu()` plus ids dokładane w kontrolerze: `scope, rodlen, level, preview, map, tp, abort`; po `Arrange` żadna pozycja nie ma pustej `section`, poza `abort` z `section=="Wyjscie"`); `abort_is_last`; `disabled_items_have_reason` (każda `!enabled` ma niepustą `why`); `order_is_stable_across_kinds` (pozycje wspólne dla ALS i Foto - `agl`, `speed`, `launch` - mają tę samą sekcję i względną kolejność); `unknown_id_goes_to_advanced`.
- [ ] **Step 2: Run** `powershell -File tests/run.ps1` → FAIL (brak `Workflow.hpp`).
- [ ] **Step 3: Implement** `Arrange` z jedną wspólną tabelą id → (sekcja, kolejność) w `Workflow.hpp`. Przydział: Prep = `reper_a, src, station, newst, free, backsight, unback, prism, inst, instrument, pole, antenna, base, area_here, area_ctrl, area_map, area_clear, plan_csv, cam`; Params = `agl, speed, sidelap, preset, res, fwd, side, rodlen, lvl`; Measure = `rod_back, rod_fore_turn, rod_fore_end, move_next, obsadzka, redo_readings, meas, view, stake, unstake, start, store, control, discard, scan, cancel, target, untarget, launch, scope, fine, level, camera`; Results = `adjust, adjust_cancel, products, products_cancel, ortho, ortho_cancel, finish`; Tools = `preview, map, tp`; Advanced = `corr, mode, prismh, faces, rgb, reg, azimuth, follow, calib, battery, profile`. Powody dla pozycji bez `hint`: `launch`→"najpierw zdefiniuj obszar" gdy brak, `cancel`→"nic nie trwa", `finish`→"najpierw wykonaj pomiar"; wartość domyślna "niedostepne w tym kroku".
- [ ] **Step 4: Run** `tests/run.ps1` → `ALL OK` (istniejące testy robót zielone).
- [ ] **Step 5: Commit** `git add src/job/Job.hpp src/job/Workflow.hpp tests/test_workflow.cpp && git commit -m "menu: sekcje, kolejnosc i powody niedostepnosci (Workflow::Arrange)"`

### Task 2: NextStep w robotach i linia „DALEJ" (niwelacja, trygonometryczna, GNSS, tachimetria)

**Files:** Modify `src/job/Job.hpp` (po `CanFinish`), `LevelingTraverse.hpp`, `TrigLevel.hpp`, `GnssRover.hpp`, `TachyStation.hpp`; Modify `Workflow.hpp`; Test `tests/test_nextstep.cpp`

**Interfaces:**
- Produces: `virtual std::string Job::NextStep() const { return ""; }` - id pozycji z `Menu()`, którą użytkownik powinien wykonać teraz; "" = brak rekomendacji. Odstępstwo od specyfikacji (tam tabela w Workflow): tylko robota wie, czy krok jest zrobiony; `Menu()/OnMenu()` bez zmian.
- Produces: `struct Next { std::string id, text; };` `Next NextLine(const std::vector<MenuItem>& items, const std::string& stepId);` → `text = "DALEJ: " + label`; gdy `stepId` pusty albo pozycja niedostępna: jeśli `finish` aktywne → "DALEJ: Zakoncz pomiar", w przeciwnym razie `{ "", "" }`.

- [ ] **Step 1: Test** `tests/test_nextstep.cpp` (świat jak w Task 1; dla GNSS/tachimetrii użyj fakeów z `tests/test_gnss_job.cpp`/`test_tachy_job.cpp`): `leveling_fresh_asks_for_benchmark` (`NextStep()=="reper_a"` na świeżej robocie), `leveling_advances_after_reper_and_instrument` (po `OnMenu("reper_a")` → "instrument", po postawieniu → "rod_back"), `gnss_fresh_asks_for_pole`, `tachy_fresh_asks_for_station`, `trig_fresh_asks_for_benchmark`, `nextstep_is_always_enabled_item` (dla każdego z 4 rodzajów: jeśli `NextStep()` niepusty, odpowiadająca pozycja `Menu()` ma `enabled`), `nextline_falls_back_to_finish`.
- [ ] **Step 2: Run** → FAIL.
- [ ] **Step 3: Implement** `NextStep()` w 4 robotach: pierwszy niespełniony warunek wstępny w kolejności menu (stan zadany polami roboty); gdy wszystko spełnione i `CanFinish()` → "" (rekomendacja spadnie na „Zakoncz pomiar"). Nie zmieniaj `Menu()`/`OnMenu()`. Dodaj `NextLine`.
- [ ] **Step 4: Run** `tests/run.ps1` → `ALL OK`.
- [ ] **Step 5: Commit** `git commit -m "menu: Job::NextStep i linia DALEJ (niwelacja, GNSS, tachimetria)"` (z `git add` wymienionych plików).

### Task 3: NextStep w skanerze, ALS i fotogrametrii

**Files:** Modify `ScanJob.hpp`, `AerialJob.hpp`, `PhotoJob.hpp`; Test `tests/test_nextstep.cpp` (dopisz; ALS/Foto z `tests/fake_aerial.hpp`)

- [ ] **Step 1: Test:** `scan_fresh_asks_for_station`, `aerial_fresh_asks_for_area` (`"area_map"`; po dodaniu 3 wierzchołków `area_here` → `"launch"`), `aerial_after_flight_asks_for_adjust_then_finish`, `photo_inherits_aerial_flow` (po wierzchołkach → `"launch"`; po locie i wyrównaniu → `"ortho"` gdy `Menu()` ma ją aktywną), `nextstep_is_always_enabled_item` dla 3 rodzajów.
- [ ] **Step 2: Run** → FAIL. **Step 3: Implement** `NextStep()` w `AerialJob` (PhotoJob dziedziczy, nadpisuje tylko po wyrównaniu → "ortho"); skaner: stanowisko → (rejestracja: kule) → "scan" → "". **Step 4: Run** `ALL OK`.
- [ ] **Step 5: Commit** `git commit -m "menu: NextStep w skanerze, ALS i fotogrametrii"`

### Task 4: Model projektu (calc/Project.hpp)

**Files:** Create `src/calc/Project.hpp`; Test `tests/test_project.cpp`

**Interfaces (Produces):**
```cpp
enum class MeasState { New, Running, Done, Aborted };
struct Measurement { std::string id, type, name, dir, params, summary, pointsFile; MeasState state = MeasState::New; bool guided = false; };
struct Project { std::string name, world, created; bool closed = false; std::vector<Measurement> items; StateKV defaults; };
bool ProjectNameOk(const std::string& name);                       // [A-Za-z0-9_-]{1,32}
Measurement& AddMeasurement(Project&, const std::string& typeId, const std::string& typeName);   // nazwa "<typeName> <n>", id "m<k>", stan New
bool CanAddMeasurement(const Project&, std::string& why);         // false: zamkniety albo jest Running
const Measurement* RunningMeasurement(const Project&);
void SetState(Project&, const std::string& id, MeasState, const std::string& dir = "", const std::string& summary = "");
void Reconcile(Project&, const std::string& activeId);             // Running != activeId -> Aborted
bool CloseProject(Project&, std::string& why);                     // odmowa przy Running
std::string ProjectToText(const Project&);  Project ProjectFromText(const std::string&, bool& ok);
bool SaveProject(const Project&, const std::string& root);         // root/projekty/<name>/projekt.txt
bool LoadProject(const std::string& root, const std::string& name, Project&);
std::vector<std::string> ListProjects(const std::string& root);
```
- [ ] **Step 1: Test** `test_project.cpp`: `name_validation` (`"Kolonia_1"` ok; `""`, `".."`, `"a/b"`, `"Gornicza\xC5\x82"`, 33 znaki odrzucone), `add_numbers_per_type` (dwie tachimetrie → "Tachimetria 1", "Tachimetria 2"; niwelacja → "Ciag niwelacyjny 1"), `cannot_add_while_running_or_closed`, `roundtrip_text` (wszystkie pola, w tym znaki `%`/nowa linia w `summary`), `corrupt_manifest_rejected` (pusty tekst, brak `ver`, uciety, nieznany `type` zachowany jako tekst), `reconcile_marks_stale_running_aborted`, `close_refused_while_running`, `save_load_list_on_temp_dir` (`std::filesystem::temp_directory_path()`).
- [ ] **Step 2: Run** → FAIL. **Step 3: Implement** na `StateKV` (klucze `m.<i>.<pole>`, `m.n`, `ver=1`, `state=open|closed`); `Save` tworzy katalogi, zapis atomowy (plik `.tmp` + rename). 
- [ ] **Step 4: Run** `ALL OK`. **Step 5: Commit** `git commit -m "projekt: model, zapis i odczyt manifestu (calc/Project.hpp)"`

### Task 5: Rekomendacja na poziomie projektu

**Files:** Modify `src/job/Workflow.hpp`; Test `tests/test_workflow.cpp` (dopisz)

**Interfaces:** `Next ProjectNext(const Project&, const Next& activeJobNext);` id ∈ {`"continue"`, `"new"`, `"close"`, `"reopen"`, `"create"`} (create: brak projektu — wywołujący poda pusty `Project` z `name==""`).
- [ ] **Step 1: Test:** `no_project_recommends_create`; `running_measurement_recommends_continue_with_name` (text zawiera nazwę pomiaru i, gdy `activeJobNext.text` niepusty, jego tekst); `empty_project_recommends_new`; `all_done_recommends_new_or_close` (id "new", text wspomina o zamknięciu); `closed_project_recommends_reopen`.
- [ ] **Step 2–4:** FAIL → implement → `ALL OK`. **Step 5: Commit** `git commit -m "menu: rekomendacja kroku na poziomie projektu"`

### Task 6: Układ menu (nagłówki, okno przewijania, zawijanie)

**Files:** Create `src/job/MenuLayout.hpp`; Test `tests/test_menulayout.cpp`

**Interfaces (Produces):**
```cpp
struct MenuRow { bool header; int item; std::string text; };       // item = indeks w items, -1 dla naglowka
struct MenuLayout { std::vector<MenuRow> rows; int first, count; int cursorRow; bool moreAbove, moreBelow; };
MenuLayout BuildLayout(const std::vector<MenuItem>& items, int cursor, int maxRows);   // naglowek przy zmianie section; okno z kursorem w srodku
std::vector<std::string> WrapText(const std::string& text, size_t width, size_t maxLines);
int MoveCursor(const std::vector<MenuItem>& items, int cursor, int dir);   // kursor chodzi po wszystkich pozycjach (tez niedostepnych); -1 gdy brak
```
- [ ] **Step 1: Test:** `empty_menu` (0 pozycji: puste wiersze, cursorRow -1, bez crasha); `all_disabled_cursor_still_moves`; `headers_inserted_on_section_change`; `window_scrolls_40_items_cursor_last` (maxRows 16: kursor widoczny, `moreAbove`, `!moreBelow`); `window_no_scroll_when_fits`; `wrap_breaks_on_space_and_limits_lines` (zawijanie jak w `Menu.hpp:42-47`: szerokość 88, 2 linie).
- [ ] **Step 2–4:** FAIL → implement → `ALL OK`. **Step 5: Commit** `git commit -m "menu: czysty uklad wierszy (naglowki, okno przewijania, zawijanie)"`

### Task 7: Silnik: menu z sekcjami, DALEJ, okruszkami i oknem przewijania

**Files:** Modify `src/engine/Menu.hpp`, `src/engine/Controller.hpp:370-413`, `src/Keys.hpp`

**Interfaces:** `struct MenuView { std::string title, crumbs, nextText, footer; std::vector<MenuItem> items; };` (stare wywołania z `{title, items}` działają bez zmian; puste pola = stary wygląd). `MenuOpen(view, onChoose, onBack = nullptr)`; Lewo → `onBack`.
- [ ] **Step 1:** `MenuRedraw` rysuje przez `BuildLayout` (nagłówki sekcji bez kursora, `^`/`v` przy `moreAbove/moreBelow`, `nextText` wyróżnione prefiksem `>> `, powód niedostępności z `why` w podpisie, wysokość panelu stała dla `maxRows`); `MenuTick` używa `MoveCursor`, Lewo woła `onBack`. `CtrlOpenMenu` dla aktywnej roboty: `Arrange(kind, items)` + pozycje dokładane przez kontroler przed `Arrange`, `crumbs = Projekt > Pomiar`, `nextText = NextLine(...)`. Skalowanie z `screen->nax/nay` jak w `MapView.hpp`. `SvTrace` rozmiaru panelu i liczby wierszy przy otwarciu.
- [ ] **Step 2: Build** `powershell -File scripts/build.ps1` → `BUILD OK`; `tests/run.ps1` → `ALL OK`.
- [ ] **Step 3: Zweryfikuj w grze** (prosi użytkownika): menu ALS z sekcjami i przewijaniem, „Przerwij pomiar" ostatnie. Poproś o fragment `GothicSurveying.log`, gdy układ odbiega.
- [ ] **Step 4: Commit** `git commit -m "menu: sekcje, DALEJ, okruszki i przewijanie w silniku"`

### Task 8: Silnik: ekran główny projektu i zarządzanie projektami

**Files:** Create `src/engine/ProjectUi.hpp`; Modify `Controller.hpp` (`CtrlOpenMenu` bez roboty), `Plugin.hpp` (include po `Menu.hpp`)

- [ ] **Step 1:** `gProject` (aktualny projekt), `ProjUiOpenHome()`: układ A ze specyfikacji (okruszki „GEODEZJA > Projekt: <nazwa>", linia świata/osnowy/liczby pomiarów, `ProjectNext` jako `nextText`, lista pomiarów ze znacznikami `[x] [~] [ ]` i „+ Nowy pomiar...", „Wyniki i raporty", „Mapa osnowy / Teleport", „Ustawienia projektu", „Zamknij projekt"). Brak projektu → pozycje „Nowy projekt" i „Pomiary luzem" (domyślny projekt `Luzem` tworzony automatycznie). „Nowy projekt" = nazwa `<swiat>_<n>` (bez wpisywania tekstu); „Projekty" = lista `ListProjects` ze zmianą aktywnego. Pozycje osnowy/mapy/teleportu/usuwania/poziomowania z dotychczasowego menu trafiają do „Narzedzia i osnowa" (podmenu), logika bez zmian. Cyfry 1-9 jako skróty (Task 11).
- [ ] **Step 2: Build** `BUILD OK`, `tests/run.ps1` `ALL OK`. **Step 3:** weryfikacja w grze (ekran główny, nowy projekt, przełączenie). **Step 4: Commit** `git commit -m "menu: ekran glowny projektu i zarzadzanie projektami"`

### Task 9: Silnik: rejestracja pomiarów i zgodność zapisu stanu

**Files:** Modify `Controller.hpp` (`CtrlStartJob`, `CtrlFinish*`, `CtrlConfirmAbort`, `CtrlSaveJobState`, `CtrlRestoreJobState`, `ControllerTick`)

- [ ] **Step 1:** `CtrlStartJob(...)` dodaje pomiar (`AddMeasurement`, stan Running, zapis manifestu); koniec/przerwanie/zmiana świata → `SetState(Done|Aborted, gJobDir, summary)` + zapis; `CtrlSaveJobState` dopisuje opcjonalne klucze `project`, `pomiar` (`ver` zostaje 1); `CtrlRestoreJobState`: brak kluczy albo nieistniejący projekt → pomiar trafia do „Luzem"; po wczytaniu zapisu `Reconcile(gProject, activeId)`. Wynik ostatniego pomiaru zapisuje też `pomiar_<id>.csv` (punkty id→H dla porównania, Task 13).
- [ ] **Step 2:** `tests/run.ps1` `ALL OK`; `build.ps1` `BUILD OK`. **Step 3:** w grze: pomiar → zapis gry → wczytanie → manifest zgodny; stary `stan_slot` bez kluczy wznawia się. **Step 4: Commit** `git commit -m "projekt: rejestracja pomiarow i zgodnosc zapisu stanu"`

### Task 10: Kreatory (presety) i tryb prowadzony

**Files:** Create `src/job/Wizards.hpp`; Modify `ProjectUi.hpp`, `Controller.hpp`; Test `tests/test_wizards.cpp`

**Interfaces:** `struct Wizard { std::string id, name, typeId, desc; JobOptions opts; std::vector<std::string> setup; };` `const std::vector<Wizard>& Wizards();` `std::unique_ptr<Job> StartWizard(IWorld&, const Wizard&);` (tworzy robotę z `JobTypes()` wg `typeId`, wykonuje `OnMenu` dla `setup`).
Presety: `osnowa` (gnss, Open/Auto), `niwelacja` (traverse, Anchored/Auto), `tachy_inwent` (tachy, Open/Auto), `skan_budynku` (scan; `setup = {"reg"}`), `nalot_als` (aerial; `setup = {"calib"}`), `zdjecia` (photo; bez setup).
- [ ] **Step 1: Test:** `six_wizards_exist`, `every_typeid_exists_in_jobtypes`, `setup_ids_enabled_in_fresh_menu` (każdy id z `setup` jest w `Menu()` świeżej roboty i `enabled`; jeśli nie - popraw preset, nie robotę), `als_wizard_turns_calibration_on` (etykieta `calib` zmienia się z „wylaczone" na „3 poprzeczne"), `fresh_wizard_job_has_nextstep`.
- [ ] **Step 2–4:** FAIL → implement → `ALL OK`. W silniku „Nowy pomiar" = lista 6 kreatorów + „Reczny wybor rodzaju" (dotychczasowe `CtrlOpenStartMenu`); pomiar z kreatora ma `guided=true`: menu pokazuje tylko DALEJ + „Pokaz wszystko" (jedno kliknięcie przełącza `guided=false`).
- [ ] **Step 5: Commit** `git commit -m "menu: kreatory (presety) i tryb prowadzony"`

### Task 11: Wygoda: klawisze, pomoc, pasek skrótów, pamięć ustawień

**Files:** Create `src/calc/UiSettings.hpp`, `src/job/Help.hpp`; Modify `Keys.hpp`, `Menu.hpp`, `ProjectUi.hpp`; Test `tests/test_workflow.cpp` (pomoc), `tests/test_project.cpp` (ustawienia)

**Interfaces:** `struct UiSettings { std::string lastProject; std::map<std::string,int> cursor; };` `std::string UiSettingsToText(const UiSettings&); UiSettings UiSettingsFromText(const std::string&); std::vector<std::string> HelpLines(const std::string& context);` (kontekst: `home`, `job:<kind>`, `map`).
- [ ] **Step 1: Test:** `settings_roundtrip`, `settings_garbage_gives_defaults`, `help_has_lines_for_every_context` (niepuste, ASCII, linie ≤ 88 znaków).
- [ ] **Step 2–4:** FAIL → implement → `ALL OK`. `Keys.hpp`: `LV_KEYNAME_BACK "Lewo"`, `LV_KEYNAME_HELP "F1"`, kody `LV_KEY_HELP = KEY_F1` (F1 do potwierdzenia w grze; `SvTrace` gdy klawisz bez reakcji), cyfry 1-9 jako skróty pozycji na ekranie głównym. Pasek skrótów u dołu z `Keys.hpp` nazw. `ustawienia.txt` w `GothicSurveying/`: ostatni projekt (Home otwiera jego ekran główny), kursor per ekran.
- [ ] **Step 5: Build + gra** (`BUILD OK`, F1/Lewo/cyfry). **Commit** `git commit -m "menu: klawisze, pomoc kontekstowa, pasek skrotow, pamiec ustawien"`

### Task 12: Wyniki, zamknięcie projektu, raport zbiorczy

**Files:** Modify `src/calc/Project.hpp` (lub nowy `src/calc/ProjectReport.hpp`); Modify `ProjectUi.hpp`; Test `tests/test_projectreport.cpp`

**Interfaces:**
```cpp
struct PointH { std::string pointId; double h; };
struct HeightRow { std::string pointId; std::vector<std::pair<std::string,double>> byMeasurement; double spanMm; };   // spanMm = max-min [mm], wiersze tylko dla punktow z >=2 pomiarow
std::vector<HeightRow> CompareHeights(const std::vector<std::pair<std::string, std::vector<PointH>>>& byMeasurement);
std::vector<std::string> ResultFiles(const std::string& dir);                 // lista plikow folderu wynikow (posortowana)
bool WriteProjectReport(const Project&, const std::string& projectDir, std::string& err);   // projekt_raport.csv + projekt_raport.docx (calc/Docx.hpp)
```
- [ ] **Step 1: Test:** `compare_only_shared_points`, `compare_span_in_mm`, `compare_empty_when_no_overlap`, `result_files_sorted`, `report_csv_has_row_per_measurement`, `report_docx_passes_check` (zapis do `tests/out/projekt.docx`, sprawdzenie `tests/check_docx.py` jak w istniejących testach, jeśli skrypt to obsługuje; inaczej tylko istnienie niepustego pliku).
- [ ] **Step 2–4:** FAIL → implement → `ALL OK`. Porównanie obejmuje pomiary z punktami wysokościowymi (niwelacja, trygonometryczna, GNSS, tachimetria) z `pomiar_<id>.csv`; ALS/foto tylko jeśli ich plik punktów istnieje (inaczej pomijane i opisane w raporcie jako „brak punktow do porownania"). Silnik: „Wyniki i raporty" = lista `ResultFiles` pomiaru + „Otworz folder" (`ShellExecute`); „Zamknij projekt" = potwierdzenie (domyślnie „Nie") → `CloseProject` → `WriteProjectReport` → panel z podsumowaniem; zamknięty projekt: tylko „Otworz ponownie".
- [ ] **Step 5: Build + gra**; **Commit** `git commit -m "projekt: wyniki, zamkniecie projektu i raport zbiorczy"`

### Task 13: Dokumentacja

**Files:** Modify `README.md` (sekcje „Sterowanie" i nowa „Projekty"), `scripts/package.py` (tekst INSTALACJA)

- [ ] **Step 1:** README: projekty, pomiary, kreatory, tryb prowadzony, klawisze (Lewo, 1-9, F1), pliki (`projekty/<nazwa>/projekt.txt`, `ustawienia.txt`, raport przy zamknięciu), zgodność ze starymi zapisami, ograniczenia (nazwy projektów automatyczne, porównanie wysokości bez ALS gdy brak punktów).
- [ ] **Step 2:** INSTALACJA w `package.py` (krótki opis nowego menu i folderu `projekty`). Bez zmiany wersji i bez budowania paczki.
- [ ] **Step 3: Run** `tests/run.ps1` `ALL OK`, `scripts/build.ps1` `BUILD OK`. **Step 4: Commit** `git commit -m "dokumentacja: projekty, kreatory, nowe menu"`
