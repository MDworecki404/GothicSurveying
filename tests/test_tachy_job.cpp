#include <set>
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
  bool levelingOn = false; bool InstrumentLevelingEnabled() const override { return levelingOn; }
  bool PlacePrism() override { placed.push_back("prism"); return ground; }
  bool PlaceTachymeterOnPoint(int) override { placed.push_back("tachymeter-on-point"); return true; }
  bool PlacePrismOnPoint(int) override { placed.push_back("prism-on-point"); return true; }
  // GNSS wewnatrz roboty tachimetrycznej: tyczka z odbiornikiem, niebo, zegar, zapis osnowy
  SkyMask mask; double now = 0; std::vector<ControlPoint> stored; std::string nextId = "O1";
  bool PlacePole() override { placed.push_back("pole"); return ground; }
  bool PlaceBase(GridPos& g) override { placed.push_back("base"); g = GridPos{5500010.0, 6500010.0, 100.0}; return ground; }
  SkyMask SkyHorizon() const override { return mask; }
  double NowSec() const override { return now; }
  std::string StoreControlPoint(const ControlPoint& cp) override { if (nextId.empty()) return ""; stored.push_back(cp); return nextId; }
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
  void menu(const std::string& id) {
    if (id.rfind("backsight", 0) == 0) { if (nBack > 0 && !done.count(nBack)) readBack(nBack); nBack++; }
    if (id == "station" || id.rfind("station:", 0) == 0) { nBack = 0; done.clear(); }
    job->OnMenu(id);
  }
  void aim(const GridPos& tgt, double tgtAboveM, double& az, double& pitch) const {
    const double dN = tgt.n - w.station.n, dE = tgt.e - w.station.e, dH = (tgt.h + tgtAboveM) - (w.station.h + 1.5);
    az = std::atan2(dE, dN) * 180.0 / PI; pitch = std::atan2(dH, std::hypot(dN, dE)) * 180.0 / PI;
  }
  std::string fireAt(const GridPos& tgt, double above, double offAz = 0, double offPitch = 0) {
    double az, pitch; aim(tgt, above, az, pitch); return job->TachyFire(az + offAz, pitch + offPitch);
  }
  int nBack = 0; std::set<int> done;   // stoi tylko ostatnie lustro nawiazania: menu("backsight") najpierw mierzy poprzednie (jak uzytkownik)
  std::string readBack(int slot, double prismH = 1.5) { done.insert(slot); return fireAt(w.back[slot - 1], prismH); }
  void setup(int backs = 1) { menu("station"); for (int i = 0; i < backs; i++) menu("backsight"); menu("prism"); }
  void orient(int backs = 1) { for (int i = 1; i <= backs; i++) if (!done.count(i)) assert(has(readBack(i), "Nawiazanie")); }
};

static void registry_lists_tachy() {
  bool found = false;
  for (const JobType& t : JobTypes()) if (t.id == "tachy") found = t.tachy && !t.gnss && t.name == "Tachimetria (pomiar biegunowy)";
  assert(found);
  Fx f; assert(f.job->Kind() == JobKind::Tachy && f.job->ActiveRodPoints().empty() && !f.job->OnReading(1, 1000));
}

static void menu_state_true_mode() {
  Fx f;
  for (const char* id : {"src", "backsight", "unback", "prism", "prismh", "meas", "view", "finish", "fine"}) assert(!f.enabled(id));   // bez osnowy zrodlo nie do zmiany
  assert(f.enabled("station") && !f.job->NeedsChoice("station"));
  f.menu("station");
  assert((f.w.placed == std::vector<std::string>{"tachymeter"}));
  assert(f.enabled("backsight") && f.enabled("prism") && f.enabled("prismh") && f.enabled("meas") && !f.enabled("view") && !f.enabled("unback"));
  f.menu("backsight"); assert((f.w.placed.back() == "backsight1") && f.enabled("view") && f.enabled("unback") && near(f.w.prismHeight, 1.5, 1e-9));
  assert(!f.enabled("fine")); f.menu("prism"); assert(f.enabled("fine"));   // precyzyjne ustawienie lustra dopiero po postawieniu tyczki
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

static void orientation_locked_after_first_point() {
  Fx f; f.setup(2); f.readBack(1);                                                // zmierzone tylko nawiazanie 1, drugie nie
  f.w.prism = GridPos{5500040.0, 6500030.0, 100.0};
  assert(has(f.fireAt(f.w.prism, 1.5), "T1"));
  const double before = f.job->FinishTachy(true).backsights[0].dirResidMgon;
  std::string m = f.readBack(2);                                                  // po pierwszym punkcie odczyt nawiazania nie moze zmienic orientacji
  assert(has(m, "zablokowana"));
  TachyReport r = f.job->FinishTachy(true);
  assert(r.backsights.size() == 1 && near(r.backsights[0].dirResidMgon, before, 1e-9) && r.points.size() == 1);
}

static void tick(Fx& f, int sec) { for (int i = 0; i < sec; i++) { f.w.now += 1; f.job->Tick(); } }

static void gnss_items_and_control_point_in_tachy_job() {
  Fx f;
  assert(f.enabled("g_pole") && f.enabled("g_antenna") && !f.enabled("g_start") && !f.enabled("g_control") && !f.enabled("g_discard"));
  assert(has(f.label("g_pole"), "GNSS") && f.job->StatusLines().empty());
  f.menu("g_pole"); assert(f.w.placed.back() == "pole" && f.enabled("g_start"));
  assert(!f.job->StatusLines().empty());                                          // podpowiedz odbiornika przy stojacej tyczce
  f.menu("g_base"); f.menu("g_start"); tick(f, 60);                                // statyka: 1 s gry = 1 min sesji
  assert(has(f.job->StatusLines()[0], "fix RTK") && f.enabled("g_control") && !f.enabled("g_pole"));
  f.menu("g_control");
  assert(f.w.stored.size() == 1 && f.w.stored[0].solution == "fix RTK" && f.w.stored[0].epochs >= 30);
  assert(f.job->FinishTachy(true).points.empty());                                // punkt GNSS to osnowa, nie punkt tachimetryczny
  f.w.allControls = f.w.stored;                                                   // swiat widzi nowa osnowe: mozna ja uzyc jako zrodlo
  assert(f.enabled("src")); f.menu("src"); assert(has(f.label("src"), "osnowa"));
}

static void gnss_pole_and_prism_exclude_each_other() {
  Fx f; f.setup(1); f.orient(1);
  f.w.prism = GridPos{5500040.0, 6500030.0, 100.0};
  f.menu("g_pole");                                                               // tyczka GNSS zajela miejsce tyczki z lustrem
  assert(has(f.fireAt(f.w.prism, 1.5), "Najpierw postaw tyczke z lustrem"));
  f.menu("g_start");
  assert(!f.enabled("prism") && f.enabled("g_discard"));                          // w trakcie pomiaru GNSS nie wolno przestawic tyczki z lustrem
  f.menu("g_discard"); assert(f.enabled("prism"));
  f.menu("prism"); assert(!f.enabled("g_start"));                                 // po tyczce z lustrem odbiornik wymaga ponownego postawienia tyczki GNSS
  assert(has(f.fireAt(f.w.prism, 1.5), "T1"));
}

static void one_backsight_mirror_at_a_time() {
  Fx f; f.menu("station"); f.menu("backsight");
  assert(!f.enabled("backsight"));                                   // najpierw zmierz stojace lustro
  assert(has(f.readBack(1), "Nawiazanie") && f.enabled("backsight"));
  f.menu("backsight");
  assert(has(f.readBack(2), "Nawiazanie") && f.job->FinishTachy(true).backsights.size() == 2);   // zmierzone wczesniej zostaje
}

static void limit_five_backsights() {
  Fx f; f.menu("station");
  for (int i = 0; i < 5; i++) { if (i > 0) f.readBack(i); assert(f.enabled("backsight")); f.menu("backsight"); }
  assert(!f.enabled("backsight"));   // lustro nr 5 niezmierzone, a limit 5 i tak blokuje; nr 4 zmierzony wyzej
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

static void two_faces_measure_in_kl_and_kp() {
  Fx f; f.setup(1);
  assert(has(f.job->TachyFlip(), "wlacz w menu") && f.enabled("faces") && has(f.label("faces"), "jedno"));
  f.menu("faces"); assert(has(f.label("faces"), "dwa"));
  std::string m = f.readBack(1);                                                   // KL nawiazania: zapisane, orientacji jeszcze nie ma
  assert(has(m, "KL zapisane") && f.job->FinishTachy(true).backsights.empty());
  f.job->TachyFlip();                                                              // KP
  assert(has(f.job->TachyPanel(0.0, 0.0)[0], "KP"));
  m = f.readBack(1);
  assert(has(m, "Nawiazanie") && has(m, "KL+KP") && f.job->FinishTachy(true).backsights.size() == 1);
  assert(has(f.job->TachyPanel(0.0, 0.0)[0], "KL"));                               // po pomiarze z powrotem KL
  f.w.prism = GridPos{5500040.0, 6500030.0, 100.0};
  assert(has(f.fireAt(f.w.prism, 1.5), "KL zapisane") && f.job->FinishTachy(true).points.empty());
  f.job->TachyFlip();
  assert(has(f.fireAt(f.w.prism, 1.5), "T1"));
  const TachyPoint p = f.job->FinishTachy(true).points[0];
  assert(near(p.n, 5500040.0, 0.02) && near(p.e, 6500030.0, 0.02) && near(p.h, 100.0, 0.03) && has(p.note, "KL+KP"));   // bledy instrumentu usuniete
}

static void single_face_in_two_face_mode_has_errors_and_kp_needs_kl() {
  Fx f; f.setup(1); f.menu("faces"); f.job->TachyFlip();
  assert(has(f.readBack(1), "najpierw zmierz ten sam cel w KL"));                  // KP bez KL
  f.job->TachyFlip();
  f.readBack(1); f.job->TachyFlip();                                               // KL i KP na nawiazaniu
  assert(has(f.readBack(1), "Nawiazanie"));
  f.menu("faces");                                                                 // przelaczenie trybu kasuje orientacje
  assert(f.job->FinishTachy(true).backsights.empty() && has(f.label("faces"), "jedno"));
}

static const GridPos ST{5500000.0, 6500000.0, 100.0}, G1p{5500100.0, 6500000.0, 100.0}, G2p{5500000.0, 6500080.0, 100.0}, G3p{5500040.0, 6500033.0, 100.0};

static void free_station_resection() {
  Fx f([](FakeTachy& w) { w.allControls = {CP("G1", G1p, 0.01, -0.01), CP("G2", G2p, -0.01, 0.01), CP("G3", G3p)}; w.nearStation = w.allControls; });
  assert(f.enabled("free") && has(f.label("free"), "znanym"));
  f.menu("free"); assert(has(f.label("free"), "dowolne") && !f.job->NeedsChoice("station"));
  f.menu("station"); f.menu("backsight:G1"); f.menu("backsight:G2"); f.menu("prism");
  assert(f.job->FinishTachy(true).backsights.empty());                              // G1 zmierzone przy stawianiu G2 (wciecie wymaga min. 2)
  assert(has(f.fireAt(G2p, 1.5), "Nawiazanie") && f.job->FinishTachy(true).backsights.size() == 2);
  TachyReport r = f.job->FinishTachy(true);
  assert(near(r.stationN, 5500000.0, 0.05) && near(r.stationE, 6500000.0, 0.05) && near(r.stationH, 100.0, 0.05) && has(r.coordSource, "dowolne"));
  f.w.prism = GridPos{5500040.0, 6500030.0, 100.0};
  assert(has(f.fireAt(f.w.prism, 1.5), "T1"));
  const TachyPoint p = f.job->FinishTachy(true).points[0];
  assert(near(p.n, 5500040.0, 0.05) && near(p.e, 6500030.0, 0.05) && near(p.h, 100.0, 0.06));
  assert(has(f.job->TachyPanel(0.0, 0.0)[3], "Stanowisko dowolne: N"));
}

static void stakeout_guides_and_confirms() {
  Fx f([](FakeTachy& w) { w.allControls = {CP("G0", ST), CP("G1", G1p), CP("G3", G3p)}; w.nearHero = {CP("G0", ST)}; w.nearStation = {CP("G1", G1p), CP("G3", G3p)}; });
  f.menu("station:G0"); f.menu("backsight:G1"); f.menu("prism");
  assert(!f.enabled("stake") && !f.enabled("unstake"));                    // najpierw orientacja
  f.orient(1);
  assert(f.enabled("stake") && f.job->NeedsChoice("stake") && f.job->TachyChoices("stake").size() == 2);
  f.menu("stake:G3"); assert(f.enabled("unstake") && has(f.label("stake"), "G3"));
  f.w.prism = GridPos{5500040.0, 6500030.0, 100.0};                         // 3 m na zachod od celu G3
  std::string m = f.fireAt(f.w.prism, 1.5);
  assert(has(m, "T1") && has(m, "G3") && (has(m, "dalej") || has(m, "blizej")) && has(m, "w prawo"));
  assert(has(f.job->FinishTachy(true).points[0].note, "wytyczanie G3"));
  f.w.prism = G3p;                                                          // tyczka na celu
  m = f.fireAt(f.w.prism, 1.5);
  assert(has(m, "wytyczony"));
  f.menu("unstake"); assert(!has(f.fireAt(f.w.prism, 1.5), "wytyczony") && !f.enabled("unstake"));
}

static void multiple_stations_in_one_job() {
  Fx f([](FakeTachy& w) { w.allControls = {CP("G0", ST), CP("G1", G1p), CP("G2", G2p)}; w.nearHero = {CP("G0", ST)}; w.nearStation = {CP("G1", G1p), CP("G2", G2p)}; });
  assert(!f.enabled("newst"));
  f.menu("station:G0"); f.menu("backsight:G1"); f.menu("prism"); f.orient(1);
  f.w.prism = GridPos{5500040.0, 6500030.0, 100.0};
  assert(has(f.fireAt(f.w.prism, 1.5), "T1") && f.job->FinishTachy(true).stations.empty());      // jedno stanowisko: raport jak dotad
  assert(f.enabled("newst") && !f.enabled("backsight") && !f.enabled("station"));
  f.menu("newst");
  assert(!f.enabled("newst") && f.enabled("station") && !f.enabled("view") && has(f.w.said.back(), "S1"));
  // drugie stanowisko: na G1 (prawdziwe polozenie G1), nawiazanie na G2
  f.w.nearHero = {CP("G1", G1p)}; f.menu("station:G1");
  f.w.nearStation = {CP("G2", G2p)}; f.menu("backsight:G2");
  assert(has(f.readBack(1), "Nawiazanie"));
  f.w.prism = GridPos{5500060.0, 6500040.0, 100.0};
  assert(has(f.fireAt(f.w.prism, 1.5), "T2"));
  TachyReport r = f.job->FinishTachy(true);
  assert(r.stations.size() == 2 && r.stations[0].label == "S1" && r.stations[1].label == "S2" && r.points.size() == 2);
  assert(near(r.stations[0].n, 5500000.0, 0.02) && near(r.stations[1].n, 5500100.0, 0.02) && near(r.stationN, 5500100.0, 0.02));
  assert(r.stations[0].backsights.size() == 1 && r.stations[1].backsights.size() == 1 && has(r.points[0].note, "stanowisko S1") && has(r.points[1].note, "stanowisko S2"));
  const std::string csv = TachyToCsv(r);
  assert(has(csv, "# stanowisko S1;") && has(csv, "# stanowisko S2;") && has(csv, "# zrodlo S2;"));
  assert(has(TachySummaryLines(r)[0], "Stanowiska: 2"));
}

static void trig_leveling_leg_both_ways() {
  FakeTachy w; w.station = GridPos{5500000.0, 6500000.0, 100.0}; w.prism = GridPos{5500000.0, 6500050.0, 100.5};
  auto job = CreateTrigLevel(w, JobOptions{TraverseKind::Open, CalcMode::Auto, true});
  Fx dummy;                                                                             // tylko do liczenia kierunku celowania
  auto aimAt = [&](double& az, double& pitch) {
    const double dN = w.prism.n - w.station.n, dE = w.prism.e - w.station.e, dH = (w.prism.h + 1.5) - (w.station.h + 1.5);
    az = std::atan2(dE, dN) * 180.0 / PI; pitch = std::atan2(dH, std::hypot(dN, dE)) * 180.0 / PI;
  };
  auto enabled = [&](const std::string& id) { for (auto& m : job->Menu()) if (m.id == id) return m.enabled; return false; };
  assert(job->Kind() == JobKind::Tachy && !enabled("inst") && !enabled("view") && !job->CanFinish());
  job->OnMenu("reper_a"); job->OnMenu("inst"); job->OnMenu("prism");
  assert(enabled("view"));
  double az, pitch; aimAt(az, pitch);
  std::string m = job->TachyFire(az, pitch);
  assert(has(m, "Tam A>B") && has(m, "dh +50") && !enabled("view") && !job->CanFinish());
  std::swap(w.station, w.prism); job->OnMenu("inst"); job->OnMenu("prism");              // powrot: tachimetr na B, lustro na A
  aimAt(az, pitch); m = job->TachyFire(az, pitch);
  assert(has(m, "Odcinek A-B") && has(m, "ok") && job->CanFinish());
  const TachyReport r = job->FinishTachy(true);
  assert(r.trig && r.legs.size() == 1 && r.points.size() == 2 && r.points[0].name == "A>B tam" && r.points[1].name == "B>A powrot");
  const TrigLeg& l = r.legs[0];
  assert(near(l.meanMm, 500.0, 6.0) && near(l.sumMm, 0.0, 10.0) && l.ok && near(l.hToM, 100.0 + 0.0 + 0.5 - 100.0, 0.01) && l.hasTrue && std::fabs(l.errMm) < 6.0);
  const std::string csv = TachyToCsv(r);
  assert(has(csv, "odcinek;od;do") && has(csv, "A;B;") && has(TachySummaryLines(r)[0], "Odcinki: 1"));
  assert(!TachyBuildDocx(r).empty());
  job->OnMenu("prism");                                                                   // nowy odcinek B>C: punkt celu dochodzi dopiero z lustrem
  for (auto& it : job->Menu()) if (it.id == "inst") assert(has(it.label, "B"));
}

static void instrument_must_be_levelled() {
  Fx f([](FakeTachy& w) { w.levelingOn = true; });
  f.setup(1);
  Libella* lib = f.job->GetLibella();
  assert(lib && lib->enabled && !lib->Within(kTachyLevelArcmin));                      // nowe stanowisko: krzywo
  std::string m = f.readBack(1);
  assert(has(m, "niewypoziomowany") && f.job->FinishTachy(true).backsights.empty());
  lib->x = 0.3; lib->y = 0.2;                                                          // w tolerancji libelli elektronicznej (0,5')
  assert(has(f.readBack(1), "Nawiazanie"));
  lib->x = 3.0;                                                                        // rozpoziomowany ponownie nie jest: pomiar blokowany
  f.w.prism = GridPos{5500040.0, 6500030.0, 100.0};
  assert(has(f.fireAt(f.w.prism, 1.5), "niewypoziomowany"));
  Fx g; assert(g.job->GetLibella() && !g.job->GetLibella()->enabled);                   // swiat bez poziomowania: instrument zawsze poziomy
}

static void state_roundtrip_tachy_multi_station() {
  auto init = [](FakeTachy& w) { w.allControls = {CP("G0", ST), CP("G1", G1p), CP("G2", G2p)}; w.nearHero = {CP("G0", ST)}; w.nearStation = {CP("G1", G1p), CP("G2", G2p)}; };
  Fx f(init);
  f.menu("station:G0"); f.menu("backsight:G1"); f.menu("prism"); f.orient(1);
  f.w.prism = GridPos{5500040.0, 6500030.0, 100.0};
  assert(has(f.fireAt(f.w.prism, 1.5), "T1") || has(f.fireAt(f.w.prism, 1.5), "KL"));
  f.job->TachyFlip(); f.fireAt(f.w.prism, 1.5);                                      // KL i KP: punkt zapisany, albo w trakcie
  StateKV st; assert(f.job->CanSaveState()); f.job->SaveState(st);
  Fx g(init); assert(g.job->LoadState(StateKV::FromText(st.ToText())));
  assert(TachyToCsv(f.job->FinishTachy(true)) == TachyToCsv(g.job->FinishTachy(true)));
  for (const char* id : {"station", "backsight", "newst", "view", "stake", "faces", "free", "finish"}) assert(f.enabled(id) == g.enabled(id));
  assert(f.job->Hud() == g.job->Hud() && f.label("faces") == g.label("faces"));
  const size_t before = g.job->FinishTachy(true).points.size();
  g.job->TachyFlip(); g.fireAt(g.w.prism, 1.5);
  g.job->TachyFlip(); const std::string m = g.fireAt(g.w.prism, 1.5);                  // dalszy pomiar dziala po wczytaniu (orientacja zachowana)
  assert(g.job->FinishTachy(true).points.size() >= before && (has(m, "T") || has(m, "KL") || has(m, "KP")));
  f.menu("newst");
  StateKV st2; f.job->SaveState(st2);                                                // stan po zamknieciu stanowiska: zachowane stanowisko S1
  Fx h(init); assert(h.job->LoadState(StateKV::FromText(st2.ToText())));
  assert(h.job->FinishTachy(true).stations.size() == f.job->FinishTachy(true).stations.size() && h.enabled("station"));
}

static void state_roundtrip_trig() {
  FakeTachy w; w.station = GridPos{5500000.0, 6500000.0, 100.0}; w.prism = GridPos{5500000.0, 6500050.0, 100.5};
  auto job = CreateTrigLevel(w, JobOptions{TraverseKind::Open, CalcMode::Auto, true});
  job->OnMenu("reper_a"); job->OnMenu("inst"); job->OnMenu("prism");
  StateKV st; assert(job->CanSaveState()); job->SaveState(st);
  FakeTachy w2; w2.station = w.station; w2.prism = w.prism;
  auto job2 = CreateTrigLevel(w2, JobOptions{TraverseKind::Open, CalcMode::Auto, true});
  assert(job2->LoadState(StateKV::FromText(st.ToText())) && job2->Hud() == job->Hud());
  const double az = std::atan2(50.0, 0.0) * 180.0 / PI, pitch = std::atan2(0.5, 50.0) * 180.0 / PI;
  assert(has(job2->TachyFire(az, pitch), "Tam A>B"));                                // wczytana robota mierzy dalej
}

static void forward_intersection_two_stations() {
  Fx f; f.setup(1); f.orient(1); f.menu("intersect");
  assert(has(f.label("intersect"), "wciecie"));
  const GridPos P{5500030.0, 6500040.0, 100.0}; f.w.prism = P;
  std::string m = f.fireAt(P, 1.5);
  assert(has(m, "Kierunek z S1") && f.job->FinishTachy(true).points.empty());
  assert(!f.enabled("backsight") && f.enabled("newst"));                        // orientacja zablokowana po pierwszym kierunku
  f.menu("newst"); f.w.station = GridPos{5500000.0, 6500100.0, 100.0};
  f.menu("station"); f.menu("backsight"); f.orient(1);
  m = f.fireAt(P, 1.5);
  assert(has(m, "T1"));
  const TachyReport r = f.job->FinishTachy(true);
  assert(r.points.size() == 1 && near(r.points[0].n, P.n, 0.02) && near(r.points[0].e, P.e, 0.02) && has(r.points[0].note, "wciecie w przod z S1 i S2"));
  Fx g; g.setup(1); g.orient(1); g.menu("intersect"); g.w.prism = P; g.fireAt(P, 1.5);       // zapis stanu: czekajacy kierunek przezywa
  StateKV s; g.job->SaveState(s);
  Fx h; h.job->LoadState(s); assert(has(h.label("intersect"), "wciecie"));
}

int main() {
  forward_intersection_two_stations();
  registry_lists_tachy(); menu_state_true_mode(); multiple_backsights_orientation_mean(); single_backsight_works(); measure_before_orient_refused();
  backsight_aiming_is_angular(); remeasure_overwrites(); orientation_locked_after_first_point(); gnss_items_and_control_point_in_tachy_job(); gnss_pole_and_prism_exclude_each_other(); changes_clear_readings(); limit_five_backsights(); one_backsight_mirror_at_a_time();
  uncertain_flag_direction(); uncertain_flag_distance(); control_source_uses_gnss_coordinates(); no_control_nearby_refused();
  prism_height_is_the_physical_height(); aim_must_be_on_prism_not_ground(); reflectorless_measures_surface_point();
  true_error_and_abort(); extreme_aim_does_not_crash();
  two_faces_measure_in_kl_and_kp(); single_face_in_two_face_mode_has_errors_and_kp_needs_kl();
  free_station_resection(); stakeout_guides_and_confirms();
  multiple_stations_in_one_job();
  trig_leveling_leg_both_ways();
  instrument_must_be_levelled();
  state_roundtrip_tachy_multi_station(); state_roundtrip_trig();
  std::puts("test_tachy_job OK");
  return 0;
}
