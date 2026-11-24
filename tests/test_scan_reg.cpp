#include <cassert>
#include <cmath>
#include <cstdio>
#include "calc/ScanReg.hpp"

static bool near(double a, double b, double eps) { return std::fabs(a - b) <= eps; }

static std::vector<P3> SpherePoints(P3 c, double r, int n, double noise) {            // czesc powierzchni widoczna z poczatku ukladu
  std::vector<P3> v; unsigned s = 12345;
  auto rnd = [&]() { s = s * 1664525u + 1013904223u; return (s >> 8) / 16777216.0 - 0.5; };
  const double d = std::sqrt(c.x * c.x + c.y * c.y + c.z * c.z);
  for (int i = 0; i < n; i++) {
    double a = rnd() * 2.0 * 3.14159, b = rnd() * 3.14159;                                // losowy kierunek na kuli
    P3 u{std::sin(b) * std::cos(a), std::sin(b) * std::sin(a), std::cos(b) };
    if (u.x * c.x + u.y * c.y + u.z * c.z > 0) { u.x = -u.x; u.y = -u.y; u.z = -u.z; }   // pol-sfera zwrocona do skanera
    v.push_back({c.x + r * u.x + noise * rnd(), c.y + r * u.y + noise * rnd(), c.z + r * u.z + noise * rnd()});
  }
  (void)d; return v;
}

static void sphere_fit_recovers_center() {
  const P3 c{12.0, -7.0, 1.2}; P3 got; double rms = 0;
  assert(ScanFitSphere(SpherePoints(c, kScanTargetRadiusM, 300, 0.002), kScanTargetRadiusM, 0.03, got, rms));
  assert(near(got.x, c.x, 0.005) && near(got.y, c.y, 0.005) && near(got.z, c.z, 0.005) && rms < 0.003);
  assert(!ScanFitSphere(SpherePoints(c, 0.5, 300, 0.002), kScanTargetRadiusM, 0.03, got, rms));      // zly promien
  assert(!ScanFitSphere({{0, 0, 0}, {1, 0, 0}}, kScanTargetRadiusM, 0.03, got, rms));              // za malo punktow
}

static void cluster_groups_by_distance() {
  std::vector<P3> pts = SpherePoints({10, 0, 0}, 0.145, 40, 0.0), b = SpherePoints({0, 15, 0}, 0.145, 40, 0.0), c = {{30, 30, 0}};
  pts.insert(pts.end(), b.begin(), b.end()); pts.insert(pts.end(), c.begin(), c.end());
  const auto cl = ScanClusterBright(pts, 0.3, 5);
  assert(cl.size() == 2 && cl[0].size() == 40 && cl[1].size() == 40);                              // samotny punkt odrzucony
}

static void register_recovers_pose_with_unknown_assignment() {
  const double sn = 5500012.0, se = 6500034.0, sh = 101.5, psi = 37.0 * 3.14159265358979323846 / 180.0;   // prawdziwe stanowisko skanera i obrot
  const std::vector<ScanRef> refs = {{"A", 5500050.0, 6500030.0, 101.0}, {"B", 5500020.0, 6500090.0, 102.0}, {"C", 5499970.0, 6500050.0, 100.5}, {"D", 5500005.0, 6500010.0, 101.2}};
  std::vector<P3> det;                                                                               // wykryte srodki (lokalne), w innej kolejnosci niz refs
  for (int k : {2, 0, 3}) {
    const double dn = refs[k].n - sn, de = refs[k].e - se, dz = refs[k].h - sh;
    // lokalne = prawdziwe obrocone o -psi: N_l = dn cos(psi) + de sin(psi), E_l = -dn sin(psi) + de cos(psi)
    det.push_back({-dn * std::sin(psi) + de * std::cos(psi) + 0.002, dn * std::cos(psi) + de * std::sin(psi) - 0.003, dz + 0.001});
  }
  const ScanReg r = ScanRegister(det, refs);
  assert(r.ok && near(r.n, sn, 0.01) && near(r.e, se, 0.01) && near(r.h, sh, 0.01) && near(r.rotDeg, 37.0, 0.05) && r.rmsM < 0.01);
  assert(r.ids.size() == 3 && r.ids[0] == "C" && r.ids[1] == "A" && r.ids[2] == "D");
  assert(!ScanRegister({det[0], det[1]}, refs).ok);                                                  // dwie kule: za malo
  std::vector<P3> bad = det; bad[0].x += 0.5;                                                        // jedna kula przesunieta o 50 cm
  assert(!ScanRegister(bad, refs).ok);
}

static void windows_group_bright_hits() {
  std::vector<ScanBrightHit> h = {{10.0, 80.0, 12.0}, {10.5, 80.5, 12.0}, {200.0, 90.0, 30.0}, {359.5, 85.0, 5.0}, {0.5, 85.2, 5.0}};
  const auto w = ScanTargetWindows(h);
  assert(w.size() == 3);                                                                              // trzy kule (ostatnia przez 0/360)
  for (const auto& x : w) assert(x.halfDeg > 0.75 && x.stepDeg >= 0.02 && x.stepDeg < 0.2);
}

int main() {
  sphere_fit_recovers_center(); cluster_groups_by_distance(); register_recovers_pose_with_unknown_assignment(); windows_group_bright_hits();
  std::puts("test_scan_reg OK");
  return 0;
}
