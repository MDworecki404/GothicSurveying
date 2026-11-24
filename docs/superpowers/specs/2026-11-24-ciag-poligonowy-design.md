# Pomiar „Ciąg poligonowy" (projekt)

Data: 2026-11-24. Baza: wydanie v0.11.0, gałąź `ciag-poligonowy`. Źródło procedury i wzorów: przewodnik `docs/przewodnik-pomiary-geodezyjne.pdf`, sekcja 5 i przykład liczbowy (sprawdzony niezależnie).

## Po co

Ósmy rodzaj pomiaru, odtwarzający **prawdziwą procedurę**: tachimetr na kolejnych punktach ciągu, kąt poziomy lewy w dwóch położeniach lunety, długość każdego boku w obu kierunkach, potem rachunek (azymuty, odchyłki, poprawki, współrzędne) z oceną względem dopuszczalnych. Gracz ma zobaczyć, skąd bierze się odchyłka i że poprawna procedura ją wykrywa.

## Cele

- Trzy rodzaje ciągu, mierzone tak samo, liczone różnie: **nawiązany obustronnie** (N1, A … B, N2), **zamknięty** (poligon), **wiszący** (jednostronnie nawiązany, bez kontroli).
- Pomiar w grze w widoku lunety (`TachyView`): na stanowisku kąt między tyłem a przodem w KL i KP (średnia), odległości skośne do obu luster w KL i KP; przerzut lunety klawiszem K jak w tachimetrii.
- Rachunek w czystym `calc/Polygon.hpp`: azymuty nawiązania, f_β, poprawki kątowe, azymuty boków, przyrosty, f_x/f_y/f_d, poprawki liniowe, współrzędne; wszystko jak w przewodniku.
- Raport CSV + DOCX (dziennik kątów, boków, rachunek, współrzędne, ocena); podsumowanie w panelu po zakończeniu; zapis stanu razem z zapisem gry; rejestracja w projekcie jak inne pomiary.

## Poza zakresem i antycele

- Wysokości punktów (poligon jest sytuacyjny: tylko N/E), redukcja do układu odwzorowania (PL-2000), poprawka atmosferyczna, wyrównanie ścisłe (metoda najmniejszych kwadratów), ciągi z punktami węzłowymi, ciąg wiszący z oceną dokładności.
- Antycel: wartości dopuszczalne podane jako liczby z instrukcji. Przewodnik ich nie podaje, więc robota **nie udaje przepisów**: limity wynikają z dokładności symulowanego instrumentu i raport nazywa je „założenie symulatora".
- Antycel: psucie zgodności (zapisy stanu `ver=1`, id pozycji menu innych robót, testy `test_*_job.cpp`).

## Podejścia

- **A. Nowa robota `PolygonJob` (wybrane):** własny automat i raport, wspólny widok lunety i szum, rachunek w `calc/`. 
- B. Rozbudowa `TachyStationJob`: odrzucone (inny przebieg i raport, plik już duży). 
- C. Rachunek bez pomiaru w grze: odrzucone (nie odtwarza procedury).

## Decyzje użytkownika

- Pomiar ma odtwarzać **prawdziwą procedurę** (stanowiska, kąty w dwóch położeniach lunety, boki w obu kierunkach).
- Odchyłki dopuszczalne **z dokładności instrumentu** (opcja 1), a nie z instrukcji.

## Procedura w grze (moje decyzje)

- **Punkty:** nawiązania (N1, A, B, N2 zależnie od rodzaju) to punkty osnowy z listy (jak „stanowisko" w tachimetrii; wzorcowe `G..` albo założone GNSS). Punkty pośrednie gracz zakłada sam („Postaw punkt ciągu pod stopami": znak jak punkt obrotowy w niwelacji). Wiszący: N1, A i punkty nowe. Zamknięty: co najmniej jeden punkt osnowy (A) i drugi do orientacji (N1); kąt przy A jest liczony z N1 jako tyłu tylko jeśli ciąg nie jest zamknięty, inaczej tył to ostatni punkt poligonu.
- **Stanowisko k:** „Postaw tachimetr na punkcie k" i „Postaw lustro na punkcie (tył | przód)" – tachimetr i lustro stają dokładnie nad znakami niezależnie od pozycji bohatera (`PlaceTachymeterOnPoint`, `PlacePrismOnPoint`, a przy punktach osnowy `PlaceTachymeterAt`/`PlaceBacksightAt`), jak w `TrigLevel`. Jedno lustro przestawiane między tyłem a przodem.
- **Obserwacje na stanowisku:** Enter w lunecie na lustrze tyłem i przodem w KL, przerzut (K), to samo w KP. Z tego: kąt lewy β = K_przód − K_tył (mod 400g), średnia z obu położeń (usuwa kolimację), odległości skośne z KL i KP uśrednione, redukcja do poziomu d = D·sinV. Każdy bok wypada zmierzony z obu końców (z dwóch stanowisk).
- **Automat:** stanowisko jest „gotowe", gdy ma komplet (tył i przód w KL i KP). Menu: wybór nawiązań, punkt ciągu, tachimetr, lustro, „Patrz przez lunetę", „Powtórz stanowisko", „Podgląd rachunku (odchyłki)" (po komplecie wszystkich stanowisk wymaganych dla rodzaju ciągu), „Zakończ i oblicz". Podgląd pozwala wykryć zły kąt i powtórzyć stanowisko przed zakończeniem.
- **Prawdziwy błąd** (opcja jak w innych robotach): współrzędne policzone vs prawdziwe pozycje znaków w grze.

## Rachunek (`calc/Polygon.hpp`)

Wzory dokładnie jak w przewodniku (kąty lewe, grady, współrzędne N=X, E=Y):
- azymut z punktów (gon, 0–400); `A_{i,i+1} = A_{i−1,i} + β_i − 200g (mod 400g)`;
- nawiązany: `f_β = A_p + Σβ − n·200g − A_k` (sprowadzone do (−200g, +200g]), `v_β = −f_β/n`; przyrosty z azymutów poprawionych; `f_x = X_A + ΣΔX − X_B`, `f_y` analogicznie, `f_d = √(f_x²+f_y²)`, `v_x,i = −f_x·d_i/Σd`;
- zamknięty: kąty lewe wewnętrzne przy obchodzie przeciwnym do wskazówek zegara (Σβ_teor = (n−2)·200g), przy zgodnym zewnętrzne ((n+2)·200g) – kierunek obchodu wykrywany ze znaku pola poligonu; `f_x = ΣΔX`, `f_y = ΣΔY`;
- wiszący: azymuty i współrzędne bez poprawek, bez odchyłek (raport to zaznacza).
- **Odchyłki dopuszczalne z instrumentu (założenie symulatora):** `m_β = σ_kier` (5″ z `TachyNoise`; różnica dwóch kierunków i średnia z dwóch położeń dają σ_β równe σ kierunku), `f_β,dop = 2·m_β·√n`; `m_d,i = (2 mm + 2 ppm·d_i)/√2` (średnia z dwóch pomiarów), `f_d,dop = 2·√(Σ m_d,i² + m_β²[rad]·Σ r_i²)`, gdzie `r_i` to odległość stanowiska i od punktu końcowego (nawiązany) albo od środka ciężkości poligonu (zamknięty). Przykład z przewodnika ma mieścić się w limitach (f_β = 0,0020g, f_d = 0,017 m).

## Raport

`pomiar_poligonowy.csv` i `.docx` (przerwany: `_przerwany`), w folderze roboty; dziennik kątów (stanowisko, tył, przód, kierunki KL/KP, β średnie, kolimacja 2c), dziennik boków (z obu końców, średnia, d), rachunek (f_β, v_β, azymuty, ΔX/ΔY, f_x/f_y/f_d, limity z oznaczeniem „założenie symulatora"), współrzędne punktów, ocena „w granicach / przekroczone", opcjonalnie prawdziwy błąd. Każda obserwacja od razu do `odczyty_biezace.csv`.

## Interfejs i integracja

- Nowy `JobKind::Polygon`, `JobType{"polygon", "Ciag poligonowy", ..., flaga}`; start jak w niwelacji: wybór rodzaju (nawiązany / zamknięty / wiszący), potem pytanie o prawdziwy błąd.
- Kontroler: zakończenie (`CtrlFinishPolygon`: panel, eksport, rejestracja w projekcie), przerwanie i zmiana świata (zapis częściowy), klawisz F10 i widok lunety jak dla tachimetrii. Nowe id pozycji w tabeli sekcji (`Workflow.hpp`), pomoc F1 (`Help.hpp`), `NextStep()` prowadzące: nawiązania → punkty → tachimetr → lustro → pomiar → następne stanowisko → podgląd → zakończ.
- Zapis stanu (`CanSaveState`, `SaveState/LoadState`): punkty, obserwacje, bieżące stanowisko i faza (jak `TrigLevel`).
- Kreator „Ciąg poligonowy" i punkty ciągu jako osnowa (`StoreControlPoint`) – poza tym zakresem; do dodania osobno.

## Etapy i testy

1. `calc/Polygon.hpp` + `tests/test_polygon.cpp`: przykład z przewodnika (A_p, A_k, f_β, v_β, azymuty, f_x/f_y/f_d, współrzędne końcowe = B), poligon zamknięty (kwadrat, obchód w obu kierunkach), wiszący, kąt lewy z kierunków (KL/KP, kolimacja), limity z instrumentu (przykład mieści się, sztucznie zepsuty kąt nie).
2. `calc/PolygonReport.hpp` + `tests/test_polygon_report.cpp`: CSV, DOCX (ZIP, treść), linie podsumowania.
3. `job/PolygonJob.hpp` + `tests/test_polygon_job.cpp` (sztuczny świat jak w `test_tachy_job.cpp`): pełne pomiary trzech rodzajów bez szumu dają współrzędne zgodne z prawdziwymi, powtórzenie stanowiska, zepsuty kąt wykrywany w podglądzie, zapis i odczyt stanu, `NextStep`.
4. Integracja z silnikiem: rejestr, kontroler, `Workflow`/`Help`, README i INSTALACJA; build i `ALL OK`; weryfikacja w grze po stronie użytkownika.

Każdy etap: testy `ALL OK`, `scripts/build.ps1` → `BUILD OK` (etap 4), osobny commit, bez Co-Authored-By, bez push/merge/wydania bez polecenia.

## Co zostaje wykonawcy

- Dokładne wywołania `IWorld` do stawiania lustra na punktach osnowy i nowych (nowe metody z domyślnym ciałem dozwolone) oraz prawdziwe pozycje znaków do celowania i błędu.
- Układ panelu lunety i komunikaty; format tabel DOCX.
- Wykrywanie kierunku obchodu poligonu i obsługa przypadków brzegowych (n < 3, brak nawiązania, punkt poza zasięgiem 3–300 m, zasłonięta linia celowa).
