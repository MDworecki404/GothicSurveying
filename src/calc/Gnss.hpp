// Symulacja odbiornika GNSS (bez Gothic API): satelity, maska horyzontu, PDOP, typ rozwiązania.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
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
    if (d.elDeg > (std::max)(10.0, m.horizonDeg[sector])) used.push_back(d);
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
inline double GnssMultipath(const SkyMask& m) { return (std::min)(1.0, (std::max)(0.0, 1.0 - m.nearestObstacleM / 5.0)); }

inline SolType GnssClassify(int nUsed, double pdop, double multipath, double sessionSec, double tFloatSec, double tFixSec) {
  if (nUsed < 4) return SolType::None;
  if (nUsed >= 6 && pdop < 2.5 && multipath < 0.3 && sessionSec >= tFixSec) return SolType::Fix;
  if (nUsed >= 5 && pdop < 3.5 && multipath < 0.6 && sessionSec >= tFloatSec) return SolType::Float;
  if (nUsed >= 5 && pdop < 4.0) return SolType::Dgps;
  return SolType::Autonomous;
}

// ---------- układ lokalny ----------
// N = z_gry/100 + N0, E = x_gry/100 + E0, H = y_gry/100 + H0 (współrzędne gry w cm; fikcyjny układ lokalny w metrach).
inline const double GNSS_N0 = 5500000.0, GNSS_E0 = 6500000.0, GNSS_H0 = 0.0;
struct GridPos { double n, e, h; };
inline GridPos GnssToGrid(double gameX, double gameY, double gameZ) { return {gameZ / 100.0 + GNSS_N0, gameX / 100.0 + GNSS_E0, gameY / 100.0 + GNSS_H0}; }
inline void GnssFromGrid(const GridPos& g, double& gameX, double& gameY, double& gameZ) {
  gameX = (g.e - GNSS_E0) * 100.0; gameY = (g.h - GNSS_H0) * 100.0; gameZ = (g.n - GNSS_N0) * 100.0;
}

// ---------- szum ----------
struct SolSigma { double hMm, vMm; };
// 1 sigma poziomo/pionowo [mm]: bazowe wg typu, razy max(0,7; PDOP/2) razy (1 + 2*wielodrogowość).
inline SolSigma GnssSigma(SolType t, double pdop, double multipath) {
  double h = 0, v = 0;
  switch (t) {
    case SolType::Fix: h = 8; v = 15; break;
    case SolType::Float: h = 300; v = 450; break;
    case SolType::Dgps: h = 700; v = 1100; break;
    case SolType::Autonomous: h = 3000; v = 5000; break;
    default: return {0, 0};
  }
  const double k = (std::max)(0.7, pdop / 2.0) * (1.0 + 2.0 * multipath);
  return {h * k, v * k};
}

// Własny generator (xorshift32 + Box-Muller): ten sam wynik na każdym kompilatorze.
class GnssRng {
 public:
  explicit GnssRng(uint32_t seed) : s_(seed ? seed : 0x9E3779B9u) {}
  double Uniform() { s_ ^= s_ << 13; s_ ^= s_ >> 17; s_ ^= s_ << 5; return (s_ + 0.5) / 4294967296.0; }
  double Normal() {
    const double u1 = Uniform(), u2 = Uniform();
    return std::sqrt(-2.0 * std::log(u1)) * std::cos(2.0 * 3.14159265358979323846 * u2);
  }
 private:
  uint32_t s_;
};

struct GnssFix {
  SolType type = SolType::None;
  int used = 0, visible = 0, epochs = 0;           // epochs = epoki uśrednione od ostatniej zmiany typu rozwiązania
  double pdop = 99, multipath = 0;
  double n = 0, e = 0, h = 0;                      // średnia z epok, H po odjęciu ustawionej wysokości anteny
  double hRmsMm = 0, vRmsMm = 0;
};

// Jedna sesja punktu: kolejne epoki co 1 s. Błąd = systematyczny (losowany przy zmianie typu rozwiązania, 0,5 sigma)
// + biały + pochylenie tyczki (0,3 stopnia). Uśrednianie zmniejsza szum biały, ale nie błąd systematyczny.
class GnssSession {
 public:
  GnssSession(uint32_t seed, double tStartSec, const GridPos& truth, double antennaSetM, double antennaTrueM = 2.0)
      : rng_(seed), t0_(tStartSec), truth_(truth), setM_(antennaSetM), trueM_(antennaTrueM) {
    tFloat_ = 5.0 + 7.0 * rng_.Uniform(); tFix_ = 15.0 + 25.0 * rng_.Uniform();
    fix_.n = truth.n; fix_.e = truth.e; fix_.h = truth.h;
  }
  void Step(double tSec, const SkyMask& mask) {
    const std::vector<SatDir> sats = GnssSatellites(tSec), used = GnssUsable(sats, mask);
    fix_.visible = 0; for (const SatDir& d : sats) if (d.elDeg > 10.0) fix_.visible++;
    fix_.used = (int)used.size(); fix_.pdop = GnssPdop(used); fix_.multipath = GnssMultipath(mask);
    const SolType type = GnssClassify(fix_.used, fix_.pdop, fix_.multipath, tSec - t0_, tFloat_, tFix_);
    if (type != fix_.type) {                       // nowy typ: nowe uśrednianie i nowy błąd systematyczny
      fix_.type = type; fix_.epochs = 0; cnt_ = 0;
      for (int i = 0; i < 3; i++) sum_[i] = sq_[i] = 0;
      const SolSigma g = GnssSigma(type, fix_.pdop, fix_.multipath);
      bias_[0] = 0.5 * g.hMm * rng_.Normal(); bias_[1] = 0.5 * g.hMm * rng_.Normal(); bias_[2] = 0.5 * g.vMm * rng_.Normal();
    }
    if (type == SolType::None) { fix_.hRmsMm = fix_.vRmsMm = 0; return; }
    const SolSigma g = GnssSigma(type, fix_.pdop, fix_.multipath);
    const double tilt = trueM_ * std::tan(0.3 * 3.14159265358979323846 / 180.0 * std::fabs(rng_.Normal())) * 1000.0;   // mm
    const double ang = 2.0 * 3.14159265358979323846 * rng_.Uniform();
    const double err[3] = {bias_[0] + g.hMm * rng_.Normal() + tilt * std::cos(ang), bias_[1] + g.hMm * rng_.Normal() + tilt * std::sin(ang),
                           bias_[2] + g.vMm * rng_.Normal()};
    for (int i = 0; i < 3; i++) { sum_[i] += err[i]; sq_[i] += err[i] * err[i]; }
    cnt_++; fix_.epochs = cnt_;
    fix_.n = truth_.n + sum_[0] / cnt_ / 1000.0; fix_.e = truth_.e + sum_[1] / cnt_ / 1000.0;
    fix_.h = truth_.h + (trueM_ - setM_) + sum_[2] / cnt_ / 1000.0;
    if (cnt_ >= 2) {
      double var[3];
      for (int i = 0; i < 3; i++) var[i] = (std::max)(0.0, sq_[i] / cnt_ - (sum_[i] / cnt_) * (sum_[i] / cnt_));
      fix_.hRmsMm = std::sqrt(var[0] + var[1]); fix_.vRmsMm = std::sqrt(var[2]);
    } else fix_.hRmsMm = fix_.vRmsMm = 0;
  }
  const GnssFix& Fix() const { return fix_; }
  double StartSec() const { return t0_; }
 private:
  GnssRng rng_; double t0_; GridPos truth_; double setM_, trueM_;
  double tFloat_ = 8, tFix_ = 20;
  GnssFix fix_;
  int cnt_ = 0; double sum_[3] = {}, sq_[3] = {}, bias_[3] = {};
};
