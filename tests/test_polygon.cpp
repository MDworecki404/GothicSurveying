#include <cassert>
#include <cmath>
#include <cstdio>
#include "calc/Polygon.hpp"

static bool near_(double a, double b, double eps) { return std::fabs(a - b) <= eps; }

static PolyInput Guide() {                                       // przyklad z przewodnika (dane przykladowe)
  PolyInput in; in.kind = TraverseKind::Anchored;
  in.nN1 = 1000; in.eN1 = 1000; in.nA = 1100; in.eA = 1200; in.nB = 1460; in.eB = 1560; in.nN2 = 1600; in.eN2 = 1640;
  in.betaGon = {184.8109, 203.7398, 172.4127, 201.6052}; in.sideM = {170.306, 199.991, 147.662};
  return in;
}

static void azimuth_gon_quadrants() {
  assert(near_(AzimuthGon(0, 0, 1, 0), 0, 1e-9) && near_(AzimuthGon(0, 0, 0, 1), 100, 1e-9) && near_(AzimuthGon(0, 0, -1, 0), 200, 1e-9) && near_(AzimuthGon(0, 0, 0, -1), 300, 1e-9));
  assert(near_(AzimuthGon(1000, 1000, 1100, 1200), 70.4833, 1e-4));
}

static void angle_mean_removes_collimation() {
  double twoC = 0;                                                // kolimacja 0.1g na tyle i 0.2g na przodzie (inne V): KL +c, KP +200 - c
  assert(near_(PolyAngleGon(10.1, 110.2, 209.9, 309.8, &twoC), 100.0, 1e-9) && near_(twoC, -0.2, 1e-9));
  assert(near_(PolyAngleGon(399.9, 0.1, 199.9, 200.1), 0.2, 1e-9));   // zawiniecie przez 0g/400g
}

static void guide_example_anchored() {
  const PolyResult r = PolygonCompute(Guide());
  assert(r.valid && r.n == 4 && r.kind == TraverseKind::Anchored);
  assert(near_(r.aStartGon, 70.4833, 1e-4) && near_(r.aEndGon, 33.0499, 1e-4));
  assert(near_(r.fBetaGon, 0.0020, 2e-4) && near_(r.vBetaGon, -0.0005, 1e-4));
  assert(r.azimuthGon.size() == 3 && near_(r.azimuthGon[0], 55.2937, 3e-4) && near_(r.azimuthGon[1], 59.0330, 3e-4) && near_(r.azimuthGon[2], 31.4452, 3e-4));
  assert(near_(r.fx, 0.014, 0.002) && near_(r.fy, 0.009, 0.002) && near_(r.fd, 0.017, 0.002));
  assert(r.x.size() == 4 && near_(r.x[3], 1460.000, 1e-6) && near_(r.y[3], 1560.000, 1e-6));   // poprawki domykaja ciag
  assert(near_(r.x[1], 1210.001, 0.003) && near_(r.y[1], 1330.008, 0.003) && near_(r.x[2], 1329.992, 0.003) && near_(r.y[2], 1489.997, 0.003));
  assert(r.hasAngleCheck && r.hasLinearCheck && r.angleOk && r.linearOk);
  assert(near_(r.fBetaLimGon, 0.00617, 1e-5) && r.fdLim > r.fd);
}

static void anchored_bad_angle_detected() {
  PolyInput in = Guide(); in.betaGon[1] += 0.05;
  const PolyResult r = PolygonCompute(in);
  assert(r.valid && !r.angleOk && r.fBetaGon > r.fBetaLimGon);
}

static void angle_residual_wraps() {                              // f_beta surowe = +400.002g: ma wyjsc +0.002g, nie 400
  PolyInput in = Guide(); in.betaGon = {300, 300, 300, 262.5686}; in.sideM = {100, 100, 100};
  const PolyResult r = PolygonCompute(in);
  assert(r.valid && near_(r.fBetaGon, 0.0020, 2e-4));
  in.betaGon = {300, 300, 300, 262.5686 - 400.0 + 800.0};          // betas spoza 0..400 sa normalizowane (to samo co 462.5686 -> 62.5686)
  assert(PolygonCompute(in).valid);
}

static void hanging_has_no_checks() {
  PolyInput in; in.kind = TraverseKind::Open; in.nN1 = 1000; in.eN1 = 1000; in.nA = 1100; in.eA = 1200;
  in.betaGon = {184.8109, 203.7398}; in.sideM = {170.306, 199.991};
  const PolyResult r = PolygonCompute(in);
  assert(r.valid && !r.hasAngleCheck && !r.hasLinearCheck && r.x.size() == 3);
  assert(near_(r.x[1], 1210.005, 1e-3) && near_(r.y[1], 1330.011, 1e-3) && near_(r.x[2], 1329.999, 1e-3) && near_(r.y[2], 1490.005, 1e-3));
}

static PolyInput Square(bool clockwise) {
  PolyInput in; in.kind = TraverseKind::Closed; in.nA = 1000; in.eA = 1000; in.sideM = {100, 100, 100, 100};
  if (clockwise) { in.nN1 = 1000; in.eN1 = 900; in.orientBetaGon = 100; in.betaGon = {300, 300, 300, 300}; }     // A->(1100,1000)->(1100,1100)->(1000,1100)
  else           { in.nN1 = 900; in.eN1 = 1000; in.orientBetaGon = 300; in.betaGon = {100, 100, 100, 100}; }     // A->(1000,1100)->(1100,1100)->(1100,1000)
  return in;
}

static void closed_square_both_directions() {
  for (bool cw : {true, false}) {
    const PolyResult r = PolygonCompute(Square(cw));
    assert(r.valid && r.clockwise == cw && near_(r.fBetaGon, 0, 1e-9) && r.hasAngleCheck && r.hasLinearCheck && r.angleOk && r.linearOk);
    assert(r.x.size() == 5 && near_(r.x[4], 1000, 1e-6) && near_(r.y[4], 1000, 1e-6) && near_(r.fx, 0, 1e-6) && near_(r.fy, 0, 1e-6));
    assert(near_(cw ? r.x[1] : r.y[1], cw ? 1100 : 1100, 1e-6));
  }
  PolyInput in = Square(true); in.betaGon[1] += 0.01;
  const PolyResult r = PolygonCompute(in);
  assert(r.valid && near_(r.fBetaGon, 0.01, 1e-9) && near_(r.vBetaGon, -0.0025, 1e-9));
}

static void invalid_inputs() {
  PolyInput a = Guide(); a.betaGon = {184.8109}; a.sideM.clear();
  PolyInput b = Square(true); b.betaGon = {300, 300};
  PolyInput c = Guide(); c.sideM = {170.306, 199.991};
  PolyInput d = Guide(); d.sideM[1] = 0;
  PolyInput e = Guide(); e.kind = TraverseKind::Open; e.betaGon.clear(); e.sideM.clear();
  for (const PolyInput* in : {&a, &b, &c, &d, &e}) { const PolyResult r = PolygonCompute(*in); assert(!r.valid && !r.error.empty()); }
}

int main() {
  azimuth_gon_quadrants(); angle_mean_removes_collimation(); guide_example_anchored(); anchored_bad_angle_detected(); angle_residual_wraps();
  hanging_has_no_checks(); closed_square_both_directions(); invalid_inputs();
  std::puts("test_polygon OK");
  return 0;
}
