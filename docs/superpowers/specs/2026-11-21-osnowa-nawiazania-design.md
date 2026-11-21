# Osnowa GNSS i wiele nawiązań w tachimetrii – projekt

Data: 2026-11-21. Status: do akceptacji. Rozszerza: `2026-11-21-gnss-rover-design.md` i `2026-11-21-tachimetria-design.md`.
Decyzje użytkownika: ekran „Pokazywać prawdziwy błąd?” przy starcie zostaje; priorytet to **wiele nawiązań do orientacji**; celowanie ma
być dokładniejsze (kąt ±0,02°); osnowę zakłada się **GNSS-em** i służy ona jako nawiązanie. Reszta (marki, szczegóły przepływu) to moje propozycje.

## Po co to jest

Jak w terenie: najpierw zakładasz osnowę (kilka punktów zmierzonych GNSS-em w fixie), potem na niej ustawiasz tachimetr i orientujesz go na kilka
punktów osnowy. Współrzędne osnowy mają błąd GNSS (centymetry), więc błąd osnowy przechodzi na pomiary tachimetryczne, a odchyłki nawiązań
pokazują, czy orientacja jest wiarygodna.

## Cele

- **GNSS:** „Załóż punkt osnowy” (fix i co najmniej 20 epok): zapis do trwałego pliku osnowy i znak (reper) w świecie, który zostaje po zakończeniu roboty.
- **Osnowa trwała:** `GothicSurveying/osnowa.csv` (współrzędne zmierzone + położenie w grze + świat); znaki są ponownie stawiane po wczytaniu świata.
- **Tachimetria:** wybór źródła współrzędnych (osnowa GNSS albo prawdziwe jak dotąd), stanowisko na punkcie osnowy, do 5 nawiązań (lustro na każdym),
  orientacja ze średniej z kierunków, odchyłki nawiązań (kąt i odległość) w panelu i w dzienniku.
- **Celowanie:** tolerancja kątowa ±0,02° (z dolnym limitem 1,5 cm bocznie), zamiast ±8 cm.

## Poza zakresem

- Wcięcie wstecz (stanowisko dowolne) i wcięcie w przód; wyrównanie osnowy; wiele stanowisk w jednej robocie; osnowa wysokościowa niwelacją.

## Anty-cele

- Zmiany psujące niwelację i dotychczasowy przebieg GNSS/tachimetrii bez osnowy (ścieżka „prawdziwe współrzędne” zostaje).
- Teksty w grze z polskimi znakami; w CSV/DOCX polskie znaki są dozwolone. Commity bez podpisów.

## Projekt

### Osnowa (`src/calc/Control.hpp`, `src/engine/ControlNet.hpp`)

- **Wiersz osnowy:** `id` (O1, O2, …), `world`, `N`, `E`, `H` (zmierzone GNSS, m), `x`, `y`, `z` (prawdziwa pozycja w grze, cm), `rozwiązanie`, `PDOP`, `epoki`, `czas`.
  Plik CSV w katalogu `GothicSurveying` (separator `;`), nazwy kolejne O1, O2…, kolizja numeru niemożliwa (numer = liczba wierszy + 1).
- **GNSS:** nowa pozycja menu „Zaloz punkt osnowy” (aktywna przy fixie i ≥ 20 epokach): zapisuje punkt jak „Zapisz punkt” (trafia też do dziennika GNSS
  z uwagą „osnowa O1”), dopisuje wiersz do osnowy i stawia znak. Znak to reper (model `LVL_REPER`) niezależny od roboty: nie znika po „Zakończ”.
- **Świat:** po wejściu do świata (start gry/wczytanie zapisu) kontroler stawia znaki wszystkich punktów osnowy z tego świata (porównanie nazwy świata);
  zmiana świata sprząta znaki bez dotykania zwolnionych voba (jak `ForgetAll`).
- **Menu startowe:** pozycja „Pokaz osnowe” (lista punktów, N, E, H, rozwiązanie) – tylko podgląd.

### Tachimetria v2

- **Źródło współrzędnych** (przełącznik w menu roboty, domyślnie osnowa, jeśli są punkty; inaczej prawdziwe): osnowa GNSS albo prawdziwe (jak w v1).
- **Stanowisko:** „Postaw tachimetr” – przy źródle *osnowa* pokazuje listę punktów osnowy w promieniu 2 m od bohatera; po wyborze tachimetr staje dokładnie
  na znaku, a współrzędne stanowiska to współrzędne zmierzone GNSS punktu (z błędem). Przy źródle *prawdziwe* jak dotąd (pod stopami).
- **Nawiązania:** lista do 5. „Dodaj nawiązanie (lustro)”: przy osnowie – wybór punktu z listy w promieniu 3–300 m od stanowiska (stawia tyczkę z lustrem na jego
  znaku, azymut z współrzędnych zmierzonych), przy prawdziwych – pod stopami jak dotąd. „Usuń ostatnie nawiązanie”.
- **Orientacja:** dla każdego nawiązania celujesz w lustro i naciskasz Enter; orientacja `az0` = średnia kołowa kierunków `az_i − Az_i` (z szumem 5″ każdy), co najmniej 1 nawiązanie.
  Dla każdego nawiązania: **odchyłka kierunku** `d_i = az_i − Az_i − az0` [mgon] i **odchyłka odległości** (zmierzona odległość skośna − odległość z współrzędnych) [mm].
  Panel i dziennik pokazują odchyłki; ostrzeżenie „nawiązanie niepewne”, gdy |odchyłka kierunku| > 20 mgon albo odległości > 20 mm + 5 ppm.
- **Pomiar punktów** bez zmian (na lustro lub bezlustrowy), z `az0` ze średniej. Każdy pomiar po zmianie nawiązań wymaga ponownej orientacji.
- **Celowanie:** `TachyAimError` akceptuje, gdy |kąt boczny| i |kąt pionowy| ≤ 0,02° oraz przesunięcie ≥ 1,5 cm jest zawsze dopuszczalne (dolny limit boczny),
  zasięg 3–300 m bez zmian.
- **Dziennik:** nagłówek ze stanowiskiem (źródło, nazwa punktu osnowy, N, E, H), tabela nawiązań (nazwa, azymut, Hz, odchyłka kierunku, odległość zmierzona i z współrzędnych,
  odchyłka, ocena), tabela punktów jak w v1; w CSV linie `# nawiazanie;...` przed nagłówkiem punktów; w DOCX dodatkowa tabela „Nawiązania”.

### Architektura

- `calc/Control.hpp`: `ControlPoint`, parsowanie i zapis CSV osnowy, filtr punktów w promieniu, nazwa kolejnego punktu.
- `calc/Tachy.hpp`: tolerancja kątowa, `TachyCircularMeanDeg`, odchyłki orientacji i odległości.
- `calc/TachyReport.hpp`: tabela nawiązań, linie w CSV, DOCX.
- `job/GnssRover.hpp` (+ `IWorld::StoreControlPoint`), `job/TachyStation.hpp` (lista nawiązań, źródło współrzędnych, wybór z osnowy przez `IWorld::ControlPoints()` i `PlaceBacksightAt(pointId)`).
- `engine/ControlNet.hpp`: plik osnowy, znaki w świecie, pozycja menu „Pokaz osnowe”, wybór punktu z listy (menu), integracja w `Controller.hpp`.

### Testy

- `calc`: parsowanie/zapis osnowy (round-trip, cudzysłowy), nazwy O1.., filtr promienia; średnia kołowa (przez 0/360), odchyłki, tolerancja kątowa (±0,02° na 10 m i 300 m), CSV nawiązań.
- `job`: GNSS „Zaloz punkt osnowy” wymaga fixu i 20 epok; tachimetria z kilkoma nawiązaniami (orientacja ze średniej, odchyłki, ostrzeżenie), źródło osnowy (współrzędne z GNSS),
  usunięcie nawiązania unieważnia orientację, limit 5.
- Silnik: sprawdza użytkownik (znaki po wczytaniu świata, lista wyboru, plik osnowy).

## Zostawione budowniczemu

Układ list wyboru w menu, wygląd znaku osnowy (może to być ten sam reper), dokładne progi ostrzeżeń (strojone w grze), nazwy kolumn poza wymienionymi.

## Ryzyka

- Rozpoznanie świata (nazwa) i ponowne stawianie znaków po wczytaniu zapisu: do sprawdzenia w grze.
- Rozmiar zmiany (dwie robóty i nowy trwały magazyn): 6–7 zadań; kolejność: tolerancja i nawiązania, potem osnowa GNSS, potem integracja.
