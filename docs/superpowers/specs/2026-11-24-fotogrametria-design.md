# Fotogrametria lotnicza - projekt i plan

Data: 2026-11-24. Osobna robota "Fotogrametria lotnicza" (v0.9.0 -> nastepna wersja). Zatwierdzone: syntetyczne zdjecia promieniami, punkty wiazace symulowane, wbudowane wyrownanie wiazki,
eksport zdjec i CSV do programow zewnetrznych, presety: rozdzielczosc 320x240 / 640x480 / 1024x768 (domyslnie 320x240), kamery 84 / 63 / 40 st. (poziomy kat widzenia).

## Cel i zakres
Plan zdjec (GSD, zakladki), nalot, zdjecia (PNG) z pozycjami GNSS/IMU, wyrownanie wiazki z punktami wiazacymi i osnowa (GCP + punkty kontrolne), raport bledow.
Poza zakresem (pozniej): ortofotomapa i NMPT ze zdjec, autokalibracja kamery (ogniskowa, dystorsja), rozpoznawanie cech na obrazach (SIFT).

## Architektura
- Osobna klasa `PhotoJob : public AerialJob` (wlasna nazwa, `JobKind::Photo`, wpis w `Jobs.hpp`): wspolne sa obszar (wierzcholki, mapa, prostokat), plan nalotu, bateria, dron, zapis stanu.
  `AerialJob` dostaje haki: `ApplySensor()` (parametry sensora w planie), `EmitPulse()` (strzal zamiast impulsu), `ShotBusy()/StepShot()` (zdjecie renderowane w kilku klatkach, czas lotu stoi),
  `ProduceResults()` (wyniki po locie). Menu `PhotoJob` = menu bazowe bez pozycji LiDAR (gestosc impulsow, szeregi kalibracyjne, wyrownanie, produkty) + kamera, rozdzielczosc, zakladki.
- `calc/Camera.hpp`: presety kamer i rozdzielczosci, geometria zdjecia (ogniskowa w pikselach, pas, GSD, odstep szeregow, baza, odstep czasowy zdjec), promien piksela i rzutowanie punktu (model otworkowy, kamera w dol,
  gorna krawedz obrazu = przod drona). `calc/Png.hpp`: zapis PNG (deflate bez kompresji, CRC i Adler). `calc/Bundle.hpp`: wyrownanie wiazki (poza + punkty 3D, Gauss-Newton z dopelnieniem Schura,
  wiezy GNSS/IMU, GCP, punkty kontrolne).
- Zdjecie: promien na piksel z prawdziwej pozycji (z ukrytym boresightem kamery), kolor z `AerialRay`. Punkty wiazace: sztuczny zestaw punktow terenu rzutowany na zdjecia z widocznoscia sprawdzana promieniem, szum 0,5 px;
  osnowa jako GCP (co drugi punkt kontrolny). Pozycje i katy zdjec w `zdjecia.csv` (estymata GNSS/IMU z szumem).
- Raport `fotogrametria_raport.txt`: GSD (renderowane i odpowiednik 20 MP), zakladki, liczba zdjec i punktow, rms reprojekcji przed/po, poprawki pozycji, boresight kamery (srednia roznica IMU - wyrownanie), RMSE na GCP i punktach kontrolnych.

## Etapy (kazdy: testy `ALL OK`, build `BUILD OK`, maly commit lokalny, bez push/merge/wydan)
1. `Camera.hpp` + testy. 2. `Png.hpp` + testy. 3. `Bundle.hpp` + testy na sztucznej scenie. 4. Haki w `AerialJob` + `PhotoJob` (plan, lot, zdjecia, CSV). 5. Punkty wiazace i wyrownanie w robocie, raport.
6. Dokumentacja, build, instalacja.
