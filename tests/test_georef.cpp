#include <cassert>
#include <cmath>
#include <cstdio>
#include "calc/Georef.hpp"

static bool near(double a, double b, double eps = 1e-9) { return std::fabs(a - b) <= eps; }
static const double N0 = 5500000.0, E0 = 6500000.0, PI = 3.14159265358979323846;

static TrajSample Pose(double x, double y, double z, double r = 0, double p = 0, double yw = 0) {
  TrajSample s{}; s.t = 0; s.pos = {x, y, z}; s.rollDeg = r; s.pitchDeg = p; s.yawDeg = yw; s.strip = -1; return s;
}

static void rpy_axes() {
  Vec3 f = Mul(RotFromRpy(0, 0, 90), {0, 1, 0}); assert(near(f.x, 1, 1e-9) && near(f.y, 0, 1e-9) && near(f.z, 0, 1e-9));   // przod -> wschod
  assert(Mul(RotFromRpy(0, 10, 0), {0, 1, 0}).z > 0);                                                                       // nos w gore
  assert(Mul(RotFromRpy(10, 0, 0), {1, 0, 0}).z < 0);                                                                        // prawe skrzydlo w dol
}

static void beam_dir() {
  const Vec3 d = LineBeamDir(0); assert(near(d.x, 0) && near(d.y, 0) && near(d.z, -1));
  assert(near(LineBeamDir(30).x, 0.5, 1e-12));
}

static void mirror_sawtooth() {
  SensorSpec s; s.fovDeg = 60; s.lineHz = 50;
  assert(near(MirrorThetaDeg(0, s), -30, 1e-9));
  assert(near(MirrorThetaDeg(0.5 / 50.0, s), 0, 1e-9));
  assert(near(MirrorThetaDeg(1.0 / 50.0 + 0.25 / 50.0, s), -15, 1e-6));   // kolejna linia od poczatku
}

static void zero_error_roundtrip() {
  const TrajSample s = Pose(10, 20, 60); const Vec3 lever{0.1, 0.2, -0.3}; const Rot3 I = RotFromRpy(0, 0, 0);
  const AlsRay ray = TrueRay(s, I, lever, 0);
  const Vec3 hit{ray.origin.x + ray.dir.x * 60, ray.origin.y + ray.dir.y * 60, ray.origin.z + ray.dir.z * 60};
  const Vec3 g = GeorefPoint(s, I, lever, 0, 60);
  assert(near(hit.x, g.x) && near(hit.y, g.y) && near(hit.z, g.z));
  assert(near(g.z, -0.3, 1e-9));
}

static void boresight_roll_bias_shift() {
  const TrajSample s = Pose(0, 0, 60); const Vec3 lever{0, 0, 0}; const Rot3 I = RotFromRpy(0, 0, 0), bad = RotFromRpy(0.1, 0, 0);
  const AlsRay ray = TrueRay(s, bad, lever, 0); const double r = 60.0 / -ray.dir.z;
  const Vec3 hit{ray.origin.x + ray.dir.x * r, ray.origin.y + ray.dir.y * r, ray.origin.z + ray.dir.z * r};
  const Vec3 g = GeorefPoint(s, I, lever, 0, r);
  assert(near(hit.z, 0, 1e-9));
  assert(near(std::fabs(hit.x - g.x), 60.0 * std::tan(0.1 * PI / 180.0), 2e-3));
}

static void attitude_error_scales_with_range() {
  const TrajSample truth = Pose(0, 0, 60), est = Pose(0, 0, 60, 0, 0.02, 0); const Rot3 I = RotFromRpy(0, 0, 0);
  const Vec3 a = GeorefPoint(truth, I, {0, 0, 0}, 0, 60), b = GeorefPoint(est, I, {0, 0, 0}, 0, 60);
  assert(near(std::fabs(b.y - a.y), 60.0 * std::sin(0.02 * PI / 180.0), 1e-3));
}

static FlightPlan MakePlan(FlightParams& p) {
  p.aglM = 50; p.speedMps = 5; p.sidelapPct = 30; p.stripAzDeg = 0; p.sensor.fovDeg = 60;
  p.area = {{N0, E0}, {N0, E0 + 100}, {N0 + 100, E0 + 100}, {N0 + 100, E0}};
  return PlanFlight(p, {N0 - 10, E0 - 10});
}

static void flightpath_phases() {
  FlightParams p; const FlightPlan pl = MakePlan(p); assert(pl.ok);
  const PlanPt home{N0 - 10, E0 - 10};
  FlightPath fp(pl, [](double, double) { return 100.0; }, p.aglM, p.speedMps, false, 100.0, home);
  assert(near(fp.Duration(), pl.flightTimeS, 1.0));
  int last = -1; bool any = false;
  for (double t = 0; t <= fp.Duration(); t += 0.25) {
    const TrajSample s = fp.At(t);
    if (s.scanning) {
      any = true; assert(s.strip >= last && s.strip <= 2); last = s.strip;
      assert(near(s.pos.z, 150.0, 1e-6));
      assert(s.pos.y >= N0 - 1e-6 && s.pos.y <= N0 + 100 + 1e-6 && s.pos.x >= E0 - 1e-6 && s.pos.x <= E0 + 100 + 1e-6);
    } else assert(s.strip == -1);
  }
  assert(any && last == 2);
  const TrajSample start = fp.At(0), end = fp.At(fp.Duration());
  assert(near(start.pos.z, 100.0, 1e-6) && near(end.pos.z, 100.0, 1e-6));
  assert(near(start.pos.y, home.n, 1e-6) && near(end.pos.x, home.e, 1e-6));
}

static void follow_terrain_tracks_slope() {
  FlightParams p; const FlightPlan pl = MakePlan(p);
  const PlanPt home{N0 - 10, E0 - 10};
  FlightPath fp(pl, [](double n, double) { return 100.0 + 0.1 * (n - 5500000.0); }, p.aglM, p.speedMps, true, 100.0, home);
  for (double t = 0; t <= fp.Duration(); t += 0.5) {
    const TrajSample s = fp.At(t);
    if (s.scanning) assert(near(s.pos.z, 100.0 + 0.1 * (s.pos.y - N0) + 50.0, 1.0));   // wygladzenie: odchylka do ok. 1 m
  }
}

static void noise_deterministic_and_profile_ordering() {
  TrajectoryNoise a(7, kAlsProfiles[0]), b(7, kAlsProfiles[0]), good(11, kAlsProfiles[0]), bad(11, kAlsProfiles[2]);
  double sg = 0, sb = 0;
  for (int i = 0; i < 1000; i++) {
    TrajSample t = Pose(E0 + i * 0.5, N0, 150); t.t = i * 0.1; t.scanning = true; t.strip = 0;
    const TrajSample ea = a.Estimate(t), eb = b.Estimate(t);
    assert(ea.pos.x == eb.pos.x && ea.pos.y == eb.pos.y && ea.pos.z == eb.pos.z && ea.rollDeg == eb.rollDeg);
    assert(ea.t == t.t && ea.scanning && ea.strip == 0);
    const TrajSample g = good.Estimate(t), w = bad.Estimate(t);
    sg += std::pow(g.pos.x - t.pos.x, 2) + std::pow(g.pos.y - t.pos.y, 2) + std::pow(g.pos.z - t.pos.z, 2);
    sb += std::pow(w.pos.x - t.pos.x, 2) + std::pow(w.pos.y - t.pos.y, 2) + std::pow(w.pos.z - t.pos.z, 2);
  }
  assert(std::sqrt(sg / 1000) < std::sqrt(sb / 1000));
  const Vec3 d = Mul(bad.TrueBoresight(), {0, 0, -1});                         // odchylenie osi od nominalnej <= ok. 0,35 st.
  assert(std::hypot(d.x, d.y) <= 0.35 * PI / 180.0 + 1e-9 && d.z < 0);
}

static void drift_is_meters_per_minute() {                                      // random walk: odchylenie standardowe po 60 s = driftMPerMin
  const ErrorProfile d{"dryf", 0, 0, 0.6, 0, 0, 0}; double sum2 = 0; const int n = 300;
  for (int k = 0; k < n; k++) {
    TrajectoryNoise tn(1000 + k, d); TrajSample t = Pose(0, 0, 0), e{};
    for (int i = 0; i <= 600; i++) { t.t = i * 0.1; e = tn.Estimate(t); }
    sum2 += e.pos.x * e.pos.x;
  }
  const double rms = std::sqrt(sum2 / n); assert(rms > 0.45 && rms < 0.75);
}

int main() {
  drift_is_meters_per_minute();
  rpy_axes(); beam_dir(); mirror_sawtooth(); zero_error_roundtrip(); boresight_roll_bias_shift(); attitude_error_scales_with_range();
  flightpath_phases(); follow_terrain_tracks_slope(); noise_deterministic_and_profile_ordering();
  std::puts("test_georef OK"); return 0;
}
