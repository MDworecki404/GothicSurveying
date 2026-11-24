#include <cassert>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include "calc/AlsAdjust.hpp"

static bool near(double a, double b, double eps = 1e-9) { return std::fabs(a - b) <= eps; }
static const double N0 = 5500000.0, E0 = 6500000.0, PI = 3.14159265358979323846;
using Terrain = std::function<double(double, double)>;            // H(E, N)

static Terrain Hills() { return [](double e, double n) { return 100.0 + 0.06 * (e - E0) + 0.04 * (n - N0) + 4.0 * std::sin((e - E0) / 23.0) * std::cos((n - N0) / 31.0); }; }
static Terrain Flat() { return [](double, double) { return 100.0; }; }

// Przeciecie promienia z terenem: marsz co 0,5 m i bisekcja.
static bool Hit(const Terrain& f, const Vec3& o, const Vec3& d, double& range) {
  auto above = [&](double t) { return o.z + t * d.z - f(o.x + t * d.x, o.y + t * d.y) > 0; };
  if (!above(0)) return false;
  double t0 = 0; for (double t = 0.5; t < 250; t += 0.5) { if (above(t)) { t0 = t; continue; } double lo = t0, hi = t; for (int i = 0; i < 40; i++) { const double m = (lo + hi) / 2; (above(m) ? lo : hi) = m; } range = (lo + hi) / 2; return true; }
  return false;
}

struct Scene { std::string raw, csv; double originN, originE, originH; Rot3 trueBore; double boreDeg[3]; long long points; };

// Lot nad terenem: trajektoria z FlightPath (wobble 1 st.), tylko boresight jako blad, pulsy 1 kHz; zapis rekordow 34 B i trajektorii CSV (format jak w AerialJob).
static Scene MakeScene(const std::string& name, const Terrain& terrain, double boreDeg, int cross, double side = 120.0, int skipStripsAfter = 99, std::vector<Vec3> stripOff = {}) {
  FlightParams p; p.aglM = 50; p.speedMps = 5; p.sidelapPct = 30; p.crossStrips = cross; p.sensor.fovDeg = 60; p.sensor.pulseHz = 1000; p.sensor.lineHz = 25;
  p.area = {{N0, E0}, {N0, E0 + side}, {N0 + side, E0 + side}, {N0 + side, E0}};
  const PlanPt home{N0 - 10, E0 - 10}; const FlightPlan pl = PlanFlight(p, home);
  FlightPath fp(pl, [&](double n, double e) { return terrain(e, n); }, p.aglM, p.speedMps, true, terrain(home.e, home.n), home);
  const ErrorProfile prof{"test", 0, 0, 0, 0, 0, boreDeg}; TrajectoryNoise tn(5, prof);
  Scene sc; sc.raw = (std::filesystem::temp_directory_path() / (name + ".bin")).string(); sc.csv = (std::filesystem::temp_directory_path() / (name + ".csv")).string();
  sc.originN = N0; sc.originE = E0; sc.originH = terrain(E0, N0); sc.trueBore = tn.TrueBoresight(); tn.BoresightDeg(sc.boreDeg[0], sc.boreDeg[1], sc.boreDeg[2]); sc.points = 0;
  AlsRawWriter w; w.Open(sc.raw); std::ofstream csv(sc.csv);
  csv << "t;N_prawdziwe;E_prawdziwe;H_prawdziwe;N_gnss;E_gnss;H_gnss;roll;pitch;yaw;roll_imu;pitch_imu;yaw_imu;szereg\n";
  const Vec3 lever{0.0, 0.15, -0.30}; const Rot3 I = RotFromRpy(0, 0, 0); char b[300];
  for (double t = 0; t <= fp.Duration(); t += 0.1) {
    const TrajSample s = fp.At(t);
    Vec3 off{0, 0, 0}; if (s.scanning && s.strip < (int)stripOff.size()) off = stripOff[s.strip];
    std::snprintf(b, sizeof b, "%.2f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%d\n", t, s.pos.y, s.pos.x, s.pos.z, s.pos.y + off.y, s.pos.x + off.x, s.pos.z + off.z, s.rollDeg, s.pitchDeg, s.yawDeg, s.rollDeg, s.pitchDeg, s.yawDeg, s.scanning ? s.strip : -1);
    csv << b;
  }
  auto lerpAng = [](double a, double b, double f) { double d = std::fmod(b - a, 360.0); if (d > 180) d -= 360; if (d < -180) d += 360; return a + d * f; };
  auto lerp = [&](const TrajSample& a, const TrajSample& b, double f) {                 // jak AerialJob: pozycja i katy liniowo miedzy probkami co 0,1 s
    TrajSample r = a; r.pos = {a.pos.x + (b.pos.x - a.pos.x) * f, a.pos.y + (b.pos.y - a.pos.y) * f, a.pos.z + (b.pos.z - a.pos.z) * f};
    r.rollDeg = a.rollDeg + (b.rollDeg - a.rollDeg) * f; r.pitchDeg = a.pitchDeg + (b.pitchDeg - a.pitchDeg) * f; r.yawDeg = lerpAng(a.yawDeg, b.yawDeg, f); return r; };
  for (double t = 0; t < fp.Duration(); t += 1.0 / p.sensor.pulseHz) {
    const double k = std::floor(t / 0.1 + 1e-9); const TrajSample s0 = fp.At(k * 0.1), s1 = fp.At((k + 1) * 0.1);
    if (!(s0.scanning && s1.scanning) || s0.strip >= skipStripsAfter) continue;         // jak AerialJob: impulsy tylko, gdy obie probki sa na szeregu
    const TrajSample s = lerp(s0, s1, (t - k * 0.1) / 0.1);
    const double th = MirrorThetaDeg(t, p.sensor); const AlsRay ray = TrueRay(s, sc.trueBore, lever, th); double range;
    if (!Hit(terrain, ray.origin, ray.dir, range)) continue;
    TrajSample est = s; if (s0.strip < (int)stripOff.size()) { est.pos.x += stripOff[s0.strip].x; est.pos.y += stripOff[s0.strip].y; est.pos.z += stripOff[s0.strip].z; }   // blad GNSS na poziomie szeregu
    const Vec3 g = GeorefPoint(est, I, lever, th, range);
    AlsPoint q{}; q.x = (float)(g.x - sc.originE); q.y = (float)(g.y - sc.originN); q.z = (float)(g.z - sc.originH); q.gps = 345600.0 + t; q.strip = (std::uint16_t)s0.strip; q.i = 100; q.r = q.g = q.b = 128; q.theta = (float)th; q.range = (float)range;
    w.Append(q); sc.points++;
  }
  w.Close(); return sc;
}

static double AngleDeg(const Rot3& a, const Rot3& b) {                  // kat obrotu laczacego dwie macierze
  double tr = 0; for (int i = 0; i < 3; i++) for (int k = 0; k < 3; k++) tr += a.m[i * 3 + k] * b.m[i * 3 + k];   // tr(A^T B)
  return std::acos((std::max)(-1.0, (std::min)(1.0, (tr - 1.0) / 2.0))) * 180.0 / PI;
}

static AdjustResult Run(const Scene& sc, std::vector<ControlPoint> cps = {}) {
  AlsAdjuster ad(sc.raw, sc.csv, sc.raw + ".out", sc.originN, sc.originE, sc.originH, cps);
  for (int i = 0; i < 100000 && !ad.Step(50); i++) {}
  return ad.Result();
}

static void trajectory_interpolation() {
  const std::string f = (std::filesystem::temp_directory_path() / "als_traj_t.csv").string();
  { std::ofstream o(f); o << "t;N_prawdziwe;E_prawdziwe;H_prawdziwe;N_gnss;E_gnss;H_gnss;roll;pitch;yaw;roll_imu;pitch_imu;yaw_imu;szereg\n";
    o << "10.00;0;0;0;100.000;200.000;50.000;0;0;0;1.000;2.000;359.000;0\n10.10;0;0;0;110.000;220.000;60.000;0;0;0;3.000;4.000;1.000;0\n"; }
  const std::vector<EstSample> tr = AlsLoadTrajectory(f); assert(tr.size() == 2 && tr[0].t == 10.0 && tr[0].pos.y == 100.0 && tr[0].pos.x == 200.0 && tr[0].pos.z == 50.0);   // N -> y, E -> x
  const EstSample m = AlsEstAt(tr, 10.05);
  assert(near(m.pos.y, 105.0) && near(m.pos.x, 210.0) && near(m.pos.z, 55.0) && near(m.roll, 2.0) && near(m.pitch, 3.0));
  assert(near(m.yaw, 0.0, 1e-9) || near(m.yaw, 360.0, 1e-9));                     // 359 -> 1 przez 0 (zawiniecie), nie przez 180
  assert(AlsEstAt(tr, 9.0).pos.y == 100.0 && AlsEstAt(tr, 11.0).pos.y == 110.0);  // poza zakresem: skrajna
  assert(AlsLoadTrajectory("brak_takiego_pliku.csv").empty());
}

static void boresight_recovered_on_hills() {
  const Scene sc = MakeScene("als_adj_hills", Hills(), 0.2, 0); assert(sc.points > 30000);
  const AdjustResult r = Run(sc);
  assert(r.ok && r.cells > 30 && r.strips == 3);
  assert(AngleDeg(RotFromRpy(r.bore[0], r.bore[1], r.bore[2]), sc.trueBore) < 0.03);          // odzyskany montaz skanera
  assert(r.determined[0] && r.determined[1] && r.determined[2]);
  assert(r.rmsCalibrated < 0.1 * r.rmsBefore && r.rmsBefore > 0.03);
}

static void cross_strips_tighten_sigma() {
  const Scene a = MakeScene("als_adj_nocross", Hills(), 0.2, 0), b = MakeScene("als_adj_cross", Hills(), 0.2, 3);
  const AdjustResult ra = Run(a), rb = Run(b); assert(ra.ok && rb.ok);
  assert(AngleDeg(RotFromRpy(rb.bore[0], rb.bore[1], rb.bore[2]), b.trueBore) < 0.03);
  assert(rb.sigma[1] < ra.sigma[1] && rb.sigma[2] < ra.sigma[2]);                              // szeregi poprzeczne zmniejszaja niepewnosc pitch i yaw
}

static void flat_terrain_only_roll_is_determined() {
  const Scene sc = MakeScene("als_adj_flat", Flat(), 0.2, 0);
  const AdjustResult r = Run(sc); assert(r.ok);
  assert(std::fabs(sc.boreDeg[0]) > 0.05);                                                      // scena ma sensowny blad roll
  assert(std::fabs(r.bore[0] - sc.boreDeg[0]) < 0.03);                                         // roll widoczny z przeciwnych szeregow
  assert(r.rmsCalibrated < 0.3 * r.rmsBefore);
}

static StripShift ShiftOf(const AdjustResult& r, int strip) { for (const StripShift& s : r.shifts) if (s.strip == strip) return s; return StripShift{}; }

static void strip_offsets_removed_and_output_written() {
  const Scene sc = MakeScene("als_adj_off", Hills(), 0.0, 0, 120.0, 99, {Vec3{0, 0, 0}, Vec3{0.3, -0.2, 0.15}, Vec3{-0.1, 0.25, -0.10}});
  const std::vector<StripQc> before = AlsStripCheck(sc.raw, 2.0);
  const AdjustResult r = Run(sc); assert(r.ok && r.shifts.size() == 3);
  const StripShift s0 = ShiftOf(r, 0), s1 = ShiftOf(r, 1), s2 = ShiftOf(r, 2);
  assert(s0.detH && s0.detEN && s1.detH && s1.detEN && s2.detH && s2.detEN);                 // teren z nachyleniem: wyznaczalne takze przesuniecia poziome
  assert(near(s1.dH - s0.dH, -0.15, 0.02) && near(s1.dE - s0.dE, -0.30, 0.08) && near(s1.dN - s0.dN, 0.20, 0.08));   // przesuniecia wzgledne = minus wstrzykniete
  assert(near(s2.dH - s0.dH, 0.10, 0.02) && near(s2.dE - s0.dE, 0.10, 0.08) && near(s2.dN - s0.dN, -0.25, 0.08));
  assert(near(s0.dH + s1.dH + s2.dH, 0.0, 0.02) && near(s0.dE + s1.dE + s2.dE, 0.0, 0.05));   // bez osnowy: srednie przesuniecie = 0
  assert(r.rmsAfter < 0.25 * r.rmsCalibrated || r.rmsAfter < 0.01);
  const std::string out = sc.raw + ".out";                                                    // wynik: te same punkty, poprawione wspolrzedne
  assert(std::filesystem::file_size(out) == std::filesystem::file_size(sc.raw));
  const std::vector<StripQc> after = AlsStripCheck(out, 2.0); assert(before.size() == 2 && after.size() == 2);
  for (size_t k = 0; k < 2; k++) assert(before[k].rmsDzM > 0.08 && after[k].rmsDzM < 0.3 * before[k].rmsDzM);
  std::ifstream a(sc.raw, std::ios::binary), b(out, std::ios::binary); AlsPoint pa{}, pb{}; assert(alsdetail::ReadRec(a, pa) && alsdetail::ReadRec(b, pb));
  assert(pa.gps == pb.gps && pa.strip == pb.strip && pa.theta == pb.theta && pa.range == pb.range && pa.r == pb.r && pa.i == pb.i);
}

static void control_fixes_absolute_height_flat() {
  const Scene sc = MakeScene("als_adj_ctrl", Flat(), 0.0, 0, 120.0, 99, {Vec3{0, 0, 0.10}, Vec3{0, 0, 0.20}, Vec3{0, 0, 0.30}});
  ControlPoint cp; cp.id = "G1"; cp.n = N0 + 60; cp.e = E0 + 60; cp.h = 100.0;
  const AdjustResult r = Run(sc, {cp}); assert(r.ok);
  assert(r.ctrlBefore.size() == 1 && r.ctrlBefore[0].pts > 20 && r.ctrlBefore[0].dhM > 0.08);          // chmura wisi nad osnowa
  assert(r.ctrlAfter.size() == 1 && std::fabs(r.ctrlAfter[0].dhM) < 0.02);                              // po wyrownaniu wysokosc bezwzgledna z osnowy
  const StripShift s0 = ShiftOf(r, 0);
  assert(s0.detH && !s0.detEN && s0.dE == 0.0 && s0.dN == 0.0);                                         // plaski teren: poziomo nieokreslone, zostaje 0
  assert(r.shifts[0].dH + r.shifts[1].dH + r.shifts[2].dH < -0.3);                                      // srednie przesuniecie nie jest zerem: osnowa ustala poziom
}

static void rejects_bad_input() {
  const Scene one = MakeScene("als_adj_one", Hills(), 0.2, 0, 120.0, 1);                       // tylko szereg 0
  const AdjustResult r = Run(one); assert(!r.ok && !r.msg.empty());
  Scene miss = one; miss.raw = "nie_ma_takiego_pliku.bin"; const AdjustResult m = Run(miss); assert(!m.ok && !m.msg.empty());
}

int main() {
  trajectory_interpolation(); boresight_recovered_on_hills(); cross_strips_tighten_sigma(); flat_terrain_only_roll_is_determined(); strip_offsets_removed_and_output_written(); control_fixes_absolute_height_flat(); rejects_bad_input();
  std::puts("test_alsadjust OK"); return 0;
}
