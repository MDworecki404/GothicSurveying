# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Czym jest projekt

**GothicSurveying** to plugin [Union](https://gothic-modding-community.github.io/gmc/zengin/union) (DLL ładowana do `system\Autorun`) do **Gothic II: Noc Kruka** (G2A, Steam Gold Classic, fix 2.6), który zamienia grę w symulator pomiarów geodezyjnych (materiały „Geodezja w Gothic”). Gracz obsługuje sprzęt **sam** (czyta łatę, celuje lunetą), a plugin liczy wyniki z *odczytanych* wartości z realistycznym szumem i porównuje je z prawdziwym położeniem w grze („prawdziwy błąd”). Dokumentacja użytkownika, sterowanie i opisy robót: `README.md` (długi, po polsku; aktualny co do funkcji, ale sekcje „Struktura” i „Ustawienia” są częściowo nieaktualne – np. nie ma `src/Level.hpp`, a w `README` przykład pakietu podaje `0.4.0`). Specyfikacje i plany funkcji: `docs/superpowers/{specs,plans}/` (`RRRR-MM-DD-<temat>-design.md` / plan). Skrypty scenariuszy wideo (`scenariusze/`) są w `.gitignore`.

Język: kod, komentarze, komunikaty, commity i dokumentacja są **po polsku**. Teksty wyświetlane w grze (`Say`, HUD, panele, etykiety menu) i nazwy plików wynikowych są **ASCII bez polskich znaków** (silnik Gothic ich nie renderuje; dotyczy też komentarzy `// ASCII` przy metodach `Job`). Polskie znaki są dopuszczalne w komentarzach i dokumentach (README, DOCX/raporty generowane osobno).

## Środowisko i polecenia

Windows, PowerShell 5.1 wystarcza. Build wymaga VS 2022 (MSVC 14.40+, **x86** – gra jest 32-bitowa), CMake ≥ 3.28, Ninja, Python 3 (generatory modeli); testy – `g++` (MSYS2 `C:\msys64\ucrt64\bin`).

```powershell
powershell -File scripts/build.ps1          # preset x86-release: configure + build + install do out/install/x86-release (wynik: BUILD OK)
.\scripts\install.ps1 -Game "C:\Program Files (x86)\Steam\steamapps\common\Gothic II"   # DLL+Signatures -> system\Autorun, modele/tekstury -> _work\Data, mapy i kolory tekstur
powershell -File tests/run.ps1               # kompiluje i uruchamia KAŻDY tests/test_*.cpp (g++ -std=c++20), potem check_docx.py; "ALL OK" albo exit 1
python scripts/package.py <wersja>           # po build.ps1: release/GothicSurveying-v<wersja>-G2A.zip (układ folderu gry + INSTALACJA.txt)
```

- **Pojedynczy test:** `g++ -std=c++20 -Wall -static -I src tests/test_tachy.cpp -o $env:TEMP\t.exe; & $env:TEMP\t.exe` (z roota repo, `g++` z `C:\msys64\ucrt64\bin`). Testy to samodzielne programy z `main()` zwracającym kod ≠ 0 przy błędzie (nie ma frameworka). `check_docx.py` działa tylko na `tests/out/sample*.docx`, które tworzą same testy.
- Nie ma lintera; formatowanie wg `.clang-format`.
- Gra jest w `C:\Program Files (x86)\Steam\steamapps\common\Gothic II` (nie w `C:\GothicII`). **Union odmawia startu („Union Fatal”), gdy ścieżka gry ma znaki spoza ASCII** – ścieżka repo (`C:\Moje_Różne\...`) jest taka, więc grę uruchamia się przez dowiązanie: `New-Item -ItemType Junction -Path C:\GothicII -Target "<katalog gry>"` i `C:\GothicII\system\Gothic2.exe`. Plugin nie da się uruchomić ani zweryfikować bez gry; zmiany w `calc/` i `job/` weryfikuj testami, zmiany w `engine/` tylko kompilacją + ręcznie w grze (powiedz wprost, czego nie sprawdziłeś).
- Wersja pluginu: domyślna wartość w `scripts/package.py` (`VERSION`, obecnie 0.14.2) i opis w `INSTALACJA.txt` w tym samym pliku; `Configuration.cmake` ma wersję szablonu (0.0.1), nie wydania. Przy nowej funkcji commity mają postać `wersja X.Y.Z: ...` / opis po polsku, ASCII, bez trailerów `Co-Authored-By`.
- Log wtyczki: `GothicSurveying.log` w katalogu gry (`SvTrace(...)`); pierwsza linia „1. DLL zaladowana” potwierdza załadowanie.

## Architektura (trzy warstwy; zależności tylko w dół)

Cały kod to **biblioteka nagłówkowa `.hpp`** (jedyna jednostka kompilacji to `src/Plugin.cpp`, który dla każdej włączonej gry – tu tylko `__G2A` – definiuje `GOTHIC_NAMESPACE` i includuje `Plugin.hpp` + `Plugin_G2A.hpp`). Dlatego:
- **`src/Plugin.hpp` ustala kolejność includów, i ona ma znaczenie**: moduły `engine/*` nie mają strażników include i nie includują się nawzajem; nowy plik silnikowy dopisuj w `Plugin.hpp` po zależnościach. Forward-deklaracje w `Controller.hpp` (np. `ProjUiOpenHome`) wskazują na pliki dołączane później.
- `calc/` i `job/` mają `#pragma once` i **nie zależą od Gothic API** – kompilują się zwykłym `g++` w testach. Zachowaj to: nic z `ZenGin`/`zCWorld`/`oCNpc` poza `engine/`.

1. **`src/calc/` – czysta matematyka, formaty, raporty** (testowane 1:1 plikiem `tests/test_<nazwa>.cpp`): `LevelMath` (odczyt łaty, dalmierz), `Traverse` (niwelacja: Δh, fh, poprawki), `Tachy` (obserwacje biegunowe, niwelacja trygonometryczna), `Polygon`/`PolygonReport` (ciąg poligonowy), `Gnss` (model odbiornika, maska nieba, PDOP, układ lokalny), `Control`/`WorldControl*` (osnowa, wzorcowe punkty światów), `Libella`, `Scan`/`ScanReg` (skaner, rejestracja na kulach), `Flight`/`Georef`/`AlsLas`/`AlsAdjust`/`AlsProducts` (ALS), `Camera`/`Bundle`/`Ortho`/`Png` (fotogrametria), `FieldBook`/`*Report` + `Docx` (CSV i DOCX), `Project`/`ProjectReport`/`UiSettings` (projekty), `State` (serializacja stanu: płaski słownik `klucz=wartość`, przedrostki `a.b.3.c`, długość wektora w `<przedrostek>.n`, `%`-kodowanie), `TexDecode`/`TexColors` (kolory tekstur).
2. **`src/job/` – automaty robót** (też bez silnika). `Job.hpp` definiuje `Job` (menu, `OnMenu`, odczyty, `Finish*`, `SaveState/LoadState`, `Tick`, `StatusLines`, `NextStep`…) oraz **`IWorld`** – abstrakcyjny „świat” (stawianie znaków/sprzętu, promienie, grunt, czas, zapis osnowy). Większość metod `IWorld` ma domyślne puste ciała, żeby roboty nie musiały znać cudzych funkcji. Testy podstawiają sztuczny świat (`tests/fake_aerial.hpp`, fake’i w `test_*_job.cpp`). **Nowa robota:** plik `job/<Nazwa>.hpp` + wpis (`id`, nazwa, factory, flagi rodzaju) w `JobTypes()` w `job/Jobs.hpp` + `JobKind` w `Job.hpp` + obsługa zakończenia w `engine/Controller.hpp` (`CtrlFinish...`, `CtrlMarkEnd`) + kreator w `Wizards.hpp`/sekcje w `Workflow.hpp` + `Help.hpp` + stan w `CanSaveState/SaveState/LoadState` + test. Menu: `Workflow.hpp` (sekcje, powody niedostępności `why`, linia DALEJ z `NextStep()`), `MenuLayout.hpp` (wiersze, przewijanie), `Wizards.hpp` (6 kreatorów i tryb prowadzony).
3. **`src/engine/` – część zależna od Gothic API:** `World.hpp` (`EngineWorld : IWorld`, ~800 linii: obiekty `oCVob`, raycasty, teren), `Controller.hpp` (stan globalny `gJob`, menu startowe/robota, F10 luneta, HUD, zapis/odczyt stanu, kończenie roboty), `Menu.hpp`/`Panels.hpp`/`TextInput.hpp` (UI na `zCView`), `Scope.hpp`/`TachyView.hpp`/`LevelView.hpp` (luneta, krzyż kresek, libella), `MapView.hpp`/`ControlNet.hpp`/`ControlLabel.hpp` (mapa i znaki osnowy), `Placement.hpp`, `PoleFine.hpp` (milimetrowe ustawianie tyczki/lustra), `Export.hpp` (zapis plików), `ProjectState.hpp`/`ProjectUi.hpp` (ekran główny Home, projekty), `FieldBook{View,Edit}.hpp`.

**Pętla i hooki** (`Plugin.hpp`, `userapi/*.inl` dodaje deklaracje metod `_Hooked` do klas Gothic API): `zCWorld::Render` → `ControllerTick()` (cała logika klatki, tylko dla `ogame->GetWorld()`, przed oryginałem – bo FOV lunety musi obowiązywać w tej samej klatce), `oCGame::WriteSavegame` → `CtrlSaveJobState(slot)`, `oCGame::LoadSavegame` → `gPendingLoadSlot` (wznowienie roboty zużywa `ControllerTick`). Adresy funkcji z `Signatures_G2A.tsv` (generowane z `Names.txt` Gothic API przez `scripts/convert-gothic-names.bat`). Klawisze tylko w `src/Keys.hpp` (Home = menu, F10 = luneta; F11 to GD3D11, F12 zrzut Steam, F5/F6 zapis – nie zajmować).

**Szablon:** `CMakeLists.txt`, `Configuration.cmake`, `cmake/`, `vdf/`, `scripts/{Clear-Build,Get/Set-UnionApiVersion,CommonFunctions}.ps1`, `Nek.ps1`, `LICENSE.md` pochodzą z [union-api-plugin-template](https://github.com/piotrmacha/union-api-plugin-template) (MIT). Union API i Gothic API pobiera CMake (UnionAPI `20240603.1019`, `UnionAPI-v143-windows-2022`); `CMakeLists.txt` zbiera `src/**.cpp` (glob), włącza tylko `GOTHIC_API_G2A`, standard C++23, wynik to `GothicSurveying.dll` + `UnionAPI.dll` + `Signatures/` + `GothicSurveying.vdf` (pakowany przez `vdf/Build-VDF.ps1`). Lokalne nadpisania: `Configuration.override.cmake` (ignorowany przez git).

## Zasoby (`assets/`, generowane skryptami Pythona)

- `make_models.py <katalog gry>` – wszystkie modele `.3DS` i tekstury `.TGA` do `_work\Data` (łata z teksturami podziałki, niwelator, tachimetr, tyczki, skaner, dron `LVL_DRONE/LVL_PROP`, kule…); wszystkie nazwy z prefiksem `LVL_`. Ustawienia osi: `UP = "Y"`, `FLIP_V` (cyfry na łacie do góry nogami). Uruchamiany przez `install.ps1` i `package.py`.
- `import_glb.py` (+ `assets/imported/ATTRIBUTION.txt`) – kask i kamizelka z Sketchfab (CC BY 4.0; obowiązkowa atrybucja w paczce i README).
- `make_world_control.py` → `calc/WorldControlData.hpp` (wzorcowa osnowa: Khorinis 21 pkt `NEWWORLD`, Dolina Górnicza 31 `OLDWORLD`, Jarkendar 11 `ADDONWORLD`, Irdorath bez zestawu); wynik jest **wygenerowany z modeli światów – nie edytuj ręcznie**.
- `make_game_maps.py` / `make_texture_colors.py "<katalog gry>"` – wyciągają z archiwów VDF **u użytkownika** pergaminy map (`MAP_<świat>.TGA`) i miniaturki tekstur (`GothicSurveying\texcolors.bin`). Pliki gry nie są w repo ani w paczce (w paczce skrypty trafiają do `NARZEDZIA`); zapasowe mapy `LVL_MAP_<świat>.TGA` w `assets/worldmaps`.

## Dane użytkownika w katalogu gry (`GothicSurveying/`)

`projekty/<nazwa>/projekt.txt` (manifest projektu; nazwa: ASCII, cyfry, `-`, `_`, ≤ 32 znaki; domyślnie `Luzem_<świat>`), `ustawienia.txt`, `osnowa.csv` (trwała osnowa: id `G..` wzorcowa, `O..` z GNSS), `stan_slot<N>.txt` (stan roboty obok zapisu gry), `odczyty_biezace.csv` (log bieżącej roboty; po końcu → `odczyty.csv` w folderze roboty), foldery wyników `RRRR-MM-DD_GG-MM-SS[_skan|_lot]/`, `texcolors.bin`. Obiekty pluginu (znaki, sprzęt) **nie są zapisywane w savegame** – znaki osnowy stawiane są od nowa z pliku, stan roboty wraca z `stan_slot<N>.txt`. Zmieniając format jakiegokolwiek pliku/stanu, zachowaj wczytywanie starych (stare zapisy wznawiają się w `Luzem_<świat>`).

## Konwencje rachunkowe (merytoryka – trzymaj się ich w `calc/` i `job/`)

- **Jednostki:** kąty w **gradach** (400g; 1g = 100c = 10000cc), przy lunecie/panelu także stopnie i mgon; niwelacja w **milimetrach**, GNSS/tachimetria/skaner/ALS w **metrach**. Gra: 1 j. = 1 cm.
- **Układ lokalny** (`calc/Gnss.hpp`): `N = z_gry/100 + 5 500 000 m`, `E = x_gry/100 + 6 500 000 m`, `H = y_gry/100 m`. Azymut od północy (N), rośnie w kierunku E: `ΔX(N) = d·cos A`, `ΔY(E) = d·sin A`.
- **Kąt pionowy = zenitalny V** (0g w zenicie), nie wysokościowy α (α = 100g − V): `d = D·sin V`, `Δh = D·cos V`. Kąty poziome **lewe** (β; azymut kolejnego boku `A(i,i+1) = A(i−1,i) + β − 200g`).
- **Niwelacja geometryczna:** `Δh = t − p` (wstecz − wprzód), `fh = ΣΔh − (H_k − H_p)`, poprawki proporcjonalne do długości celowych, `fh_dop = 20 mm·√L[km]` (**założenie projektu**, nie wzór z przewodnika); dalmierz `D = 100·(g − d)`; obsadzka niwelatora (±2–7 mm, średnia z dwóch kompletów, ostrzeżenie > 5 mm).
- **Niwelacja trygonometryczna:** `Δh = D·cosV + i − t + (1−k)·d²/(2R)`, k = 0,13, R = 6371 km, pomiar tam i z powrotem (kontrola 10 mm + 20 mm/km).
- **Tachimetria:** średnia orientacja z nawiązań (niepewne > 20 mgon / 20 mm + 5 ppm), dwa położenia lunety (KL+KP; `Hz = Hz_KL + ½(Hz_KP − 200 − Hz_KL)`, `V = ½(V_KL − V_KP + 400)`, ukryte błędy kolimacji `c` i indeksu `i`), wcięcie wstecz (obrót + przesunięcie), szum kątów 5″, odległości 2 mm + 2 ppm, wytyczanie, tolerancja celowania ±0,02° (≥ 1,5 cm).
- **Poligon:** `f_β = A_p + Σβ − n·200g − A_k`, `v_β = −f_β/n`, `f_x = X_A + ΣΔX − X_B`, poprawki liniowe ∝ długości boków; ciąg zamknięty: `Σβ = (n−2)·200g` (kąty lewe wewnętrzne, obieg przeciwny do zegara) albo `(n+2)·200g` (zewnętrzne) – kierunek obchodu jest wykrywany; limity `f_β,dop = 2·m_β·√n` (m_β = 5″) to założenie symulatora (raport mówi o tym wprost). Osnowę zapisuje tylko ciąg nawiązany obustronnie w limitach.
- **GNSS:** odbiornik nie zna prawdziwej wysokości – wynik = prawda + szum zależny od maski nieba (promienie 36 kier. × 6 elewacji → liczba satelitów, PDOP, wielodrogowość); fix (cm), float, DGPS, autonomiczne; własna baza: 1 ppm poziomo / 2 ppm pionowo + błąd pozycji bazy; statyka ≥ 30 min (1 s gry = 1 min), szum ×0,4; punkty osnowy zakłada się tylko ze statyki (i z poligonizacji).
- **Skaner/ALS/foto:** szum 1,5 mm + 10 ppm; georeferencja `P = X_gnss + R(imu)·(R_boresight·r + lever)` (wiązka leci z *prawdziwej* pozycji, punkt liczony z *estymaty* trajektorii); LAS 1.2 (format 2 skaner / 3 ALS, skala 1 mm); wyrównanie szeregów: Gauss-Newton na kątach boresightu + przesunięciach szeregów z oceną niepewności („nieokreślony” przy > 0,05°/0,05 m/0,1 m); fotogrametria: wyrównanie wiązki (Schur, LM), GCP i punkty kontrolne z osnowy, ortofotomapa + `.pgw`.
- **Obliczenia bez wyrównania ścisłego** są świadomym ograniczeniem (lista „Znane ograniczenia” w README) – nie obiecuj w raportach więcej niż robi kod. Każdy raport pokazuje założenia (limity, szumy) wprost.
- **Czas:** gra jest zatrzymana w menu i w lunecie; epoki GNSS i symulacje używają czasu gry (`IWorld::NowSec`), a ciężkie zadania (skan, lot, render zdjęć, ortofoto, wyrównanie) mają budżet ~12 ms/klatkę i są przerywalne (`Tick()`), wyniki idą strumieniowo na dysk.

## Przewodnik PDF „Pomiary geodezyjne - jak wykonywać i obliczać (przewodnik krok po kroku)" (Downloads, 9 s., 2026-11-24)

Sprawdzone 2026-11-24: wszystkie wzory, kolejność etapów i **wszystkie przykłady liczbowe są poprawne** (przeliczone niezależnie, zgodność do 1 mm / 0,0001 g).
Wartości dopuszczalnych odchyłek PDF celowo podaje tylko jako odwołanie do instrukcji; kod wtyczki przyjmuje `fh_dop = 20 mm * sqrt(L[km])` (`calc/Traverse.hpp`) i to jest założenie projektu, nie wzór z PDF.

### Poprawiona wersja w repo
`docs/przewodnik-pomiary-geodezyjne.pdf` (źródło: `docs/przewodnik-pomiary-geodezyjne.html`; PDF z HTML: `msedge --headless --no-pdf-header-footer --print-to-pdf=...`). Usunięto wady 1-6 poniżej: wzory wyrenderowane, uwaga o kącie wysokościowym i zenitalnym (α = 100g − V) z wzorami dla V, rozdział o ciągu zamkniętym ze wzorami i obchodem przeciwnym/zgodnym ze wskazówkami zegara, wysokość celu oznaczona `s`, odwołanie do G-4 zastąpione ogólnym (bez numeru załącznika), źródła: tylko dwa zweryfikowane linki, reszta samym tytułem.

### Co zweryfikowano (OK)
- **Grady i zaokrąglanie:** 1g = 100c = 10000cc; α[g] = 10/9 · α[°]; 23g07c17cc = 23,0717g; Bradis-Kryłow (4,25→4,2; 4,15→4,2).
- **Azymut:** przyrosty ΔX = d cos A, ΔY = d sin A; tabela ćwiartek (φ, 200−φ, 200+φ, 400−φ) zgodna ze znakami ΔX, ΔY; azymut przeciwny ±200g.
- **Niwelacja geometryczna:** Δh = t − p; f_h = ΣΔh − (H_Rk − H_Rp); v = −f_h/n (lub proporcjonalnie do długości celowych, jak w `calc/Traverse.hpp`); kontrola Σt − Σp = ΣΔh. Przykład (4,813 − 3,610 = 1,203; f_h = +3 mm; B = 151,200) poprawny.
- **Niwelacja trygonometryczna:** Δh = d·tgα + i − l; h = d(tgα_g − tgα_d) → 20,13 m (tg40g = 0,72654; tg5g = 0,07870); poprawka p = (1−k)d²/(2R) → 17 mm dla d = 500 m (k = 0,13, R = 6371 km); ta sama poprawka jest w `TrigHeightDiffM` (`calc/Tachy.hpp`).
- **Tachimetria:** β = K_P − K_N (mod 400g), A_SP = A_SN + β, d = d'·cosα, H_P = H_S + i + d'·sinα − l. Przykład: d = 49,944; ΔX = −35,316; ΔY = +35,316; P(964,684; 2035,316); H = 102,355 poprawne.
- **Ciąg poligonowy nawiązany obustronnie (kąty lewe):** A_{i,i+1} = A_{i−1,i} + β − 200g; f_β = A_p + Σβ − n·200g − A_k; v_β = −f_β/n; f_x = X_A + ΣΔX − X_B; v_x,i = −f_x·d_i/Σd. Przykład: A_p = 70,4833g, A_k = 33,0499g, f_β = +0,0020g, f_x = +0,014, f_y = +0,009, f_d = 0,017 m, współrzędne końcowe = B (1460,000; 1560,000) poprawne.

### Wady oryginalnego PDF (nie błędy rachunkowe; poprawione w wersji z repo)
1. **Wzory nie są wyrenderowane:** w blokach kodu stoi surowy LaTeX (`\Delta h = t - p`, `\frac{10}{9}`…) w całym dokumencie; do wydruku/nauki trzeba je wyrenderować.
2. **Kąt pionowy = kąt wysokościowy α (0g poziomo).** Tachimetry i kod wtyczki używają kąta **zenitalnego V** (0g w zenit). Konwersja: α = 100g − V, więc d = d'·sinV, Δh = d'·cosV. PDF o tym nie wspomina, a to częsta pomyłka; przy przenoszeniu wzorów do kodu używaj V (`calc/Tachy.hpp`).
3. **Kąty wewnętrzne a kąty lewe:** „suma kątów wewnętrznych (n−2)·200g" w ciągu zamkniętym jest prawdziwa tylko gdy kąty lewe są wewnętrzne (obieg przeciwnie do ruchu wskazówek zegara); przy obiegu zgodnym lewe kąty są zewnętrzne (suma (n+2)·200g). PDF tego nie zaznacza.
4. **Ciąg zamknięty bez wzorów:** PDF opisuje go jednym zdaniem (suma kątów, ΣΔX = ΣΔY = 0), bez wzorów f_β i f_x/f_y dla tego przypadku.
5. **Oznaczenie „l"** (wysokość celu) wygląda jak wielkie „I" w użytym kroju; lepiej np. „w" albo „ℓ".
6. **Nie do sprawdzenia:** powołanie na „Instrukcję G-4 (załącznik 3)" i numer załącznika oraz aktualność tej instrukcji (przepisy techniczne mogły zostać zmienione); linki w źródłach nie były otwierane. PDF sam zastrzega, że podręczniki są „z pamięci" i dane w przykładach są wymyślone.

### Jak z tego korzystać
Traktuj PDF jako poprawny materiał dydaktyczny do wzorów i kolejności etapów; przy zmianach w `calc/` pilnuj konwencji (zenit V, grady vs stopnie/radiany, kąty lewe) i nie przenoś wartości dopuszczalnych z PDF, bo ich tam nie ma.
