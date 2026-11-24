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

**Home** otwiera ekran główny projektu (↑/↓ wybór, Enter zatwierdza, ← wraca do poprzedniego ekranu, cyfry 1–9 wybierają pozycję listy,
**F1** pokazuje pomoc kontekstowa, Home zamyka; gra jest wtedy zatrzymana). Menu prowadzi: linia **DALEJ** podpowiada następny krok (kursor staje na nim od razu),
pozycje niedostępne są w [nawiasach], a podpis pod listą mówi, dlaczego. Szczegóły w sekcji „Projekty i pomiary” poniżej.
W trakcie pomiaru menu zawiera kroki tej roboty pogrupowane w sekcje (Przygotowanie, Parametry, Pomiar, Wyniki, Narzędzia, Zaawansowane), „Patrz przez lunetę” i „Przerwij pomiar” (zawsze ostatnie); u dołu ekranu widać bieżący krok.

Na czas roboty bohater zakłada żółty kask i kamizelkę odblaskową (modele doczepione do węzłów `ZS_HELMET` i `ZS_TORSO`, jego pancerz zostaje); zdejmuje je po **Zakończ robotę** (także po „Przerwij” i po wczytaniu zapisu).

Sprzęt staje pod stopami bohatera. Przebieg ciągu niwelacyjnego: **Postaw reper A** → **Postaw niwelator** → **Postaw łatę wstecz** →
odczyt wstecz w lunecie → **Postaw łatę wprzód** (punkt obrotowy z żabką albo punkt końcowy) → odczyt wprzód →
**Przełóż na następne stanowisko** (niwelator i tylna łata zabierane, łata wprzód zostaje jako wstecz) → … → po odczycie
na punkcie końcowym **Zakończ robotę**. Punkt końcowy: dla ciągu dowiązanego reper B, dla zamkniętego powrót na reper A
(stań na nim), dla otwartego dowolny punkt.

| Klawisz | Akcja |
|---|---|
| **Home** | ekran główny projektu / menu pomiaru; w menu: **←** wstecz, **1–9** skrót pozycji (ekran główny), **F1** pomoc |
| **F10** | wejdź do widoku niwelatora / wyjdź z niego (gra zatrzymana, zbliżenie 20×, krzyż kresek) |
| **←/→** | w lunecie: obrót niwelatora w poziomie (**Shift** = powoli) |
| **Z** / **X** | w lunecie: płynny obrót na łatę wstecz / wprzód (przerywa go ręczny obrót strzałkami); po dolocie pionowa kreska leży na łacie |
| **Enter** | w lunecie: odczyt; pionowa kreska musi leżeć na łacie; wpisz odczyt środkowej kreski w mm (0–9, Backspace, Enter); F10 anuluje |

Pierwszy odczyt na stanowisku może być wstecz lub wprzód (kolejność dowolna); przewyższenie to wstecz − wprzód. Wysokość punktu to wysokość stopy łaty
(szczyt znaku reperu lub żabki).

## Projekty i pomiary

**Projekt** to praca na jednym świecie; w nim wykonujesz **pomiary** (ciąg niwelacyjny, GNSS, tachimetria, niwelacja trygonometryczna, skaner, nalot ALS, fotogrametria) – także kilka tego samego rodzaju, po kolei (naraz trwa jeden pomiar).
Projekt jest niezależny od zapisu gry: leży w `GothicSurveying/projekty/<nazwa>/projekt.txt` (nazwa nadawana automatycznie jako `<świat>_<n>`) i działa w każdym zapisie tego samego świata.
Przy pierwszym uruchomieniu powstaje projekt `Luzem_<świat>`; zapisy stanu zrobione przed wprowadzeniem projektów wznawiają się właśnie w nim.

**Ekran główny** pokazuje okruszki (projekt, świat, liczba punktów osnowy i pomiarów), wyróżnioną linię **DALEJ**, listę pomiarów ze stanem (`[x]` gotowy, `[~]` w toku, `[!]` przerwany, `[ ]` nowy),
**+ Nowy pomiar…**, **Wyniki i raporty**, **Zamknij projekt**, **Projekty** (zmiana i zakładanie) oraz **Osnowa, mapa, teleport, poziomowanie**. Home otwiera ostatnio używany projekt (pamięć w `GothicSurveying/ustawienia.txt`, tam też ostatnia pozycja kursora).

**Nowy pomiar** oferuje sześć **kreatorów** (gotowe ustawienia i tryb prowadzony): Pomierz osnowę (GNSS, tryb statyczny), Niwelacja ciągu A → B, Inwentaryzacja tachimetrem, Zeskanuj budynek (rejestracja na kulach),
Nalot z drona (LiDAR, szeregi kalibracyjne) i Zdjęcia z drona. W **trybie prowadzonym** menu pokazuje tylko następny krok, „Pokaż wszystkie pozycje” i „Przerwij”; „Pokaż wszystkie” przełącza pomiar na pełne menu.
Zamiast kreatora można wybrać rodzaj ręcznie (pełne menu). Dawna pozycja „Rozpocznij robotę” z opisów robót poniżej to teraz „+ Nowy pomiar…” → „Ręczny wybór rodzaju pomiaru…”.

**Wyniki i raporty** wypisują pliki z folderów pomiarów (`GothicSurveying/<data>`, nazwy bez zmian) i otwierają folder w Eksploratorze. Raportów robót nie zastępuje nic nowego; **raport zbiorczy** powstaje dopiero przy **zamknięciu projektu**
(potwierdzenie, domyślnie „Nie”): `projekt_raport.docx` i `projekt_raport.csv` w folderze projektu – lista pomiarów ze stanem i podsumowaniem oraz porównanie wysokości na wspólnych punktach
(ta sama nazwa albo położenie w promieniu 0,5 m; z pomiarów, które zapisują punkty wysokościowe: niwelacja, GNSS, tachimetria i niwelacja trygonometryczna – dla ALS i fotogrametrii porównania nie ma).
Zamknięty projekt jest tylko do odczytu; „Otwórz projekt ponownie” przywraca możliwość dodawania pomiarów.

Enter na pomiarze z listy otwiera jego menu: szczegóły, pliki wynikowe, otwarcie folderu i **Usuń pomiar i jego wyniki** (potwierdzenie, domyślnie „Nie”; dotyczy też pomiarów przerwanych). Kasuje folder wyników pomiaru (jeśli nie dzieli go inny pomiar), plik punktów w folderze projektu i wpis w projekcie; **nie usuwa punktów osnowy** założonych przez pomiar (są wspólne – służy do tego „Usuń osnowę”). Pomiaru w toku i pomiarów w zamkniętym projekcie usunąć nie można.

## Poprawianie błędów w trakcie roboty

- „Powtórz odczyty na tym stanowisku” kasuje odczyty bieżącego stanowiska (np. po pomyłce przy wpisywaniu), a „Przestaw niwelator” pozwala
  postawić go w innym miejscu, dopóki nie ma odczytów.
- „Przerwij robotę” pyta o potwierdzenie (domyślnie „Nie”) i zapisuje to, co zmierzono, jako `dziennik_przerwany.csv` w folderze roboty.
- W trakcie roboty każdy przyjęty odczyt trafia też do pomocniczego logu `GothicSurveying/odczyty_biezace.csv` (punkt, odczyt środkowy); po zakończeniu roboty log trafia do jej folderu jako `odczyty.csv`.

## Dziennik i pliki

Po **Zakończ robotę** gra pokazuje tabelę dziennika (z celowymi z dalmierza) (↑/↓ przewijanie, Enter zamyka) i zapisuje w katalogu głównym gry:
`dziennik.docx` (A4 pozioma, do wydruku: tabela, Σ, fh, fh dop., wynik, data) oraz `dziennik.csv` (dane, średniki). Wszystko ląduje w folderze `GothicSurveying/RRRR-MM-DD_GG-MM-SS/` w katalogu gry (nazwa = czas zakończenia roboty).
Dziennik liczy fh = ΣΔh − (H_koniec − H_start), poprawki proporcjonalne do długości celowych stanowiska (równe, gdy celowe jednakowe) i wysokości wyrównane;
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

### Własna baza i statyka

„Poprawki RTK: sieć / własna baza”: przy własnej bazie **Postaw bazę (stacja bazowa)** (menu robót GNSS). Bez bazy odbiornik daje tylko wynik autonomiczny (metry). Baza na znaku osnowy (≤ 2 m)
zna swoje współrzędne (błąd mm), baza na dowolnym punkcie ma błąd pozycji rzędu metra, który przesuwa wszystkie punkty rovera tak samo; dochodzi błąd linii bazowej (1 ppm poziomo, 2 ppm pionowo).
„Pomiar: statyczny” wymaga fixu i 120 epok, a szum fixu jest wtedy 0,4 zwykłego; uwaga przy punkcie podaje bazę, linię bazową i statykę.

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
(id `G01`, `G02`…, rozwiązanie „osnowa wzorcowa”), stawia ich znaki i pokazuje listę od najbliższych z odległością i kierunkiem od ciebie. Ponowne
załadowanie nic nie duplikuje, a punkty z robót GNSS (O1, O2…) zostają. Współrzędne punktów wzorcowych są dokładne (do 1 mm), więc nadają się jako osnowa wyższego rzędu niż punkty z GNSS.
Rozmieszczenie wyliczył skrypt `assets/make_world_control.py` z modeli światów (płaski i otwarty teren w pobliżu dróg, odstępy co najmniej 40–70 m, wzajemna widoczność, nazwy
od najbliższej lokacji). **Usuń osnowę** (menu główne): usuwa jeden punkt z listy, wszystkie punkty wzorcowe (G..), wszystkie założone GNSS-em (O..) albo całą osnowę bieżącego świata; zawsze po potwierdzeniu (domyślnie „Nie”). Znika wpis z `osnowa.csv` i znak w świecie. Wzorcowe punkty można potem załadować ponownie. **Etykieta punktu**: gdy jesteś w promieniu 12 m od znaku osnowy, nad nim unosi się panel z jego numerem i odległością (np. „G07 (7 m)”); znika w menu, w lunecie i na mapie. **Mapa osnowy** (pozycja „Mapa osnowy” w menu głównym i w menu każdej roboty) otwiera panel z mapą świata (**mapa z gry**, pergamin `MAP_<świat>.TGA` wyciągany z archiwów VDF przez `assets/make_game_maps.py`, który uruchamia `scripts/install.ps1`; bez niej wygenerowany cieniowany teren z siecią dróg, północ u góry),
punktami osnowy z numerami (czerwone: wzorcowa G.., zielone: założona GNSS-em O..), żółtym znacznikiem ciebie i informacją o najbliższym punkcie; Enter albo Home zamyka. Dane osnowy generuje ten sam skrypt, a zapasowe mapy `LVL_MAP_<świat>.TGA` są w `assets/worldmaps` i trafiają do gry razem z modelami. Mapa z gry jest rysunkiem (granice świata z `Doc_SetLevelCoords` skryptów, sprawdzone na punktach osnowy), więc jest dokładna do kilku–kilkunastu metrów; podgląd planu nalotu rysuje się metrycznie. Pliki map gry nie są w repozytorium ani w paczce, tylko wyciągane u użytkownika. **Teleport do punktu osnowy** (menu głównym i w menu roboty) pokazuje listę punktów świata (numery jak na mapie, odległość i kierunek; po 10 na stronę) i przenosi bohatera nad wybrany znak. W ciasnych miejscach (miasto, wąwozy) jakość nieba bywa gorsza, więc fix GNSS tam nie jest pewny.

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
**Położenia lunety** (menu: jedno / dwa KL+KP): w trybie dwóch instrument ma ukryte błędy kolimacji i indeksu pionowego (kilka mgon), więc pojedynczy odczyt jest obciążony.
Każdy cel (nawiązanie, punkt na lustro, punkt bezlustrowy) mierzysz **Enter** w KL, naciskasz **K** (przerzut lunety: panel pokazuje Hz+200 g i V′ = 400 g − V), celujesz ponownie i **Enter** w KP;
wynik to średnia (Hz = Hz_KL + ½(Hz_KP − 200 − Hz_KL), V = ½(V_KL − V_KP + 400)), a komunikat i uwaga punktu podają wyznaczone `c` (kolimacja) i `i` (indeks) w mgon.
Zmiana trybu kasuje orientację. Domyślnie jedno położenie, bez tych błędów.

**Stanowisko dowolne (wcięcie wstecz)** (menu „Stanowisko: dowolne”, przy źródle osnowa GNSS): tachimetr stawiasz gdziekolwiek, dodajesz co najmniej 2 nawiązania na punktach osnowy
i mierzysz je (kierunek i odległość). Współrzędne stanowiska, orientację koła i wysokość liczy obrót + przesunięcie z dopasowania do wszystkich nawiązań; panel pokazuje N, E i rms dopasowania.
**Wiele stanowisk**: po zmierzeniu punktu menu ma „Nowe stanowisko (zamknij bieżące)”. Zamyka ono stanowisko (S1, S2…) razem z jego nawiązaniami i odchyłkami, a następne stawiasz od nowa
(na znanym punkcie albo dowolnie, z własną orientacją). Punkty numerują się dalej (T1, T2…) i mają w uwagach „stanowisko S<n>”; raport (CSV i DOCX) opisuje każde stanowisko osobno
i podsumowuje liczbę stanowisk. Punkt mierzony z dwóch stanowisk daje porównanie współrzędnych bez wyrównania.

**Wytyczanie** („Wytycz punkt osnowy”, po orientacji): wybierasz punkt osnowy 3–300 m od stanowiska, ustawiasz tyczkę z lustrem i mierzysz **Enter**; komunikat mówi, o ile przesunąć tyczkę
(„dalej/bliżej” wzdłuż celowej i „w prawo/w lewo” prostopadle, razem m), a przy odchyłce ≤ 2 cm: „wytyczony” z odchyłką i dH. Pomiar trafia do punktów z uwagą „wytyczanie <id>”. „Zakończ wytyczanie” wyłącza podpowiedź.

**Wysokość lustra** (menu: 1,500 / 1,800 / 2,000 / 1,300 m) to fizyczna wysokość środka lustra: tyczki (nawiązań i pomiarowa) mają model o tej wysokości.

W lunecie: **← →** obrót w poziomie, **↑ ↓** pochylanie w pionie, **Shift** = powoli, w niwelacji menu ma pozycję **Wysun laty** (3 → 4 → 5 m; łata ma wtedy 10 odcinków z opisem decymetrów do 49, a odczyt może być do 5000 mm), **PgUp / PgDn** zmieniają powiększenie lunety (3×–40×, domyślnie 20×; działa też w niwelatorze), **Z** szybko najeżdża na kolejne lustro (nawiązania i tyczka) — lustro ląduje w polu widzenia lunety, ale celowo obok krzyża, więc dokładne naprowadzenie robisz strzałkami; **Enter** mierzy nawiązanie albo punkt, **F10** wychodzi. Panel w lewym dolnym rogu pokazuje
Hz i V na żywo (grady), stan nawiązań, rodzaj pomiaru, liczbę punktów i ostatni wynik. Celowanie wymaga trafienia w środek lustra z dokładnością **±0,02°** (nie mniej niż 1,5 cm),
odległości 3–300 m i niezasłoniętej linii celowej; lustro nawiązania ma pierwszeństwo, gdy oba lustra leżą na tej samej linii.

Obliczenia: `Az = Hz` (koło poziome zorientowane na współrzędne), `d = D·sin V`, `N = Ns + d·cos Az`, `E = Es + d·sin Az`, `H = Hs + ih + D·cos V − wysokość lustra`
(`ih` = 1,500 m, V = kąt zenitalny). Szum: kąty 5″, odległość 2 mm + 2 ppm. Kąty w gradach, odległości i współrzędne w metrach (układ lokalny jak w GNSS).
Wyniki: `pomiar_tachimetryczny.csv` i `.docx` w folderze roboty (stanowisko, źródło współrzędnych, tabela nawiązań z odchyłkami, tabela punktów;
przerwana: `pomiar_tachimetryczny_przerwany.csv`), każdy punkt od razu w `odczyty_biezace.csv`; opcja prawdziwego błędu dodaje kolumny `blad_N_m`, `blad_E_m`, `blad_H_m`, `blad_2D_m`
(przy osnowie błąd zawiera też błąd osnowy GNSS; przy pomiarze bezlustrowym jest liczony względem trafionego punktu).

## Niwelacja trygonometryczna

„Rozpocznij robotę” → „Niwelacja trygonometryczna” (pytanie o prawdziwy błąd). Przewyższenia mierzysz tachimetrem: z odczytanego kąta zenitalnego V i odległości skośnej D,
`Δh = D·cosV + i − t + (1−k)·d²/(2R)` (i = 1,5 m wysokość instrumentu, t = wysokość lustra, k = 0,13, R = 6371 km). Odcinek S→T mierzysz **tam** (tachimetr na S, lustro na T)
i **z powrotem** (tachimetr na T, lustro na S): średnia usuwa wpływ wysokości instrumentu i lustra oraz większość krzywizny i refrakcji, a suma obu pomiarów to kontrola
(dopuszczalnie 10 mm + 20 mm/km).

Przebieg: **Postaw reper A** (wysokość znana) → **Postaw tachimetr na punkcie A** (staje nad znakiem, bez chodzenia) → przejdź i **Postaw punkt celu z lustrem** (znak i lustro pod stopami) → **Patrz przez lunetę** (**F10**),
wyceluj w lustro (jak w tachimetrii, ±0,02°) i **Enter** → komunikat prosi o przeniesienie tachimetru na B i lustra na A → pomiar z powrotem → wynik odcinka (Δh średnie, suma, H punktu B).
Następny odcinek zaczyna się na B: postaw tachimetr na B (staje nad znakiem) i nowy punkt celu C itd. Przy pomiarze z powrotem tachimetr i lustro też stają nad znakami punktów, więc nie musisz przy nich stać. Wyniki: `pomiar_tachimetryczny.csv` i `.docx` w folderze roboty (tabela odcinków i pojedyncze obserwacje); opcja prawdziwego błędu dodaje błąd Δh.

## Zapis gry w trakcie roboty

Gdy zapisujesz grę w trakcie roboty (F5/F6 albo menu zapisu), plugin zapisuje obok niej stan roboty i sprzętu w `GothicSurveying/stan_slot<N>.txt` (N = numer slotu zapisu): zmierzone odczyty i punkty,
kroki roboty, ustawienia (KL/KP, źródło współrzędnych, własna baza, statyka, wysokość lustra, wysunięcie łaty…), pochylenie libelli oraz położenie znaków, instrumentu, łat, tyczek, luster i bazy.
Po **wczytaniu** takiego zapisu robota wraca w tym samym stanie, ze sprzętem na swoich miejscach (komunikat „Wznowiono robotę”). Zapis bez roboty usuwa stan tego slotu, a wczytanie zapisu
bez stanu albo z innego świata kończy bieżącą robotę jak dotąd (zmierzone punkty trafiają do folderu „przerwana”). Wznowiona robota ma nowy dziennik bieżący (`odczyty_biezace.csv`) bez wcześniejszych odczytów; raport końcowy jest kompletny.

## Skaner laserowy naziemny

„Rozpocznij robotę” → „Skaner laserowy”. **Postaw skaner (stanowisko)** (pod stopami; stając przy znaku osnowy dziedziczy jego współrzędne GNSS, inaczej używa prawdziwych),
**wypoziomuj** go (menu „Poziomuj instrument”, jak niwelator: babelek w kółku ≤ 8′), wybierz **rozdzielczość** i **Rozpocznij skan**. Skaner wykonuje siatkę promieni jak prawdziwy:
360° w poziomie, kąt zenitalny V od 0° do 125° (martwe pole pod statywem), kolumna po kolumnie; każdy promień trafia w teren albo w voby świata (z kolizją; bez postaci)
i daje punkt, jeśli odległość mieści się w 0,5–120 m. Szum odległości 1,5 mm + 10 ppm, intensywność 0–255 z kąta padania i odległości (powierzchnie prostopadłe są jaśniejsze, voby nieco jaśniejsze od terenu).
Skan trwa w czasie gry (klatka po klatce w budżecie ok. 12 ms; pasek postępu z liczbą punktów), a „Przerwij skan” go anuluje. Rozdzielczości: krok 1,0° (ok. 45 tys. kierunków), 0,5°, 0,25° i 0,1° (ok. 4,5 mln kierunków; to już minuty).

Wyniki w folderze `GothicSurveying/RRRR-MM-DD_GG-MM-SS_skan/`: `skanNN.pts` (liczba punktów w pierwszej linii, potem `E N H intensywnosc R G B`; X = E, Y = N, Z = H w układzie lokalnym siatki),
`skanNN.las` (LAS 1.2, format 2 z kolorem RGB, skala 1 mm; otwiera się np. w CloudCompare) i `skanNN_opis.txt` (parametry i stanowisko). Można zrobić wiele skanów na różnych stanowiskach w jednej robocie;
po „Zakończ robotę” panel podsumowuje stanowiska i liczby punktów. Punkty zapisywane są strumieniowo na dysk, więc chmura nie obciąża pamięci gry. **Widać, że skaner mierzy:** obrotowa głowica (osobny model) kręci się za azymutem skanowanej kolumny, a od głowicy do ostatnich ~40 trafień biegną czerwone linie (wiązki lasera), które znikają po skanie;
HUD pokazuje procent i liczbę punktów.

**Kolor RGB** (menu „Kolor RGB”, domyślnie włączony): punkty na terenie i w budynkach ze świata dostają **kolor tekstury** w miejscu trafienia: UV z interpolacji cech wierzchołków wielokąta, a kolor z miniaturki
tekstury (16×16, interpolacja dwuliniowa) z pliku `GothicSurveying/texcolors.bin`, który tworzy jednorazowo `python assets/make_texture_colors.py "<katalog gry>"` (robi to też `scripts/install.ps1`; w wydaniu jest w `NARZEDZIA`).
Miniaturki wylicza narzędzie z archiwów VDF gry (średnia z nagłówków tekstur zgadza się z nimi w granicach ±2), bo bufory tekstur w pamięci silnika okazały się niewiarygodne. Plik jest niezależny od silnika; poniżej zapasowy odczyt z bufora, jeśli nie ma miniaturki; gdy odczyt się nie uda, bierze się średnią barwę tekstury, a dla vobów (ławki, płoty, skrzynie) średnią z materiałów ich siatki, bo nie wiadomo, która część została trafiona.
Kolor jest przyciemniony dla powierzchni ustawionych skośnie do promienia; kule referencyjne są pomarańczowe, a bez żadnych danych punkt jest szary. Pierwsze próbki tekstur trafiają do `GothicSurveying.log`
(uv, rozmiar tekstury, wynik). Z wyłączonym kolorem punkty są szare wg intensywności.

**Rejestracja skanów na kulach referencyjnych** (domyślnie włączona; pozycja „Rejestracja” w menu): skan powstaje w **układzie lokalnym skanera** z losowym zerem azymutu, a stanowisko i obrót wyznacza się z kul.
Dodaj co najmniej 3 (max 6) **kul referencyjnych** na znakach osnowy („Dodaj kulę referencyjną”: punkt osnowy do 150 m od ciebie; kula o średnicy 29 cm staje 0,5 m nad znakiem). Kule są retrorefleksyjne
(intensywność 255), więc po skanie przeglądowym plugin je wykrywa, **doskanowuje gęsto** wokół każdej (pasek „doskanowanie kul”), dopasowuje do nich kulę metodą najmniejszych kwadratów (środek z dokładnością mm),
przyporządkowuje do punktów osnowy (sprawdza wszystkie przyporządkowania, odrzuca niejednoznaczne) i dopasowuje obrót wokół pionu oraz przesunięcie (skaner jest poziomy). Komunikat i `skanNN_opis.txt` podają
liczbę kul, rms dopasowania, współrzędne stanowiska (N, E, H osi skanera) i obrót; skan zapisuje się w siatce, więc skany z różnych stanowisk składają się w jedną chmurę. Kule zasłonięte przez teren lub
voby nie są wykrywane (wystarczą 3 widoczne); gdy rejestracja się nie uda (za mało kul, niezgodność współrzędnych), skan zostaje w układzie lokalnym jako `skanNN_lokalny.pts/.las`.
Przy zbyt grubej rozdzielczości mała kula może wypaść między promieniami skanu przeglądowego — wtedy wybierz drobniejszy krok.

## Skaning lotniczy z drona (ALS)

„Rozpocznij robotę” → „Skaning lotniczy z drona (ALS)” (pytanie, czy pokazać prawdziwy błąd, czyli ukryty błąd boresightu w opisie lotu). Cel: zaplanować nalot i zobaczyć, jak błędy trajektorii GNSS/IMU,
boresightu i ramienia dźwigni (lever arm) przenoszą się na chmurę punktów. Dron startuje spod stóp bohatera i leci sam; skaner liniowy (obracające się lustro, FOV 60°, linie skanu prostopadle do kierunku lotu)
puszcza promienie jak skaner naziemny (zasięg do 250 m, szum 1,5 mm + 10 ppm).

**Plan nalotu.** Obszar można też **zaznaczyć na mapie**: „Zaznacz obszar na mapie (dwa narożniki)” otwiera mapę świata (ok. 2–2,6 m na piksel), strzałki przesuwają celownik (Shift szybko, Ctrl wolno), Enter ustawia pierwszy narożnik (dolny albo górny, dowolna kolejność), drugi Enter zatwierdza prostokąt o bokach równoległych do N i E (pasek pokazuje wymiary i pole), Backspace cofa, Home anuluje; zaznaczenie zastępuje obecny obszar. Poza zmapowanym fragmentem świata wierzchołki zadajesz jak niżej. Obszar to 3–12 wierzchołków: „Dodaj wierzchołek obszaru (pod stopami)” albo „(punkt osnowy)” z listy. Parametry (menu): AGL 30/50/80/100/150 m, prędkość 3/5/6/8 m/s,
zakładka boczna 20/30/40%, kierunek szeregów (domyślnie wzdłuż najdłuższego boku albo 0/45/90/135°), wysokość „podąża za terenem” (stałe AGL, wygładzone) albo stała bezwzględna, profil błędów
(RTK dobry ok. 2 cm / PPK / słaby z dryfem) i gęstość impulsów (2/5/10/20 kHz). Plan to szeregi z naprzemiennym kierunkiem (wielokąty wklęsłe dzielą się na odcinki). Hud i „Podgląd planu nalotu”
(powiększony kadr: obszar żółty, szeregi zielone, dojazd czerwony, punkty osnowy w kadrze) pokazują liczbę szeregów, pas, odstęp, czas lotu, szacowaną gęstość i liczbę lotów na bateriach. **Bateria** (menu: 5/10/15/25/40 min lotu, zapas 25%) nie blokuje startu: gdy następny szereg się nie mieści, dron kończy szereg, wraca do bazy, ląduje,
wymienia baterię (10 s) i leci dalej od następnego szeregu (szeregi dostają numery lotów, plan pokazuje „N loty (wymiana baterii)”). „Zapisz plan nalotu (CSV)” zapisuje `plan_nalotu.csv`. Przed startem plugin ostrzega o szeregach przecinających teren lub obiekty (bez omijania).

**Lot.** „Start lotu”: dron (korpus, cztery obracające się śmigła, głowica LiDAR) wznosi się (8 m/s), dolatuje (12 m/s) do pierwszego szeregu, skanuje po kolei (zawis przy zakręcie 2 s) i wraca. Czas symulacji rośnie z impulsami, więc gęstość
nie zależy od FPS; w każdej klatce plugin zużywa ok. 12 ms. „Przerwij lot” kończy z częściowym wynikiem, „Kamera za dronem” (eksperymentalna) ustawia kamerę 12 m za nim.
Georeferencja jest bezpośrednia: `P = X_gnss + R(imu)·(R_boresight·r + lever)`; wiązka biegnie z prawdziwej pozycji i z prawdziwym (ukrytym) montażem skanera, a punkt jest liczony
z estymaty trajektorii (GNSS: szum + dryf, IMU: szum + bias, boresight nominalny), więc błąd trajektorii widać w chmurze tak jak w prawdziwym ALS.

**Wyniki** (folder `GothicSurveying/RRRR-MM-DD_GG-MM-SS_lot/`): `lot.las` (LAS 1.2, format 3: RGB, intensywność, czas GPS = 345600 s + czas lotu, numer szeregu jako „point source id”, skala 1 mm z offsetem),
`lot.pts`, `trajektoria.csv` (co 0,1 s: prawdziwa pozycja i kąty oraz estymata GNSS/IMU), `plan_nalotu.csv` i `lot_opis.txt`. Opis zawiera parametry, profil błędów, zakres chmury, **kontrolę szeregów**
(różnica wysokości w zakładkach, `meanDz` i rms dla par kolejnych szeregów; bez wyrównania) oraz **kontrolę na punktach osnowy** w obszarze (mediana dolnej ćwiartki H chmury w promieniu 2 m minus H punktu osnowy).
Przy opcji prawdziwego błędu opis podaje też ukryty błąd boresightu. Punkty zapisują się strumieniowo.

**Zapis gry w trakcie lotu:** zapisuje plan i ustawienia; po wczytaniu lot wznawia się od początku bieżącego szeregu (wcześniejsze szeregi zostają, trajektoria dojazdu po wznowieniu nie jest zapisywana; gdy plik roboczy lotu już nie pasuje do zapisu albo lot został dokończony, pozostałe szeregi trafiają do nowego folderu i nie nadpisują wyniku).
Dron i jego śmigła to modele `LVL_DRONE.3DS` i `LVL_PROP.3DS` z `assets/make_models.py`. Plan i trajektoria są niezależne od sensora (`SensorSpec`), żeby później posłużyć fotogrametrii lotniczej.

### Szeregi kalibracyjne, wyrównanie szeregów i kalibracja boresightu

W menu planu „Szeregi kalibracyjne” dodaje 3 szeregi prostopadłe do głównych (w tym samym obszarze): na płaskim terenie pozwalają wyznaczyć też pitch i yaw boresightu.
Po locie pozycja **„Wyrównaj szeregi i skalibruj boresight”** (praca w kilku klatkach, przerywalna) działa na plikach lotu (`lot_surowy.bin`, `trajektoria.csv`): w nakładkach szeregów
(komórki 4 m, płaszczyzny na szereg, wspólny punkt porównania w komórce) estymuje **jednocześnie** trzy kąty boresightu (Gauss-Newton, odrzucanie odstających) i przesunięcia E/N/H każdego szeregu.
Punkty osnowy w obszarze (G.., O..) ustalają wysokość i przesunięcia bezwzględne; bez nich przesunięcia mają średnią 0. Niepewność każdego kąta i przesunięcia wynika z macierzy normalnej:
składowa z niepewnością powyżej 0,05° (kąty), 0,05 m (H) albo 0,1 m (E/N) jest oznaczona „nieokreślony” i zostaje 0 (np. pitch/yaw na płaskim terenie bez szeregów poprzecznych).
Wyznaczana jest łączna poprawka montażu: stały bias IMU i błąd boresightu działają tak samo, więc raport przy „pokaż prawdziwy błąd” podaje oba osobno i oczekiwaną poprawkę (boresight − bias).
Wynik: `lot_wyrownany.las/.pts` (oryginał `lot.las` bez zmian) i `lot_wyrownanie.txt` (kąty ± niepewność, rms różnic w nakładkach przed / po kalibracji / po wyrównaniu, przesunięcia szeregów,
osnowa przed → po). Kontrola szeregów w `lot_opis.txt` porównuje teraz także szeregi o numerach nienastępujących po sobie, gdy mają wspólne komórki (szeregi kalibracyjne, wielokąty wklęsłe).

### Produkty z chmury

Pozycja **„Produkty z chmury”** (z wyrównanej chmury, a bez niej z surowej) tworzy w folderze lotu podfolder `produkty/`: `lot_klasyfikacja.las` (klasy ASPRS: 2 grunt, 3 roślinność niska do 1 m, 4 średnia do 5 m, 5 wysoka lub obiekty),
`nmt.asc` i `nmpt.asc` (siatka 1 m, ESRI ASCII, układ siatki N/E), `warstwice.csv` i `warstwice.dxf` (co 1 m) oraz `produkty.txt` z liczbami punktów w klasach i **objętościami** względem poziomu odniesienia
(średnia NMT): wykop (teren powyżej), nasyp (teren poniżej) i objętość nad minimum terenu. Powierzchnię gruntu wyznacza otwarcie morfologiczne siatki minimów (okno ok. 21 m, siatka dopełniona odbiciem punktowym
na brzegach); budynki nie są rozpoznawane osobno (klasa 5), a gęsta roślinność bez prześwitów może zawyżać NMT.

### Prostokąt obrócony, wiązki lasera

Zaznaczając obszar na mapie dwoma narożnikami możesz **obrócić prostokąt** klawiszami PgUp / PgDn (1°, z Shiftem 10°, z Ctrl 0,1°): narożniki są przeciwległe, a boki biegną wzdłuż i w poprzek wybranego azymutu
(szeregi lecą wzdłuż dłuższego boku). Podczas lotu od drona do ostatnich trafień biegną czerwone wiązki lasera (ok. 20 na sekundę, znikają po locie).

## Fotogrametria lotnicza

„Rozpocznij robotę” → „Fotogrametria lotnicza” (pytanie o prawdziwy błąd). To osobna robota z tym samym nalotem co ALS (obszar z wierzchołków, z mapy albo prostokąt obrócony; AGL, prędkość, bateria z wymianą, profil błędów GNSS/IMU),
ale sensorem jest **kamera** patrząca w dół. Menu: kamera (szerokokątna 84° / standardowa 63° / tele 40°, poziomy kąt widzenia), rozdzielczość zdjęć (320×240 domyślnie, 640×480, 1024×768; im większa, tym dłużej w grze),
zakładka podłużna (70/80/90%) i poprzeczna (40/60/70%). Hud pokazuje liczbę szeregów, szacowaną liczbę zdjęć, **GSD** (dla renderowanej rozdzielczości i odpowiednik matrycy 20 MP, 5472 px) oraz czas lotu.

**Zdjęcia** powstają przez rzucanie promienia przez każdy piksel z prawdziwej pozycji drona (perspektywa, przesłonięcia, kolory z tekstur gry); dron zawisa na czas renderowania w kilku klatkach. Zapis: `zdjecia/IMG_0001.png` ... (PNG bez kompresji,
więc pliki są duże) i `zdjecia.csv` (id, plik, czas, szereg, pozycja E/N/H z GNSS i kąty roll/pitch/yaw z IMU z szumem; przy opcji prawdziwego błędu także kolumny z prawdziwymi wartościami) — da się je obrobić w zewnętrznym programie (np. Metashape, ODM).

**Wyrównanie wiązki** (po locie, w grze): punkty wiążące są symulowane (znane punkty terenu rzutowane na zdjęcia z widocznością sprawdzaną promieniem i szumem 0,5 px, jak po automatycznym dopasowaniu cech), a punkty osnowy w obszarze to
GCP (co drugi, stałe) i punkty kontrolne (pozostałe, wolne). Niewiadome: poza każdego zdjęcia i punkty 3D; obserwacje: piksele, a-priori GNSS/IMU z wagami i GCP; Gauss-Newton / Levenberg-Marquardt z dopełnieniem Schura.
`fotogrametria_raport.txt`: GSD, zakładki, liczby zdjęć, punktów, obserwacji, rms reprojekcji przed i po, poprawka pozycji zdjęć względem GNSS, **błąd na punktach kontrolnych**, boresight kamery
(średnia różnica IMU − wyrównanie), a przy prawdziwym błędzie także prawdziwy błąd pozycji i oczekiwana poprawka; `fotogrametria_wyrownanie.csv` — poprawione pozy zdjęć. Potrzeba co najmniej 2 punktów osnowy w obszarze, żeby były i GCP, i punkty kontrolne.
**Ortofotomapa.** Po wyrównaniu pozycja „Ortofotomapa ze zdjęć” (praca w kilku klatkach, przerywalna) buduje NMPT z punktów wiążących po wyrównaniu (interpolacja ważona odległością), a potem dla każdego piksela
(rozmiar = GSD zdjęć, ograniczony do ok. 6 mln pikseli) wybiera zdjęcie, na którym ten punkt jest najbliżej nadiru, i próbkuje jego kolor interpolacją dwuliniową przez poprawioną pozę; zdjęcia są czytane z dysku po jednym.
Wynik: `ortofotomapa.png` i `ortofotomapa.pgw` (plik pozycjonujący w układzie siatki N/E, otwiera go QGIS i CloudCompare) oraz `ortofotomapa_raport.txt` z pokryciem i **błędem planimetrycznym na znakach osnowy**
(promień przez zmierzony piksel znaku przez poprawioną pozę, przecięty z poziomem znaku; osobno punkty stałe i kontrolne, a przy prawdziwym błędzie także względem prawdziwego położenia). NMPT jest wygładzony (kilkaset punktów),
bez wykrywania przesłonięć (budynki i drzewa mogą być „pochylone”) i bez wyrównania kolorów między zdjęciami. Dane wyrównania są w pamięci, więc po wczytaniu zapisu ortofotomapa jest niedostępna do kolejnego lotu.
Zapis gry w trakcie lotu fotogrametrycznego nie wznawia lotu (zapamiętuje obszar i ustawienia). Ograniczenia: punkty wiążące są symulowane (dokładności z raportu to dolna granica tego, co da prawdziwy SfM), kamera bez dystorsji i bez autokalibracji,
bez NMPT ze zdjęć (ortofotomapa korzysta z NMPT z punktów wiążących).

## Poziomowanie instrumentu

**Domyślnie włączone** (dla niwelatora i tachimetru). Tachimetr domyślnie mierzy też w **dwóch położeniach lunety** (KL+KP) — wyłączysz to w menu roboty „Położenia lunety”.

Każdy niwelator i tachimetr (także w niwelacji trygonometrycznej) staje po postawieniu **krzywo na statywie** (pochylenie 9–16′, babelek poza kółkiem libelli).
W menu roboty jest **„Poziomuj instrument (libella)”**: panel z libellą okrągłą, babelkiem i trzema śrubami ustawczymi (1 u góry, 2 w lewym dolnym, 3 w prawym dolnym rogu).
**1 / 2 / 3** podnoszą śrubę (obrót w prawo) i przesuwają babelek w jej stronę, **4 / 5 / 6** opuszczają, **Shift** = dokładnie (0,1′ zamiast 1′); Enter zamyka.
Niwelator wymaga babelka w kółku (≤ 8′), tachimetr libelli elektronicznej (≤ 0,5′ = 30″; poniżej 1,25′ panel powiększa widok). Bez tego odczyt jest blokowany
(„niwelator niewypoziomowany”). Każde nowe stanowisko trzeba poziomować od nowa. W menu głównym można wyłączyć poziomowanie („Poziomowanie instrumentów”, dla następnych stanowisk).

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
- `src/calc/` (lotnictwo): `Flight.hpp` (plan nalotu), `Georef.hpp` (trajektoria, błędy GNSS/IMU, georeferencja), `AlsLas.hpp` (LAS 1.2 format 3, PTS, kontrole szeregów i osnowy).
- `src/job/` – interfejs `Job`/`IWorld` i automat ciągu niwelacyjnego (bez silnika, testowany sztucznym światem); nowe roboty rejestruje się w `Jobs.hpp`. Prowadzenie w menu: `Workflow.hpp` (sekcje, powody niedostępności, DALEJ), `MenuLayout.hpp` (układ wierszy i przewijanie), `Wizards.hpp` (kreatory), `Help.hpp` (F1).
- `src/calc/Project.hpp`, `ProjectReport.hpp`, `UiSettings.hpp` – model projektu (manifest), raport zbiorczy z porównaniem wysokości i pamięć ustawień; `src/engine/ProjectState.hpp`, `ProjectUi.hpp` – ekran główny i zarządzanie projektami.
- `src/engine/` – część zależna od Gothic API: panele, stawianie, luneta, menu, adapter `EngineWorld`, kontroler, eksport, podgląd i edytor dziennika.
- `src/Plugin.hpp` – hook `zCWorld::Render` (pętla klatki), `userapi/zCWorld.inl` – deklaracja hooka.
- `assets/make_models.py` – modele `.3DS` (łata 3 m z 6 teksturami, niwelator na statywie, żabka, znak reperu) i tekstury `.TGA` (podziałka, krzyż, kreski, panel).
- `tests/` – `powershell -File tests/run.ps1` kompiluje i uruchamia testy `g++` oraz sprawdza wygenerowane DOCX (`check_docx.py`).
- Reszta (`CMakeLists.txt`, `cmake/`, `scripts/`, `vdf/`) pochodzi z szablonu; `scripts/build.ps1` buduje plugin, `scripts/install.ps1` instaluje w grze.

## Ustawienia

Stałe `LV_*` na początku `src/Level.hpp`: wysokość osi lunety, zbliżenie, prędkości obrotu, maksymalna celowa, błąd kolimacji `LV_COLLIM` (w sekundach, np. 20 pokazuje błąd systematyczny).
Jeśli cyfry na łacie są w grze do góry nogami, ustaw `FLIP_V = True` w `assets/make_models.py`; jeśli modele leżą poziomo – `UP = "Y"`.

## Znane ograniczenia

- Zapis gry w trakcie roboty zapisuje też stan roboty i sprzętu (patrz „Zapis gry w trakcie roboty”). Trwający pomiar punktu GNSS (sesja) i połowa pomiaru KL bez KP nie są zapisywane; zapisy zrobione wersją bez tej funkcji nie mają stanu.
- Roboty: ciąg niwelacyjny, pomiar GNSS (RTK), tachimetria, niwelacja trygonometryczna (jeden odcinek naraz, bez wyrównania całego ciągu), skaner laserowy naziemny i skaning lotniczy z drona.
- ALS: wyrównanie szeregów jest sztywne (przesunięcia E/N/H na szereg, bez poprawki dryfu wewnątrz szeregu i bez kalibracji ramienia dźwigni); pitch/yaw boresightu wymagają nachylonego terenu albo szeregów poprzecznych; pierwszy zwrot (bez wielokrotnych odbić); bez omijania przeszkód i ręcznego latania; trajektoria dojazdu po wznowieniu lotu nie jest zapisywana; kamera za dronem jest eksperymentalna (kamera gry może ją nadpisać); fotogrametria lotnicza to osobna, późniejsza faza.
- Tachimetria: bez wcięcia w przód i bez wyrównania osnowy ani wspólnego wyrównania stanowisk.
- Pomiar GNSS: poprawki z własnej bazy to model uproszczony (1 ppm + przesunięcie bazy), bez wielu baz i bez przetwarzania linii bazowych; statyka to pojedyncza sesja 120 epok.
- Kreski dalmiercze liczone są dla pola widzenia gry; przy bardzo wąskim FOV mogą wypaść poza lunetę (wtedy są pomijane, wpis w `GothicSurveying.log`).
- Hook na `zCWorld::Render` sprawdzono tylko na Gothic II NotR 2.6 (fix) ze Steam Gold Classic.

## Licencja

Szkielet CMake pochodzi z [union-api-plugin-template](https://github.com/piotrmacha/union-api-plugin-template) (MIT, Piotr Macha), zob. `LICENSE.md`.
Licencja własnego kodu (`src/Level*.hpp`, `assets/`, `tests/`, `scripts/install.ps1`) nie została jeszcze wybrana.
