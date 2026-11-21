#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include "job/Jobs.hpp"

static bool has(const std::string& s, const std::string& part) { return s.find(part) != std::string::npos; }
static bool near(double a, double b, double eps) { return std::fabs(a - b) <= eps; }
static const double PI = 3.14159265358979323846;

struct FakeTachy : IWorld {
  GridPos station{5500000.0, 6500000.0, 100.0}, back{5500100.0, 6500000.0, 100.0}, prism{5500000.0, 6500050.0, 100.0};
  bool ground = true, clear = true;
  bool sightHit = false; GridPos sightGrid{0, 0, 0}; double sightDist = 0;      // wynik promienia pomiaru bezlustrowego
  double prismHeight = 0;                                                        // ostatnia wysokosc lustra przekazana swiatu
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
  bool PlaceBacksight() override { placed.push_back("backsight"); return ground; }
  void SetPrismHeightM(double h) override { prismHeight = h; }
  bool TraceSight(double, double, GridPos& hit, double& slopeM) const override { hit = sightGrid; slopeM = sightDist; return sightHit; }
  GridPos StationGrid() const override { return station; }
  GridPos PointGrid(int) const override { return back; }
  bool PlacePrism() override { placed.push_back("prism"); return ground; }
  GridPos PoleGroundGrid() const override { return prism; }
  bool LineClear(bool) const override { return clear; }
  std::string TimeText() const override { return "09:15:03"; }
};

struct Fx {
  FakeTachy w; std::unique_ptr<Job> job;
  explicit Fx(bool trueErr = false) { job = CreateTachyStation(w, JobOptions{TraverseKind::Open, CalcMode::Auto, trueErr}); }
  bool enabled(const std::string& id) const { for (auto& m : job->Menu()) if (m.id == id) return m.enabled; return false; }
  std::string label(const std::string& id) const { for (auto& m : job->Menu()) if (m.id == id) return m.label; return ""; }
  void menu(const std::string& id) { job->OnMenu(id); }
  // kamera tak, by krzyz lezal dokladnie na celu (grunt + tgtAboveM) widzianym z oka stanowiska
  void aim(const GridPos& tgt, double tgtAboveM, double& az, double& pitch) const {
    const double dN = tgt.n - w.station.n, dE = tgt.e - w.station.e, dH = (tgt.h + tgtAboveM) - (w.station.h + 1.5);
    az = std::atan2(dE, dN) * 180.0 / PI; pitch = std::atan2(dH, std::hypot(dN, dE)) * 180.0 / PI;
  }
  std::string fireAt(const GridPos& tgt, double above, double offAz = 0, double offPitch = 0) {
    double az, pitch; aim(tgt, above, az, pitch); return job->TachyFire(az + offAz, pitch + offPitch);
  }
  void setup() { menu("station"); menu("backsight"); menu("prism"); }
  void orient(double prismH = 1.5) { menu("orient"); assert(has(fireAt(w.back, prismH), "Orientacja")); }   // cel = lustro na nawiazaniu
};

static void registry_lists_tachy() {
  bool found = false;
  for (const JobType& t : JobTypes()) if (t.id == "tachy") found = t.tachy && !t.gnss && t.name == "Tachimetria (pomiar biegunowy)";
  assert(found);
  Fx f; assert(f.job->Kind() == JobKind::Tachy && f.job->ActiveRodPoints().empty() && !f.job->OnReading(1, 1000));
}

static void uses_tachymeter_and_backsight_prism() {
  Fx f; f.menu("station"); f.menu("backsight"); f.menu("prism");
  assert((f.w.placed == std::vector<std::string>{"tachymeter", "backsight", "prism"}));        // osobny model tachimetru, lustro na nawiazaniu
  assert(near(f.w.prismHeight, 1.5, 1e-9));
}

static void menu_enablement_follows_state() {
  Fx f;
  for (const char* id : {"backsight", "orient", "prism", "prismh", "meas", "view", "finish"}) assert(!f.enabled(id));
  assert(f.enabled("station"));
  f.menu("station"); assert(f.enabled("backsight") && f.enabled("prism") && f.enabled("prismh") && f.enabled("meas") && !f.enabled("orient"));
  f.menu("backsight"); assert(f.enabled("orient") && !f.enabled("view"));
  f.menu("prism"); assert(!f.enabled("view"));                                   // dopiero po orientacji
  f.orient(); assert(f.enabled("view") && !f.enabled("finish"));
}

static void measure_before_orient_refused() {
  Fx f; f.setup();
  std::string m = f.fireAt(f.w.prism, 1.5);
  assert(has(m, "rientacj") && f.job->FinishTachy(true).points.empty());
}

static void orient_then_measure_gives_coordinates() {
  Fx f; f.setup(); f.orient();
  std::string m = f.fireAt(f.w.prism, 1.5);                                       // po orientacji od razu tryb pomiaru
  TachyReport r = f.job->FinishTachy(true);
  assert(r.points.size() == 1 && has(m, "T1"));
  const TachyPoint& p = r.points[0];
  assert(near(p.n, 5500000.0, 0.02) && near(p.e, 6500050.0, 0.02) && near(p.h, 100.0, 0.03));
  assert(near(p.hzGon, 100.0, 0.01) && near(p.vGon, 100.0, 0.01) && p.timeText == "09:15:03" && !p.hasTrue && p.note.empty());
  assert(near(r.stationN, 5500000.0, 1e-6) && near(r.instrH, 1.5, 1e-9) && near(r.azBackGon, 0.0, 1e-6));
  assert(f.job->CanFinish() && f.enabled("finish") && has(f.job->TachyPanel(90.0, 0.0)[0], "pomiar"));
}

static void aim_must_be_on_prism_not_ground() {
  Fx f; f.setup(); f.orient();
  double az, pitch; f.aim(f.w.prism, 1.5, az, pitch);
  const double offAz = 0.3 / 50.0 * 180.0 / PI;                                   // 0,3 m w poprzek na 50 m
  assert(has(f.job->TachyFire(az + offAz, pitch), "Nie celujesz"));
  assert(has(f.fireAt(f.w.prism, 0.0), "Nie celujesz"));                          // cel na gruncie zamiast na lustrze
  f.w.clear = false; assert(has(f.fireAt(f.w.prism, 1.5), "zaslonieta"));
  assert(f.job->FinishTachy(true).points.empty());
}

static void prism_height_is_the_physical_height() {
  Fx f; f.setup();
  f.menu("prismh"); assert(has(f.label("prismh"), "1.800") && near(f.w.prismHeight, 1.8, 1e-9));   // swiat dostaje wysokosc: model lustra tej wysokosci
  f.orient(1.8);                                                                  // lustro na nawiazaniu tez na 1,8 m
  assert(has(f.fireAt(f.w.prism, 1.5), "Nie celujesz"));                          // lustro stoi na 1,8 m, nie na 1,5 m
  assert(has(f.fireAt(f.w.prism, 1.8), "T1"));
  assert(near(f.job->FinishTachy(true).points[0].h, 100.0, 0.03));                // wysokosc lustra zgodna z modelem: brak bledu w H
  assert(near(f.job->FinishTachy(true).points[0].prismM, 1.8, 1e-9));
}

static void reflectorless_measures_surface_point() {
  Fx f; f.menu("station"); f.menu("backsight");                                   // bez tyczki z lustrem
  assert(!f.enabled("view"));
  f.orient();
  f.menu("meas"); assert(has(f.label("meas"), "bezlustrowy") && f.enabled("view"));
  f.w.sightHit = false;
  double az, pitch; f.aim(GridPos{5500000.0, 6500030.0, 102.0}, 0.0, az, pitch);
  assert(has(f.job->TachyFire(az, pitch), "Brak odbicia") && f.job->FinishTachy(true).points.empty());
  f.w.sightHit = true; f.w.sightGrid = GridPos{5500000.0, 6500030.0, 102.0};
  f.w.sightDist = std::hypot(30.0, 102.0 - 101.5);                                // odleglosc skosna od oka do trafionego punktu
  std::string m = f.job->TachyFire(az, pitch);
  TachyReport r = f.job->FinishTachy(true);
  assert(has(m, "T1") && r.points.size() == 1);
  const TachyPoint& p = r.points[0];
  assert(near(p.n, 5500000.0, 0.02) && near(p.e, 6500030.0, 0.02) && near(p.h, 102.0, 0.03));   // H bez wysokosci lustra
  assert(near(p.prismM, 0.0, 1e-9) && has(p.note, "bezlustrowy"));
}

static void reposition_invalidates_orientation() {
  Fx f; f.setup(); f.orient(); assert(f.enabled("view"));
  f.menu("station"); assert(!f.enabled("view") && f.enabled("orient"));
  f.orient(); assert(f.enabled("view"));
  f.menu("backsight"); assert(!f.enabled("view"));
}

static void orientation_switches_view_to_measure() {
  Fx f; f.setup();
  f.menu("orient");
  assert(has(f.fireAt(f.w.back, 1.5), "Orientacja"));
  assert(has(f.fireAt(f.w.prism, 1.5), "T1") && f.job->FinishTachy(true).points.size() == 1);   // bez wychodzenia z lunety
}

static void true_error_and_abort() {
  Fx f(true); f.setup(); f.orient(); f.fireAt(f.w.prism, 1.5);
  TachyReport r = f.job->FinishTachy(false);
  assert(!r.complete && r.showTrueError && r.points.size() == 1 && r.points[0].hasTrue && r.points[0].d2Dm < 0.05);
  Fx g; assert(g.job->FinishTachy(false).points.empty());
}

static void true_error_for_reflectorless_uses_hit_point() {
  Fx f(true); f.menu("station"); f.menu("backsight"); f.orient(); f.menu("meas");
  f.w.sightHit = true; f.w.sightGrid = GridPos{5500020.0, 6500020.0, 101.0};
  f.w.sightDist = std::hypot(std::hypot(20.0, 20.0), 101.0 - 101.5);
  double az, pitch; f.aim(f.w.sightGrid, 0.0, az, pitch);
  f.job->TachyFire(az, pitch);
  const TachyPoint& p = f.job->FinishTachy(true).points[0];
  assert(p.hasTrue && p.d2Dm < 0.05 && near(p.dHm, 0.0, 0.03));
}

static void extreme_aim_does_not_crash() {
  Fx f; f.setup(); f.orient();
  for (double pitch : {-90.0, 90.0, 400.0}) for (double az : {-720.0, 0.0, 1e9}) {
    std::string m = f.job->TachyFire(az, pitch);
    assert(!m.empty());
    for (const std::string& l : f.job->TachyPanel(az, pitch)) assert(!has(l, "nan") && !has(l, "inf"));
  }
  assert(f.job->FinishTachy(true).points.empty());
}

int main() {
  registry_lists_tachy(); uses_tachymeter_and_backsight_prism(); menu_enablement_follows_state(); measure_before_orient_refused();
  orient_then_measure_gives_coordinates(); aim_must_be_on_prism_not_ground(); prism_height_is_the_physical_height();
  reflectorless_measures_surface_point(); reposition_invalidates_orientation(); orientation_switches_view_to_measure();
  true_error_and_abort(); true_error_for_reflectorless_uses_hit_point(); extreme_aim_does_not_crash();
  std::puts("test_tachy_job OK");
  return 0;
}
