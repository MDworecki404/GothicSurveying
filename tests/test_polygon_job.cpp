#include <cassert>
#include <cmath>
#include <cstdio>
#include <map>
#include <string>
#include <vector>
#include "job/PolygonJob.hpp"

static bool has(const std::string& s, const std::string& part) { return s.find(part) != std::string::npos; }
static const double kPi = 3.14159265358979323846;

// Sztuczny swiat: prawdziwe pozycje znakow, tachimetr i lustro "na punkcie" ustawiaja sie dokladnie nad znakiem.
struct FakePoly : IWorld {
  std::map<std::string, GridPos> controlTruth; std::vector<ControlPoint> controls;
  GridPos hero{0, 0, 100}, station{0, 0, 100}, pole{0, 0, 100}, bs{0, 0, 100}; bool heroOk = true, clear = true;
  std::map<int, GridPos> marker; std::vector<std::string> said, placed;
  bool PlaceMarker(int id, MarkerKind) override { if (!heroOk) return false; marker[id] = hero; placed.push_back("marker" + std::to_string(id)); return true; }
  bool FeetWithinCm(int, double) const override { return true; }
  void PlaceRod(RodRole, int) override {}
  void RemoveRod(RodRole) override {}
  bool PlaceInstrument() override { return true; }
  void RemoveInstrument() override {}
  double PointHeightM(int) const override { return 100; }
  double SightDistCm(int) const override { return 0; }
  double Random01() override { return 0.5; }
  void ShiftInstrumentMm(double) override {}
  void Say(const std::string& t) override { said.push_back(t); }
  bool HeroGround(GridPos& g) const override { if (!heroOk) return false; g = hero; return true; }
  bool PlaceTachymeterAt(const ControlPoint& cp) override { station = controlTruth[cp.id]; placed.push_back("tachymeter@" + cp.id); return true; }
  bool PlaceTachymeterOnPoint(int id) override { station = marker[id]; placed.push_back("tachymeter-on-" + std::to_string(id)); return true; }
  bool PlaceBacksightAt(int, const ControlPoint& cp) override { bs = controlTruth[cp.id]; placed.push_back("prism@" + cp.id); return true; }
  bool PlacePrismOnPoint(int id) override { pole = marker[id]; placed.push_back("prism-on-" + std::to_string(id)); return true; }
  void RemoveBacksight(int) override {}
  std::vector<ControlPoint> ControlNearHero(double) const override { return controls; }
  GridPos StationGrid() const override { return station; }
  GridPos PoleGroundGrid() const override { return pole; }
  GridPos PointGrid(int) const override { return bs; }
  bool LineClear(bool, int) const override { return clear; }
};

static ControlPoint Cp(const std::string& id, double n, double e) { ControlPoint c; c.id = id; c.n = n; c.e = e; c.h = 100; return c; }

static const GridPos N1{1000, 1000, 100}, A{1100, 1200, 100}, B{1460, 1560, 100}, N2{1600, 1640, 100}, P1{1210, 1330, 100}, P2{1330, 1490, 100};

struct Fx {
  FakePoly w; std::unique_ptr<Job> job; PolygonJob* P = nullptr;
  explicit Fx(TraverseKind k, bool trueErr = false) {
    const std::pair<const char*, GridPos> cs[] = {{"N1", N1}, {"A", A}, {"B", B}, {"N2", N2}};
    for (const auto& c : cs) { w.controls.push_back(Cp(c.first, c.second.n, c.second.e)); w.controlTruth[c.first] = c.second; }
    job = CreatePolygonJob(w, JobOptions{k, CalcMode::Auto, trueErr}); P = static_cast<PolygonJob*>(job.get());
    P->SetNoise(TachyNoise{0, 0, 0});
  }
  bool enabled(const std::string& id) const { for (const MenuItem& m : job->Menu()) if (m.id == id) return m.enabled; return false; }
  bool listed(const std::string& id) const { for (const MenuItem& m : job->Menu()) if (m.id == id) return true; return false; }
  void Refs(bool b = true) { job->OnMenu("ref_n1:N1"); job->OnMenu("ref_a:A"); if (b) { job->OnMenu("ref_b:B"); job->OnMenu("ref_n2:N2"); } }
  void AddPoint(const GridPos& g) { w.hero = g; job->OnMenu("addpt"); }
  std::string Shoot(const GridPos& from, const GridPos& to) {
    const double dn = to.n - from.n, de = to.e - from.e, hd = std::hypot(dn, de);
    return job->TachyFire(std::atan2(de, dn) * 180.0 / kPi, std::atan2((to.h + 1.5) - (from.h + 1.5), hd) * 180.0 / kPi);
  }
  // pelne stanowisko: KL tyl, KL przod, przerzut, KP przod, KP tyl
  void Station(const GridPos& st, const GridPos& back, const GridPos& fore) {
    job->OnMenu("inst");
    job->OnMenu("prism_back"); Shoot(st, back); job->OnMenu("prism_fore"); Shoot(st, fore);
    job->TachyFlip(); Shoot(st, fore); job->OnMenu("prism_back"); Shoot(st, back);
  }
};

static double LeftAngle(const GridPos& st, const GridPos& back, const GridPos& fore) {
  return NormGon(AzimuthGon(st.n, st.e, fore.n, fore.e) - AzimuthGon(st.n, st.e, back.n, back.e));
}

static void registry_and_kind() {
  Fx f(TraverseKind::Anchored);
  assert(f.job->Kind() == JobKind::Polygon && f.job->Name().find("poligonowy") != std::string::npos && !f.job->CanFinish());
}

static void setup_refs_by_kind() {
  Fx a(TraverseKind::Anchored);
  for (const char* id : {"ref_n1", "ref_a", "ref_b", "ref_n2", "addpt", "inst", "view", "redo", "calc", "finish"}) assert(a.listed(id));
  assert(!a.enabled("inst") && !a.enabled("view") && !a.enabled("addpt"));                    // najpierw nawiazania
  assert(a.job->NeedsChoice("ref_n1") && a.job->NeedsChoice("ref_a") && !a.job->NeedsChoice("addpt"));
  assert(a.job->TachyChoices("ref_a").size() == 4);
  a.job->OnMenu("ref_n1:N1"); a.job->OnMenu("ref_a:A"); assert(!a.enabled("inst"));          // B i N2 jeszcze brak
  a.job->OnMenu("ref_b:B"); a.job->OnMenu("ref_n2:N2"); assert(a.enabled("inst") && a.enabled("addpt"));
  Fx o(TraverseKind::Open); assert(!o.listed("ref_b") && !o.listed("ref_n2"));               // wiszacy nie ma punktow koncowych
  o.Refs(false); assert(!o.enabled("inst"));                                                  // wymaga co najmniej 1 punktu nowego
  o.AddPoint(P1); assert(o.enabled("inst"));
  Fx c(TraverseKind::Closed); c.Refs(false); c.AddPoint(P1); assert(!c.enabled("inst")); c.AddPoint(P2); assert(c.enabled("inst"));     // poligon: min. 3 punkty (A + 2)
}

static void addpt_places_marker_and_names_points() {
  Fx f(TraverseKind::Anchored);
  f.AddPoint(P1); assert(f.w.placed.empty());                                                 // przed A: odmowa
  f.Refs(); f.AddPoint(P1); f.AddPoint(P2);
  assert(f.w.placed.size() == 2 && f.w.placed[0] == "marker1" && f.w.placed[1] == "marker2");
  f.w.heroOk = false; f.w.said.clear(); f.job->OnMenu("addpt"); assert(f.w.placed.size() == 2 && !f.w.said.empty());   // brak gruntu
  const PolygonReport r = f.job->FinishPolygon(false); assert(r.names.size() == 4 && r.names[0] == "A" && r.names[1] == "1" && r.names[2] == "2" && r.names[3] == "B");
}

static void station_collects_four_observations() {
  Fx f(TraverseKind::Anchored); f.Refs(); f.AddPoint(P1); f.AddPoint(P2);
  f.Station(A, N1, P1);                                                                        // stanowisko A: tyl N1, przod 1
  PolygonReport r = f.job->FinishPolygon(false);
  assert(r.stations.size() == 1 && r.stations[0].name == "A" && r.stations[0].back == "N1" && r.stations[0].fore == "1");
  assert(std::fabs(r.stations[0].betaGon - LeftAngle(A, N1, P1)) < 1e-6);                     // kolimacja, indeks i zero kola poziomego sie skracaja
  assert(!f.enabled("view"));                                                                  // po kompletnym stanowisku tachimetr trzeba postawic na nastepnym
  f.Station(P1, A, P2); r = f.job->FinishPolygon(false);
  assert(r.stations.size() == 2 && std::fabs(r.stations[1].betaGon - LeftAngle(P1, A, P2)) < 1e-6);
}

static void refuses_bad_shots() {
  Fx f(TraverseKind::Anchored); f.Refs(); f.AddPoint(P1); f.AddPoint(P2);
  f.job->OnMenu("inst"); f.job->OnMenu("prism_back");
  assert(has(f.job->TachyFire(0, 0), "celujesz"));                                              // nie celujemy w lustro
  const GridPos far{1100 + 500, 1200, 100}; f.w.bs = far;                                     // lustro za daleko (>300 m)
  assert(has(f.Shoot(A, far), "zasiegiem"));
  f.w.bs = N1; f.w.clear = false; assert(has(f.Shoot(A, N1), "zaslonieta")); f.w.clear = true;
  assert(f.job->FinishPolygon(false).stations.empty());                                         // nic nie zapisano
  assert(!has(f.Shoot(A, N1), "juz zmierzone") && has(f.Shoot(A, N1), "juz zmierzone"));      // pierwszy strzal przechodzi, powtorka w tym samym polozeniu nie
}

static void sides_measured_from_both_ends() {
  Fx f(TraverseKind::Anchored); f.Refs(); f.AddPoint(P1); f.AddPoint(P2);
  f.Station(A, N1, P1); f.Station(P1, A, P2);
  const PolygonReport r = f.job->FinishPolygon(false);
  assert(r.sides.size() == 2 && r.sides[0].from == "A" && r.sides[0].to == "1");               // bok N1-A nie jest bokiem ciagu
  assert(r.sides[0].hasFwd && r.sides[0].hasBack && std::fabs(r.sides[0].dMeanM - std::hypot(P1.n - A.n, P1.e - A.e)) < 1e-6);
  assert(r.sides[1].hasFwd && !r.sides[1].hasBack && std::fabs(r.sides[1].dMeanM - r.sides[1].dFwdM) < 1e-12 && std::fabs(r.sides[1].dMeanM - std::hypot(P2.n - P1.n, P2.e - P1.e)) < 1e-6);
}

static void redo_station_replaces_observations() {
  Fx f(TraverseKind::Anchored); f.Refs(); f.AddPoint(P1); f.AddPoint(P2);
  assert(!f.enabled("redo"));
  f.Station(A, N1, P1); assert(f.enabled("redo"));
  f.job->OnMenu("redo");                                                                        // czysci ostatnie ukonczone stanowisko
  assert(f.job->FinishPolygon(false).stations.empty());
  f.Station(A, N1, P1);                                                                         // ten sam przebieg po raz drugi nie jest odrzucany jako powtorka
  assert(f.job->FinishPolygon(false).stations.size() == 1);
}

int main() {
  registry_and_kind(); setup_refs_by_kind(); addpt_places_marker_and_names_points(); station_collects_four_observations(); refuses_bad_shots();
  sides_measured_from_both_ends(); redo_station_replaces_observations();
  std::puts("test_polygon_job OK");
  return 0;
}
