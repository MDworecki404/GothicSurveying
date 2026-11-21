# Robota „Tachimetria (pomiar biegunowy)” – projekt

Data: 2026-11-21. Status: do akceptacji. Cel i zakres to **moje założenia** (użytkownik powiedział „działaj” bez doprecyzowania): analogicznie do GNSS.

## Po co to jest

Gracz ma **nauczyć się pracy z tachimetrem**: ustawić stanowisko, zorientować instrument na punkt nawiązania, mierzyć kąty
i odległości do lustra na tyczce i dostać współrzędne punktów metodą biegunową. Instrument nie zna „prawdziwych” współrzędnych:
liczy je z odczytanych kątów (z szumem i błędem celowania gracza) i odległości. Dziennik zapisuje cały pomiar (kąty, odległości,
wyniki), do porównania z prawdziwym położeniem punktów (opcjonalnie).

## Cele

- Nowa robota w rejestrze `JobTypes()`: „Tachimetria (pomiar biegunowy)”, obok niwelacji i GNSS.
- Widok przez lunetę tachimetru: obrót w poziomie **i pochylanie w pionie**, krzyż, zbliżenie, podgląd odczytów Hz i V na żywo.
- Orientacja na punkt nawiązania (zerowanie koła poziomego) i pomiar punktów z liczeniem N, E, H.
- Dziennik tachimetryczny (tabela w grze, CSV, DOCX) w folderze roboty, opcja prawdziwego błędu jak w GNSS.

## Poza zakresem (v1)

- Wcinanie wstecz i stanowisko dowolne (stanowisko ma znane współrzędne), wytyczanie, pomiar w dwóch położeniach lunety,
  błędy kolimacji i inklinacji, poprawki atmosferyczne, kilka stanowisk w jednej robocie.
- Zmiana wysokości instrumentu (stała 1,500 m = `LV_INSTR_H`).
- Uogólnianie dziennika (osobny `TachyReport`, tak jak `GnssReport`).

## Anty-cele

- Wynik „z gry” zamiast z odczytów (współrzędne mają wychodzić z kątów i odległości, więc błąd celowania gracza jest widoczny).
- Zmiany psujące niwelację, GNSS, lunetę niwelacyjną, wpis odczytu, eksport.
- Teksty w grze z polskimi znakami; w CSV/DOCX polskie znaki są dozwolone.

## Ograniczenia

- Gothic API: pętla klatki tylko hook `Render`; panele przez `PanelOpen`; klawisze ze stałych w `src/Keys.hpp`; menu pod Home.
- TDD dla `calc` i `job`; część silnikowa testowana w grze przez użytkownika.
- Układ lokalny N/E/H jak w GNSS (`GnssToGrid`, stałe N0/E0/H0); azymut = od północy (+Z) zgodnie z ruchem wskazówek zegara (+X = wschód).

## Podejścia

**Widok lunety.**
1. *Rozszerzyć `Scope.hpp`* o pochylenie i tryb pomiaru: jeden widok dla wszystkiego, ale `Scope.hpp` jest ściśle związany z niwelacją
   (wpis odczytu, wybór łaty), więc to ryzyko regresji niwelacji.
2. *Nowy `TachyView.hpp`* (wybrane): osobny widok z pochyleniem, współdzielący tylko funkcje pomocnicze (`ScopeApplyZoom`, tekstura
   krzyża). Niwelacja nietknięta, koszt to trochę powtórzonego kodu (obrót, kamera).
3. *Pomiar bez widoku* (wpisywanie kątów w menu): nie uczy celowania, odrzucone.

**Dziennik:** osobny `TachyReport` (jak `GnssReport`), wspólne tylko `WrapDocx`, `fbdetail`, helpery eksportu.

## Projekt

### Przebieg roboty

Start: Rozpocznij robotę → „Tachimetria (pomiar biegunowy)” → pytanie o prawdziwy błąd (jak GNSS).

Menu robota (Home):
- **Postaw tachimetr (stanowisko)** – pod stopami; współrzędne stanowiska = prawdziwe położenie gruntu pod stopami (znany reper), H stanowiska
  = H gruntu. Przestawienie kasuje orientację (dozwolone, gdy nie zmierzono jeszcze punktu).
- **Postaw punkt nawiazania** – marker repera (model reperu) pod stopami. Azymut stanowisko→nawiązanie jest znany (liczony z prawdziwych
  współrzędnych, zaokrąglony do 1 mm).
- **Orientacja na nawiazanie** – wchodzi do lunety w trybie orientacji (aktywne po obu postawieniach).
- **Postaw tyczke z lustrem** – tyczka 1,500 m z lustrem (nowy model `LVL_PRISM.3DS`), pod stopami; poprzednia znika.
- **Wysokosc lustra: X m (zmien)** – presety 1,300 / 1,500 / 1,800 / 2,000 m (prawdziwa 1,500; zła wartość daje błąd systematyczny w H).
- **Patrz przez lunete (F10)** – pomiar punktów (aktywne po orientacji i postawieniu tyczki).
- **Zakoncz robote** (≥ 1 punkt), **Przerwij robote** (jak w pozostałych robotach).

### Widok lunety (`engine/TachyView.hpp`)

Kamera w osi lunety (stanowisko + 1,500 m), zbliżenie 20×, krzyż jak w niwelacji, brak kresek dalmierczych. Sterowanie: ← → obrót w poziomie,
↑ ↓ pochylenie (Shift = powoli, jak w lunecie niwelacyjnej), Enter = wykonaj (orientacja albo pomiar), F10 = wyjście. Panel w lewym górnym
rogu w złotej ramce: tryb (orientacja / pomiar), Hz i V na żywo w gradach (gon, 4 miejsca po przecinku), po orientacji liczba punktów,
ostatni wynik. Komunikaty: „Nie celujesz w lustro”, „Linia celowa zaslonieta”, „Najpierw orientacja”.

### Obliczenia (`src/calc/Tachy.hpp`, czyste, testowane)

- **Orientacja:** gracz celuje w nawiązanie i naciska Enter w trybie orientacji. Warunek: linia celowa przechodzi w odległości ≤ 8 cm od
  środka znaku (poziomo i pionowo, na odległości do nawiązania). Zapamiętany azymut kamery `az0` (z szumem kątowym 5").
- **Pomiar punktu:** warunek celowania jak wyżej względem środka lustra (`wysokość lustra nad gruntem`); linia celowa niezasłonięta (promień).
  Odczyty: `Hz = norm(azKamery − az0 + szum)`, `V = 90° − pochylenie + szum` (kąt zenitalny), `D = odległość skośna z prawdziwej geometrii + szum`.
- **Szum:** kąty σ = 5" (≈ 1,5 mgon), odległość σ = 2 mm + 2 ppm, deterministyczny RNG (`GnssRng` z `Gnss.hpp`).
- **Wynik:** `Az = AzNawiazania + Hz`, `dPoz = D·sin V`, `N = Ns + dPoz·cos Az`, `E = Es + dPoz·sin Az`,
  `H = Hs + ih + D·cos V − wysokość_lustra_ustawiona`, `ih` = 1,500 m. Wynik zaokrąglony do mm.
- **Jednostki:** kąty w gradach (gon), odległości i współrzędne w metrach (jak GNSS), azymut od północy zgodnie z ruchem wskazówek zegara.

### Dziennik

Wiersz punktu: nr, nazwa (T1, T2, …), czas, Hz [g], V [g], D skośna [m], d pozioma [m], Δh [m], N, E, H [m], wysokość lustra [m],
uwagi; przy prawdziwym błędzie dodatkowo błąd N, E, H, 2D [m]. Nagłówek: stanowisko (N, E, H), wysokość instrumentu, azymut nawiązania.
Podsumowanie: liczba punktów, średnia odległość, najdłuższa celowa, przy prawdziwym błędzie najgorszy punkt.
Pliki w folderze roboty: `pomiar_tachimetryczny.csv`, `.docx`; przerwana: `pomiar_tachimetryczny_przerwany.csv`; log punktów na bieżąco
w `odczyty_biezace.csv`.

### Architektura

- `calc/Tachy.hpp`: konwersje gon/stopnie, azymut z dwóch punktów, orientacja, odczyty Hz/V/D z geometrii i szumu, liczenie biegunowe, warunek celowania.
- `calc/TachyReport.hpp`: `TachyPoint`, `TachyReport`, CSV, DOCX (`WrapDocx`), podsumowanie.
- `job/TachyStation.hpp`: automat kroków i menu; `IWorld` dostaje metody z domyślnymi ciałami (stanowisko, nawiązanie, lustro, geometria
  celowania, widoczność). `Job` dostaje `JobKind::Tachy` i `FinishTachy`.
- `engine/TachyView.hpp`: widok lunety z pochyleniem; `World.hpp`: tyczka z lustrem, marker nawiązania, prawdziwe współrzędne; `Controller.hpp`: przepływ
  startu, zakończenia i przerwania; `make_models.py`: `LVL_PRISM.3DS`.
- Reuse: menu, ramka, `FeetGround`, `SpawnVob`, foldery wyników, `MakeJobDir`, `TextPanelShow`, `GnssRng`, `GnssToGrid`, ubiór bohatera.

### Testy

- `calc`: konwersje gon↔stopnie (100 g = 90°), azymut dla kierunków świata, wynik biegunowy dla znanej geometrii (stanowisko, nawiązanie na północ,
  punkt na wschodzie Hz = 100 g, V = 100 g, D = 50 m ⇒ E = Es + 50, N = Ns, H = Hs + ih − wysokość lustra), błąd H przy złej wysokości lustra,
  statystyka szumu (σ kątów ≈ 5", σ odległości ≈ 2 mm + 2 ppm), warunek celowania (8 cm), orientacja zeruje Hz na nawiązaniu.
- `job`: kolejność kroków i dostępność menu z fałszywym światem, odmowa pomiaru bez orientacji, błędy celowania, przerwanie z częściowym raportem.
- Silnik: sprawdza użytkownik w grze (widok z pochyleniem, celowanie, zapis plików).

## Zmiany po testach w grze (2026-11-21)

- Własny model tachimetru na statywie (`LVL_TACHY.3DS`) zamiast modelu niwelatora; oś lunety nadal 1,500 m nad gruntem.
- **Lustro na nawiązaniu**: znak reperu ma tyczkę z lustrem; orientacja polega na celowaniu w środek tego lustra (nie w znak na gruncie).
- **Wysokość lustra jest fizyczna**: tyczki (nawiązanie i pomiarowa) mają modele `LVL_PRISM13/15/18/20.3DS` o tej wysokości; instrument
  odejmuje tę samą wysokość, więc nie ma sztucznego błędu w H z niezgodności ustawienia i modelu (błąd wynika z celowania i szumu).
- **Pomiar bezlustrowy** (menu „Rodzaj pomiaru”): odległość z promienia do pierwszego trafionego punktu (1,5–300 m, szum odległości 3 mm + 2 ppm),
  bez wysokości lustra, punkt z uwagą „bezlustrowy”; prawdziwy błąd liczony względem trafionego punktu.

## Zostawione budowniczemu

Rozmieszczenie paneli, wygląd modelu lustra, dokładne progi celowania (strojone w grze), nazwy pól CSV poza wymienionymi, sposób współdzielenia
funkcji z `Scope.hpp` (kopiowanie vs wyciągnięcie do wspólnego pliku).

## Ryzyka

- Kamera pochylana z `SetHeadingAtWorld` (działa w `PoleFine.hpp`), ale w lunecie z zoomem 20× precyzja wymaga strojenia kroku pochylenia.
- Zasłonięcie linii celowej przez voby bez kolizji (drzewa) będzie niedoszacowane, jak w GNSS.
- Wiele równoległych torów dziennika (niwelacja, GNSS, tachimetria): rozważyć wspólny `Report` przy czwartej robocie.
