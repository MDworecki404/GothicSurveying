// Czysta arytmetyka niwelacji geometrycznej (bez Gothic API; jednostki: cm).
#pragma once
#include <cmath>

// Odczyt na łacie = wysokość linii celowej nad stopą łaty + błąd kolimacji + szum.
inline double LevelReading( double lineY, double footY, double distCm, double collimationArcsec, double noiseCm ) {
  return ( lineY - footY ) + distCm * std::tan( collimationArcsec / 206264.806 ) + noiseCm;
}

// Wstecz (BS) na reperze o znanej wysokości -> wysokość horyzontu instrumentu.
inline double LevelHorizon( double knownH, double backReading ) { return knownH + backReading; }

// W przód (FS) -> wysokość nowego punktu.
inline double LevelForeHeight( double horizon, double foreReading ) { return horizon - foreReading; }

// Dalmierz nitkowy (stała k = 100): odległość = k * (odczyt na górnej kresce - odczyt na dolnej); jednostki jak wejście.
inline double LevelStadiaDist( double upperReading, double lowerReading, double k = 100.0 ) { return k * std::fabs( upperReading - lowerReading ); }

// Odczyt idealny kresek dalmierczych leżących symetrycznie wokół środkowej: środek +- odległość / (2k).
inline double LevelStadiaReading( double midReading, double dist, bool upper, double k = 100.0 ) {
  return midReading + ( upper ? 1 : -1 ) * dist / ( 2 * k );
}
