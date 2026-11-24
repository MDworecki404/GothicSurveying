#include <cassert>
#include <cstdio>
#include <set>
#include <string>
#include "fake_aerial.hpp"
#include "job/Wizards.hpp"

static std::string LabelOf(const Job& j, const std::string& id) { for (const MenuItem& m : j.Menu()) if (m.id == id) return m.label; return ""; }
static bool EnabledOf(const Job& j, const std::string& id) { for (const MenuItem& m : j.Menu()) if (m.id == id) return m.enabled; return false; }

static void six_wizards_exist() {
  const auto& w = Wizards(); assert(w.size() == 6);
  const char* ids[] = {"osnowa", "niwelacja", "tachy_inwent", "skan_budynku", "nalot_als", "zdjecia"};
  for (size_t i = 0; i < 6; i++) { assert(w[i].id == ids[i] && !w[i].name.empty() && !w[i].desc.empty()); }
  std::set<std::string> uniq; for (const Wizard& x : w) uniq.insert(x.id); assert(uniq.size() == 6);
}

static void every_typeid_exists_in_jobtypes() {
  for (const Wizard& x : Wizards()) { bool found = false; for (const JobType& t : JobTypes()) if (t.id == x.typeId) found = true; assert(found); }
}

static void setup_steps_apply_in_fresh_menu() {                   // kazdy krok: pozycja istnieje, jest aktywna, a jej etykieta zawiera warunek
  for (const Wizard& x : Wizards()) {
    FakeAerial w; w.dir = (std::filesystem::temp_directory_path() / ("gs_wiz_" + x.id)).string();
    auto job = StartWizardUnapplied(w, x);
    for (const WizStep& s : x.setup) { assert(EnabledOf(*job, s.id)); assert(LabelOf(*job, s.id).find(s.whenLabel) != std::string::npos); }
  }
}

static void als_wizard_turns_calibration_on() {
  const Wizard* als = nullptr; for (const Wizard& x : Wizards()) if (x.id == "nalot_als") als = &x; assert(als);
  FakeAerial w; w.dir = (std::filesystem::temp_directory_path() / "gs_wiz_als2").string();
  auto before = StartWizardUnapplied(w, *als); assert(LabelOf(*before, "calib").find("wylaczone") != std::string::npos);
  auto job = StartWizard(w, *als); assert(LabelOf(*job, "calib").find("3 poprzeczne") != std::string::npos);
}

static void setup_is_idempotent_when_state_already_matches() {   // gdy swiat ma juz wlaczona opcje, krok jej nie wylacza
  const Wizard* als = nullptr; for (const Wizard& x : Wizards()) if (x.id == "nalot_als") als = &x;
  FakeAerial w; w.dir = (std::filesystem::temp_directory_path() / "gs_wiz_als3").string();
  auto job = StartWizard(w, *als); ApplyWizardSetup(*job, *als);   // drugi raz: etykieta nie zawiera juz warunku
  assert(LabelOf(*job, "calib").find("3 poprzeczne") != std::string::npos);
}

static void osnowa_uses_static_mode_and_niwelacja_is_anchored() {
  FakeAerial w; const Wizard *os = nullptr, *ni = nullptr;
  for (const Wizard& x : Wizards()) { if (x.id == "osnowa") os = &x; if (x.id == "niwelacja") ni = &x; }
  assert(LabelOf(*StartWizard(w, *os), "mode").find("statyczny") != std::string::npos);
  assert(LabelOf(*StartWizard(w, *ni), "rod_fore_end").find("reper B") != std::string::npos);
}

static void fresh_wizard_job_has_nextstep() {
  for (const Wizard& x : Wizards()) {
    FakeAerial w; w.dir = (std::filesystem::temp_directory_path() / ("gs_wiz_n_" + x.id)).string();
    auto job = StartWizard(w, x); assert(!job->NextStep().empty());
  }
}

int main() {
  six_wizards_exist(); every_typeid_exists_in_jobtypes(); setup_steps_apply_in_fresh_menu(); als_wizard_turns_calibration_on();
  setup_is_idempotent_when_state_already_matches(); osnowa_uses_static_mode_and_niwelacja_is_anchored(); fresh_wizard_job_has_nextstep();
  std::puts("test_wizards OK");
  return 0;
}
