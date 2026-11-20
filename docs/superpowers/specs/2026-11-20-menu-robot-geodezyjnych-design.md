# Menu robót geodezyjnych i ciąg niwelacyjny – projekt

Data: 2026-11-20. Repo: GothicSurveying (plugin Union do Gothic II NotR).

## Intencja i po co

Dziś plugin to jeden instrument (niwelator z łatą) sterowany stałymi klawiszami F7–F11 i liczący pojedyncze odczyty.
Autor robi materiały „Geodezja w Gothic”, a plugin ma być narzędziem do **całych robót geodezyjnych**: ciągu niwelacyjnego,
a w przyszłości tachimetrii i niwelacji trygonometrycznej. Potrzebne są: wspólne menu, pojęcie „roboty” z własnymi
narzędziami i krokami oraz **dziennik pomiarowy** kompletowany na końcu roboty. Architektura ma pozwalać dodawać kolejne
roboty i instrumenty bez przepisywania rdzenia.

Dlaczego teraz: dodanie drugiego instrumentu do obecnego, jednoplikowego `Level.hpp` ze wspólnym stanem globalnym
i klawiszami na sztywno zrobiłoby z niego plątaninę.

## Cele

- Jeden klawisz otwiera **menu geodezyjne**. Poza robotą: „Rozpocznij robotę” → wybór typu roboty i ustawień.
  W trakcie roboty menu staje się menu tej roboty (kroki), do czasu „Zakończ robotę”.
- **Pierwsza robota: ciąg niwelacyjny** w trzech rodzajach: **dowiązany** (reper A → reper B), **zamknięty** (z A do A),
  **otwarty** (bez kontroli).
- Gracz sam czyta łatę w lunecie (jak dziś). Dziennik wypełnia się z jego odczytów.
- **Tryb liczenia dziennika wybiera gracz przy starcie roboty**: *automatyczne* (plugin sam liczy przewyższenia, błąd zamknięcia,
  poprawki, wysokości) albo *samodzielne* (gracz wpisuje wyniki obliczeń, plugin je sprawdza).
- Architektura: **rdzeń + moduły robót**. Nowa robota = nowy plik z implementacją interfejsu `Job`.
- Logika roboty i obliczenia dziennika działają bez silnika gry, więc da się je testować jak dziś `LevelMath.hpp`.

## Poza zakresem (v1)

- Tachimetria, niwelacja trygonometryczna i inne instrumenty (architektura ma je tylko umożliwić).
- Zapis roboty między sesjami gry i wznawianie po wczytaniu zapisu.
- Katalog stałych reperów w świecie gry (repery stawia gracz).
- Wyrównanie ścisłe (metoda najmniejszych kwadratów); tylko klasyczne wyrównanie równomierne.
- Kontrola równości celowych, niwelacja precyzyjna, pomiary dwuhoryzontowe, zapis w formatach zewnętrznych (poza CSV).

## Antycele (porażka, nawet jeśli „działa”)

- Zepsucie tego, co dziś działa dobrze: stawianie sprzętu pod stopami, widok lunety z krzyżem i kreskami dalmierczymi,
  ręczny odczyt w mm, panele tekstowe.
- Interpreter skryptów lub własny język kroków (przerost infrastruktury na jeden instrument).
- Menu wymagające kilkunastu naciśnięć klawiszy na prostą czynność.

## Ograniczenia środowiska (sprawdzone w grze)

- Tylko hook `zCWorld::Render` jest wołany co klatkę; `oCGame::Render` nie.
- Tekst w grze: tylko przez osobne widoki `zCView` z tłem (`LVL_PANEL.TGA`); `screen->Print` jest niewidoczne pod nakładką.
  Teksty w grze **bez polskich znaków** (kodowanie); w plikach CSV i dokumentacji polskie znaki są dozwolone.
- Klawisze: **F5/F6** (szybki zapis/odczyt Gothica) zajęte, **brak klawiatury numerycznej**. Cyfry 0–9 w trybie wpisywania
  działają, bo gra jest wtedy zatrzymana (`ogame->Pause()`).
- Gra uruchamiana przez junction bez polskich znaków w ścieżce (Union Fatal).
- Plugin kompilowany przez szablon CMake; `Plugin.hpp` jest dołączany **wewnątrz** `namespace GOTHIC_NAMESPACE` i nie może
  mieć strażników include ani `#pragma once` w kodzie zależnym od silnika.
- Pierwsze wczytanie nowych modeli/tekstur wymaga skasowania cache `_compiled/LVL_*` (robi to `install.ps1`).

## Rozważone podejścia

1. **Jeden plik z przełącznikiem roboty** – `switch` w istniejących funkcjach. Najtaniej teraz, ale każda nowa robota rozlewa
   się po całym kodzie. Odrzucone.
2. **Rdzeń + moduły robót (wybrane).** Wspólny rdzeń (stawianie sprzętu, luneta, panele, menu, zapis) i osobny plik na robotę
   z interfejsem `Job`. Dziennik to wspólny komponent. Środkowa złożoność, naturalny podział odpowiedzialności.
3. **Roboty jako dane (JSON/Lua)** – silnik interpretuje kroki. Najbardziej elastyczne, ale wymaga parsera i języka kroków
   ponad potrzeby jednego instrumentu. Odrzucone.

Wybór (2) potwierdzony przez autora.

## Projekt

### Warstwy i pliki (propozycja podziału; szczegóły nazw zostawione wykonawcy)

```
src/
  calc/                      czyste, bez silnika, testowane
    LevelMath.hpp            (jest) odczyt idealny, BS/FS, dalmierz
    Traverse.hpp             przewyższenia, błąd zamknięcia, poprawki, wysokości wyrównane, fh_dop
    FieldBook.hpp            model dziennika (wiersze, podsumowanie) + eksport CSV
  job/
    Job.hpp                  interfejs Job + rejestr robót
    LevelingTraverse.hpp     automat stanów ciągu niwelacyjnego (bez silnika, przez interfejs IWorld)
  engine/                    zależne od Gothic API, w namespace GOTHIC_NAMESPACE
    World.hpp                implementacja IWorld: stawianie vobów pod stopami, usuwanie, wysokość terenu, linia celowa
    Panels.hpp               panele tekstowe zCView (HUD, pole wpisywania)
    Menu.hpp                 widget menu (lista, kursor, pozycje nieaktywne)
    ScopeView.hpp            widok lunety: kamera, zbliżenie, krzyż, kreski dalmiercze, obrót, wybór łaty pod kreską
    FieldBookView.hpp        wyświetlanie i edycja dziennika (tabela)
  Plugin.hpp                 hook zCWorld::Render → pętla klatki
```

Reguły zależności: `calc/` i `job/` **nie** includują Gothic API. `job/` rozmawia ze światem tylko przez `IWorld`
(postaw punkt, usuń, podaj wysokość terenu, odległość, czy linia celowa jest wolna), dzięki czemu automat ciągu da się
przetestować w `tests/` bez gry.

### Interfejs roboty (zarys)

`Job` udostępnia: nazwę, `menuItems()` zależne od bieżącego kroku (z flagą aktywna/nieaktywna i podpowiedzią),
`onMenu(item)`, `onReading(rodRole, mm, dalmierz)` wywoływane po wpisaniu odczytu z lunety, `hudText()` (bieżący krok),
`finish()` zwracające `FieldBook`. Rdzeń nic nie wie o niwelacji; wie tylko, że robota ma kroki, menu i dziennik.

### Menu i sterowanie

- **Klawisz menu: F12** (stała `LV_KEY_MENU`, zmienna). Moja propozycja: F5/F6 zajęte, F12 nie jest używany przez Gothica.
- Menu otwarte = gra zatrzymana (`ogame->Pause()`). Nawigacja: ↑/↓, Enter wybiera, ten sam F12 zamyka.
  Po wyborze akcji menu zamyka się i akcja wykonuje się w świecie (stawianie sprzętu używa pozycji bohatera),
  a menu otwierasz ponownie klawiszem. Pozycje niedostępne w danym kroku są wyszarzone z podpowiedzią.
- Poza robotą: `Rozpocznij robotę` → lista typów (dziś tylko „Ciąg niwelacyjny”) → rodzaj ciągu
  (dowiązany / zamknięty / otwarty) → tryb liczenia dziennika (automatyczny / samodzielny).
- W robocie stały panel HUD z bieżącym krokiem („Ciąg dowiązany, stanowisko 2: postaw łatę wprzód”).
- Luneta zostaje pod **F10** (wejście/wyjście), z obrotem ←/→, Shift = powoli, Enter = odczyt, jak dziś.
  Stare F7/F8/F9/F11 znikają (zastępuje je menu). Wybór łaty przy odczycie: ta, której pionowa kreska leży na łacie;
  rola (wstecz/wprzód) wynika z kroku roboty, nie z klawisza.

### Ciąg niwelacyjny – przebieg

Role i obiekty: **repery** i **punkty obrotowe z żabkami** są trwałymi znacznikami w świecie. Są dwie łaty (obiekty), którym
menu przypisuje rolę *wstecz* i *wprzód*. Niwelator jest jeden i przenoszony na kolejne stanowiska.

1. Start: wybór rodzaju i trybu liczenia. Krok „Postaw reper A” (znacznik reper z wysokością = prawdziwa wysokość terenu
   w tym miejscu, na mm; wyświetlana w HUD). A staje się punktem 1.
2. Stanowisko k: `Postaw niwelator` → `Postaw łatę wstecz` na bieżącym punkcie (przy k=1 na A, później już stoi) →
   odczyt wstecz w lunecie → łata wprzód → odczyt wprzód. Menu ma dwie pozycje dla łaty wprzód: **„Postaw łatę wprzód (punkt obrotowy)”**
   (nowy punkt z żabką, ciąg trwa dalej) i **„Postaw łatę wprzód (punkt końcowy)”** (ostatni punkt: dla dowiązanego tworzy reper B,
   dla zamkniętego łata staje na reperze A, dla otwartego jest to dowolny ostatni punkt; po odczycie na nim ciąg jest domknięty
   i aktywne staje się `Zakończ robotę`). Przewyższenie stanowiska Δh = wstecz − wprzód; kolejność odczytów dowolna.
3. `Przełóż na następne stanowisko`: niwelator i łata z tyłu są zabierane; łata, która stała na punkcie wprzód, staje się łatą
   wstecz; stara łata wstecz do niesienia staje się kolejną łatą wprzód. Stanowisko k+1 zaczyna się od `Postaw niwelator`.
4. Zakończenie ciągu (rodzaj decyduje): *dowiązany* – ostatni punkt to **reper B** (znacznik o znanej wysokości, stawiany w
   krokach „Postaw reper B”); *zamknięty* – ostatni punkt wprzód to powrót na reper A (łata < 30 cm od niego przykleja się do znacznika);
   *otwarty* – dowolny ostatni punkt.
5. `Zakończ robotę` (aktywne, gdy jest ≥ 1 kompletne stanowisko i ciąg jest domknięty zgodnie z rodzajem): kompletuje dziennik
   (patrz niżej), pokazuje go w grze i zapisuje CSV. Przy zmianie świata (wczytanie zapisu) w trakcie roboty plugin zapisuje
   `dziennik_N_przerwany.csv` z tym, co jest, i kończy robotę.

### Dziennik pomiarowy

Kolumny (po polsku, jak w dzienniku niwelacji): nr stanowiska, nr punktu, odczyt wstecz [mm], odczyt wprzód [mm],
przewyższenie Δh [mm], poprawka v [mm], przewyższenie wyrównane [mm], wysokość H [m], uwagi (reper / żabka). Podsumowanie:
Σ wstecz, Σ wprzód, ΣΔh, **fh**, **fh_dop**, wynik (w tolerancji / przekroczony), długość ciągu L [m]. Dla ciągu otwartego bez fh i bez
poprawek. Jeśli wpisano kreski dalmiercze, w dzienniku są też długości celowych z dalmierza; do L używane są prawdziwe odległości.

**Tryb automatyczny:** po każdym odczycie plugin liczy Δh; na końcu fh = ΣΔh − (H_koniec − H_start) (zamknięty: H_koniec = H_A),
poprawki v = −fh / n (równomiernie na stanowisko), Δh wyrównane, wysokości wyrównane punktów.

**Tryb samodzielny:** dziennik ma wypełnione kolumny odczytów (z lunety), a pozostałe (Δh, Σ, fh, poprawki v, wysokości
wyrównane) są puste do wpisania przez gracza w edytorze tabeli (kursor po komórkach, cyfry, Enter). Przy `Zakończ robotę`
plugin porównuje każdą wpisaną komórkę z poprawną wartością (tolerancja 1 mm), oznacza trafione/chybione, pokazuje poprawne
wartości i zapisuje do CSV obie wersje (gracza i poprawną).

**Eksport:** `dziennik_N.csv` (średniki, jak dziś `niwelacja_N.csv`) w katalogu głównym gry. Dotychczasowy `niwelacja_N.csv`
z pojedynczymi odczytami zostaje jako log pomocniczy w trakcie roboty.

**fh_dop:** ±20 mm·√L (L w km) jako domyślne kryterium niwelacji technicznej; stała konfigurowalna.

### Testowanie

- Testy jednostkowe `calc/Traverse.hpp` (3 rodzaje ciągu, błąd zamknięcia, poprawki, kolejność odczytów), z danymi liczonymi ręcznie.
- Test automatu `LevelingTraverse` przez sztuczne `IWorld` (poprawne i niepoprawne sekwencje kroków, przełożenie, zakończenie
  w każdym rodzaju, ścieżka przerwania).
- Rzeczy zależne od silnika (menu, panele, luneta, edytor tabeli) sprawdza autor w grze; plugin loguje do `GothicSurveying.log`.

## Rozstrzygnięcia autora (jego słowami: ustalone w rozmowie)

- Menu pod jednym klawiszem F; „Rozpocznij robotę”; wybór typu roboty; narzędzia zależą od roboty; menu można wywoływać
  w trakcie roboty aż do „Zakończ robotę”, które kompletuje dziennik.
- Typy robót: ciąg niwelacyjny, tachimetria, niwelacja trygonometryczna (w tej kolejności ważności: najpierw ciąg).
- Ciąg: łata wstecz/wprzód, niwelator, przekładanie łaty wstecz na wprzód aż do końca.
- Dziennik: użytkownik wybiera, czy liczy automatycznie, czy samodzielnie.
- Rodzaje ciągu: dowiązany, zamknięty i otwarty – wszystkie w pierwszej wersji.
- Repery: gracz je stawia (opcja A), z wysokością podaną przez plugin.
- Architektura: rdzeń + moduły robót.

## Moje decyzje (do sprawdzenia przez autora)

- Klawisz menu F12 (zmienna stała); F7/F8/F9/F11 usunięte, F10 zostaje dla lunety.
- Menu zatrzymuje grę i zamyka się po wyborze akcji; kroki roboty są dostępne tylko wtedy, gdy mają sens.
- Δh = wstecz − wprzód niezależnie od kolejności odczytów; poprawki równomierne na stanowisko; tolerancja 1 mm w trybie samodzielnym.
- Wysokość reperu = prawdziwa wysokość terenu, w HUD podana w metrach z dokładnością do mm.
- fh_dop = 20 mm·√L km (niwelacja techniczna).
- Dwie łaty jako obiekty wielokrotnego użytku; punkty obrotowe z żabką pozostają na świecie.
- Brak zapisu roboty między sesjami; przy zmianie świata zapis `dziennik_N_przerwany.csv`.
- Dziennik pokazywany w grze jako tabela w panelu (przewijana), oprócz CSV.

## Ryzyka i luki

- **Edytor tabeli w trybie samodzielnym** to największa część interfejsu (kursor po komórkach, wpisywanie liczb ze znakiem,
  przewijanie). Sugerowana kolejność prac: (1) rdzeń + menu + ciąg w trybie automatycznym z podglądem dziennika i CSV,
  (2) edytor i sprawdzanie dla trybu samodzielnego.
- Kolejka wejść z klawiatury podczas pauzy była sprawdzona dla cyfr i Enter; strzałki ↑/↓ w menu i Backspace/minus w edytorze
  (liczby ujemne dla Δh i poprawek) wymagają sprawdzenia w grze.
- Panele zCView mają ograniczoną liczbę znaków w wierszu; dziennik ze wszystkimi kolumnami może nie zmieścić się na jednym
  ekranie przy 1080p – wykonawca ma dobrać czcionkę/układ albo ograniczyć kolumny na ekranie (CSV zawsze pełny).
- Wysokość terenu w grze jest dowolnym układem (Y silnika); „wysokość reperu” nie ma związku z żadnym układem odniesienia.

## Zostawione wykonawcy

Nazwy klas i plików, dokładny układ paneli i menu, format komunikatów, szczegóły automatu stanów (np. czy dopuszcza
cofnięcie kroku), układ tabeli dziennika na ekranie, sposób przewijania, obsługa błędów wpisu.
