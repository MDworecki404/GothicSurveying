# Menu prowadzące: projekty, pomiary, kreatory (projekt)

Data: 2026-11-24. Baza: wydanie v0.10.0, gałąź `menu-projekty`.

## Po co

Menu (Home) jest dziś płaską listą. Roboty mają po 20+ pozycji (ALS, fotogrametria), użytkownik nie wie, co zrobić teraz, co jest następne, dlaczego coś jest wyszarzone i gdzie są wyniki. Cel: menu, które **prowadzi**: pokazuje, gdzie jestem, co zrobiłem, jaki jest rekomendowany następny krok i dlaczego reszta jest niedostępna. Do tego organizacja pracy w **projekty** z wieloma pomiarami; każda robota zachowuje własny raport, a zbiorczy raport powstaje dopiero po zamknięciu projektu.

## Cele

- Ekran główny z projektem, jedną wyróżnioną rekomendacją „DALEJ", listą pomiarów ze stanem i skrótami do wyników i osnowy.
- Menu każdego pomiaru w sekcjach, w tej samej kolejności między robotami, „Przerwij" zawsze w tym samym miejscu, powód niedostępności jednym zdaniem.
- Wiele pomiarów w projekcie, także tego samego rodzaju (np. 3 niwelacje, 2 tachimetrie), wykonywanych kolejno: po zakończeniu jednego uruchamia się następny.
- 6 kreatorów jako gotowe zestawy ustawień plus tryb prowadzony.
- Lista plików wynikowych z otwarciem folderu w trakcie pracy; zbiorczy raport (CSV + DOCX) z porównaniem wysokości na wspólnych punktach osnowy dopiero przy zamknięciu projektu (decyzja użytkownika: w trakcie wystarczą raporty robót).
- Wygoda: spójne klawisze w `Keys.hpp`, pamięć ustawień i ostatniego projektu, pomoc kontekstowa, pasek skrótów, czytelny stan.

## Poza zakresem i antycele

- **Poza zakresem:** nowa geodezja, zmiana obliczeń w robotach, równoczesne wiele robót (silnik trzyma jedną `gJob`), ponowne użycie wyrównanych chmur ALS między nalotami (pamięć 32-bit, zmiany w robotach lotniczych), tekstury TGA w pierwszej iteracji (znaczniki w ASCII).
- **Antycel:** psucie zgodności. Zapisy stanu (`stan_slot<N>.txt`), pliki wyników, nazwy folderów wyników i skróty klawiszy mają działać dalej. Nie wolno przepisać `Job::Menu()` ani zmienić id pozycji bez świadomej poprawki testów.
- **Antycel:** modalny kreator „Krok 3 z 7" blokujący świat. Gracz ma się swobodnie poruszać, a menu tylko prowadzi.

## Ograniczenia

- Kod silnika sprawdza się tylko buildem i grą. Logika (projekt, kroki, rekomendacje, raport) w `calc/` i `job/` z testami g++ (`tests/run.ps1` → `ALL OK`), a `engine/` ma tylko rysować i przekazywać klawisze.
- Teksty w grze bez polskich znaków (ASCII). Ścieżki względem katalogu gry.
- Wtyczka jest 32-bitowa; nie twórz setek `zCView` na klatkę, odświeżaj tylko po zmianie.
- Gra jest zatrzymana, gdy menu otwarte; robota liczy czas przez `Tick()` tylko przy działającej grze. Zachować.
- Pułapki: makra `near/min/max` (`(std::min)(...)`), SEH bez obiektów z destruktorami, pliki łatające pisać przez Write/Edit, nie heredoc.
- Niepewne wywołania silnika (rysowanie, klawisze) z diagnostyką w `GothicSurveying.log` (`SvTrace`).

## Podejścia (rozważone)

- **A. Nakładka na istniejące roboty (wybrane):** `MenuItem` dostaje opcjonalne `section` i `why`, a tabela per rodzaj roboty w `job/Workflow.hpp` mapuje id → sekcja, kolejność, powód i „co dalej". `Job::Menu()/OnMenu()` bez zmian. Zaleta: małe ryzyko, testy robót zielone. Koszt: tabela do utrzymania obok `Menu()`.
- **B. Deklaratywne kroki w robotach:** jedno źródło prawdy, ale dotyka 7 robót i ich testów. Odrzucone (ryzyko regresji).
- **C. Tylko silnik, grupowanie po prefiksach:** nie prowadzi (brak wiedzy o stanie). Odrzucone.

Kreatory: wybrane „preset + tryb prowadzony". Odrzucone: liniowy kreator modalny (blokuje świat, kosztowne rysowanie) i brak kreatorów.

## Projekt niezależny od zapisu gry (decyzja użytkownika)

Projekt to folder `GothicSurveying/projekty/<nazwa>/` z manifestem, wspólny dla wszystkich zapisów tego samego świata. Trwającą robotę nadal przywraca `stan_slot<N>.txt`. Rozjazd manifestu z zapisem (np. starszy zapis) rozwiązuje wiązanie pomiaru w toku z projektem po nazwie projektu i id pomiaru zapisanych w stanie (opcjonalne klucze, brak = „pomiary luzem").

## Nazwy

- **Projekt:** praca na obszarze jednego świata.
- **Pomiar:** dzisiejsza robota, np. „Tachimetria 2" (rodzaj + numer lub nazwa).
- **Kroki/sekcje** menu pomiaru: Przygotowanie, Parametry, Pomiar, Wyniki, Narzędzia, Zaawansowane. Przerwij zawsze ostatnie.

## Model danych (`calc/Project.hpp`)

Format: `StateKV` (klucz=wartość), bez nowego parsera. Plik `projekty/<nazwa>/projekt.txt`:

- `ver`, `name`, `world` (`ControlWorldKey`), `created`, ustawienia domyślne (parametry presetów, poziomowanie instrumentów, itp.).
- Lista pomiarów `m.<i>.*`: `id`, `type` (id z `JobTypes()`), `name`, `state` (nowy / w toku / gotowy / przerwany; projekt ma osobno `state` open/closed), `dir` (folder wyników, dotychczasowa nazwa `GothicSurveying/<data>`), `params` (zestaw parametrów), `sum` (1–2 liczby podsumowania), `points` (kompaktowa tabela id punktu → H dla porównania, plik `pomiar_<id>.csv` w folderze projektu).
- Pomiary sprzed projektów: domyślny projekt „Pomiary luzem" bez migracji plików, tworzony przy pierwszym uruchomieniu.
- Czyste API: tworzenie/wczytanie/zapis, dodanie pomiaru, zmiana stanu, lista plików wynikowych, walidacja (świat, unikalność nazw).
- `GothicSurveying/ustawienia.txt`: ostatni projekt, ostatnia pozycja kursora per ekran, ostatnie preset-y (pamięć ustawień).

## Prowadzenie (`job/Workflow.hpp`)

- `Section(kind, id)` i `Why(kind, id, jobState)`: tabela per `JobKind`, uzupełnia `MenuItem` (sekcja, kolejność, krótka etykieta, powód niedostępności). Powód zastępuje dzisiejszy `hint` przy `enabled=false`.
- `Recommend(project, aktywnyPomiar)`: czysta funkcja zwracająca „DALEJ" (id pozycji + tekst) wg stanu. Bez aktywnego pomiaru: kontynuuj pomiar „w toku", w przeciwnym razie „nowy pomiar" (lub kreator). Przy aktywnym: pierwsza dostępna pozycja z sekcji w kolejności Przygotowanie → Parametry → Pomiar → Wyniki.
- Tryb prowadzony: po starcie z kreatora menu pomiaru pokazuje linię DALEJ i pozycję „Pokaz wszystko" rozwijającą sekcje. Do trybu zaawansowanego jedno kliknięcie.
- Kreatory (6): Osnowa GNSS, Niwelacja ciągu A→B, Inwentaryzacja tachimetrem, Skan budynku, Nalot ALS, Zdjęcia z drona. Każdy to preset (rodzaj + `JobOptions` + lista `OnMenu(id)` ustawiających parametry) plus pytania: nazwa i opcjonalnie preset parametrów.

## Interfejs w silniku

- **Ekran główny (układ A):** okruszki „GEODEZJA > Projekt", linia świata/osnowy/liczby pomiarów, wyróżnione „DALEJ", lista pomiarów (`[x]` gotowy, `[~]` w toku, `[ ]` nowy, `+ Nowy pomiar...`), „Wyniki i raporty" (pliki robót), „Mapa osnowy / Teleport", „Ustawienia projektu", „Zamknij projekt" (po zakończeniu pomiarów), pasek skrótów u dołu.
- **Menu pomiaru:** nagłówek `Pomiar  Projekt > Pomiar > Sekcja`, DALEJ, sekcje rozwijane, pozycje niedostępne z powodem w podpisie, „Przerwij pomiar" zawsze ostatnie.
- **Renderowanie (moja decyzja):** `MenuView` dostaje wiersze nagłówkowe (niewybieralne, kursor je pomija), okno przewijania (~16 wierszy z „..." gdy jest więcej, naprawia dzisiejszy brak przewijania przy ALS), zawijanie podpisu, skalowanie z `screen->nax/nay` jak w `MapView.hpp`. Znaczniki w ASCII, TGA odłożone.
- Zmiana w `Menu.hpp`/`Controller.hpp` tylko cienka: `CtrlOpenMenu` buduje `MenuView` z `Job::Menu()` plus `Workflow`.

## Klawisze (moja decyzja)

W jednym miejscu `Keys.hpp`: istniejące Home (menu), strzałki, Enter; dodane: Lewo = wstecz/poprzedni ekran, cyfry 1–9 = skrót pozycji na ekranie głównym, **F1 = pomoc kontekstowa** (do potwierdzenia w grze, F11/F12/F5/F6 zajęte; `SvTrace` przy braku reakcji), Home = zamknij. „Przerwij" nie ma skrótu klawiszowego (potwierdzenie przed utratą danych, domyślnie „Nie", jak dziś).

## Wygoda

- Pamięć: ostatni projekt, ostatnia pozycja kursora, ostatnie ustawienia (w `ustawienia.txt` i w ustawieniach projektu). Szybkie wznowienie ostatniego projektu: Home otwiera ekran główny ostatniego projektu.
- Potwierdzenia tylko tam, gdzie coś ginie (przerwanie, usunięcie osnowy, nadpisanie projektu).
- Stan czytelny w jednej linii: „trwa lot 45%", „czeka na wypoziomowanie" (z `Job::Hud()/StatusLines()`).

## Zamknięcie projektu: raport zbiorczy i porównanie (ostatni etap)

- Projekt ma stan `open` / `closed` (klucz `state` w manifeście). „Zamknij projekt" wymaga braku pomiaru w toku, pyta o potwierdzenie i generuje raport; zamknięty projekt jest tylko do odczytu (bez nowych pomiarów), można go ponownie otworzyć (raport powstaje od nowa przy kolejnym zamknięciu).
- W trakcie pracy nie ma raportu zbiorczego: użytkownik korzysta z raportów poszczególnych robót, które istnieją od dawna.

- Raport projektu (tylko przy zamknięciu): CSV (lista pomiarów, stany, parametry, liczby podsumowania) i DOCX (przez `calc/Docx.hpp`): opis projektu, tabela pomiarów, wyniki kluczowe.
- Porównanie wysokości: dla każdego punktu osnowy, który wystąpił w ≥2 pomiarach (niwelacja/GNSS/tachimetria/ALS), tabela H i różnic. Źródłem są `pomiar_<id>.csv` zapisane przy zamknięciu pomiaru.
- Lista plików wynikowych projektu z otwarciem folderu (cienka funkcja silnika, `ShellExecute`).

## Etapy i testy

1. **Warstwa prowadzenia w job/:** `MenuItem` + pola `section`, `why`; `job/Workflow.hpp` (tabele 7 robót, `Recommend`); testy `test_workflow.cpp`, testy robót bez zmian (zielone).
2. **Model projektu:** `calc/Project.hpp` + persystencja i testy (`test_project.cpp`): tworzenie, zapis/odczyt, dodawanie pomiarów, zgodność z zapisem stanu.
3. **Interfejs menu (silnik):** `MenuView` z sekcjami, przewijaniem, DALEJ, okruszkami; ekran główny A; wybór/tworzenie projektu; rejestracja pomiarów przy starcie, zakończeniu i przerwaniu. Weryfikacja: build + gra.
4. **Tryb prowadzony i kreatory:** presety 6 scenariuszy, „Pokaz wszystko", testy presetów w `Workflow`.
5. **Wygoda:** `Keys.hpp`, pomoc (F1), pasek skrótów, pamięć ustawień (`ustawienia.txt`), wznowienie ostatniego projektu.
6. **Wyniki i zamknięcie projektu:** lista plików z otwarciem folderu, stan otwarty/zamknięty, raport CSV + DOCX z porównaniem wysokości generowany przy zamknięciu, testy.
7. **Dokumentacja:** README (Sterowanie/Projekty), INSTALACJA w `scripts/package.py`. Wydanie osobno, na polecenie.

Każdy etap: testy `ALL OK`, `scripts/build.ps1` → `BUILD OK`, osobny commit na gałęzi `menu-projekty` (bez linii Co-Authored-By; bez pushu, wydania i merge'a bez polecenia).

## Co zostaje wykonawcy

- Dokładne tabele kolejności i sekcji dla każdej z 7 robót (trzeba przejść `Menu()` każdej).
- Szczegóły okna przewijania, szerokości linii i skalowania w silniku, w ostrożnych krokach z `SvTrace`.
- Wybór klawisza pomocy po sprawdzeniu w grze (F1 jest propozycją).
- Format `pomiar_<id>.csv` i schemat DOCX raportu.
- Obsługa wiązania pomiaru w toku z projektem po wczytaniu starszego zapisu (przypadki brzegowe).
