#include <cassert>
#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>
#include "job/Jobs.hpp"

static bool has(const std::string& s, const std::string& part) { return s.find(part) != std::string::npos; }
static bool near(double a, double b, double eps) { return std::fabs(a - b) <= eps; }
static const double PI = 3.14159265358979323846;

// Punkt osnowy: x,y,z = prawdziwe polozenie znaku w grze, n,e,h = "zmierzone" (mozna przesunac o dn, de).
static ControlPoint CP(const std::string& id, const GridPos& truth, double dn = 0, double de = 0) {
  ControlPoint c; c.id = id; c.world = "W"; c.n = truth.n + dn; c.e = truth.e + de; c.h = truth.h;
  GnssFromGrid(truth, c.x, c.y, c.z); c.solution = "fix RTK"; c.pdop = 1.5; c.epochs = 30; c.timeText = "09:00:00";
  return c;
}

struct FakeTachy : IWorld {
  GridPos station{5500000.0, 6500000.0, 100.0}, prism{5500000.0, 6500050.0, 100.0};
  GridPos back[5] = {{5500100.0, 6500000.0, 100.0}, {5500000.0, 6500080.0, 100.0}, {5499940.0, 6500000.0, 100.0}, {5500050.0, 6500050.0, 100.0}, {5499950.0, 6499950.0, 100.0}};
  bool ground = true, clear = true;
  bool sightHit = false; GridPos sightGrid{0, 0, 0}; double sightDist = 0;
  double prismHeight = 0;
  std::vector<ControlPoint> allControls, nearHero, nearStation;                    // osnowa tego swiata i wyniki zapytan o promien
  std::vector<std::string> said, placed;
  bool PlaceMarker(int, MarkerKind) override { return ground; }
  bool FeetWithinCm(int, double) const override { return true; }
  void PlaceRod(RodRole, int) override {}
  void RemoveRod(RodRole) override {}
  bool PlaceInstrument() override { placed.push_back("level-instrument"); return ground; }
  void RemoveInstrument() override {}
  double PointHeightM(int) const override { return 0; }
  double SightDistCm(int) const override { return 0; }
  double Random01() override { return 0.5; }
  void ShiftInstrumentMm(double) override {}
  void Say(const std::string& t) override { said.push_back(t); }
  bool PlaceTachymeter() override { placed.push_back("tachymeter"); return ground; }
  bool PlaceTachymeterAt(const ControlPoint& cp) override { placed.push_back("tachymeter@" + cp.id); station = GnssToGrid(cp.x, cp.y, cp.z); return true; }
  bool PlaceBacksight(int slot) override { placed.push_back("backsight" + std::to_string(slot)); return ground; }
  bool PlaceBacksightAt(int slot, const ControlPoint& cp) override { placed.push_back("backsight" + std::to_string(slot) + "@" + cp.id); back[slot - 1] = GnssToGrid(cp.x, cp.y, cp.z); return true; }
  void RemoveBacksight(int slot) override { placed.push_back("remove" + std::to_string(slot)); }
  std::vector<ControlPoint> ControlNearHero(double maxM) const override { return maxM > 1e6 ? allControls : nearHero; }
  std::vector<ControlPoint> ControlNearStation(double, double) const override { return nearStation; }
  void SetPrismHeightM(double h) override { prismHeight = h; }
  bool TraceSight(double, double, GridPos& hit, double& slopeM) const override { hit = sightGrid; slopeM = sightDist; return sightHit; }
  GridPos StationGrid() const override { return station; }
  GridPos PointGrid(int slot) const override { return back[slot - 1]; }
  bool PlacePrism() override { placed.push_back("prism"); return ground; }
  GridPos PoleGroundGrid() const override { return prism; }
  bool LineClear(bool, int) const override { return clear; }
  std::string TimeText() const override { return "09:15:03"; }
};

struct Fx {
  FakeTachy w; std::unique_ptr<Job> job;
  explicit Fx(std::function<void(FakeTachy&)> init = nullptr, bool trueErr = false) {
    if (init) init(w);
    job = CreateTachyStation(w, JobOptions{TraverseKind::Open, CalcMode::Auto, trueErr});
  }
  bool enabled(const std::string& id) const { for (auto& m : job->Menu()) if (m.id == id) return m.enabled; return false; }
  std::string label(const std::string& id) const { for (auto& m : job->Menu()) if (m.id == id) return m.label; return ""; }
  void menu(const std::string& id) { job->OnMenu(id); }
  void aim(const GridPos& tgt, double tgtAboveM, double& az, double& pitch) const {
    const double dN = tgt.n - w.station.n, dE = tgt.e - w.station.e, dH = (tgt.h + tgtAboveM) - (w.station.h + 1.5);
    az = std::atan2(dE, dN) * 180.0 / PI; pitch = std::atan2(dH, std::hypot(dN, dE)) * 180.0 / PI;
  }
  std::string fireAt(const GridPos& tgt, double above, double offAz = 0, double offPitch = 0) {
    double az, pitch; aim(tgt, above, az, pitch); return job->TachyFire(az + offAz, pitch + offPitch);
  }
  std::string readBack(int slot, double prismH = 1.5) { return fireAt(w.back[slot - 1], prismH); }
  void setup(int backs = 1) { menu("station"); for (int i = 0; i < backs; i++) menu("backsight"); menu("prism"); }
  void orient(int backs = 1) { for (int i = 1; i <= backs; i++) assert(has(readBack(i), "Nawiazanie")); }
};

static void registry_lists_tachy() {
  bool found = false;
  for (const JobType& t : JobTypes()) if (t.id == "tachy") found = t.tachy && !t.gnss && t.name == "Tachimetria (pomiar biegunowy)";
  assert(found);
  Fx f; assert(f.job->Kind() == JobKind::Tachy && f.job->ActiveRodPoints().empty() && !f.job->OnReading(1, 1000));
}

static void menu_state_true_mode() {
  Fx f;
  for (const char* id : {"src", "backsight", "unback", "prism", "prismh", "meas", "view", "finish"}) assert(!f.enabled(id));   // bez osnowy zrodlo nie do zmiany
  assert(f.enabled("station") && !f.job->NeedsChoice("station"));
  f.menu("station");
  assert((f.w.placed == std::vector<std::string>{"tachymeter"}));
  assert(f.enabled("backsight") && f.enabled("prism") && f.enabled("prismh") && f.enabled("meas") && !f.enabled("view") && !f.enabled("unback"));
  f.menu("backsight"); assert((f.w.placed.back() == "backsight1") && f.enabled("view") && f.enabled("unback") && near(f.w.prismHeight, 1.5, 1e-9));
}

static void multiple_backsights_orientation_mean() {
  Fx f; f.setup(3); f.orient(3);
  TachyReport r = f.job->FinishTachy(true);
  assert(r.backsights.size() == 3 && r.coordSource == "prawdziwe");
  for (const TachyBacksight& b : r.backsights) assert(std::fabs(b.dirResidMgon) < 6.0 && std::fabs(b.distResidMm) < 10.0 && !b.uncertain);
  assert(near(r.backsights[0].azGon, 0.0, 1e-3) && near(r.backsights[1].azGon, 100.0, 1e-3) && near(r.backsights[2].azGon, 200.0, 1e-3));
  f.w.prism = GridPos{5500040.0, 6500030.0, 100.0};                              // poza linia celowania w nawiazania (inaczej lustro nawiazania ma pierwszenstwo)
  std::string m = f.fireAt(f.w.prism, 1.5);
  TachyReport r2 = f.job->FinishTachy(true);
  assert(has(m, "T1") && r2.points.size() == 1);
  assert(near(r2.points[0].n, 5500040.0, 0.02) && near(r2.points[0].e, 6500030.0, 0.02) && near(r2.points[0].h, 100.0, 0.03));
  assert(has(f.job->TachyPanel(90.0, 0.0)[1], "3 z 3"));
}

static void single_backsight_works() {
  Fx f; f.setup(1); f.orient(1);
  f.fireAt(f.w.prism, 1.5);
  const TachyPoint& p = f.job->FinishTachy(true).points[0];
  assert(near(p.n, 5500000.0, 0.02) && near(p.e, 6500050.0, 0.02) && near(p.hzGon, 100.0, 0.01) && near(p.vGon, 100.0, 0.01));
}

static void measure_before_orient_refused() {
  Fx f; f.setup(1);
  std::string m = f.fireAt(f.w.prism, 1.5);
  assert(has(m, "Najpierw zmierz nawiazanie") && f.job->FinishTachy(true).points.empty());
}

static void backsight_aiming_is_angular() {
  Fx f; f.setup(1);
  const double off3cm = 0.03 / 100.0 * 180.0 / PI, off8cm = 0.08 / 100.0 * 180.0 / PI;       // nawiazanie 100 m na polnoc
  assert(has(f.fireAt(f.w.back[0], 1.5, off8cm), "Najpierw") && f.job->FinishTachy(true).backsights.empty());     // 8 cm bokiem: poza tolerancja
  assert(has(f.fireAt(f.w.back[0], 1.5, off3cm), "Nawiazanie"));                                                  // 3 cm: w tolerancji 0,02 st.
  f.w.clear = false; assert(has(f.readBack(1), "zaslonieta"));
}

static void remeasure_overwrites() {
  Fx f; f.setup(2); f.readBack(1); f.readBack(1); f.readBack(2);
  assert(f.job->FinishTachy(true).backsights.size() == 2);
}

static void changes_clear_readings() {
  Fx f; f.setup(2); f.orient(2);
  f.menu("unback"); assert(f.w.placed.back() == "remove2");
  assert(f.job->FinishTachy(true).backsights.empty());                            // odczyty skasowane
  assert(has(f.fireAt(f.w.prism, 1.5), "Najpierw zmierz nawiazanie"));
  f.orient(1);                                                                    // ponowna orientacja na pozostalym nawiazaniu
  f.menu("station");
  assert(f.job->FinishTachy(true).backsights.empty() && !f.enabled("view"));      // przestawienie stanowiska usuwa nawiazania i odczyty
}

static void limit_five_backsights() {
  Fx f; f.menu("station");
  for (int i = 0; i < 5; i++) { assert(f.enabled("backsight")); f.menu("backsight"); }
  assert(!f.enabled("backsight") && f.w.placed.back() == "backsight5");
}

static void uncertain_flag_direction() {
  auto build = [](double de) { return [de](FakeTachy& w) {
    w.allControls = {CP("O1", w.station), CP("O2", w.back[0], 0, de), CP("O3", w.back[1]), CP("O4", w.back[2])};
    w.nearHero = {w.allControls[0]}; w.nearStation = {w.allControls[1], w.allControls[2], w.allControls[3]};
  }; };
  Fx f(build(0.12)); f.menu("station:O1"); f.menu("backsight:O2"); f.menu("backsight:O3"); f.menu("backsight:O4"); f.orient(3);
  TachyReport r = f.job->FinishTachy(true);
  assert(r.backsights.size() == 3 && r.backsights[0].uncertain && std::fabs(r.backsights[0].dirResidMgon) > 40.0);   // 0,12 m na 100 m = ok. 76 mgon
  Fx g(build(0.02)); g.menu("station:O1"); g.menu("backsight:O2"); g.menu("backsight:O3"); g.menu("backsight:O4"); g.orient(3);
  for (const TachyBacksight& b : g.job->FinishTachy(true).backsights) assert(!b.uncertain);                       // 0,02 m = 13 mgon -> odchylki < 20
}

static void uncertain_flag_distance() {
  auto build = [](double dn) { return [dn](FakeTachy& w) {
    w.allControls = {CP("O1", w.station), CP("O2", w.back[0], dn, 0), CP("O3", w.back[1])};
    w.nearHero = {w.allControls[0]}; w.nearStation = {w.allControls[1], w.allControls[2]};
  }; };
  Fx f(build(0.05)); f.menu("station:O1"); f.menu("backsight:O2"); f.menu("backsight:O3"); f.orient(2);
  TachyReport r = f.job->FinishTachy(true);
  assert(r.backsights[0].uncertain && std::fabs(r.backsights[0].distResidMm) > 30.0 && !r.backsights[1].uncertain);
  Fx g(build(0.01)); g.menu("station:O1"); g.menu("backsight:O2"); g.menu("backsight:O3"); g.orient(2);
  for (const TachyBacksight& b : g.job->FinishTachy(true).backsights) assert(!b.uncertain);
}

static void control_source_uses_gnss_coordinates() {
  Fx f([](FakeTachy& w) {
    w.allControls = {CP("O1", w.station, 0.02, -0.01), CP("O2", w.back[0]), CP("O3", w.back[1])};
    w.nearHero = {w.allControls[0]}; w.nearStation = {w.allControls[0], w.allControls[1], w.allControls[2]};
  });
  assert(has(f.label("src"), "osnowa") && f.job->NeedsChoice("station") && f.job->NeedsChoice("backsight"));
  std::vector<MenuItem> st = f.job->TachyChoices("station");
  assert(st.size() == 1 && st[0].id == "station:O1" && has(st[0].label, "O1"));
  f.menu("station:O1");
  assert((f.w.placed == std::vector<std::string>{"tachymeter@O1"}));
  std::vector<MenuItem> bk = f.job->TachyChoices("backsight");
  assert(bk.size() == 2 && bk[0].id == "backsight:O2" && bk[1].id == "backsight:O3");                           // stanowisko (O1) pominiete
  f.menu("backsight:O2"); f.menu("prism");
  bk = f.job->TachyChoices("backsight"); assert(bk.size() == 1 && bk[0].id == "backsight:O3");                  // uzyte pominiete
  f.orient(1);
  TachyReport r = f.job->FinishTachy(true);
  assert(r.coordSource == "osnowa GNSS" && r.stationName == "O1");
  assert(near(r.stationN, 5500000.02, 1e-6) && near(r.stationE, 6499999.99, 1e-6));                              // wspolrzedne stanowiska = zmierzone, nie prawdziwe
  assert(r.backsights.size() == 1 && r.backsights[0].name == "O2" && r.backsights[0].distCoordM > 99.0);
  f.menu("src"); assert(has(f.label("src"), "prawdziwe") || !f.enabled("src"));                                  // po stanowisku zrodla nie zmieniamy
}

static void no_control_nearby_refused() {
  Fx f([](FakeTachy& w) { w.allControls = {CP("O1", w.station)}; });                                             // osnowa jest, ale nie przy bohaterze
  assert(f.job->NeedsChoice("station") && f.job->TachyChoices("station").empty());
  f.menu("station:O9"); f.menu("station");
  assert(f.w.placed.empty() && !f.enabled("backsight") && !f.w.said.empty() && has(f.w.said.back(), "osnowy"));
}

static void prism_height_is_the_physical_height() {
  Fx f; f.setup(1);
  f.menu("prismh"); assert(has(f.label("prismh"), "1.800") && near(f.w.prismHeight, 1.8, 1e-9));
  f.readBack(1, 1.8);                                                             // lustro nawiazania tez na 1,8 m
  assert(has(f.fireAt(f.w.prism, 1.5), "Nie celujesz"));
  assert(has(f.fireAt(f.w.prism, 1.8), "T1"));
  assert(near(f.job->FinishTachy(true).points[0].h, 100.0, 0.03) && near(f.job->FinishTachy(true).points[0].prismM, 1.8, 1e-9));
}

static void aim_must_be_on_prism_not_ground() {
  Fx f; f.setup(1); f.orient(1);
  assert(has(f.fireAt(f.w.prism, 0.0), "Nie celujesz"));
  f.w.clear = false; assert(has(f.fireAt(f.w.prism, 1.5), "zaslonieta"));
  assert(f.job->FinishTachy(true).points.empty());
}

static void reflectorless_measures_surface_point() {
  Fx f; f.menu("station"); f.menu("backsight");
  assert(f.enabled("view"));                                                      // bez tyczki z lustrem mozna wejsc do lunety (orientacja)
  f.orient(1); f.menu("meas"); assert(has(f.label("meas"), "bezlustrowy"));
  double az, pitch; f.aim(GridPos{5500000.0, 6500030.0, 102.0}, 0.0, az, pitch);
  assert(has(f.job->TachyFire(az, pitch), "Brak odbicia") && f.job->FinishTachy(true).points.empty());
  f.w.sightHit = true; f.w.sightGrid = GridPos{5500000.0, 6500030.0, 102.0}; f.w.sightDist = std::hypot(30.0, 102.0 - 101.5);
  assert(has(f.job->TachyFire(az, pitch), "T1"));
  const TachyPoint& p = f.job->FinishTachy(true).points[0];
  assert(near(p.n, 5500000.0, 0.02) && near(p.e, 6500030.0, 0.02) && near(p.h, 102.0, 0.03) && near(p.prismM, 0.0, 1e-9) && has(p.note, "bezlustrowy"));
}

static void true_error_and_abort() {
  Fx f(nullptr, true); f.setup(1); f.orient(1); f.fireAt(f.w.prism, 1.5);
  TachyReport r = f.job->FinishTachy(false);
  assert(!r.complete && r.showTrueError && r.points.size() == 1 && r.points[0].hasTrue && r.points[0].d2Dm < 0.05);
  Fx g; assert(g.job->FinishTachy(false).points.empty());
}

static void extreme_aim_does_not_crash() {
  Fx f; f.setup(1); f.orient(1);
  for (double pitch : {-90.0, 90.0, 400.0}) for (double az : {-720.0, 0.0, 1e9}) {
    std::string m = f.job->TachyFire(az, pitch);
    assert(!m.empty());
    for (const std::string& l : f.job->TachyPanel(az, pitch)) assert(!has(l, "nan") && !has(l, "inf"));
  }
  assert(f.job->FinishTachy(true).points.empty());
}

int main() {
  registry_lists_tachy(); menu_state_true_mode(); multiple_backsights_orientation_mean(); single_backsight_works(); measure_before_orient_refused();
  backsight_aiming_is_angular(); remeasure_overwrites(); changes_clear_readings(); limit_five_backsights();
  uncertain_flag_direction(); uncertain_flag_distance(); control_source_uses_gnss_coordinates(); no_control_nearby_refused();
  prism_height_is_the_physical_height(); aim_must_be_on_prism_not_ground(); reflectorless_measures_surface_point();
  true_error_and_abort(); extreme_aim_does_not_crash();
  std::puts("test_tachy_job OK");
  return 0;
}
