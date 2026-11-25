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
  int poles = 0, tripods = 0;
  bool PlacePole() override { poles++; return ground; }
  bool PlaceGnssTripod() override { tripods++; return ground; }
  GridPos baseAt{5500100.0, 6500150.0, 12.0}; bool controlNear = false;
  bool PlaceBase(GridPos& g) override { g = baseAt; return ground; }
  std::vector<ControlPoint> ControlNearHero(double) const override { return controlNear ? std::vector<ControlPoint>{ControlPoint{}} : std::vector<ControlPoint>{}; }
  SkyMask SkyHorizon() const override { return mask; }
  GridPos PoleGroundGrid() const override { return {5500100.0, 6500200.0, 12.5}; }
  double NowSec() const override { return now; }
  std::string TimeText() const override { return "09:15:03"; }
  std::vector<ControlPoint> stored; std::string nextId = "O1";                 // osnowa: zapisane punkty i id nadawane przez swiat
  std::string StoreControlPoint(const ControlPoint& cp) override { if (nextId.empty()) return ""; stored.push_back(cp); return nextId; }
};

struct Fx {
  FakeGnss w; std::unique_ptr<Job> job;
  explicit Fx(bool trueErr = false, bool isStatic = false) { job = (isStatic ? CreateGnssStatic : CreateGnssRover)(w, JobOptions{TraverseKind::Open, CalcMode::Auto, trueErr}); }
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
  for (const char* id : {"start", "store", "control", "discard", "finish", "fine"}) assert(!f.enabled(id));
  f.menu("pole"); assert(f.enabled("start") && f.enabled("fine"));      // precyzyjne ustawienie tyczki: gdy stoi i nie trwa pomiar
  f.menu("start");
  assert(!f.enabled("pole") && !f.enabled("antenna") && !f.enabled("start") && !f.enabled("fine") && f.enabled("discard") && !f.enabled("store"));
}

static void pole_blocked_without_ground() {
  Fx f; f.w.ground = false; f.menu("pole");
  assert(!f.enabled("start") && !f.w.said.empty() && has(f.w.said.back(), "gruntu"));
}

static void open_sky_fix_then_store() {
  Fx f; f.measure(60);
  assert(has(f.status(), "fix RTK") && has(f.status(), "Niebo zasloniete: 0%") && f.enabled("store"));
  f.menu("store");
  GnssReport r = f.job->FinishGnss(true);
  assert(r.points.size() == 1 && r.points[0].name == "P1" && r.points[0].solution == "fix RTK" && r.points[0].note.empty());
  assert(r.points[0].timeText == "09:15:03" && r.complete && !r.points[0].hasTrue);
  assert(f.enabled("finish") && f.job->CanFinish());
}

static void store_without_fix_is_flagged() {
  Fx f; f.w.mask.nearestObstacleM = 2.5; f.measure(20);                 // wielodrogowosc 0,5: float, nigdy fix
  assert(has(f.status(), "przeszkoda blisko"));                          // powod braku fixu widoczny w panelu
  assert(f.enabled("store")); f.menu("store");
  GnssReport r = f.job->FinishGnss(true);
  assert(r.points.size() == 1 && r.points[0].note == "zapis bez fixu" && r.points[0].solution == "float RTK");
}

static void blocked_sky_never_enables_store() {
  Fx f; for (double& h : f.w.mask.horizonDeg) h = 90; f.measure(120);
  assert(has(f.status(), "Niebo zasloniete: 100%"));
  assert(!f.enabled("store") && has(f.status(), "brak rozwiazania") && !has(f.status(), "nan") && !has(f.status(), "inf"));
}

static int epochsOf(const std::string& status) { size_t i = status.find("Epoki: "), k = 7; if (i == std::string::npos) { i = status.find("Sesja: "); } return i == std::string::npos ? -1 : std::atoi(status.c_str() + i + k); }

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

static void control_requires_fix_and_30_minutes() {
  Fx f(false, true); f.menu("pole"); f.menu("base"); f.menu("start"); bool sawEnabled = false, sawFixBelow30 = false;
  for (int t = 1; t <= 60; t++) {
    f.advance(1);
    const bool fix = has(f.status(), "fix RTK"); const int ep = epochsOf(f.status());
    assert(f.enabled("control") == (fix && ep >= 30));
    if (f.enabled("control")) sawEnabled = true;
    if (fix && ep < 30) sawFixBelow30 = true;
  }
  assert(sawEnabled && sawFixBelow30);
  Fx g(false, true); g.w.mask.nearestObstacleM = 2.5; g.menu("pole"); g.menu("base"); g.menu("start");   // wielodrogowosc: nigdy fix
  for (int t = 1; t <= 60; t++) { g.advance(1); assert(!g.enabled("control")); }
  Fx r; assert(!r.enabled("control")); r.measure(60); assert(!r.enabled("control"));                    // RTK nie zaklada osnowy
}

static void staticMeasure(Fx& f, int min) { f.menu("pole"); f.menu("base"); f.menu("start"); f.advance(min); }

static void control_stores_point_and_report() {
  Fx f(false, true); staticMeasure(f, 60); assert(f.enabled("control"));
  f.menu("control");
  assert(f.w.stored.size() == 1);
  GnssReport r = f.job->FinishGnss(true);
  assert(r.points.size() == 1 && has(r.points[0].note, "osnowa O1"));
  const ControlPoint& cp = f.w.stored[0];
  assert(cp.n == r.points[0].n && cp.e == r.points[0].e && cp.h == r.points[0].h && cp.solution == "fix RTK" && cp.epochs >= 30 && cp.timeText == "09:15:03");
  double x, y, z; GnssFromGrid(f.w.PoleGroundGrid(), x, y, z);
  assert(std::fabs(cp.x - x) < 1e-6 && std::fabs(cp.y - y) < 1e-6 && std::fabs(cp.z - z) < 1e-6);
  assert(f.enabled("start") && !f.enabled("control"));                          // pomiar zakonczony, mozna mierzyc nastepny
}

static void control_failure_does_not_store() {
  Fx f(false, true); f.w.nextId = ""; staticMeasure(f, 60);
  f.menu("control");
  assert(f.w.stored.empty() && f.job->FinishGnss(true).points.empty());
  assert(!f.w.said.empty() && has(f.w.said.back(), "osnowy") && f.enabled("control"));   // nadal mierzymy, mozna ponowic
}

static void own_base_needed_for_corrections() {
  Fx f; f.menu("corr"); assert(has(f.label("corr"), "wlasna") && f.enabled("base"));
  f.measure(60);
  assert(has(f.status(), "brak bazy") && !has(f.status(), "fix RTK"));            // bez bazy tylko rozwiazanie autonomiczne
  f.menu("discard"); f.menu("base"); f.menu("start"); f.advance(60);
  assert(has(f.status(), "fix RTK") && has(f.status(), "wlasna baza"));
  f.menu("store");
  const GnssPoint p = f.job->FinishGnss(true).points[0];
  assert(has(p.note, "baza na dowolnym punkcie") && has(p.note, "linia bazowa 50 m"));
}

static void base_on_control_point_is_accurate_arbitrary_base_shifts_all() {
  auto err = [](bool control) {
    Fx f(true); f.w.controlNear = control; f.menu("corr"); f.menu("pole"); f.menu("base"); f.menu("start"); f.advance(60); f.menu("store");
    return f.job->FinishGnss(true).points[0].d2Dmm;
  };
  assert(err(true) < 100.0 && err(false) > 300.0);                                // baza na znaku osnowy: cm; baza na dowolnym punkcie: przesuniecie rzedu metra
}

static void static_is_separate_job_needing_30_minutes() {
  bool found = false;
  for (const JobType& t : JobTypes()) if (t.id == "gnss_static") found = t.gnss && t.name == "Statyka GNSS (osnowa)";
  assert(found);
  Fx f(false, true);
  assert(f.enabled("corr") && has(f.label("corr"), "siec") && f.job->Name() == "Statyka GNSS (osnowa)");   // domyslnie poprawki z sieci, bez bazy
  f.menu("pole"); f.menu("start"); f.advance(10);                                   // 1 s gry = 1 min obserwacji, bez bazy
  assert(has(f.status(), "min") && !f.enabled("store") && !f.enabled("control"));
  Fx g(false, true); assert(!g.enabled("base")); g.menu("corr"); assert(has(g.label("corr"), "wlasna baza") && g.enabled("base"));   // wlasna baza opcjonalna
  f.advance(25); assert(f.enabled("store") && f.enabled("control"));
  f.menu("store");
  assert(has(f.job->FinishGnss(true).points[0].note, "statyka"));
}

static void static_uses_tripod_rtk_uses_pole() {
  Fx s(false, true); s.menu("pole"); assert(s.w.tripods == 1 && s.w.poles == 0 && has(s.label("pole"), "statywie"));
  Fx r; r.menu("pole"); assert(r.w.poles == 1 && r.w.tripods == 0);
}

static void rtk_has_no_static_switch() { Fx f; assert(f.enabled("corr") && f.label("mode").empty()); }

static void state_roundtrip_gnss() {
  Fx f; f.menu("corr"); f.menu("base"); f.menu("antenna");
  f.measure(160); f.menu("store");                                                  // jeden punkt zapisany (wlasna baza)
  f.menu("start"); f.advance(5);                                                    // drugi pomiar w trakcie: sesja nie jest zapisywana
  StateKV st; assert(f.job->CanSaveState()); f.job->SaveState(st);
  Fx g; assert(g.job->LoadState(StateKV::FromText(st.ToText())));
  const GnssReport a = f.job->FinishGnss(true), b = g.job->FinishGnss(true);
  assert(a.points.size() == 1 && b.points.size() == 1 && GnssToCsv(a) == GnssToCsv(b));
  assert(g.label("corr") == f.label("corr") && g.label("antenna") == f.label("antenna") && g.enabled("start") && !g.enabled("discard"));   // sesja zerwana, tyczka stoi
}

int main() {
  registry_lists_gnss(); menu_enablement_follows_state(); pole_blocked_without_ground(); open_sky_fix_then_store();
  store_without_fix_is_flagged(); blocked_sky_never_enables_store(); clock_frozen_means_no_epochs();
  antenna_presets_cycle_and_bias_height(); true_error_columns_when_enabled(); abort_returns_partial_report(); discard_clears_measurement();
  control_requires_fix_and_30_minutes(); control_stores_point_and_report(); control_failure_does_not_store();
  own_base_needed_for_corrections(); base_on_control_point_is_accurate_arbitrary_base_shifts_all(); static_is_separate_job_needing_30_minutes(); rtk_has_no_static_switch(); static_uses_tripod_rtk_uses_pole();
  state_roundtrip_gnss();
  std::puts("test_gnss_job OK");
  return 0;
}
