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

int main() {
  pdop_matches_analytic_geometry(); constellation_is_deterministic_and_open_sky_is_good();
  mask_removes_exactly_blocked_satellites(); multipath_scales_with_obstacle_distance(); classify_table();
  std::puts("test_gnss OK");
  return 0;
}
