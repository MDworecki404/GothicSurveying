#include <cassert>
#include <cmath>
#include <cstdio>
#include <map>
#include <string>
#include <vector>
#include "job/Jobs.hpp"
#include "job/PolygonJob.hpp"

static bool has(const std::string& s, const std::string& part) { return s.find(part) != std::string::npos; }
static const double kPi = 3.14159265358979323846;

// Sztuczny swiat: prawdziwe pozycje znakow, tachimetr i lustro "na punkcie" ustawiaja sie dokladnie nad znakiem.
struct FakePoly : IWorld {
  std::map<std::string, GridPos> controlTruth; std::vector<ControlPoint> controls;
  GridPos hero{0, 0, 100}, station{0, 0, 100}, pole{0, 0, 100}, bs{0, 0, 100}; bool heroOk = true, clear = true;
  std::map<int, GridPos> marker; std::vector<std::string> said, placed;
  void RemoveMarker(int id) override { marker.erase(id); placed.push_back("unmarker" + std::to_string(id)); }
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
  void RemoveBacksight(int) override { placed.push_back("unbacksight"); }
  void RemovePole() override { placed.push_back("unpole"); }
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
  { bool found = false; for (const JobType& t : JobTypes()) if (t.id == "polygon") found = t.polygon && !t.gnss && !t.tachy && !t.aerial && t.name.find("poligonowy") != std::string::npos; assert(found); }
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

static void delpt_removes_last_mid_point_only() {
  Fx f(TraverseKind::Anchored); f.Refs();
  assert(!f.enabled("delpt"));                                                                // brak punktow ciagu
  f.AddPoint(P1); f.AddPoint(P2); assert(f.enabled("delpt"));
  f.job->OnMenu("delpt");
  assert(f.w.placed.back() == "unmarker2" && f.w.marker.count(1) && !f.w.marker.count(2));
  PolygonReport r = f.job->FinishPolygon(false); assert(r.names.size() == 3 && r.names[1] == "1" && r.names[2] == "B");   // osnowa (A, B) zostaje
  f.AddPoint(P2); assert(f.w.placed.back() == "marker2");                                      // numeracja wraca do 2
  f.job->OnMenu("delpt"); f.job->OnMenu("delpt"); assert(!f.enabled("delpt"));                // pusto: A i B nietkniete
  r = f.job->FinishPolygon(false); assert(r.names.size() == 2 && r.names[0] == "A" && r.names[1] == "B");
}

static void prism_is_removed_when_instrument_moves() {
  Fx f(TraverseKind::Anchored); f.Refs(); f.AddPoint(P1);
  f.job->OnMenu("inst"); f.job->OnMenu("prism_fore");                                          // lustro na punkcie 1
  const size_t n = f.w.placed.size();
  f.job->OnMenu("prism_back"); assert(f.w.placed.back() == "prism@N1" && f.w.placed[n] == "unpole");   // z tyczki na osnowe: tyczka znika
  f.job->OnMenu("prism_fore"); assert(f.w.placed.back() == "prism-on-1");
  f.job->OnMenu("inst");                                                                      // stanowisko na A (jeszcze bez pomiaru), lustro z punktu 1 znika (tyczka na znaku)
  assert(f.w.placed.back() == "unpole");
  assert(!f.enabled("view"));                                                                 // lustro trzeba postawic na nowo
}

static void points_can_be_added_during_measurement() {
  Fx f(TraverseKind::Anchored); f.Refs(); f.AddPoint(P1);
  f.Station(A, N1, P1);                                                                        // zmierzone A: bok A-1 nie zmieni sie przy dodaniu 2
  assert(f.enabled("addpt") && !f.enabled("delpt"));                                           // dodac mozna, usunac 1 nie (zmienilby przod A)
  f.AddPoint(P2); assert(f.w.marker.count(2));
  assert(f.enabled("addpt") && f.enabled("delpt"));                                            // stanowisko 1 (przod 2) niezmierzone: 2 mozna jeszcze usunac
  f.Station(P1, A, P2);
  assert(f.enabled("addpt") && !f.enabled("delpt"));                                           // dodanie 3 nie zmienia zmierzonego stanowiska 1
  f.AddPoint(B); assert(f.w.marker.count(3));
  Fx g(TraverseKind::Anchored); g.Refs(); g.AddPoint(P1); g.AddPoint(P2);
  g.Station(A, N1, P1); g.Station(P1, A, P2); g.Station(P2, P1, B);                            // zmierzone P2 (przod B): dodanie 3 zmienilo by jego przod
  assert(!g.enabled("addpt") && !g.enabled("delpt"));
  Fx h(TraverseKind::Closed); h.Refs(false); h.AddPoint(P1); h.AddPoint(P2);
  h.Station(A, P2, P1);                          // poligon zamkniety: po pierwszym pomiarze bez zmian
  assert(!h.enabled("addpt") && !h.enabled("delpt"));
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

static void RunAnchored(Fx& f) {                                  // pelny przebieg przykladu: stanowiska A, 1, 2, B
  f.Station(A, N1, P1); f.Station(P1, A, P2); f.Station(P2, P1, B); f.Station(B, P2, N2);
}

static void full_anchored_matches_truth() {
  Fx f(TraverseKind::Anchored, true); f.Refs(); f.AddPoint(P1); f.AddPoint(P2);
  assert(!f.job->CanFinish() && !f.enabled("finish") && !f.enabled("calc"));
  RunAnchored(f);
  assert(f.job->CanFinish() && f.enabled("finish") && f.enabled("calc"));
  const PolygonReport r = f.job->FinishPolygon(true);
  assert(r.res.valid && r.res.n == 4 && r.stations.size() == 4 && r.sides.size() == 3);
  assert(std::fabs(r.res.fBetaGon) < 1e-6 && r.res.fd < 1e-5 && r.res.angleOk && r.res.linearOk);   // dane dokladne: odchylki zerowe
  assert(std::fabs(r.res.x[1] - P1.n) < 1e-6 && std::fabs(r.res.y[1] - P1.e) < 1e-6 && std::fabs(r.res.x[2] - P2.n) < 1e-6 && std::fabs(r.res.y[2] - P2.e) < 1e-6);
  assert(r.showTrueError && r.trueN.size() == 4 && std::fabs(r.trueN[1] - P1.n) < 1e-9);
  assert(!f.job->SummaryLines().empty());
}

static void calc_preview_before_finish() {
  Fx f(TraverseKind::Anchored); f.Refs(); f.AddPoint(P1); f.AddPoint(P2);
  RunAnchored(f);
  f.w.said.clear(); f.job->OnMenu("calc");
  bool beta = false, dist = false; for (const std::string& s : f.w.said) { if (has(s, "f_beta")) beta = true; if (has(s, "f_d")) dist = true; } assert(beta && dist);
}

static void bad_angle_flagged_then_redo_fixes() {
  Fx f(TraverseKind::Anchored); f.Refs(); f.AddPoint(P1); f.AddPoint(P2);
  f.Station(A, N1, P1);
  f.P->SetNoise(TachyNoise{20000.0, 0, 0}); f.Station(P1, A, P2); f.P->SetNoise(TachyNoise{0, 0, 0});     // zly kat na stanowisku 1 (ok. 5 g bledu)
  f.Station(P2, P1, B); f.Station(B, P2, N2);
  PolygonReport r = f.job->FinishPolygon(true);
  assert(r.res.valid && !r.res.angleOk);
  f.w.said.clear(); f.job->OnMenu("calc"); bool exceeded = false; for (const std::string& s : f.w.said) if (has(s, "PRZEKROCZONE")) exceeded = true; assert(exceeded);
  assert(f.job->NeedsChoice("redo") && f.job->TachyChoices("redo").size() == 4);                     // wiele stanowisk: wybor z listy
  f.job->OnMenu("redo:1");
  assert(!f.job->CanFinish());
  f.Station(P1, A, P2);                                                                              // poprawny pomiar tego samego stanowiska
  r = f.job->FinishPolygon(true);
  assert(r.res.valid && r.res.angleOk && std::fabs(r.res.fBetaGon) < 1e-6);
}

static const GridPos C1{1200, 1120, 100}, C2{1260, 1230, 100}, C3{1180, 1320, 100};   // poligon A-C1-C2-C3 (A = (1100,1200))

static double Area(const std::vector<GridPos>& p) { double a = 0; for (size_t i = 0; i < p.size(); i++) { const size_t j = (i + 1) % p.size(); a += p[i].n * p[j].e - p[j].n * p[i].e; } return a; }

static void closed_and_hanging_flows() {
  Fx c(TraverseKind::Closed); c.job->OnMenu("ref_n1:N1"); c.job->OnMenu("ref_a:A");
  c.AddPoint(C1); c.AddPoint(C2); c.AddPoint(C3);
  const GridPos pts[4] = {A, C1, C2, C3};
  for (int i = 0; i < 4; i++) {
    const GridPos &st = pts[i], &bk = pts[(i + 3) % 4], &fr = pts[(i + 1) % 4];
    c.job->OnMenu("inst");
    c.job->OnMenu("prism_back"); c.Shoot(st, bk); c.job->OnMenu("prism_fore"); c.Shoot(st, fr);
    if (i == 0) { c.job->OnMenu("prism_n1"); c.Shoot(st, N1); }                                      // orientacja na N1 w KL
    c.job->TachyFlip();
    if (i == 0) { c.Shoot(st, N1); }
    c.job->OnMenu("prism_fore"); c.Shoot(st, fr); c.job->OnMenu("prism_back"); c.Shoot(st, bk);
  }
  assert(c.job->CanFinish());
  const PolygonReport r = c.job->FinishPolygon(true);
  assert(r.res.valid && r.res.clockwise == (Area({A, C1, C2, C3}) > 0) && std::fabs(r.res.fBetaGon) < 1e-6 && r.res.fd < 1e-5 && r.res.angleOk && r.res.linearOk);
  assert(r.sides.size() == 4 && r.sides[3].from == "3" && r.sides[3].to == "A" && r.sides[3].hasFwd && r.sides[3].hasBack);   // bok zamykajacy mierzony z obu koncow
  assert(std::fabs(r.res.x[1] - C1.n) < 1e-6 && std::fabs(r.res.y[2] - C2.e) < 1e-6);

  Fx h(TraverseKind::Open); h.Refs(false); h.AddPoint(P1); h.AddPoint(P2);
  h.Station(A, N1, P1); h.Station(P1, A, P2);                                                         // stanowiska A i 1 (P2 to koniec ciagu bez stanowiska)
  assert(h.job->CanFinish() && h.enabled("finish"));
  const PolygonReport hr = h.job->FinishPolygon(true);
  assert(hr.res.valid && !hr.res.hasAngleCheck && hr.res.x.size() == 3 && std::fabs(hr.res.x[1] - P1.n) < 1e-6 && std::fabs(hr.res.y[2] - P2.e) < 1e-6);
  assert(hr.sides.size() == 2 && hr.sides[1].hasFwd && !hr.sides[1].hasBack);
}

static void save_load_midstation() {
  Fx a(TraverseKind::Anchored); a.Refs(); a.AddPoint(P1); a.AddPoint(P2);
  a.Station(A, N1, P1);
  a.job->OnMenu("inst"); a.job->OnMenu("prism_back"); a.Shoot(P1, A); a.job->OnMenu("prism_fore"); a.Shoot(P1, P2);    // 2 z 4 obserwacji stanowiska 1
  StateKV kv; a.job->SaveState(kv);
  Fx b(TraverseKind::Anchored);                                                                         // nowa robota, ten sam swiat (w grze swiat przywraca gWorld.LoadState)
  b.w.controls = a.w.controls; b.w.controlTruth = a.w.controlTruth; b.w.marker = a.w.marker; b.w.station = a.w.station; b.w.pole = a.w.pole; b.w.bs = a.w.bs;
  assert(b.job->LoadState(StateKV::FromText(kv.ToText())));
  b.job->TachyFlip(); b.Shoot(P1, P2); b.job->OnMenu("prism_back"); b.Shoot(P1, A);                    // dokonczenie stanowiska 1 w KP
  b.Station(P2, P1, B); b.Station(B, P2, N2);
  Fx ref(TraverseKind::Anchored); ref.Refs(); ref.AddPoint(P1); ref.AddPoint(P2); RunAnchored(ref);
  const PolygonReport rb = b.job->FinishPolygon(true), rr = ref.job->FinishPolygon(true);
  assert(rb.res.valid && rb.stations.size() == 4 && std::fabs(rb.res.fBetaGon - rr.res.fBetaGon) < 1e-6 && std::fabs(rb.res.x[2] - rr.res.x[2]) < 1e-6);
  Fx other(TraverseKind::Closed); assert(!other.job->LoadState(StateKV::FromText(kv.ToText())));       // inny rodzaj ciagu: stan nie pasuje
  assert(a.job->CanSaveState());
}

static void nextstep_always_enabled_or_special() {
  Fx f(TraverseKind::Anchored);
  auto ok = [&]() { const std::string id = f.job->NextStep(); assert(!id.empty()); bool found = false; for (const MenuItem& m : f.job->Menu()) if (m.id == id) { found = true; assert(m.enabled); } assert(found); return id; };
  assert(ok() == "ref_n1"); f.job->OnMenu("ref_n1:N1"); assert(ok() == "ref_a"); f.job->OnMenu("ref_a:A"); assert(ok() == "ref_b");
  f.job->OnMenu("ref_b:B"); assert(ok() == "ref_n2"); f.job->OnMenu("ref_n2:N2"); assert(ok() == "addpt");
  f.AddPoint(P1); f.AddPoint(P2); assert(ok() == "inst");
  f.job->OnMenu("inst"); assert(ok() == "prism_back");
  f.job->OnMenu("prism_back"); assert(ok() == "view"); f.Shoot(A, N1);
  assert(ok() == "prism_fore"); f.job->OnMenu("prism_fore"); assert(ok() == "view"); f.Shoot(A, P1);
  assert(ok() == "view");                                                                          // komplet w KL: przerzut klawiszem K w lunecie
  f.job->TachyFlip(); assert(ok() == "view"); f.Shoot(A, P1); assert(ok() == "prism_back"); f.job->OnMenu("prism_back"); f.Shoot(A, N1);
  assert(ok() == "inst");
  f.Station(P1, A, P2); f.Station(P2, P1, B); f.Station(B, P2, N2);
  assert(ok() == "calc"); f.job->OnMenu("calc"); assert(f.job->NextStep().empty());                // po podgladzie: tylko zakonczenie (linia DALEJ spadnie na "finish")
}

static void aiming_within_tolerance_does_not_spoil_angle() {      // celowanie w granicach tolerancji (0.01 st. = ok. 3 cm na 170 m) nie moze psuc kata: bledy daje tylko instrument
  Fx f(TraverseKind::Anchored); f.Refs(); f.AddPoint(P1); f.AddPoint(P2);
  f.job->OnMenu("inst");
  auto shootOff = [&](const GridPos& to, double dAz) {
    const double dn = to.n - A.n, de = to.e - A.e;
    return f.job->TachyFire(std::atan2(de, dn) * 180.0 / kPi + dAz, 0.0);
  };
  f.job->OnMenu("prism_back"); assert(!has(shootOff(N1, +0.01), "celujesz")); f.job->OnMenu("prism_fore"); assert(!has(shootOff(P1, -0.01), "celujesz"));
  f.job->TachyFlip(); assert(!has(shootOff(P1, +0.008), "celujesz")); f.job->OnMenu("prism_back"); assert(!has(shootOff(N1, -0.008), "celujesz"));
  assert(std::fabs(f.job->FinishPolygon(false).stations.at(0).betaGon - LeftAngle(A, N1, P1)) < 1e-6);
}

static void reinstalling_instrument_starts_station_over() {       // nowe postawienie tachimetru = nowe zero kola i bledy: stare obserwacje stanowiska nie moga zostac
  Fx f(TraverseKind::Anchored); f.Refs(); f.AddPoint(P1); f.AddPoint(P2);
  f.job->OnMenu("inst"); f.job->OnMenu("prism_back"); f.Shoot(A, N1);
  assert(f.enabled("redo"));
  f.job->OnMenu("inst");
  assert(!f.enabled("redo"));
  f.Station(A, N1, P1);
  assert(std::fabs(f.job->FinishPolygon(false).stations.at(0).betaGon - LeftAngle(A, N1, P1)) < 1e-6);
  f.job->OnMenu("prism_n1"); assert(!f.enabled("redo") || f.job->FinishPolygon(false).stations.size() == 1);   // prism_n1 nie istnieje poza poligonem: ignorowane
}

int main() {
  registry_and_kind(); setup_refs_by_kind(); addpt_places_marker_and_names_points(); delpt_removes_last_mid_point_only(); prism_is_removed_when_instrument_moves(); points_can_be_added_during_measurement(); station_collects_four_observations(); refuses_bad_shots();
  sides_measured_from_both_ends(); redo_station_replaces_observations();
  full_anchored_matches_truth(); calc_preview_before_finish(); bad_angle_flagged_then_redo_fixes(); closed_and_hanging_flows(); save_load_midstation(); nextstep_always_enabled_or_special();
  aiming_within_tolerance_does_not_spoil_angle(); reinstalling_instrument_starts_station_over();
  std::puts("test_polygon_job OK");
  return 0;
}
