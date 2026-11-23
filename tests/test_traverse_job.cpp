#include <cassert>
#include <cmath>
#include <cstdio>
#include <map>
#include <string>
#include <vector>
#include "job/Jobs.hpp"

static bool near(double a, double b, double eps = 1e-6) { return std::fabs(a - b) <= eps; }
static bool has(const std::string& s, const std::string& part) { return s.find(part) != std::string::npos; }

struct FakeWorld : IWorld {
  std::vector<std::string> calls, said;
  bool feetNear = true, ground = true;
  std::map<int, double> heights;
  bool PlaceMarker(int id, MarkerKind k) override { calls.push_back("marker " + std::to_string(id) + (k == MarkerKind::Reper ? " reper" : " turning")); return ground; }
  bool FeetWithinCm(int, double) const override { return feetNear; }
  void PlaceRod(RodRole r, int id) override { calls.push_back(std::string("place_rod ") + (r == RodRole::Back ? "back " : "fore ") + std::to_string(id)); }
  void RemoveRod(RodRole r) override { calls.push_back(std::string("remove_rod ") + (r == RodRole::Back ? "back" : "fore")); }
  bool PlaceInstrument() override { calls.push_back("place_instrument"); return ground; }
  void RemoveInstrument() override { calls.push_back("remove_instrument"); }
  double PointHeightM(int id) const override { auto it = heights.find(id); return it == heights.end() ? 0.0 : it->second; }
  double SightDistCm(int) const override { return 3000; }
  std::vector<double> rnd; size_t rndAt = 0; double shifted = 0;
  double Random01() override { return rndAt < rnd.size() ? rnd[rndAt++] : 0.5; }
  void ShiftInstrumentMm(double dy) override { shifted += dy; }
  void Say(const std::string& t) override { said.push_back(t); }
};

struct Fx {
  FakeWorld w; std::unique_ptr<Job> job;
  explicit Fx(TraverseKind k = TraverseKind::Anchored) {
    w.heights[1] = 100.000; w.heights[4] = 100.240;                 // A = 1, B = 4 przy 3 stanowiskach
    job = CreateLevelingTraverse(w, JobOptions{k, CalcMode::Auto});
  }
  bool enabled(const std::string& id) const { for (auto& m : job->Menu()) if (m.id == id) return m.enabled; return false; }
  void menu(const std::string& id) { job->OnMenu(id); }
  bool read(int p, int mid) { return job->OnReading(p, mid); }
};

static void menu_at_start_only_allows_reper_a() {
  Fx f;
  assert(f.enabled("reper_a"));
  for (const char* id : {"instrument", "rod_back", "rod_fore_turn", "rod_fore_end", "move_next", "finish"}) assert(!f.enabled(id));
  assert(!f.job->CanFinish());
}

static void rod_fore_blocked_until_instrument_and_back_rod() {
  Fx f; f.menu("reper_a");
  assert(!f.enabled("rod_fore_turn") && !f.enabled("rod_back") && f.enabled("instrument"));
  size_t before = f.w.calls.size();
  f.menu("rod_fore_turn");                                          // niedozwolone: nic się nie dzieje
  assert(f.w.calls.size() == before);
  f.menu("instrument"); assert(!f.enabled("rod_fore_turn") && f.enabled("rod_back"));
  f.menu("rod_back");   assert(f.enabled("rod_fore_turn") && f.enabled("rod_fore_end"));
}

static void do_stand(Fx& f, int backPoint, int backMm, const char* foreMenu, int forePoint, int foreMm, bool carriedBack) {
  f.menu("instrument");
  if (!carriedBack) f.menu("rod_back");
  assert(f.read(backPoint, backMm));
  f.menu(foreMenu);
  assert(f.read(forePoint, foreMm));
}

static void full_anchored_run_produces_expected_fieldbook() {
  Fx f; f.menu("reper_a");
  do_stand(f, 1, 1500, "rod_fore_turn", 2, 1200, false); f.menu("move_next");
  do_stand(f, 2, 1400, "rod_fore_turn", 3, 1650, true);  f.menu("move_next");
  assert(!f.job->CanFinish());
  do_stand(f, 3, 1300, "rod_fore_end", 4, 1100, true);
  assert(f.job->CanFinish() && f.enabled("finish"));
  FieldBook fb = f.job->Finish(true);
  assert(fb.complete && fb.rows.size() == 3 && fb.rows[2].pointName == "B");
  assert(near(fb.sum.fhMm, 10.0, 1e-6) && near(fb.rows[0].dhMm, 300) && near(fb.startHeightM, 100.000));
}

static void move_next_swaps_roles_and_removes_instrument() {
  Fx f; f.menu("reper_a");
  do_stand(f, 1, 1500, "rod_fore_turn", 2, 1200, false);
  f.w.calls.clear(); f.menu("move_next");
  assert((f.w.calls == std::vector<std::string>{"remove_instrument", "remove_rod back", "place_rod back 2", "remove_rod fore"}));
  assert(f.enabled("instrument") && !f.enabled("rod_back"));         // łata wstecz już stoi na punkcie 2
}

static void closed_end_requires_feet_on_reper_a() {
  Fx f(TraverseKind::Closed); f.menu("reper_a"); f.menu("instrument"); f.menu("rod_back"); assert(f.read(1, 1500));
  f.w.feetNear = false;
  f.menu("rod_fore_end");
  assert(!f.w.said.empty() && has(f.w.said.back(), "reperze A"));
  for (auto& c : f.w.calls) assert(!has(c, "place_rod fore"));
  assert(f.enabled("rod_fore_end"));
  f.w.feetNear = true; f.menu("rod_fore_end");
  assert(f.w.calls.back() == "place_rod fore 1");
}

static void open_end_finishes_without_closure() {
  Fx f(TraverseKind::Open); f.menu("reper_a");
  do_stand(f, 1, 1500, "rod_fore_end", 2, 1200, false);
  assert(f.job->CanFinish());
  FieldBook fb = f.job->Finish(true);
  assert(!fb.sum.hasClosure && fb.rows.size() == 1 && near(fb.rows[0].dhMm, 300));
}

static void finish_disabled_until_end_point_read() {
  Fx f; f.menu("reper_a"); assert(!f.job->CanFinish());
  f.menu("instrument"); f.menu("rod_back"); assert(f.read(1, 1500));
  f.menu("rod_fore_end"); assert(!f.job->CanFinish());                // koniec postawiony, brak odczytu wprzód
  assert(f.read(2, 1200)); assert(f.job->CanFinish());                // punkt końcowy ma id 2 (A = 1)
}

static void abort_without_stands_returns_incomplete_empty_book() {
  Fx f; FieldBook fb = f.job->Finish(false);
  assert(!fb.complete && fb.rows.empty());
  f.menu("reper_a"); fb = f.job->Finish(false);
  assert(!fb.complete && fb.rows.empty());
}

static void reading_rejects_out_of_range() {
  Fx f; f.menu("reper_a"); f.menu("instrument"); f.menu("rod_back");
  assert(!f.read(1, 5001));
  assert(!f.read(1, -5));
  assert(f.read(1, 1500));
}

static void reading_on_unplaced_rod_is_rejected() {
  Fx f; assert(!f.read(1, 1000));
  f.menu("reper_a"); assert(!f.read(1, 1000));                        // brak niwelatora i łaty
  f.menu("instrument"); assert(!f.read(1, 1000));                     // brak łaty wstecz
  f.menu("rod_back"); assert(!f.read(9, 1000));                       // zły punkt
  assert(f.read(1, 1000)); assert(!f.read(1, 1000));                  // powtórny odczyt tej samej łaty odrzucony
}

static void readings_order_is_free() {
  Fx f(TraverseKind::Open); f.menu("reper_a"); f.menu("instrument"); f.menu("rod_back");
  f.menu("rod_fore_end");
  assert(f.read(2, 1200)); assert(f.read(1, 1500));                   // najpierw wprzód, potem wstecz
  assert(near(f.job->Finish(true).rows[0].dhMm, 300));
}

static void job_registry_lists_traverse() {
  const auto& types = JobTypes();
  assert(!types.empty() && types[0].id == "traverse" && types[0].name == "Ciag niwelacyjny");
  FakeWorld w; auto j = types[0].create(w, JobOptions{TraverseKind::Open, CalcMode::Auto});
  assert(j && j->Name() == "Ciag niwelacyjny");
}

static void stadia_is_measured_automatically() {
  Fx f; f.menu("reper_a"); f.menu("instrument"); f.menu("rod_back");
  assert(f.read(1, 1500));                                            // celowa 30 m (FakeWorld) -> D = 30,0 m
  f.menu("rod_fore_end"); assert(f.read(2, 1200));
  FieldBook fb = f.job->Finish(true);
  assert(near(fb.rows[0].stadiaBackCm, 3000.0, 1e-6) && near(fb.rows[0].stadiaForeCm, 3000.0, 1e-6));
}

static void rod_point_for_reports_roles() {
  Fx f; f.menu("reper_a");
  assert(f.job->RodPointFor(RodRole::Back) < 0 && f.job->RodPointFor(RodRole::Fore) < 0);
  f.menu("instrument"); f.menu("rod_back");
  assert(f.job->RodPointFor(RodRole::Back) == 1 && f.job->RodPointFor(RodRole::Fore) < 0);
  f.menu("rod_fore_turn");
  assert(f.job->RodPointFor(RodRole::Fore) == 2);
}

static void obsadzka_takes_second_set_and_averages() {
  Fx f(TraverseKind::Open); f.w.rnd = {0.9, 1.0};                    // znak +, wielkość 7 mm
  f.menu("reper_a"); f.menu("instrument"); f.menu("rod_back");
  assert(!f.enabled("obsadzka"));                                    // dopiero po obu odczytach
  assert(f.read(1, 1500)); assert(!f.enabled("obsadzka"));
  f.menu("rod_fore_end"); assert(f.read(2, 1200));
  assert(f.enabled("obsadzka") && f.job->CanFinish());
  f.menu("obsadzka");
  assert(near(f.w.shifted, 7.0) && !f.job->CanFinish() && !f.enabled("obsadzka") && !f.enabled("instrument") && !f.enabled("finish"));
  assert(f.read(1, 1507)); assert(f.read(2, 1209));                  // niwelator wyżej o 7 mm: oba odczyty większe
  assert(f.job->CanFinish());
  FieldBook fb = f.job->Finish(true);
  assert(near(fb.rows[0].dhMm, 299.0) && fb.rows[0].back2Mm == 1507 && fb.rows[0].fore2Mm == 1209 && near(fb.rows[0].dh2DiffMm, 2.0));
}

static void abort_during_second_set_keeps_first_set() {
  Fx f(TraverseKind::Open); f.menu("reper_a"); f.menu("instrument"); f.menu("rod_back"); assert(f.read(1, 1500));
  f.menu("rod_fore_end"); assert(f.read(2, 1200)); f.menu("obsadzka"); assert(f.read(1, 1503));   // drugi komplet niepełny
  FieldBook fb = f.job->Finish(false);
  assert(fb.rows.size() == 1 && fb.rows[0].backMm == 1500 && fb.rows[0].foreMm == 1200 && fb.rows[0].back2Mm < 0 && near(fb.rows[0].dhMm, 300));
}

static void redo_readings_allows_second_reading() {
  Fx f; f.menu("reper_a"); f.menu("instrument"); f.menu("rod_back");
  assert(!f.enabled("redo_readings"));                                // nic jeszcze nie odczytano
  assert(f.read(1, 1500)); assert(!f.read(1, 1510));
  assert(f.enabled("redo_readings"));
  f.menu("redo_readings");
  assert(f.read(1, 1510));                                            // poprawiony odczyt
  f.menu("rod_fore_end"); assert(f.read(2, 1200));
  assert(near(f.job->Finish(true).rows[0].dhMm, 310));
}

static void instrument_can_be_repositioned_before_readings() {
  Fx f; f.menu("reper_a"); f.menu("instrument");
  assert(f.enabled("instrument"));                                    // można przestawić
  f.menu("instrument");
  int placed = 0; for (auto& c : f.w.calls) if (c == "place_instrument") placed++;
  assert(placed == 2);
  f.menu("rod_back"); assert(f.read(1, 1500));
  assert(!f.enabled("instrument"));                                   // po odczycie już nie
}

static void state_roundtrip_mid_traverse() {
  Fx f; f.menu("reper_a");
  do_stand(f, 1, 1500, "rod_fore_turn", 2, 1200, false); f.menu("move_next");
  f.menu("instrument"); assert(f.read(2, 1400));                                 // w trakcie drugiego stanowiska: odczyt wstecz, bez wprzod
  StateKV st; assert(f.job->CanSaveState()); f.job->SaveState(st);
  Fx g; assert(g.job->LoadState(StateKV::FromText(st.ToText())));
  for (const char* id : {"reper_a", "instrument", "rod_back", "rod_fore_turn", "rod_fore_end", "move_next", "obsadzka", "finish"}) assert(f.enabled(id) == g.enabled(id));
  assert(f.job->Hud() == g.job->Hud());
  g.menu("rod_fore_end"); assert(g.read(3, 1100));                                  // dalszy ciag dziala: odczyt wprzod, potem koniec
  const FieldBook a = g.job->Finish(true);
  assert(a.rows.size() == 2 && a.rows[0].backMm == 1500 && a.rows[1].backMm == 1400 && a.rows[1].foreMm == 1100);
}

int main() {
  menu_at_start_only_allows_reper_a(); rod_fore_blocked_until_instrument_and_back_rod();
  full_anchored_run_produces_expected_fieldbook(); move_next_swaps_roles_and_removes_instrument();
  closed_end_requires_feet_on_reper_a(); open_end_finishes_without_closure(); finish_disabled_until_end_point_read();
  abort_without_stands_returns_incomplete_empty_book(); reading_rejects_out_of_range(); reading_on_unplaced_rod_is_rejected();
  readings_order_is_free(); job_registry_lists_traverse();
  obsadzka_takes_second_set_and_averages(); abort_during_second_set_keeps_first_set(); stadia_is_measured_automatically(); rod_point_for_reports_roles(); redo_readings_allows_second_reading(); instrument_can_be_repositioned_before_readings();
  state_roundtrip_mid_traverse();
  std::puts("test_traverse_job OK");
  return 0;
}
