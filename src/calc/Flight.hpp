// Plan nalotu ALS (bez Gothic API): szeregi z obszaru, zakladka, czas lotu, bateria, gestosc. Sensor opisany struktura (pod fotogrametrie).
// Uklad: N, E w metrach siatki; azymut szeregow 0 = wzdluz N, rosnie ku wschodowi.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

struct PlanPt { double n, e; };
struct SensorSpec { double fovDeg = 60, pulseHz = 5000, lineHz = 50; };
struct AlsPreset { const char* name; double pulseHz, lineHz; };
inline const AlsPreset kAlsPresets[4] = {
  {"2 kHz (20 linii/s)", 2000, 20}, {"5 kHz (50 linii/s)", 5000, 50}, {"10 kHz (80 linii/s)", 10000, 80}, {"20 kHz (100 linii/s)", 20000, 100},
};

struct FlightParams {
  std::vector<PlanPt> area;
  double stripAzDeg = 0, aglM = 50, speedMps = 5, sidelapPct = 30, batteryMin = 25, reservePct = 25;
  bool followTerrain = true;
  SensorSpec sensor;
};
struct Strip { int id; PlanPt a, b; double lengthM; };            // a->b = kierunek lotu
struct FlightPlan {
  std::vector<Strip> strips;
  double swathM = 0, spacingM = 0, scanTimeS = 0, flightTimeS = 0, estDensityPtsM2 = 0;
  bool ok = false, batteryOk = false; std::string msg;
};

inline FlightPlan PlanFlight(const FlightParams& p, PlanPt home) {
  FlightPlan pl; const double pi = 3.14159265358979323846;
  if (p.area.size() < 3) { pl.msg = "obszar wymaga co najmniej 3 wierzcholkow"; return pl; }
  const double az = p.stripAzDeg * pi / 180.0, ca = std::cos(az), sa = std::sin(az), n0 = p.area[0].n, e0 = p.area[0].e;
  struct UV { double u, v; };
  std::vector<UV> q;                                              // u wzdluz szeregow, v w poprzek (v = wschod dla azymutu 0)
  for (const PlanPt& a : p.area) q.push_back({(a.n - n0) * ca + (a.e - e0) * sa, -(a.n - n0) * sa + (a.e - e0) * ca});
  double area2 = 0, vmin = q[0].v, vmax = q[0].v;
  for (size_t i = 0, j = q.size() - 1; i < q.size(); j = i++) { area2 += q[j].u * q[i].v - q[i].u * q[j].v; vmin = (std::min)(vmin, q[i].v); vmax = (std::max)(vmax, q[i].v); }
  if (std::fabs(area2) / 2.0 < 1.0) { pl.msg = "pole obszaru zbyt male (mniej niz 1 m2)"; return pl; }
  pl.swathM = 2.0 * p.aglM * std::tan(p.sensor.fovDeg * pi / 360.0);
  pl.spacingM = pl.swathM * (1.0 - p.sidelapPct / 100.0);
  if (pl.spacingM < 0.5) { pl.msg = "odstep szeregow zbyt maly (zakladka lub AGL)"; return pl; }
  const double width = vmax - vmin;
  const int nLines = (std::max)(1, (int)std::ceil(width / pl.spacingM - 1e-9));
  const double v0 = vmin + (width - (nLines - 1) * pl.spacingM) / 2.0;
  for (int li = 0; li < nLines; li++) {
    const double c = v0 + li * pl.spacingM; std::vector<double> xs;
    for (size_t i = 0, j = q.size() - 1; i < q.size(); j = i++)    // polotwarta regula: wierzcholek na linii liczy sie raz
      if ((q[j].v <= c) != (q[i].v <= c)) xs.push_back(q[j].u + (c - q[j].v) / (q[i].v - q[j].v) * (q[i].u - q[j].u));
    std::sort(xs.begin(), xs.end());
    std::vector<Strip> row;
    for (size_t k = 0; k + 1 < xs.size(); k += 2) {
      if (xs[k + 1] - xs[k] < 1e-6) continue;
      const double u1 = xs[k], u2 = xs[k + 1];
      const PlanPt a{n0 + u1 * ca - c * sa, e0 + u1 * sa + c * ca}, b{n0 + u2 * ca - c * sa, e0 + u2 * sa + c * ca};
      row.push_back({0, a, b, u2 - u1});
    }
    if (li % 2 == 1) { std::reverse(row.begin(), row.end()); for (Strip& s : row) std::swap(s.a, s.b); }   // naprzemienny kierunek
    for (Strip& s : row) { s.id = (int)pl.strips.size(); pl.strips.push_back(s); }
  }
  if (pl.strips.empty()) { pl.msg = "brak szeregow w obszarze"; return pl; }
  auto dist = [](PlanPt a, PlanPt b) { return std::hypot(a.n - b.n, a.e - b.e); };
  double t = p.aglM / 3.0; PlanPt cur = home;            // wznios 3 m/s
  for (const Strip& s : pl.strips) {
    t += dist(cur, s.a) / p.speedMps + s.lengthM / p.speedMps + 5.0; pl.scanTimeS += s.lengthM / p.speedMps; cur = s.b;
  }
  t += dist(cur, home) / p.speedMps + p.aglM / 3.0;                // powrot i ladowanie
  pl.flightTimeS = t;
  pl.estDensityPtsM2 = p.sensor.pulseHz / (p.speedMps * pl.swathM);
  const double availS = p.batteryMin * 60.0 * (1.0 - p.reservePct / 100.0);
  pl.batteryOk = pl.flightTimeS <= availS; pl.ok = true;
  if (!pl.batteryOk) { char b[120]; std::snprintf(b, sizeof b, "bateria za mala: lot %.1f min, dostepne %.1f min", pl.flightTimeS / 60.0, availS / 60.0); pl.msg = b; }
  return pl;
}

inline std::string PlanCsv(const FlightPlan& plan, const FlightParams&) {
  std::string s = "szereg;N_a;E_a;N_b;E_b;dlugosc_m\n"; char b[160];
  for (const Strip& t : plan.strips) { std::snprintf(b, sizeof b, "%d;%.3f;%.3f;%.3f;%.3f;%.3f\n", t.id, t.a.n, t.a.e, t.b.n, t.b.e, t.lengthM); s += b; }
  return s;
}
