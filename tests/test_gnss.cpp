#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include "calc/Gnss.hpp"

static bool sameSats(const std::vector<SatDir>& a, const std::vector<SatDir>& b) {
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); i++) if (a[i].azDeg != b[i].azDeg || a[i].elDeg != b[i].elDeg) return false;
  return true;
}

static void pdop_matches_analytic_geometry() {
  std::vector<SatDir> s = {{0, 90}, {0, 30}, {120, 30}, {240, 30}};
  assert(std::fabs(GnssPdop(s) - 8.0 / 3.0) < 1e-9);
  s.pop_back();
  assert(GnssPdop(s) == 99.0);
}

static void constellation_is_deterministic_and_open_sky_is_good() {
  assert(sameSats(GnssSatellites(0), GnssSatellites(0)));
  auto s0 = GnssSatellites(0);
  assert(s0.size() == 30);
  for (const SatDir& d : s0) assert(d.elDeg >= 0 && d.elDeg <= 90);
  assert(!sameSats(s0, GnssSatellites(600)));
  for (double t : {0.0, 1000.0, 5000.0}) {
    auto used = GnssUsable(GnssSatellites(t), SkyMask{});
    assert(used.size() >= 8);
    assert(GnssPdop(used) < 2.5);
  }
}

static void mask_removes_exactly_blocked_satellites() {
  SkyMask m; m.horizonDeg[0] = 60;
  auto sats = GnssSatellites(300);
  size_t expected = 0;
  for (const SatDir& d : sats) {
    int sector = (int)std::floor(d.azDeg / 10.0) % 36;
    double lim = std::fmax(10.0, m.horizonDeg[sector]);
    if (d.elDeg > lim) expected++;
  }
  assert(GnssUsable(sats, m).size() == expected);
  assert(GnssUsable(sats, m).size() < GnssUsable(sats, SkyMask{}).size() || expected == GnssUsable(sats, SkyMask{}).size());
}

static void multipath_scales_with_obstacle_distance() {
  SkyMask m;
  m.nearestObstacleM = 5;   assert(GnssMultipath(m) == 0.0);
  m.nearestObstacleM = 100; assert(GnssMultipath(m) == 0.0);
  m.nearestObstacleM = 0;   assert(GnssMultipath(m) == 1.0);
  m.nearestObstacleM = 2.5; assert(std::fabs(GnssMultipath(m) - 0.5) < 1e-12);
}

static void classify_table() {
  const double tf = 8, tx = 20;
  assert(GnssClassify(3, 1, 0, 100, tf, tx) == SolType::None);
  assert(GnssClassify(6, 1.2, 0.1, 25, tf, tx) == SolType::Fix);
  assert(GnssClassify(6, 1.2, 0.1, 10, tf, tx) == SolType::Float);
  assert(GnssClassify(6, 1.2, 0.1, 3, tf, tx) == SolType::Dgps);
  assert(GnssClassify(6, 2.8, 0.1, 25, tf, tx) == SolType::Float);
  assert(GnssClassify(6, 1.2, 0.5, 25, tf, tx) == SolType::Float);
  assert(GnssClassify(6, 1.2, 0.7, 25, tf, tx) == SolType::Dgps);
  assert(GnssClassify(5, 3.8, 0, 100, tf, tx) == SolType::Dgps);
  assert(GnssClassify(4, 6, 0, 100, tf, tx) == SolType::Autonomous);
  assert(std::string(GnssSolName(SolType::Fix)) == "fix RTK" && std::string(GnssSolName(SolType::None)) == "brak");
}

static const GridPos TRUTH{5500100.0, 6500200.0, 12.5};
static double meanErrAfter(uint32_t seed, double antennaSet, const SkyMask& m, double* eN, double* eE, double* eH, SolType* type) {
  GnssSession s(seed, 0.0, TRUTH, antennaSet);
  for (int t = 1; t <= 245; t++) s.Step(t, m);
  *eN = s.Fix().n - TRUTH.n; *eE = s.Fix().e - TRUTH.e; *eH = s.Fix().h - TRUTH.h; *type = s.Fix().type;
  return s.Fix().pdop;
}

static void grid_roundtrip() {
  GridPos g = GnssToGrid(1234.5, -678.9, 4321.0);
  double x, y, z; GnssFromGrid(g, x, y, z);
  assert(std::fabs(x - 1234.5) < 1e-6 && std::fabs(y + 678.9) < 1e-6 && std::fabs(z - 4321.0) < 1e-6);
  GridPos o = GnssToGrid(0, 0, 0);
  assert(o.n == 5500000.0 && o.e == 6500000.0 && o.h == 0.0);
  GridPos p = GnssToGrid(100, 200, 300);
  assert(p.n == 5500003.0 && p.e == 6500001.0 && p.h == 2.0);
}

static void sigma_ordering() {
  SolSigma f = GnssSigma(SolType::Fix, 2, 0);
  assert(f.hMm == 8.0 && f.vMm == 15.0);
  assert(GnssSigma(SolType::Float, 2, 0).hMm > f.hMm);
  assert(GnssSigma(SolType::Dgps, 2, 0).hMm > GnssSigma(SolType::Float, 2, 0).hMm);
  assert(GnssSigma(SolType::Autonomous, 2, 0).hMm > GnssSigma(SolType::Dgps, 2, 0).hMm);
  assert(GnssSigma(SolType::Fix, 3, 0).hMm > f.hMm && GnssSigma(SolType::Fix, 2, 0.5).hMm > f.hMm);
  assert(GnssSigma(SolType::None, 2, 0).hMm == 0.0 && GnssSigma(SolType::None, 2, 0).vMm == 0.0);
}

static void rng_is_deterministic() {
  GnssRng a(7), b(7);
  for (int i = 0; i < 100; i++) assert(a.Normal() == b.Normal());
  GnssRng r(11); double sum = 0, sq = 0; const int n = 20000;
  for (int i = 0; i < n; i++) { double v = r.Normal(); sum += v; sq += v * v; }
  double mean = sum / n, sd = std::sqrt(sq / n - mean * mean);
  assert(std::fabs(mean) < 0.03 && sd > 0.97 && sd < 1.03);
}

static void session_reaches_fix_in_open_sky() {
  GnssSession s(1, 0.0, TRUTH, 2.0);
  for (int t = 1; t <= 60; t++) {
    s.Step(t, SkyMask{});
    if (t == 3) assert(s.Fix().type == SolType::Dgps || s.Fix().type == SolType::Autonomous);
    if (t == 45) assert(s.Fix().type == SolType::Fix);
  }
  assert(s.Fix().epochs >= 5);
}

static void fix_error_is_centimeters_but_bias_survives_averaging() {
  double sum = 0, sq = 0; const int n = 40;
  for (int seed = 1; seed <= n; seed++) {
    double eN, eE, eH; SolType t;
    meanErrAfter(seed, 2.0, SkyMask{}, &eN, &eE, &eH, &t);
    assert(t == SolType::Fix);
    assert(std::fabs(eN) < 0.05 && std::fabs(eE) < 0.05);
    sum += eN; sq += eN * eN;
  }
  double mean = sum / n, sd = std::sqrt(sq / n - mean * mean);
  assert(sd > 0.001 && sd < 0.02);                       // blad systematyczny widoczny, nie znika przy usrednianiu
}

static void antenna_mismatch_biases_height() {
  double eN, eE, eH; SolType t;
  meanErrAfter(3, 1.8, SkyMask{}, &eN, &eE, &eH, &t);
  assert(t == SolType::Fix && std::fabs(eH - 0.2) < 0.03);
}

static void blocked_sky_gives_no_solution() {
  SkyMask m; for (double& h : m.horizonDeg) h = 90;
  GnssSession s(5, 0.0, TRUTH, 2.0);
  for (int t = 1; t <= 30; t++) s.Step(t, m);
  const GnssFix& f = s.Fix();
  assert(f.type == SolType::None && f.used < 4 && f.pdop == 99.0);
  assert(std::isfinite(f.n) && std::isfinite(f.e) && std::isfinite(f.h));
}

int main() {
  pdop_matches_analytic_geometry(); constellation_is_deterministic_and_open_sky_is_good();
  mask_removes_exactly_blocked_satellites(); multipath_scales_with_obstacle_distance(); classify_table();
  grid_roundtrip(); sigma_ordering(); rng_is_deterministic(); session_reaches_fix_in_open_sky();
  fix_error_is_centimeters_but_bias_survives_averaging(); antenna_mismatch_biases_height(); blocked_sky_gives_no_solution();
  std::puts("test_gnss OK");
  return 0;
}
