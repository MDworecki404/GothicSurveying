#include <cassert>
#include <cstdio>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "job/Jobs.hpp"
#include "job/Workflow.hpp"
#include "job/Help.hpp"

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
};

// Pozycje, ktore kontroler dokleja do Menu() roboty (Controller.hpp: CtrlOpenMenu).
static std::vector<MenuItem> ControllerExtras() {
  return { {"scope", "Luneta", "", true}, {"rodlen", "Lata", "", true}, {"level", "Poziomuj", "", true}, {"preview", "Podglad planu", "", true},
           {"map", "Mapa osnowy", "", true}, {"tp", "Teleport", "", true}, {"abort", "Przerwij pomiar", "", true} };
}

static std::vector<MenuItem> FullMenu(size_t typeIdx, FakeWorld& w) {
  JobOptions o{TraverseKind::Open, CalcMode::Auto, false};
  auto job = JobTypes()[typeIdx].create(w, o);
  std::vector<MenuItem> items = job->Menu();
  for (const MenuItem& e : ControllerExtras()) items.push_back(e);
  return Arrange(job->Kind(), items);
}

static JobKind KindOf(size_t typeIdx, FakeWorld& w) { JobOptions o{TraverseKind::Open, CalcMode::Auto, false}; return JobTypes()[typeIdx].create(w, o)->Kind(); }

static void every_menu_id_has_a_section() {
  FakeWorld w;
  for (size_t t = 0; t < JobTypes().size(); t++)
    for (const MenuItem& m : FullMenu(t, w)) {
      assert(!m.section.empty());
      if (m.id == "abort") assert(m.section == "Wyjscie"); else assert(m.section != "Wyjscie");
    }
}

static void abort_is_last() {
  FakeWorld w;
  for (size_t t = 0; t < JobTypes().size(); t++) { const auto items = FullMenu(t, w); assert(items.back().id == "abort"); }
}

static void disabled_items_have_reason() {
  FakeWorld w;
  for (size_t t = 0; t < JobTypes().size(); t++)
    for (const MenuItem& m : FullMenu(t, w)) if (!m.enabled) assert(!m.why.empty());
}

static void order_is_stable_across_kinds() {
  FakeWorld w;
  std::map<std::string, std::string> sectionOf;          // wspolne id maja te sama sekcje we wszystkich robotach
  std::map<std::string, std::vector<std::string>> idsOf;  // wzgledna kolejnosc wspolnych id jest taka sama w ALS i Foto
  for (size_t t = 0; t < JobTypes().size(); t++) {
    for (const MenuItem& m : FullMenu(t, w)) {
      auto it = sectionOf.find(m.id);
      if (it == sectionOf.end()) sectionOf[m.id] = m.section; else assert(it->second == m.section);
      idsOf[JobTypes()[t].id].push_back(m.id);
    }
  }
  const auto& a = idsOf["aerial"]; const auto& p = idsOf["photo"];
  std::vector<std::string> common;
  for (const std::string& id : a) for (const std::string& q : p) if (q == id) common.push_back(id);
  size_t pos = 0;
  for (const std::string& id : p) if (pos < common.size() && id == common[pos]) pos++;
  assert(pos == common.size());                           // wspolne id w Foto wystepuja w tej samej kolejnosci co w ALS
  assert(sectionOf["agl"] == "Parametry" && sectionOf["launch"] == "Pomiar" && sectionOf["finish"] == "Wyniki");
}

static void unknown_id_goes_to_advanced() {
  std::vector<MenuItem> in = { {"zzz_nowa", "Cos", "", true}, {"abort", "Przerwij", "", true}, {"finish", "Koniec", "", true} };
  const auto out = Arrange(JobKind::Leveling, in);
  assert(out.size() == 3 && out[0].id == "finish" && out[1].id == "zzz_nowa" && out[1].section == "Zaawansowane" && out[2].id == "abort");
}

static void hint_becomes_reason_else_table() {
  std::vector<MenuItem> in = { {"launch", "Start lotu", "", false}, {"finish", "Zakoncz", "najpierw wykonaj lot", false}, {"zzz", "X", "", false} };
  const auto out = Arrange(JobKind::Aerial, in);
  for (const MenuItem& m : out) {
    if (m.id == "launch") assert(m.why == "lot juz trwa");
    if (m.id == "finish") assert(m.why == "najpierw wykonaj lot");
    if (m.id == "zzz") assert(m.why == "niedostepne w tym kroku");
  }
}


static Project Proj() { Project p; p.name = "Kolonia_1"; p.world = "NEWWORLD"; return p; }

static void gnss_prefix_follows_base_item() {                  // GNSS wbudowany w tachimetrie ("g_*") dziedziczy sekcje pozycji bazowej
  std::vector<MenuItem> in = { {"g_start", "GNSS: start", "", true}, {"g_pole", "GNSS: tyczka", "", true}, {"meas", "Pomiar", "", true} };
  const auto out = Arrange(JobKind::Tachy, in);
  assert(out[0].id == "g_pole" && out[0].section == "Przygotowanie" && out[2].id == "g_start" && out[2].section == "Pomiar");
}
static void no_project_recommends_create() { assert(ProjectNext(Project{}, Next{}).id == "create"); }
static void running_measurement_recommends_continue_with_name() {
  Project p = Proj(); AddMeasurement(p, "tachy", "Tachimetria"); SetState(p, "m1", MeasState::Running);
  const Next n = ProjectNext(p, Next{"station", "DALEJ: Postaw tachimetr"});
  assert(n.id == "continue" && n.text.find("Tachimetria 1") != std::string::npos && n.text.find("Postaw tachimetr") != std::string::npos && n.text.find("DALEJ: Kontynuuj") == 0);
  assert(n.text.find("DALEJ: Postaw") == std::string::npos);        // bez podwojnego prefiksu
  assert(ProjectNext(p, Next{}).text.find("Tachimetria 1") != std::string::npos);
}
static void empty_project_recommends_new() { assert(ProjectNext(Proj(), Next{}).id == "new" && ProjectNext(Proj(), Next{}).text.find("kreator") != std::string::npos); }
static void all_done_recommends_new_or_close() {
  Project p = Proj(); AddMeasurement(p, "tachy", "Tachimetria"); SetState(p, "m1", MeasState::Done);
  const Next n = ProjectNext(p, Next{}); assert(n.id == "new" && n.text.find("zamknij") != std::string::npos);
}
static void closed_project_recommends_reopen() { Project p = Proj(); p.closed = true; assert(ProjectNext(p, Next{}).id == "reopen"); }

static void help_has_lines_for_every_context() {
  for (const char* c : {"home", "job:leveling", "job:gnss", "job:tachy", "job:scan", "job:aerial", "job:photo", "map"}) {
    const auto h = HelpLines(c); assert(h.size() >= 3);
    for (const std::string& l : h) { assert(l.size() <= 88); for (unsigned char ch : l) assert(ch < 128); }
  }
  assert(HelpLines("nieznany").size() >= 1);                       // nieznany kontekst: pomoc ogolna, nigdy pusto
  assert(HelpContextFor(JobKind::Photo) == "job:photo" && HelpContextFor(JobKind::Leveling) == "job:leveling");
}

int main() {
  every_menu_id_has_a_section(); abort_is_last(); disabled_items_have_reason(); order_is_stable_across_kinds(); unknown_id_goes_to_advanced(); hint_becomes_reason_else_table();
  gnss_prefix_follows_base_item(); no_project_recommends_create(); running_measurement_recommends_continue_with_name(); empty_project_recommends_new();
  all_done_recommends_new_or_close(); closed_project_recommends_reopen(); help_has_lines_for_every_context();
  FakeWorld w; (void)KindOf;
  std::puts("test_workflow OK");
  return 0;
}
