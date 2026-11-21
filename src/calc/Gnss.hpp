// Symulacja odbiornika GNSS (bez Gothic API): satelity, maska horyzontu, PDOP, typ rozwiązania.
#pragma once
#include <algorithm>
#include <cmath>
#include <vector>

struct SatDir { double azDeg, elDeg; };

// Horyzont widziany z tyczki: elewacja zasłonięcia na sektor azymutu [10s, 10s+10) i odległość najbliższej przeszkody.
struct SkyMask { double horizonDeg[36] = {}; double nearestObstacleM = 100.0; };

enum class SolType { None, Autonomous, Dgps, Float, Fix };

inline const char* GnssSolName(SolType t) {
  switch (t) {
    case SolType::Fix: return "fix RTK";
    case SolType::Float: return "float RTK";
    case SolType::Dgps: return "DGPS";
    case SolType::Autonomous: return "autonomiczne";
    default: return "brak";
  }
}

// 30 pseudo-satelitów; położenie deterministycznie zależy od czasu [s].
inline std::vector<SatDir> GnssSatellites(double tSec) {
  std::vector<SatDir> s;
  for (int k = 0; k < 30; k++) {
    double az = std::fmod(12.0 * k + 0.004 * tSec * (k % 3 + 1), 360.0);
    s.push_back({az, 5.0 + 85.0 * std::fabs(std::sin(1.7 * k + 0.0007 * tSec))});
  }
  return s;
}

inline std::vector<SatDir> GnssUsable(const std::vector<SatDir>& sats, const SkyMask& m) {
  std::vector<SatDir> used;
  for (const SatDir& d : sats) {
    const int sector = (int)std::floor(d.azDeg / 10.0) % 36;
    if (d.elDeg > std::max(10.0, m.horizonDeg[sector])) used.push_back(d);
  }
  return used;
}

// PDOP z macierzy geometrii; 99 gdy < 4 satelitów albo macierz osobliwa.
inline double GnssPdop(const std::vector<SatDir>& used) {
  if (used.size() < 4) return 99.0;
  const double rad = 3.14159265358979323846 / 180.0;
  double a[4][8] = {};
  for (const SatDir& d : used) {
    const double az = d.azDeg * rad, el = d.elDeg * rad;
    const double g[4] = {std::cos(el) * std::sin(az), std::cos(el) * std::cos(az), std::sin(el), 1.0};
    for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) a[i][j] += g[i] * g[j];   // GtG
  }
  for (int i = 0; i < 4; i++) a[i][4 + i] = 1.0;
  for (int c = 0; c < 4; c++) {                                  // Gauss-Jordan z wyborem elementu głównego
    int p = c;
    for (int r = c + 1; r < 4; r++) if (std::fabs(a[r][c]) > std::fabs(a[p][c])) p = r;
    if (std::fabs(a[p][c]) < 1e-12) return 99.0;
    for (int j = 0; j < 8; j++) std::swap(a[c][j], a[p][j]);
    const double piv = a[c][c];
    for (int j = 0; j < 8; j++) a[c][j] /= piv;
    for (int r = 0; r < 4; r++) if (r != c) { const double f = a[r][c]; for (int j = 0; j < 8; j++) a[r][j] -= f * a[c][j]; }
  }
  return std::sqrt(a[0][4] + a[1][5] + a[2][6]);
}

// 0 przy przeszkodzie dalszej niż 5 m, liniowo do 1 przy 0 m.
inline double GnssMultipath(const SkyMask& m) { return std::min(1.0, std::max(0.0, 1.0 - m.nearestObstacleM / 5.0)); }

inline SolType GnssClassify(int nUsed, double pdop, double multipath, double sessionSec, double tFloatSec, double tFixSec) {
  if (nUsed < 4) return SolType::None;
  if (nUsed >= 6 && pdop < 2.5 && multipath < 0.3 && sessionSec >= tFixSec) return SolType::Fix;
  if (nUsed >= 5 && pdop < 3.5 && multipath < 0.6 && sessionSec >= tFloatSec) return SolType::Float;
  if (nUsed >= 5 && pdop < 4.0) return SolType::Dgps;
  return SolType::Autonomous;
}
