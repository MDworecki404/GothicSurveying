# GothicSurveying

Niwelacja geometryczna w **Gothic II: Noc Kruka** jako plugin [Union](https://gothic-modding-community.github.io/gmc/zengin/union).
Stawiasz niwelator, łaty i żabki, patrzysz przez lunetę z krzyżem kresek, **sam odczytujesz podziałkę z łaty**
i wpisujesz wynik w milimetrach. Plugin liczy wysokości (BS/FS) i porównuje je z prawdziwą wysokością terenu.
Powstał do materiałów „Geodezja w Gothic”.

## Co potrzebujesz

- Gothic II NotR z zainstalowanym **Union** (w `system\` musi być `Union.patch`).
- Do zbudowania: Visual Studio 2022 z pakietem C++ (MSVC 14.40+), CMake 3.28+, Ninja, Python 3 (generator modeli).
- Windows PowerShell 5.1 wystarczy (`pwsh` nie jest wymagany).

> **Ścieżka gry bez polskich znaków.** Union odmawia startu z komunikatem *Union Fatal*, jeśli w ścieżce są znaki spoza ASCII
> (np. `C:\Moje_Różne\...`). Utwórz dowiązanie i uruchamiaj grę z niego:
> `New-Item -ItemType Junction -Path C:\GothicII -Target "<prawdziwy katalog gry>"`, potem `C:\GothicII\system\Gothic2.exe`.
> Skróty z Eksploratora zamieniają dowiązanie na prawdziwą ścieżkę, więc zrób skrót ręcznie do ścieżki z dowiązania.

## Budowanie i instalacja

W „Developer PowerShell for VS 2022” (x86) w katalogu repo:

```powershell
cmake --preset x86-release
cmake --build out/build/x86-release
cmake --install out/build/x86-release --prefix out/install/x86-release
.\scripts\install.ps1 -Game C:\GothicII
```

Konfiguracja pobiera Union API i Gothic API (przez CMake, [szablon](https://github.com/piotrmacha/union-api-plugin-template)).
`install.ps1` kopiuje DLL i `Signatures` do `system\Autorun`, generuje modele i tekstury do `_work\Data` i czyści ich cache.
Jeśli `Signatures_G2A.tsv` nie powstał podczas konfiguracji, uruchom
`scripts\convert-gothic-names.bat <Names.txt z union-api> out\build\x86-release\Signatures\Signatures_G2A.tsv`.

## Sterowanie

**F12** otwiera menu geodezyjne (↑/↓ wybór, Enter zatwierdza, F12 zamyka; gra jest wtedy zatrzymana).
Poza robotą menu ma pozycję „Rozpocznij robotę”: wybierasz typ roboty (na razie ciąg niwelacyjny), rodzaj ciągu
(dowiązany, zamknięty, otwarty) i tryb liczenia dziennika (automatyczny albo samodzielny). W trakcie roboty menu zawiera
kroki tej roboty (niedostępne są wyszarzone z podpowiedzią), „Patrz przez lunetę” i „Przerwij robotę”; u dołu ekranu widać bieżący krok.

Sprzęt staje pod stopami bohatera. Przebieg ciągu niwelacyjnego: **Postaw reper A** → **Postaw niwelator** → **Postaw łatę wstecz** →
odczyt wstecz w lunecie → **Postaw łatę wprzód** (punkt obrotowy z żabką albo punkt końcowy) → odczyt wprzód →
**Przełóż na następne stanowisko** (niwelator i tylna łata zabierane, łata wprzód zostaje jako wstecz) → … → po odczycie
na punkcie końcowym **Zakończ robotę**. Punkt końcowy: dla ciągu dowiązanego reper B, dla zamkniętego powrót na reper A
(stań na nim), dla otwartego dowolny punkt.

| Klawisz | Akcja |
|---|---|
| **F12** | menu geodezyjne |
| **F10** | wejdź do widoku niwelatora / wyjdź z niego (gra zatrzymana, zbliżenie 20×, krzyż kresek) |
| **←/→** | w lunecie: obrót niwelatora w poziomie (**Shift** = powoli) |
| **Enter** | w lunecie: odczyt; pionowa kreska musi leżeć na łacie; wpisz odczyt środkowej kreski w mm (0–9, Backspace, Enter), potem opcjonalnie górnej i dolnej kreski dalmierczej (Enter bez liczby pomija odległość); F10 anuluje |

Pierwszy odczyt na stanowisku może być wstecz lub wprzód (kolejność dowolna); przewyższenie to wstecz − wprzód. Wysokość punktu to wysokość stopy łaty
(szczyt znaku reperu lub żabki).

## Poprawianie błędów w trakcie roboty

- „Powtórz odczyty na tym stanowisku” kasuje odczyty bieżącego stanowiska (np. po pomyłce przy wpisywaniu), a „Przestaw niwelator” pozwala
  postawić go w innym miejscu, dopóki nie ma odczytów.
- „Przerwij robotę” pyta o potwierdzenie (domyślnie „Nie”) i zapisuje to, co zmierzono, do `dziennik_N_przerwany.csv`.
- W trakcie roboty każdy przyjęty odczyt trafia też do pomocniczego logu `niwelacja_N.csv` (punkt, odczyt środkowy, górna i dolna kreska).

## Dziennik i pliki

Po **Zakończ robotę** gra pokazuje tabelę dziennika (z celowymi z dalmierza, jeśli wpisano kreski) (↑/↓ przewijanie, Enter zamyka) i zapisuje w katalogu głównym gry:
`dziennik_N.docx` (A4 pozioma, do wydruku: tabela, Σ, fh, fh dop., wynik, data) oraz `dziennik_N.csv` (dane, średniki).
Dziennik liczy fh = ΣΔh − (H_koniec − H_start), poprawki równomierne na stanowisko i wysokości wyrównane;
dopuszczalny błąd fh_dop = 20 mm·√L (L w km, suma celowych). Ciąg otwarty nie ma kontroli ani poprawek.
Jeśli w trakcie roboty wczytasz zapis, plugin zapisze to, co zmierzono, do `dziennik_N_przerwany.csv` (bez DOCX).
„Przerwij robotę” kończy robotę bez zapisu dziennika.

## Tryb samodzielny

Przy starcie roboty możesz wybrać „Dziennik liczę sam”. Po **Zakończ robotę** pojawia się edytor: tabela z pustymi komórkami
(Δh, poprawki v, wysokości wyrównane, Σ Δh, fh). Strzałki zmieniają komórkę, cyfry, `-` i `.` wpisują wartość (Δh i v w mm, H w m),
Backspace kasuje, **F12** kończy. Plugin porównuje każdą komórkę z poprawną wartością (tolerancja 1 mm), pokazuje listę chybionych
i zapisuje CSV oraz DOCX z tabelą „Sprawdzenie” (wartości poprawne, Twoje, różnice).

## Dalmierz nitkowy

Nad i pod poziomą kreską leżą dwie krótkie kreski dalmiercze (stała k = 100). Odległość do łaty to
**D = 100 × (odczyt górnej − odczyt dolnej)**, np. 1,578 m − 1,378 m = 0,200 m, czyli 20,0 m. Po odczycie środkowej kreski
wpisz odczyty górnej i dolnej (albo pomiń Enterem). Plugin porównuje wynik z prawdziwą odległością i zapisuje oba w CSV
(`dalmierz_m`, `blad_dalmierza_m`). Odstęp kresek na ekranie jest liczony z pola widzenia gry, więc zgadza się ze stałą k niezależnie od ustawień FOV.

## Jak czytać łatę

Opis przy decymetrze to jego numer od podstawy łaty (`14` = 1,40 m). Nieparzyste metry są czerwone. Centymetry liczysz w literze E
(5 cm: pręt, przerwa, pręt, przerwa, pręt po 1 cm), a milimetry szacujesz na oko tam, gdzie poziomą kreską przecina łatę krzyż kresek.

## Struktura

- `src/calc/` – czyste obliczenia bez silnika (testowane): `LevelMath.hpp` (odczyt, dalmierz), `Traverse.hpp` (przewyższenia, fh, poprawki, wysokości),
  `FieldBook.hpp` (dziennik + CSV), `Docx.hpp` (zapis DOCX), `ManualCheck.hpp` (sprawdzanie trybu samodzielnego).
- `src/job/` – interfejs `Job`/`IWorld` i automat ciągu niwelacyjnego (bez silnika, testowany sztucznym światem); nowe roboty rejestruje się w `Jobs.hpp`.
- `src/engine/` – część zależna od Gothic API: panele, stawianie, luneta, menu, adapter `EngineWorld`, kontroler, eksport, podgląd i edytor dziennika.
- `src/Plugin.hpp` – hook `zCWorld::Render` (pętla klatki), `userapi/zCWorld.inl` – deklaracja hooka.
- `assets/make_models.py` – modele `.3DS` (łata 3 m z 6 teksturami, niwelator na statywie, żabka, znak reperu) i tekstury `.TGA` (podziałka, krzyż, kreski, panel).
- `tests/` – `powershell -File tests/run.ps1` kompiluje i uruchamia testy `g++` oraz sprawdza wygenerowane DOCX (`check_docx.py`).
- Reszta (`CMakeLists.txt`, `cmake/`, `scripts/`, `vdf/`) pochodzi z szablonu; `scripts/build.ps1` buduje plugin, `scripts/install.ps1` instaluje w grze.

## Ustawienia

Stałe `LV_*` na początku `src/Level.hpp`: wysokość osi lunety, zbliżenie, prędkości obrotu, maksymalna celowa, błąd kolimacji `LV_COLLIM` (w sekundach, np. 20 pokazuje błąd systematyczny).
Jeśli cyfry na łacie są w grze do góry nogami, ustaw `FLIP_V = True` w `assets/make_models.py`; jeśli modele leżą poziomo – `UP = "Y"`.

## Znane ograniczenia

- Plugin nie zapisuje stanu: po wyjściu z gry sprzęt trzeba postawić od nowa (wyniki zostają w CSV).
- Tylko jedna robota (ciąg niwelacyjny); tachimetria i niwelacja trygonometryczna dojdą jako kolejne moduły `Job`.
- Kreski dalmiercze liczone są dla pola widzenia gry; przy bardzo wąskim FOV mogą wypaść poza lunetę (wtedy są pomijane, wpis w `GothicSurveying.log`).
- Hook na `zCWorld::Render` sprawdzono tylko na Gothic II NotR 2.6 (fix) ze Steam Gold Classic.

## Licencja

Szkielet CMake pochodzi z [union-api-plugin-template](https://github.com/piotrmacha/union-api-plugin-template) (MIT, Piotr Macha), zob. `LICENSE.md`.
Licencja własnego kodu (`src/Level*.hpp`, `assets/`, `tests/`, `scripts/install.ps1`) nie została jeszcze wybrana.
