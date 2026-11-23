// Poziomowanie instrumentu (bez Gothic API): pochylenie osi pionowej w minutach katowych i trzy sruby ustawcze.
// Pochylenie (x, y) to kierunek ucieczki babelka libelli (x w prawo, y w gore na ekranie). Sruby 1, 2, 3 leza na obwodzie kola pod katami 90, 210 i 330 stopni;
// podniesienie sruby (obrot w prawo) przesuwa babelek w jej strone.
#pragma once
#include <algorithm>
#include <cmath>

struct Libella {
  bool enabled = false;                     // wylaczone = instrument zawsze poziomy
  double x = 0, y = 0;                      // pochylenie [minuty katowe]

  static constexpr double kMax = 20.0;      // zakres pochylenia [']

  // u1, u2 w [0, 1): losowy kierunek i wielkosc 9..16' (poza kolkiem libelli, zeby trzeba bylo poziomowac).
  void Reset(double u1, double u2) {
    if (!enabled) { x = y = 0; return; }
    const double a = u1 * 2.0 * 3.14159265358979323846, m = 9.0 + 7.0 * u2;
    x = m * std::cos(a); y = m * std::sin(a);
  }
  static void ScrewDir(int screw, double& ux, double& uy) {          // sruba 0..2
    const double a = (90.0 + 120.0 * screw) * 3.14159265358979323846 / 180.0;
    ux = std::cos(a); uy = std::sin(a);
  }
  void Turn(int screw, double deltaArcmin) {                          // delta > 0: sruba w gore (obrot w prawo)
    if (!enabled || screw < 0 || screw > 2) return;
    double ux, uy; ScrewDir(screw, ux, uy);
    x = (std::max)(-kMax, (std::min)(kMax, x + deltaArcmin * ux));
    y = (std::max)(-kMax, (std::min)(kMax, y + deltaArcmin * uy));
  }
  double Mag() const { return std::hypot(x, y); }
  bool Within(double limitArcmin) const { return !enabled || Mag() <= limitArcmin; }
};

inline const double kLevelBubbleArcmin = 8.0;   // niwelator: babelek w kolku libelli okraglej
inline const double kTachyLevelArcmin = 0.5;    // tachimetr: libella elektroniczna (30")
