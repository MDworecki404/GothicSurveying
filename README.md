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

## Wydanie (paczka do rozpakowania)

`python scripts/package.py 0.4.0` (po `scripts/build.ps1`) tworzy `release/GothicSurveying-v0.1.2-G2A.zip`. Archiwum ma układ folderu gry
(`system/Autorun/...` i `_work/Data/...`) i `INSTALACJA.txt`; wystarczy je rozpakować w folderze Gothic II z zainstalowanym Union.

## Sterowanie

**Home** otwiera menu geodezyjne (↑/↓ wybór, Enter zatwierdza, Home zamyka; gra jest wtedy zatrzymana).
Poza robotą menu ma pozycję „Rozpocznij robotę”: wybierasz typ roboty (na razie ciąg niwelacyjny), rodzaj ciągu
(dowiązany, zamknięty, otwarty) i tryb liczenia dziennika (automatyczny albo samodzielny). W trakcie roboty menu zawiera
kroki tej roboty (niedostępne są wyszarzone z podpowiedzią), „Patrz przez lunetę” i „Przerwij robotę”; u dołu ekranu widać bieżący krok.

Na czas roboty bohater zakłada żółty kask i kamizelkę odblaskową (modele doczepione do węzłów `ZS_HELMET` i `ZS_TORSO`, jego pancerz zostaje); zdejmuje je po **Zakończ robotę** (także po „Przerwij” i po wczytaniu zapisu).

Sprzęt staje pod stopami bohatera. Przebieg ciągu niwelacyjnego: **Postaw reper A** → **Postaw niwelator** → **Postaw łatę wstecz** →
odczyt wstecz w lunecie → **Postaw łatę wprzód** (punkt obrotowy z żabką albo punkt końcowy) → odczyt wprzód →
**Przełóż na następne stanowisko** (niwelator i tylna łata zabierane, łata wprzód zostaje jako wstecz) → … → po odczycie
na punkcie końcowym **Zakończ robotę**. Punkt końcowy: dla ciągu dowiązanego reper B, dla zamkniętego powrót na reper A
(stań na nim), dla otwartego dowolny punkt.

| Klawisz | Akcja |
|---|---|
| **Home** | menu geodezyjne |
| **F10** | wejdź do widoku niwelatora / wyjdź z niego (gra zatrzymana, zbliżenie 20×, krzyż kresek) |
| **←/→** | w lunecie: obrót niwelatora w poziomie (**Shift** = powoli) |
| **Z** / **X** | w lunecie: płynny obrót na łatę wstecz / wprzód (przerywa go ręczny obrót strzałkami); po dolocie pionowa kreska leży na łacie |
| **Enter** | w lunecie: odczyt; pionowa kreska musi leżeć na łacie; wpisz odczyt środkowej kreski w mm (0–9, Backspace, Enter); F10 anuluje |

Pierwszy odczyt na stanowisku może być wstecz lub wprzód (kolejność dowolna); przewyższenie to wstecz − wprzód. Wysokość punktu to wysokość stopy łaty
(szczyt znaku reperu lub żabki).

## Poprawianie błędów w trakcie roboty

- „Powtórz odczyty na tym stanowisku” kasuje odczyty bieżącego stanowiska (np. po pomyłce przy wpisywaniu), a „Przestaw niwelator” pozwala
  postawić go w innym miejscu, dopóki nie ma odczytów.
- „Przerwij robotę” pyta o potwierdzenie (domyślnie „Nie”) i zapisuje to, co zmierzono, jako `dziennik_przerwany.csv` w folderze roboty.
- W trakcie roboty każdy przyjęty odczyt trafia też do pomocniczego logu `GothicSurveying/odczyty_biezace.csv` (punkt, odczyt środkowy); po zakończeniu roboty log trafia do jej folderu jako `odczyty.csv`.

## Dziennik i pliki

Po **Zakończ robotę** gra pokazuje tabelę dziennika (z celowymi z dalmierza) (↑/↓ przewijanie, Enter zamyka) i zapisuje w katalogu głównym gry:
`dziennik.docx` (A4 pozioma, do wydruku: tabela, Σ, fh, fh dop., wynik, data) oraz `dziennik.csv` (dane, średniki). Wszystko ląduje w folderze `GothicSurveying/RRRR-MM-DD_GG-MM-SS/` w katalogu gry (nazwa = czas zakończenia roboty).
Dziennik liczy fh = ΣΔh − (H_koniec − H_start), poprawki równomierne na stanowisko i wysokości wyrównane;
dopuszczalny błąd fh_dop = 20 mm·√L (L w km, suma celowych). Ciąg otwarty nie ma kontroli ani poprawek.
Jeśli w trakcie roboty wczytasz zapis, plugin zapisze to, co zmierzono, jako `dziennik_przerwany.csv` w folderze roboty (bez DOCX).
„Przerwij robotę” kończy robotę bez zapisu dziennika.

## Tryb samodzielny

Przy starcie roboty możesz wybrać „Dziennik liczę sam”. Po **Zakończ robotę** pojawia się edytor: tabela z pustymi komórkami
(Δh, poprawki v, wysokości wyrównane, Σ Δh, fh). Strzałki zmieniają komórkę, cyfry, `-` i `.` wpisują wartość (Δh i v w mm, H w m),
Backspace kasuje, **Home** kończy. Plugin porównuje każdą komórkę z poprawną wartością (tolerancja 1 mm), pokazuje listę chybionych
i zapisuje CSV oraz DOCX z tabelą „Sprawdzenie” (wartości poprawne, Twoje, różnice).

## Pomiar GNSS (RTK)

„Rozpocznij robotę” → „Pomiar GNSS (RTK)” (pytanie, czy pokazać prawdziwy błąd po pomiarze). Cel: nauczyć się obsługi odbiornika i zebrać
współrzędne punktów. Odbiornik **nie zna prawdziwej wysokości**: podaje wynik z szumem, typem rozwiązania i wskaźnikami jakości.

Przebieg: **Postaw tyczkę z odbiornikiem** (pod stopami bohatera, tyczka 2,000 m) → ewentualnie **Wysokość anteny** (presety 1,500 / 1,800 /
2,000 / 2,100 / 2,500 m; zły preset daje błąd systematyczny w H) → **Rozpocznij pomiar punktu** → czekaj, aż w panelu w lewym górnym rogu
pojawi się „fix RTK” → **Zapisz punkt** (fix i co najmniej 10 epok; bez fixu po 5 epokach z adnotacją „zapis bez fixu”) → kolejne punkty →
**Zakończ robotę**. „Odrzuć pomiar” kasuje bieżącą sesję.

Pomiar GNSS jest w metrach (niwelacja zostaje w milimetrach). Panel odbiornika pokazuje rozwiązanie, satelity (użyte/widoczne), PDOP, epoki (co 1 s czasu gry; gdy gra zatrzymana, epoki nie płyną), RMS [m] i bieżące
N, E, H. Typy rozwiązania i orientacyjna dokładność: fix RTK ok. 1–2 cm, float RTK kilkadziesiąt cm, DGPS ok. metra, autonomiczne kilka metrów.
Fix pojawia się po 8–20 s, float po 3–6 s – o ile pozwalają warunki. Gdy fixu brak, panel pokazuje zasłonięcie nieba, odległość do najbliższej przeszkody i powód („za mało satelitów”, „słaba geometria PDOP”, „przeszkoda blisko”, „inicjalizacja – czekaj”).

**Precyzyjne ustawienie tyczki** (menu: „Precyzyjne ustawienie tyczki”, gdy tyczka stoi i nie trwa pomiar): gra się zatrzymuje, kamera schodzi nad podstawę
tyczki, a strzałki przesuwają ją względem widoku (góra = od kamery) o **1 mm**, z **Shift** o 10 mm, z **Ctrl** o 100 mm; przytrzymanie powtarza krok.
Panel pokazuje przesunięcie od startu i pozycję tyczki w układzie lokalnym; **Backspace** wraca na start, **Enter** (albo Home) zatwierdza.
Tyczka osadza się na gruncie pod nowym miejscem, a dziennik porównuje pomiar z tą pozycją (opcja prawdziwego błędu).

Jakość zależy od otoczenia: z miejsca tyczki liczone są promienie ku niebu (36 kierunków × 6 elewacji), z nich maska horyzontu, z maski liczba
użytych satelitów i prawdziwy PDOP, a bliska ściana lub drzewo zwiększa wielodrogowość. Pod gołym niebem fix łapie się szybko, przy ścianie
tylko float, pod dachem często wcale. Uśrednianie epok zmniejsza szum, ale nie błąd systematyczny sesji (jak w prawdziwym GNSS).

Współrzędne w lokalnym układzie: N = z_gry/100 + 5 500 000 m, E = x_gry/100 + 6 500 000 m, H = y_gry/100 m (stałe w `src/calc/Gnss.hpp`).
Każdy zapisany punkt trafia od razu do `GothicSurveying/odczyty_biezace.csv` (przetrwa awarię gry; po zakończeniu roboty plik ląduje w jej folderze jako `odczyty.csv`).
Wyniki w folderze roboty: `pomiar_gnss.csv`, `pomiar_gnss.docx` (przerwana robota: `pomiar_gnss_przerwany.csv`); przy opcji prawdziwego błędu
dodatkowe kolumny `blad_N_m`, `blad_E_m`, `blad_H_m`, `blad_2D_m` względem punktu w grze.

## Osnowa GNSS

W robocie GNSS, gdy odbiornik ma **fix** i co najmniej **20 epok**, menu ma pozycję **Załóż punkt osnowy**. Zapisuje ona punkt tak jak „Zapisz punkt”
(w dzienniku GNSS z uwagą „osnowa O1”), dopisuje go do trwałego pliku `GothicSurveying/osnowa.csv` (id O1, O2…, świat, zmierzone N/E/H,
położenie znaku w grze, rozwiązanie, PDOP, epoki, czas) i **stawia znak (reper)**, który zostaje po zakończeniu roboty. Po wczytaniu zapisu albo wejściu
do świata znaki wracają z pliku. W menu głównym (bez roboty) pozycja **Pokaż osnowę** wyświetla punkty bieżącego świata.

Osnowę można zakładać także **w trakcie roboty Tachimetria**: menu ma pozycje „GNSS: Postaw tyczkę z odbiornikiem”, „GNSS: Wysokość anteny”, „GNSS: Rozpocznij pomiar punktu”,
„GNSS: Załóż punkt osnowy” i „GNSS: Odrzuć pomiar” (te same zasady co w robocie GNSS: fix i 20 epok, panel odbiornika w lewym górnym rogu). Tyczka GNSS zajmuje miejsce tyczki
z lustrem: po założeniu punktu postaw tyczkę z lustrem ponownie; w trakcie pomiaru GNSS nie można jej przestawić. Nowy punkt trafia do osnowy i jest od razu dostępny jako źródło
współrzędnych (przełącznik „Źródło współrzędnych”, możliwy przed postawieniem tachimetru). Punkty GNSS założone w tachimetrii nie trafiają do dziennika tachimetrycznego, tylko do `osnowa.csv`.

**Osnowa geodezyjna świata (wzorcowa).** Wtyczka ma wbudowany gotowy zestaw punktów osnowy dla czterech światów Gothica II: Khorinis (21 punktów, `NEWWORLD`), Dolina Górnicza (31, `OLDWORLD`)
i Jarkendar (11, `ADDONWORLD`); Irdorath (statek) nie ma zestawu. W menu głównym (bez roboty) pozycja **Załaduj osnowę geodezyjną świata** dopisuje punkty bieżącego świata do osnowy
(id `G01`, `G02`…, opis miejsca w polu rozwiązania, np. „osnowa wzorcowa: Cmentarz”), stawia ich znaki i pokazuje listę od najbliższych z odległością i kierunkiem od ciebie. Ponowne
załadowanie nic nie duplikuje, a punkty z robót GNSS (O1, O2…) zostają. Współrzędne punktów wzorcowych są dokładne (do 1 mm), więc nadają się jako osnowa wyższego rzędu niż punkty z GNSS.
Rozmieszczenie wyliczył skrypt `assets/make_world_control.py` z modeli światów (płaski i otwarty teren w pobliżu dróg, odstępy co najmniej 40–70 m, wzajemna widoczność, nazwy
od najbliższej lokacji). **Mapa osnowy** (pozycja „Mapa osnowy” w menu głównym i w menu każdej roboty) otwiera panel z mapą świata (cieniowany teren i sieć dróg, północ u góry),
punktami osnowy z numerami (czerwone: wzorcowa G.., zielone: założona GNSS-em O..), żółtym znacznikiem ciebie i informacją o najbliższym punkcie; Enter albo Home zamyka. Mapy i dane
generuje ten sam skrypt, a tekstury `LVL_MAP_<świat>.TGA` są w `assets/worldmaps` i trafiają do gry razem z modelami. **Teleport do punktu osnowy** (menu głównym i w menu roboty) pokazuje listę punktów świata (numery jak na mapie, nazwa miejsca, odległość i kierunek; po 10 na stronę) i przenosi bohatera nad wybrany znak. W ciasnych miejscach (miasto, wąwozy) jakość nieba bywa gorsza, więc fix GNSS tam nie jest pewny.

Współrzędne osnowy mają błąd GNSS (centymetry), więc przechodzą na pomiary tachimetryczne oparte na osnowie. Obiekty pluginu (znaki, tyczki, sprzęt) nie są
zapisywane w zapisach gry, a znaki osnowy są stawiane na nowo z pliku, więc nie powinny się dublować po wczytaniu zapisu. Zapisy zrobione starszą wersją pluginu mogą mieć
„duchy” (zapisany kask, tyczkę lub znak) - wczytaj zapis sprzed pomiarów.

## Tachimetria (pomiar biegunowy)

„Rozpocznij robotę” → „Tachimetria (pomiar biegunowy)” (pytanie, czy pokazać prawdziwy błąd po pomiarze). Cel: nauczyć się ustawiać stanowisko,
orientować tachimetr na kilka nawiązań i mierzyć punkty metodą biegunową. Współrzędne wychodzą z **odczytanych** kątów i odległości, więc widać błąd
celowania i szum instrumentu, a nie wynik „z gry”.

**Źródło współrzędnych** (menu): *osnowa GNSS* (domyślnie, gdy w świecie są punkty osnowy) albo *prawdziwe*. Przy osnowie stanowisko i nawiązania wybierasz z listy
punktów osnowy: tachimetr staje na znaku (musisz stać nie dalej niż 2 m), lustra nawiązań na znakach 3–300 m od stanowiska; współrzędne to te zmierzone
GNSS-em (z błędem). Przy źródle *prawdziwe* stawiasz je pod stopami, jak wcześniej.

Przebieg: **Postaw tachimetr (stanowisko)** → **Dodaj nawiązanie (lustro)** (do 5; znak z tyczką i lustrem zwróconym ku tachimetrowi; „Usuń ostatnie nawiązanie”)
→ **Patrz przez lunetę** (**F10**) i **zmierz nawiązania**: celujesz krzyżem w środek lustra każdego nawiązania i naciskasz **Enter**. Orientacja to średnia z kierunków
wszystkich zmierzonych nawiązań (co najmniej jedno). Panel i komunikat pokazują dla każdego **odchyłkę kierunku** (mgon) i **odchyłkę odległości** (mm) względem współrzędnych;
powyżej 20 mgon albo 20 mm + 5 ppm nawiązanie jest oznaczone „niepewne”. Potem **Postaw tyczkę z lustrem**, celuj w lustro punktu i **Enter**. Dodanie, usunięcie
nawiązania albo przestawienie stanowiska kasuje odczyty (orientację trzeba zrobić od nowa).

**Rodzaj pomiaru** (menu): *na lustro* albo *bezlustrowy*. Pomiar bezlustrowy nie wymaga tyczki: celujesz w dowolny punkt terenu lub budynku, a odległość
mierzy promień (pierwszy trafiony punkt, od 1,5 m do 300 m, szum odległości 3 mm + 2 ppm); wysokość lustra nie jest odejmowana, a punkt ma uwagę „bezlustrowy”.
**Wysokość lustra** (menu: 1,500 / 1,800 / 2,000 / 1,300 m) to fizyczna wysokość środka lustra: tyczki (nawiązań i pomiarowa) mają model o tej wysokości.

W lunecie: **← →** obrót w poziomie, **↑ ↓** pochylanie w pionie, **Shift** = powoli, **Enter** mierzy nawiązanie albo punkt, **F10** wychodzi. Panel w lewym dolnym rogu pokazuje
Hz i V na żywo (grady), stan nawiązań, rodzaj pomiaru, liczbę punktów i ostatni wynik. Celowanie wymaga trafienia w środek lustra z dokładnością **±0,02°** (nie mniej niż 1,5 cm),
odległości 3–300 m i niezasłoniętej linii celowej; lustro nawiązania ma pierwszeństwo, gdy oba lustra leżą na tej samej linii.

Obliczenia: `Az = Hz` (koło poziome zorientowane na współrzędne), `d = D·sin V`, `N = Ns + d·cos Az`, `E = Es + d·sin Az`, `H = Hs + ih + D·cos V − wysokość lustra`
(`ih` = 1,500 m, V = kąt zenitalny). Szum: kąty 5″, odległość 2 mm + 2 ppm. Kąty w gradach, odległości i współrzędne w metrach (układ lokalny jak w GNSS).
Wyniki: `pomiar_tachimetryczny.csv` i `.docx` w folderze roboty (stanowisko, źródło współrzędnych, tabela nawiązań z odchyłkami, tabela punktów;
przerwana: `pomiar_tachimetryczny_przerwany.csv`), każdy punkt od razu w `odczyty_biezace.csv`; opcja prawdziwego błędu dodaje kolumny `blad_N_m`, `blad_E_m`, `blad_H_m`, `blad_2D_m`
(przy osnowie błąd zawiera też błąd osnowy GNSS; przy pomiarze bezlustrowym jest liczony względem trafionego punktu).

## Obsadzka (zmiana wysokości niwelatora)

Gdy na stanowisku masz odczyty wstecz i wprzód, w menu (Home) dostępna jest **Obsadzka**: niwelator zostaje losowo podniesiony albo obniżony
o 2–7 mm (znak i wielkość losuje plugin, nie mówi którą), po czym czytasz obie łaty jeszcze raz (komplet II). Przewyższenie stanowiska
to **średnia z obu kompletów**, a różnica Δh I − Δh II trafia do dziennika. Gdy przekracza 5 mm, w uwagach pojawia się „UWAGA: roznica obsadzek > 5 mm”.
W CSV i DOCX są trzy dodatkowe kolumny: `odczyt_wstecz2_mm`, `odczyt_wprzod2_mm`, `roznica_dh_mm`; kolumny odczytów wstecz/wprzód zawierają komplet I,
a tabela w grze pokazuje oba odczyty jako `I/II`. Obsadzka jest opcjonalna i możliwa na każdym stanowisku, także na końcowym (przed „Zakończ robotę”).
Do czasu odczytów kompletu II nie można zakończyć roboty ani przełożyć niwelatora; „Przerwij” zachowuje komplet I.

## Dalmierz nitkowy

Dalmierz mierzy się sam. Odległość nitkowa (k = 100) to **D = 100 × (odczyt górnej − odczyt dolnej)**. Po odczycie środkowej kreski
plugin bierze prawdziwą poziomą odległość instrument→łata, wyznacza idealne odczyty górnej i dolnej kreski (środek ± D/200), zaokrągla każdy do 1 mm
(jak przy odczycie na oko, bez szumu) i liczy z nich D. Dlatego celowe wstecz i wprzód (`celowa_wstecz_m`, `celowa_wprzod_m`) są w CSV i DOCX zawsze
wypełnione, z rozdzielczością 0,1 m wynikającą z zaokrąglenia do 1 mm. Dwie krótkie kreski dalmiercze na krzyżu lunety są tylko elementem wizualnym
(odstęp liczony z pola widzenia gry, zgodny ze stałą k).

## Klawisze (zmiana w jednym miejscu)

Wszystkie klawisze są stałymi w `src/Keys.hpp`. Pod F11 jest menu GD3D11, F12 to zrzut ekranu Steam, F5/F6 to zapis/odczyt gry,
więc menu geodezyjne jest pod **Home**, luneta pod **F10**, a obrót na łatę pod **Z** / **X**.

## Licencje modeli

Kask i kamizelka odblaskowa to modele z Sketchfab na licencji [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/), uproszczone i
skonwertowane do `.3DS` (`assets/import_glb.py`): [„Construction Hat”](https://sketchfab.com/3d-models/construction-hat-80094363c4c24725be0ff0bb9f3b65e5)
autorstwa ermalkoci i [„safety vest”](https://sketchfab.com/3d-models/safety-vest-8510a33da89a41cea6150b2af4d10df0) autorstwa teacherap123.
Pozostałe modele są generowane przez `assets/make_models.py`.

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
- Trzy roboty: ciąg niwelacyjny, pomiar GNSS (RTK) i tachimetria; niwelacja trygonometryczna dojdzie jako kolejny moduł `Job`.
- Tachimetria: bez wcięcia wstecz i w przód, bez stanowiska dowolnego, bez wyrównania osnowy, bez wytyczania i bez pomiaru w dwóch położeniach lunety; jedno stanowisko na robotę.
- Pomiar GNSS: bez stacji bazowej i bez pomiaru statycznego.
- Kreski dalmiercze liczone są dla pola widzenia gry; przy bardzo wąskim FOV mogą wypaść poza lunetę (wtedy są pomijane, wpis w `GothicSurveying.log`).
- Hook na `zCWorld::Render` sprawdzono tylko na Gothic II NotR 2.6 (fix) ze Steam Gold Classic.

## Licencja

Szkielet CMake pochodzi z [union-api-plugin-template](https://github.com/piotrmacha/union-api-plugin-template) (MIT, Piotr Macha), zob. `LICENSE.md`.
Licencja własnego kodu (`src/Level*.hpp`, `assets/`, `tests/`, `scripts/install.ps1`) nie została jeszcze wybrana.
