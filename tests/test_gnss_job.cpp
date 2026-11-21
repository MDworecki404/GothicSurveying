#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include "job/Jobs.hpp"

static bool has(const std::string& s, const std::string& part) { return s.find(part) != std::string::npos; }

struct FakeGnss : IWorld {
  bool ground = true; SkyMask mask; double now = 0; std::vector<std::string> said;
  bool PlaceMarker(int, MarkerKind) override { return true; }
  bool FeetWithinCm(int, double) const override { return true; }
  void PlaceRod(RodRole, int) override {}
  void RemoveRod(RodRole) override {}
  bool PlaceInstrument() override { return true; }
  void RemoveInstrument() override {}
  double PointHeightM(int) const override { return 0; }
  double SightDistCm(int) const override { return 0; }
  double Random01() override { return 0.5; }
  void ShiftInstrumentMm(double) override {}
  void Say(const std::string& t) override { said.push_back(t); }
  bool PlacePole() override { return ground; }
  SkyMask SkyHorizon() const override { return mask; }
  GridPos PoleGroundGrid() const override { return {5500100.0, 6500200.0, 12.5}; }
  double NowSec() const override { return now; }
  std::string TimeText() const override { return "09:15:03"; }
};

struct Fx {
  FakeGnss w; std::unique_ptr<Job> job;
  explicit Fx(bool trueErr = false) { job = CreateGnssRover(w, JobOptions{TraverseKind::Open, CalcMode::Auto, trueErr}); }
  bool enabled(const std::string& id) const { for (auto& m : job->Menu()) if (m.id == id) return m.enabled; return false; }
  std::string label(const std::string& id) const { for (auto& m : job->Menu()) if (m.id == id) return m.label; return ""; }
  void menu(const std::string& id) { job->OnMenu(id); }
  void advance(int sec) { for (int i = 0; i < sec; i++) { w.now += 1; job->Tick(); } }
  std::string status() const { std::string s; for (auto& l : job->StatusLines()) s += l + "\n"; return s; }
  void measure(int sec) { menu("pole"); menu("start"); advance(sec); }
};

static void registry_lists_gnss() {
  bool found = false;
  for (const JobType& t : JobTypes()) if (t.id == "gnss") { found = t.gnss && t.name == "Pomiar GNSS (RTK)"; }
  assert(found);
  FakeGnss w; auto j = CreateGnssRover(w, JobOptions{TraverseKind::Open, CalcMode::Auto});
  assert(j->Kind() == JobKind::Gnss && j->ActiveRodPoints().empty() && j->RodPointFor(RodRole::Back) < 0 && !j->OnReading(1, 1000));
}

static void menu_enablement_follows_state() {
  Fx f;
  assert(f.enabled("pole") && f.enabled("antenna"));
  for (const char* id : {"start", "store", "discard", "finish"}) assert(!f.enabled(id));
  f.menu("pole"); assert(f.enabled("start"));
  f.menu("start");
  assert(!f.enabled("pole") && !f.enabled("antenna") && !f.enabled("start") && f.enabled("discard") && !f.enabled("store"));
}

static void pole_blocked_without_ground() {
  Fx f; f.w.ground = false; f.menu("pole");
  assert(!f.enabled("start") && !f.w.said.empty() && has(f.w.said.back(), "gruntu"));
}

static void open_sky_fix_then_store() {
  Fx f; f.measure(60);
  assert(has(f.status(), "fix RTK") && f.enabled("store"));
  f.menu("store");
  GnssReport r = f.job->FinishGnss(true);
  assert(r.points.size() == 1 && r.points[0].name == "P1" && r.points[0].solution == "fix RTK" && r.points[0].note.empty());
  assert(r.points[0].timeText == "09:15:03" && r.complete && !r.points[0].hasTrue);
  assert(f.enabled("finish") && f.job->CanFinish());
}

static void store_without_fix_is_flagged() {
  Fx f; f.w.mask.nearestObstacleM = 2.5; f.measure(20);                 // wielodrogowosc 0,5: float, nigdy fix
  assert(f.enabled("store")); f.menu("store");
  GnssReport r = f.job->FinishGnss(true);
  assert(r.points.size() == 1 && r.points[0].note == "zapis bez fixu" && r.points[0].solution == "float RTK");
}

static void blocked_sky_never_enables_store() {
  Fx f; for (double& h : f.w.mask.horizonDeg) h = 90; f.measure(120);
  assert(!f.enabled("store") && has(f.status(), "brak rozwiazania") && !has(f.status(), "nan") && !has(f.status(), "inf"));
}

static int epochsOf(const std::string& status) { size_t i = status.find("Epoki: "); return i == std::string::npos ? -1 : std::atoi(status.c_str() + i + 7); }

static void clock_frozen_means_no_epochs() {
  Fx f; f.menu("pole"); f.menu("start");
  for (int i = 0; i < 10; i++) f.job->Tick();
  assert(epochsOf(f.status()) == 0);
  f.w.now += 1000; f.job->Tick();                                        // duzy skok: najwyzej 5 epok na Tick
  assert(epochsOf(f.status()) <= 5);
  for (int i = 0; i < 250; i++) f.job->Tick();                           // dogonienie zegara (po 5 epok na Tick)
  const int before = epochsOf(f.status());
  f.w.now = 10; f.job->Tick();                                           // skok wstecz nie psuje stanu
  assert(epochsOf(f.status()) == before);
  f.advance(3); assert(epochsOf(f.status()) >= 1);
}

static void antenna_presets_cycle_and_bias_height() {
  Fx f; const char* expect[] = {"2.100", "2.500", "1.500", "1.800", "2.000"};
  assert(has(f.label("antenna"), "2.000"));
  for (int i = 0; i < 4; i++) { f.menu("antenna"); assert(has(f.label("antenna"), expect[i])); }   // zostajemy na 1.800
  f.measure(60); f.menu("store");
  GnssReport r = f.job->FinishGnss(true);
  assert(r.points.size() == 1 && r.points[0].antennaM == 1.8 && std::fabs(r.points[0].h - 12.5 - 0.2) < 0.05);
  f.menu("antenna"); assert(has(f.label("antenna"), "2.000"));
}

static void true_error_columns_when_enabled() {
  Fx f(true); f.measure(60); f.menu("store");
  GnssPoint p = f.job->FinishGnss(true).points[0];
  assert(f.job->FinishGnss(true).showTrueError && p.hasTrue);
  assert(std::fabs(p.d2Dmm - std::sqrt(p.dNmm * p.dNmm + p.dEmm * p.dEmm)) < 1e-9 && p.d2Dmm < 50.0);
  Fx g; g.measure(60); g.menu("store"); assert(!g.job->FinishGnss(true).points[0].hasTrue);
}

static void abort_returns_partial_report() {
  Fx f; assert(f.job->FinishGnss(false).points.empty());
  f.measure(60); f.menu("store"); f.menu("start"); f.advance(5);
  GnssReport r = f.job->FinishGnss(false);
  assert(!r.complete && r.points.size() == 1);
  assert(!f.job->CanFinish());                                           // trwa pomiar
}

static void discard_clears_measurement() {
  Fx f; f.measure(30); f.menu("discard");
  assert(f.enabled("start") && f.job->FinishGnss(true).points.empty() && !f.enabled("discard") && !f.job->CanFinish());
}

int main() {
  registry_lists_gnss(); menu_enablement_follows_state(); pole_blocked_without_ground(); open_sky_fix_then_store();
  store_without_fix_is_flagged(); blocked_sky_never_enables_store(); clock_frozen_means_no_epochs();
  antenna_presets_cycle_and_bias_height(); true_error_columns_when_enabled(); abort_returns_partial_report(); discard_clears_measurement();
  std::puts("test_gnss_job OK");
  return 0;
}
