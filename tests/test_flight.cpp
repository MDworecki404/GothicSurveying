#include <cassert>
#include <cmath>
#include <cstdio>
#include "calc/Flight.hpp"

static bool near(double a, double b, double eps = 1e-9) { return std::fabs(a - b) <= eps; }
static const double N0 = 5500000.0, E0 = 6500000.0;

static FlightParams Square() {
  FlightParams p; p.aglM = 50; p.speedMps = 5; p.sidelapPct = 30; p.stripAzDeg = 0; p.sensor.fovDeg = 60;
  p.area = {{N0, E0}, {N0, E0 + 100}, {N0 + 100, E0 + 100}, {N0 + 100, E0}};
  return p;
}

static bool InPoly(const std::vector<PlanPt>& poly, double n, double e, double eps = 1e-6) {
  bool in = false;
  for (size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++) {
    const PlanPt a = poly[i], b = poly[j];
    // na krawedzi (z tolerancja) liczy sie jako wewnatrz
    const double dx = b.e - a.e, dy = b.n - a.n, len2 = dx * dx + dy * dy;
    double t = len2 > 0 ? ((e - a.e) * dx + (n - a.n) * dy) / len2 : 0; t = (std::max)(0.0, (std::min)(1.0, t));
    if (std::hypot(a.e + t * dx - e, a.n + t * dy - n) <= eps) return true;
    if (((a.n > n) != (b.n > n)) && (e < (b.e - a.e) * (n - a.n) / (b.n - a.n) + a.e)) in = !in;
  }
  return in;
}

static void square_plan_values() {
  const FlightParams p = Square();
  const FlightPlan pl = PlanFlight(p, {N0 - 10, E0 - 10});
  assert(pl.ok);
  assert(near(pl.swathM, 57.735, 1e-3));
  assert(near(pl.spacingM, 40.414, 1e-3));
  assert(pl.strips.size() == 3);
  for (size_t i = 1; i < pl.strips.size(); i++) assert(near(pl.strips[i].a.e - pl.strips[i - 1].a.e, pl.spacingM, 1e-6));
  assert(pl.strips[0].b.n > pl.strips[0].a.n);       // szereg 0 ku N
  assert(pl.strips[1].b.n < pl.strips[1].a.n);       // szereg 1 ku S
  assert(pl.strips[2].b.n > pl.strips[2].a.n);
  for (const Strip& s : pl.strips) assert(near(s.lengthM, 100.0, 1e-6));
  assert(near(pl.scanTimeS, 60.0, 1e-6));
  assert(near(pl.estDensityPtsM2, p.sensor.pulseHz / (p.speedMps * pl.swathM), 1e-6));
  assert(pl.flightTimeS > pl.scanTimeS);
}

static void degenerate_area_refused() {
  FlightParams p = Square(); p.area = {{N0, E0}, {N0 + 10, E0 + 10}};
  FlightPlan pl = PlanFlight(p, {N0, E0}); assert(!pl.ok && !pl.msg.empty() && pl.strips.empty());
  p.area = {{N0, E0}, {N0 + 10, E0 + 10}, {N0 + 20, E0 + 20}};
  pl = PlanFlight(p, {N0, E0}); assert(!pl.ok && !pl.msg.empty() && pl.strips.empty());
}

static void concave_u_shape_splits() {
  FlightParams p = Square(); p.stripAzDeg = 90;   // szeregi wzdluz E przecinaja oba ramiona litery U
  p.area = {{N0, E0}, {N0 + 100, E0}, {N0 + 100, E0 + 30}, {N0 + 30, E0 + 30}, {N0 + 30, E0 + 70}, {N0 + 100, E0 + 70}, {N0 + 100, E0 + 100}, {N0, E0 + 100}};
  const FlightPlan pl = PlanFlight(p, {N0, E0});
  assert(pl.ok && !pl.strips.empty());
  double sum = 0;
  for (const Strip& s : pl.strips) {
    assert(InPoly(p.area, s.a.n, s.a.e) && InPoly(p.area, s.b.n, s.b.e));
    assert(InPoly(p.area, (s.a.n + s.b.n) / 2, (s.a.e + s.b.e) / 2));
    sum += s.lengthM;
  }
  assert(sum > 0);
  assert(pl.strips.size() > PlanFlight(Square(), {N0, E0}).strips.size());   // linia przez wciecie dzieli sie na dwa odcinki
}

static void battery_swaps() {                                                  // bateria wyczerpana: powrot, wymiana i dalszy lot, plan zawsze wykonalny
  FlightParams p = Square(); p.batteryMin = 25;
  const FlightPlan one = PlanFlight(p, {N0, E0}); assert(one.ok && one.sorties == 1);
  for (const Strip& s : one.strips) assert(s.sortie == 0);
  p.batteryMin = 1;                                                              // 45 s uzytecznych: kazdy 100-metrowy szereg (20 s) to osobny lot
  const FlightPlan many = PlanFlight(p, {N0, E0});
  assert(many.ok && many.sorties == 3 && many.msg.find("wymian") != std::string::npos);
  for (size_t i = 0; i < many.strips.size(); i++) assert(many.strips[i].sortie == (int)i);
  assert(many.flightTimeS > one.flightTimeS + 2 * kSwapS);                      // dodatkowy wznios, dojazd, ladowanie i wymiana
  p.batteryMin = 0.01;                                                           // szereg dluzszy niz bateria: lot mimo to, z ostrzezeniem
  const FlightPlan tiny = PlanFlight(p, {N0, E0}); assert(tiny.ok && tiny.sorties == 3 && tiny.msg.find("dluzszy") != std::string::npos);
}

static void takeoff_is_quick() {                                               // start: 50 m AGL w ok. 6 s, zakret 2 s (wczesniej 17 s i 5 s)
  assert(kClimbMps >= 8.0 && kTransitMps >= 12.0 && kTurnS <= 2.0 && kSwapS <= 10.0);
  FlightParams p = Square(); const FlightPlan pl = PlanFlight(p, {N0, E0});
  assert(pl.flightTimeS < 150.0);
}

static void cross_strips_perpendicular() {                                      // szeregi kalibracyjne: prostopadle do glownych, wewnatrz obszaru
  FlightParams p = Square(); const FlightPlan base = PlanFlight(p, {N0, E0});
  assert(base.crossCount == 0 && base.strips.size() == 3);                       // domyslnie wylaczone
  p.crossStrips = 3; const FlightPlan pl = PlanFlight(p, {N0, E0});
  assert(pl.ok && pl.crossCount == 3 && pl.strips.size() == 6 && pl.flightTimeS > base.flightTimeS);
  auto dir = [](const Strip& s) { const double d = std::hypot(s.b.n - s.a.n, s.b.e - s.a.e); return PlanPt{(s.b.n - s.a.n) / d, (s.b.e - s.a.e) / d}; };
  const PlanPt d0 = dir(pl.strips[0]);
  for (size_t k = 3; k < 6; k++) {
    const PlanPt d = dir(pl.strips[k]); assert(near(d0.n * d.n + d0.e * d.e, 0.0, 1e-9));
    assert(InPoly(p.area, pl.strips[k].a.n, pl.strips[k].a.e) && InPoly(p.area, pl.strips[k].b.n, pl.strips[k].b.e) && near(pl.strips[k].lengthM, 100.0, 1e-6));
    assert(pl.strips[k].id == (int)k);
  }
  assert(near(pl.strips[3].a.n, N0 + 100.0 - 100.0 / 6.0, 1e-6) && near(pl.strips[4].a.n, N0 + 50.0, 1e-6) && near(pl.strips[5].a.n, N0 + 100.0 / 6.0, 1e-6));   // rownomiernie w poprzek (od polnocy ku poludniu dla azymutu 90)
  for (int k : {3, 4}) assert(pl.strips[k].a.e != pl.strips[k + 1].a.e);          // kierunki naprzemienne
}

static void cross_strips_in_concave_area() {
  FlightParams p = Square(); p.stripAzDeg = 90; p.crossStrips = 3;
  p.area = {{N0, E0}, {N0 + 100, E0}, {N0 + 100, E0 + 30}, {N0 + 30, E0 + 30}, {N0 + 30, E0 + 70}, {N0 + 100, E0 + 70}, {N0 + 100, E0 + 100}, {N0, E0 + 100}};
  const FlightPlan pl = PlanFlight(p, {N0, E0}); assert(pl.ok && pl.crossCount >= 3);
  for (const Strip& s : pl.strips) assert(InPoly(p.area, (s.a.n + s.b.n) / 2, (s.a.e + s.b.e) / 2) && InPoly(p.area, s.a.n, s.a.e));
}

static void csv_header_and_rows() {
  const FlightParams p = Square(); const FlightPlan pl = PlanFlight(p, {N0, E0});
  const std::string csv = PlanCsv(pl, p);
  assert(csv.rfind("szereg;N_a;E_a;N_b;E_b;dlugosc_m", 0) == 0);
  size_t lines = 0; for (char c : csv) if (c == '\n') lines++;
  assert(lines == 1 + pl.strips.size());
}

int main() {
  square_plan_values(); degenerate_area_refused(); concave_u_shape_splits(); battery_swaps(); takeoff_is_quick(); cross_strips_perpendicular(); cross_strips_in_concave_area(); csv_header_and_rows();
  std::puts("test_flight OK"); return 0;
}
