#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>
#include "calc/Traverse.hpp"

static bool near(double a, double b, double eps = 1e-6) { return std::fabs(a - b) <= eps; }

static std::vector<Stand> S3() {
  return { {1500, 1200, 3000, 3000}, {1400, 1650, 3000, 3000}, {1300, 1100, 3000, 3000} };
}

static void anchored_example() {
  TraverseResult r = ComputeTraverse(TraverseKind::Anchored, S3(), 100.000, 100.240);
  assert(r.dhMm.size() == 3 && near(r.dhMm[0], 300) && near(r.dhMm[1], -250) && near(r.dhMm[2], 200));
  assert(near(r.sumBackMm, 4200) && near(r.sumForeMm, 3950) && near(r.sumDhMm, 250));
  assert(r.hasClosure && near(r.fhMm, 10) && near(r.lengthM, 180));
  assert(near(r.fhDopMm, 8.485, 0.001) && !r.withinTolerance);
  for (double v : r.vMm) assert(near(v, -10.0 / 3, 1e-4));
  assert(near(r.dhAdjMm[0], 296.667, 1e-3) && near(r.dhAdjMm[1], -253.333, 1e-3) && near(r.dhAdjMm[2], 196.667, 1e-3));
  const double adj[] = {100.000, 100.296667, 100.043333, 100.240};
  const double raw[] = {100.000, 100.300, 100.050, 100.250};
  assert(r.heightAdjM.size() == 4 && r.heightRawM.size() == 4);
  for (int i = 0; i < 4; i++) { assert(near(r.heightAdjM[i], adj[i], 1e-6)); assert(near(r.heightRawM[i], raw[i], 1e-6)); }
}

static void closed_example() {
  std::vector<Stand> s = S3();
  s[2].foreMm = 1340;                                         // dh3 = -40, suma = 10
  TraverseResult r = ComputeTraverse(TraverseKind::Closed, s, 100.000, 0.0);  // hEnd ignorowane
  assert(r.hasClosure && near(r.fhMm, 10));
  assert(near(r.heightAdjM.back(), 100.000, 1e-6));
}

static void open_example() {
  TraverseResult r = ComputeTraverse(TraverseKind::Open, S3(), 100.000, 0.0);
  assert(!r.hasClosure && near(r.fhMm, 0) && near(r.fhDopMm, 0) && r.withinTolerance);
  assert(r.heightAdjM == r.heightRawM);
  for (double v : r.vMm) assert(near(v, 0));
}

static void zero_length_has_no_tolerance() {
  std::vector<Stand> s = { {1500, 1200, 0, 0} };
  TraverseResult bad = ComputeTraverse(TraverseKind::Anchored, s, 100.000, 100.290);   // dh=300, expected 290 -> fh=10
  assert(near(bad.lengthM, 0) && near(bad.fhDopMm, 0) && !bad.withinTolerance);
  TraverseResult ok = ComputeTraverse(TraverseKind::Anchored, s, 100.000, 100.300);    // fh=0
  assert(near(ok.fhMm, 0) && ok.withinTolerance);
}

static void empty_stands_returns_empty_result() {
  TraverseResult r = ComputeTraverse(TraverseKind::Anchored, {}, 100.000, 100.240);
  assert(r.dhMm.empty() && r.vMm.empty() && r.dhAdjMm.empty());
  assert(r.heightAdjM.size() == 1 && near(r.heightAdjM[0], 100.000));
  assert(near(r.fhMm, 0));
}

static void correction_is_proportional_to_sight_length() {
  std::vector<Stand> s = { {1500, 1000, 1000, 1000}, {1500, 1000, 3000, 3000} };      // dh = 500 + 500, celowe 20 m i 60 m
  TraverseResult r = ComputeTraverse(TraverseKind::Anchored, s, 100.000, 100.980);    // fh = +20 mm
  assert(std::fabs(r.fhMm - 20.0) < 1e-9);
  assert(std::fabs(r.vMm[0] - (-5.0)) < 1e-9 && std::fabs(r.vMm[1] - (-15.0)) < 1e-9);  // 1/4 i 3/4 odchylki, z minusem
  assert(std::fabs(r.vMm[0] + r.vMm[1] + r.fhMm) < 1e-9 && std::fabs(r.heightAdjM.back() - 100.980) < 1e-9);
}

int main() {
  correction_is_proportional_to_sight_length(); anchored_example(); closed_example(); open_example();
  zero_length_has_no_tolerance(); empty_stands_returns_empty_result();
  std::puts("test_traverse OK");
  return 0;
}
