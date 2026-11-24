# Skaning lotniczy z drona (ALS) - projekt

Data: 2026-11-24. Stan wyjsciowy: main, v0.7.0. Status: wdrozone na galezi als-dron (lokalnie, bez wydania).

## Cel i po co

Nowa robota "Skaning lotniczy z drona": uczy planowania nalotu i pokazuje, jak bledy trajektorii GNSS/IMU, boresightu i lever armu
przenosza sie na chmure punktow (georeferencja bezposrednia). Wynik to realistyczna chmura (LAS 1.2, PTS) z trajektoria i opisem lotu.
Plan nalotu i trajektoria maja byc niezalezne od sensora, zeby pozniej uzyc ich do fotogrametrii lotniczej (kamera zamiast skanera).

## Zakres

W zakresie: model drona, plan nalotu (podglad na mapie osnowy, eksport CSV), autopilot, skaner liniowy, georeferencja z szumem,
LAS 1.2 (format 3) + PTS + trajektoria CSV + opis, kontrola zgodnosci szeregow, kontrola na punktach osnowy, "prawdziwy blad", zapis planu w zapisie gry, README i INSTALACJA.

Poza zakresem (anty-cele): fotogrametria/SfM, kalibracja boresightu, wyrownanie szeregow, reczne latanie, widok z gimbala,
automatyczne omijanie przeszkod, zapis pozycji w polowie szeregu.

## Decyzje (wszystkie potwierdzone domyslne)

- **Sterowanie:** autopilot; bohater stoi, dron startuje z punktu pod jego stopami. Przelacznik kamery "sledz drona" (widok z tylu).
- **Dron w grze:** vob w prawdziwej pozycji, korpus + 4 smigla (obrot) + glowica LiDAR pod spodem, osobne voby jak skaner naziemny (`assets/make_models.py`).
- **Skala:** obszar do ok. 300x300 m, AGL 30-150 m, predkosc 3-8 m/s. Presety gestosci impulsow, cel 1-5 mln punktow na lot.
- **Obszar:** wierzcholki "pod stopami" albo na punktach osnowy (G.., O..); prostokat z dwoch przeciwleglych wierzcholkow i kata szeregow, albo wielokat. Podglad (obszar, szeregi, start) na `MapView`; eksport planu CSV.
- **Blad:** profile "RTK dobry" (ok. 2 cm, IMU 0,01-0,02 st.), "PPK", "slaby" (odcinki float, dryf dziesiatki cm). Boresight ma ukryty blad systematyczny, lever arm zadany.
  `P = X_gnss + R(imu) * (R_boresight * r_skaner + lever)`. Prawdziwa trajektoria jest znana, blad dostepny w trybie "pokaz prawdziwy blad".
- **Teren:** AGL wzgledem terenu statycznego (promien w dol, wygladzenie okna ruchomego); "podazaj za terenem" = stale AGL, inaczej stala wysokosc bezwzgledna.
  Trasa przez budynki/drzewa daje ostrzezenie w planie, bez omijania.
- **Kontrola osnowy:** w podsumowaniu porownanie wysokosci chmury przy znakach G../O.. z ich H.
- **Zapis gry:** zapisywany plan i ustawienia; lot w toku wznawiany od poczatku biezacego szeregu (`SaveState`/`LoadState`).
- **Czas lotu:** zegar symulacji rosnie z wypuszczonymi impulsami (jak skaner naziemny), wiec gestosc nie zalezy od FPS. Budzet ok. 12 ms/klatke w `Tick()`.

## Podejscie (jedyne sensowne)

Rozbudowa istniejacych wzorcow, bez nowej architektury: nowy `JobKind::Aerial` i flaga w `JobType`, obliczenia w `calc/`, logika w `job/`, silnik cienko.
Odrzucone: przerabianie `IWorld::ScanRay` (jest zwiazany z okiem stojacego instrumentu i 120 m) - dodajemy nowe wywolania z domyslnymi cialami,
by swiaty testowe i inne roboty ich nie musialy znac.

## Zmiany w interfejsie `IWorld` (domyslne ciala)

- `AerialRay(origin GridPos, azDeg, elevDeg, maxM, &distM, &cosInc, &isVob, &rgb)` - promien z dowolnego punktu, zasieg do ok. 250 m.
- `GroundHeightM(n, e, &h)` - wysokosc terenu w dowolnym punkcie (promien w dol).
- `PlaceDrone`/`SetDronePose(pos, yaw, pitch, roll, propPhase)`/`RemoveDrone`, `SetFollowCamera(bool)`.
- `RouteBlocked(a, b)` - ostrzezenie o przeszkodach na trasie.

## Moduly

| Plik | Zawartosc | Testy |
|---|---|---|
| `calc/Flight.hpp` | plan nalotu: wielokat -> szeregi (kierunek, odstep z AGL, FOV, zakladka), czas lotu, loty na bateriach (wymiana w bazie zamiast odmowy startu; zmiana po wdrozeniu), gestosc, eksport CSV; parametry sensora jako struktura (pod fotogrametrie) | `tests/test_flight.cpp` |
| `calc/Georef.hpp` | trajektoria prawdziwa i estymowana (GNSS szum/dryf, IMU szum/bias), boresight, lever arm, skaner liniowy (lustro, FOV), wzor georeferencji | `tests/test_georef.cpp` |
| `calc/AlsLas.hpp` | strumieniowy zapis surowy (rekord z czasem GPS i numerem szeregu), LAS 1.2 format 3 (offset, skala 1 mm), PTS; kontrola szeregow i osnowy | `tests/test_alslas.cpp` |
| `job/AerialJob.hpp` | robota: menu, plan, autopilot, `Tick()`, raport, stan | `tests/test_aerial.cpp` (falszywy swiat) |
| `engine/*`, `assets/make_models.py` | adapter `IWorld`, panele/menu, podglad na `MapView`, model drona, hook kamery | build + gra |

## Etapy (kazdy: zielone testy + build, maly commit lokalny)

1. `calc/Flight.hpp` + testy.
2. `calc/Georef.hpp` + testy (m.in. zerowe bledy = zero odchylki, bias roll daje oczekiwane przesuniecie).
3. `calc/AlsLas.hpp` + testy (naglowek LAS, format 3, offset przy N ~ 5,5 mln m).
4. `AerialJob` + falszywy swiat + zapis stanu; rejestracja w `Jobs.hpp`.
5. Adapter silnikowy, model drona, podglad na mapie, diagnostyka w `GothicSurveying.log`.
6. README, INSTALACJA (`scripts/package.py`); wydanie tylko na polecenie.

## Ryzyka

- Koszt promienia w grze (zasieg 250 m, 1-5 mln punktow): stad presety gestosci i budzet na klatke; pomiar w grze po etapie 5.
- Wywolania silnikowe (promien z dowolnego punktu, voby drona) sprawdza tylko gra: diagnostyka do logu, prosba o fragment zamiast zgadywania.
- Wtyczka 32-bitowa: zapis strumieniowy, wspolrzedne siatki z offsetem (LAS) i float lokalny wzgledem srodka obszaru w rekordzie surowym.

## Dla wykonawcy (moje decyzje drobne)

- LAS: "point source id" = numer szeregu, GPS time = czas symulacji lotu, intensywnosc jak w `ScanIntensity`.
- Teksty w grze bez polskich znakow; komunikaty po polsku.
