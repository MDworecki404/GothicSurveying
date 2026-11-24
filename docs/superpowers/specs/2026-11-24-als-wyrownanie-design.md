# ALS: kalibracja boresightu i wyrownanie szeregow - projekt

Data: 2026-11-24. Rozszerza `2026-11-24-als-dron-design.md` (v0.8.0).

## Cel i po co

Po locie ALS uzytkownik widzi bledy (roznice szeregow, odchylki na osnowie), ale nie moze ich usunac. Narzedzie ma domknac petle: kalibracja katow
boresightu z zakladek, wyrownanie szeregow, wynik przed/po. Uczy tez granic metody: co jest wyznaczalne, a co nie.

## Zakres / poza zakresem

W zakresie: kalibracja 3 katow boresightu (roll, pitch, yaw) i wyrownanie szeregow (przesuniecie E/N/H kazdego szeregu), z punktami osnowy jako wiezami,
opcja "szeregi kalibracyjne" (poprzeczne) w planie, wyniki w osobnych plikach, praca w kilku klatkach, testy na sztucznym terenie.
Poza zakresem: kalibracja ramienia dzwigni (lever arm), wyrownanie trajektorii (dryf wewnatrz szeregu), pelna blokowa aerotriangulacja, klasyfikacja gruntu.

## Decyzje (potwierdzone)

- **Kiedy:** na zadanie, pozycja menu "Wyrownaj szeregi i skalibruj boresight" po locie (bez automatu).
- **Szeregi kalibracyjne:** opcja planu (domyslnie wylaczona): 3 szeregi prostopadle do glownych w tym samym obszarze; dodaja obserwowalnosc pitch/yaw na plaskim terenie.
- **Osnowa jako wiezy:** punkty osnowy (G.., O..) w obszarze ustalaja wysokosc i przesuniecia bezwzgledne; bez nich wyrownanie jest wzgledne (srednie przesuniecie = 0).
- **Wynik:** `lot_wyrownany.las/.pts` (oryginal nietkniety) i `lot_wyrownanie.txt` (katy +- niepewnosc, przesuniecia szeregow, rms przed/po, osnowa przed/po; w trybie prawdziwego bledu tez prawdziwy boresight i bias IMU).

## Metoda

1. **Dane:** rekord surowy rosnie z 26 do 34 B (kat lustra theta i zmierzona odleglosc); `lot.raw` po locie zostaje jako `lot_surowy.bin`, trajektoria estymowana z `trajektoria.csv`.
2. **Wiazanie:** komorki 4 m w nakladkach szeregow (>= 2 szeregi, >= 8 punktow kazdy, ok. 24 punkty na szereg i komorke, max ok. 3000 komorek). W kazdej komorce plaszczyzna z = a + b dE + c dN na szereg; reszta = roznica wysokosci w srodku komorki miedzy kolejnymi szeregami.
3. **Boresight:** Gauss-Newton po 3 katach (pochodne numeryczne, regularyzacja), punkty liczone ponownie `P = X + R(imu)(R_b(katy) r + lever)`. Niepewnosc z (J^T J)^-1; kat z niepewnoscia > 0,05 st. oznaczony "nieokreslony" (zostaje 0).
4. **Szeregi:** liniowe najmniejsze kwadraty, niewiadome (sE, sN, sH) na szereg; rownania: roznice w komorkach z gradientem plaszczyzny (przesuniecie poziome zmienia wysokosc o -g*s), wiezy osnowy (plaszczyzna w promieniu 3 m wokol punktu), slaby warunek sredniej.
5. **Zapis:** ponowne georeferowanie wszystkich punktow z kalibracja i przesunieciami do `lot_wyrownany.raw` -> `AlsConvert`.
- Staly bias IMU i blad boresightu sa nierozroznialne: wyznaczana jest ich suma (jak w prawdziwym ALS).

## Przypadki brzegowe

Jeden szereg lub brak nakladek: komunikat, bez wyrownania. Brak plikow roboczych (np. folder usuniety): komunikat. Plaski teren: pitch/yaw "nieokreslony".
Wyrownanie trwa kilka klatek (budzet 12 ms); przerywalne; stan wyrownania nie jest zapisywany w zapisie gry.

## Ryzyka

- Wiele milionow punktow: przebiegi strumieniowe po pliku, w pamieci tylko wybrane punkty (ok. 150 tys.). 32-bit: bez duzych tablic.
- Zaokraglenia trajektorii w CSV (mm, 0,001 st.) ogranicza dokladnosc kalibracji do ok. 0,005 st.
