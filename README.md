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

| Klawisz | Akcja |
|---|---|
| **F7** | postaw niwelator pod stopami bohatera (nowe stanowisko) |
| **F8** | postaw łatę (pierwsza łata = reper o znanej, prawdziwej wysokości) |
| **F9** | postaw żabkę (łata postawiona < 30 cm od niej stoi na niej) |
| **F10** | wejdź do widoku niwelatora / wyjdź z niego (gra zatrzymana, zbliżenie 20×, krzyż kresek) |
| **←/→** | obrót niwelatora w poziomie (**Shift** = powoli) |
| **Enter** | odczyt: pionowa kreska musi leżeć na łacie; wpisz odczyt w mm (0–9, Backspace, Enter, F10 anuluje) |
| **F11** | usuń wszystko |

Pierwszy odczyt na stanowisku to wstecz (BS) na łacie o znanej wysokości, kolejne to w przód (FS). Linia celowa jest zawsze pozioma.
Wyniki trafiają do `niwelacja_N.csv` w katalogu głównym gry: Twój odczyt, odczyt idealny, wysokość, błąd względem prawdziwej.

## Jak czytać łatę

Opis przy decymetrze to jego numer od podstawy łaty (`14` = 1,40 m). Nieparzyste metry są czerwone. Centymetry liczysz w literze E
(5 cm: pręt, przerwa, pręt, przerwa, pręt po 1 cm), a milimetry szacujesz na oko tam, gdzie poziomą kreską przecina łatę krzyż kresek.

## Struktura

- `src/Level.hpp` – logika pluginu i interfejs (widok lunety, pola tekstowe, CSV), `src/LevelMath.hpp` – czysta arytmetyka niwelacji.
- `src/Plugin.hpp` – hook `zCWorld::Render` (pętla klatki), `userapi/zCWorld.inl` – deklaracja hooka.
- `assets/make_models.py` – generuje modele `.3DS` (łata 3 m z 6 teksturami, niwelator na statywie, żabka) i tekstury `.TGA` (podziałka, krzyż, panel).
- `tests/test_level_math.cpp` – test arytmetyki: `g++ tests/test_level_math.cpp -o t && ./t`.
- Reszta (`CMakeLists.txt`, `cmake/`, `scripts/`, `vdf/`) pochodzi z szablonu.

## Ustawienia

Stałe `LV_*` na początku `src/Level.hpp`: wysokość osi lunety, zbliżenie, prędkości obrotu, maksymalna celowa, błąd kolimacji `LV_COLLIM` (w sekundach, np. 20 pokazuje błąd systematyczny).
Jeśli cyfry na łacie są w grze do góry nogami, ustaw `FLIP_V = True` w `assets/make_models.py`; jeśli modele leżą poziomo – `UP = "Y"`.

## Znane ograniczenia

- Plugin nie zapisuje stanu: po wyjściu z gry sprzęt trzeba postawić od nowa (wyniki zostają w CSV).
- Brak kresek dalmierczych i automatycznego ciągu niwelacyjnego z wyrównaniem.
- Hook na `zCWorld::Render` sprawdzono tylko na Gothic II NotR 2.6 (fix) ze Steam Gold Classic.

## Licencja

Szkielet CMake pochodzi z [union-api-plugin-template](https://github.com/piotrmacha/union-api-plugin-template) (MIT, Piotr Macha), zob. `LICENSE.md`.
Licencja własnego kodu (`src/Level*.hpp`, `assets/`, `tests/`, `scripts/install.ps1`) nie została jeszcze wybrana.
