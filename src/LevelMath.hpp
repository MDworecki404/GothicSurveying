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
