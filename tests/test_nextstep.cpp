#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
#include "job/Jobs.hpp"
#include "job/Workflow.hpp"

struct FakeWorld : IWorld {
  bool PlaceMarker(int, MarkerKind) override { return true; }
  bool FeetWithinCm(int, double) const override { return true; }
  void PlaceRod(RodRole, int) override {}
  void RemoveRod(RodRole) override {}
  bool PlaceInstrument() override { return true; }
  void RemoveInstrument() override {}
  double PointHeightM(int) const override { return 0; }
  double SightDistCm(int) const override { return 3000; }
  double Random01() override { return 0.5; }
  void ShiftInstrumentMm(double) override {}
  void Say(const std::string&) override {}
  bool PlaceTachymeter() override { return true; }
  bool PlaceBacksight(int) override { return true; }
  bool PlaceTachymeterOnPoint(int) override { return true; }
  bool PlacePrismOnPoint(int) override { return true; }
  bool PlacePole() override { return true; }
};

static std::unique_ptr<Job> Make(const std::string& typeId, FakeWorld& w, TraverseKind k = TraverseKind::Open) {
  for (const JobType& t : JobTypes()) if (t.id == typeId) { JobOptions o{k, CalcMode::Auto, false}; return t.create(w, o); }
  assert(false); return nullptr;
}

// NextStep wskazuje pozycje Menu() roboty (aktywna) albo pozycje dokladana przez kontroler (scope).
static void check_enabled(const Job& j) {
  const std::string id = j.NextStep();
  if (id.empty() || id == "scope") return;
  bool found = false;
  for (const MenuItem& m : j.Menu()) if (m.id == id) { found = true; assert(m.enabled); }
  assert(found);
}

static void leveling_fresh_asks_for_benchmark() { FakeWorld w; auto j = Make("traverse", w); assert(j->NextStep() == "reper_a"); check_enabled(*j); }

static void leveling_advances_after_reper_and_instrument() {
  FakeWorld w; auto j = Make("traverse", w);
  j->OnMenu("reper_a"); assert(j->NextStep() == "instrument"); check_enabled(*j);
  j->OnMenu("instrument"); assert(j->NextStep() == "rod_back"); check_enabled(*j);
  j->OnMenu("rod_back"); assert(j->NextStep() == "rod_fore_turn"); check_enabled(*j);
  j->OnMenu("rod_fore_turn"); assert(j->NextStep() == "scope");
}

static void trig_fresh_asks_for_benchmark() {
  FakeWorld w; auto j = Make("trig", w);
  assert(j->NextStep() == "reper_a"); check_enabled(*j);
  j->OnMenu("reper_a"); assert(j->NextStep() == "inst"); check_enabled(*j);
  j->OnMenu("inst"); assert(j->NextStep() == "prism"); check_enabled(*j);
  j->OnMenu("prism"); assert(j->NextStep() == "view"); check_enabled(*j);
}

static void gnss_fresh_asks_for_pole() {
  FakeWorld w; auto j = Make("gnss", w);
  assert(j->NextStep() == "pole"); check_enabled(*j);
  j->OnMenu("pole"); assert(j->NextStep() == "start"); check_enabled(*j);
}

static void tachy_fresh_asks_for_station() {
  FakeWorld w; auto j = Make("tachy", w);
  assert(j->NextStep() == "station"); check_enabled(*j);
  j->OnMenu("station"); assert(j->NextStep() == "backsight"); check_enabled(*j);
  j->OnMenu("backsight"); assert(j->NextStep() == "view"); check_enabled(*j);
}

static void nextline_uses_label_and_falls_back_to_finish() {
  std::vector<MenuItem> items = { {"a", "Postaw cos", "", true}, {"b", "Inne", "", false}, {"finish", "Zakoncz robote", "", true} };
  assert(NextLine(items, "a").id == "a" && NextLine(items, "a").text == "DALEJ: Postaw cos");
  assert(NextLine(items, "").id == "finish" && NextLine(items, "").text == "DALEJ: Zakoncz pomiar");
  assert(NextLine(items, "b").id == "finish");            // niedostepny krok: spada na zakonczenie
  assert(NextLine(items, "zzz").id == "finish");          // nieznany krok tez
  items[2].enabled = false;
  assert(NextLine(items, "").id.empty() && NextLine(items, "").text.empty());
}

int main() {
  leveling_fresh_asks_for_benchmark(); leveling_advances_after_reper_and_instrument(); trig_fresh_asks_for_benchmark();
  gnss_fresh_asks_for_pole(); tachy_fresh_asks_for_station(); nextline_uses_label_and_falls_back_to_finish();
  std::puts("test_nextstep OK");
  return 0;
}
