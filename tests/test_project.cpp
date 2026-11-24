#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include "calc/Project.hpp"
#include "calc/UiSettings.hpp"

static Project Fresh() { Project p; p.name = "Kolonia_1"; p.world = "NEWWORLD"; p.created = "2026-11-24 10:00"; return p; }

static void name_validation() {
  assert(ProjectNameOk("Kolonia_1") && ProjectNameOk("a") && ProjectNameOk("A-b_9") && ProjectNameOk(std::string(32, 'x')));
  assert(!ProjectNameOk("") && !ProjectNameOk("..") && !ProjectNameOk(".") && !ProjectNameOk("a/b") && !ProjectNameOk("a\\b") && !ProjectNameOk("a b"));
  assert(!ProjectNameOk("Gornicza\xC5\x82") && !ProjectNameOk(std::string(33, 'x')) && !ProjectNameOk("C:"));
}

static void add_numbers_per_type() {
  Project p = Fresh();
  Measurement& a = AddMeasurement(p, "tachy", "Tachimetria"); assert(a.name == "Tachimetria 1" && a.id == "m1" && a.state == MeasState::New);
  SetState(p, "m1", MeasState::Done);
  Measurement& b = AddMeasurement(p, "tachy", "Tachimetria"); assert(b.name == "Tachimetria 2" && b.id == "m2");
  SetState(p, "m2", MeasState::Done);
  Measurement& c = AddMeasurement(p, "traverse", "Ciag niwelacyjny"); assert(c.name == "Ciag niwelacyjny 1" && c.id == "m3");
  assert(p.items.size() == 3);
}

static void cannot_add_while_running_or_closed() {
  Project p = Fresh(); std::string why;
  assert(CanAddMeasurement(p, why));
  AddMeasurement(p, "gnss", "Pomiar GNSS (RTK)"); SetState(p, "m1", MeasState::Running);
  assert(!CanAddMeasurement(p, why) && why.find("Pomiar GNSS (RTK) 1") != std::string::npos);
  assert(RunningMeasurement(p) && RunningMeasurement(p)->id == "m1");
  SetState(p, "m1", MeasState::Done, "GothicSurveying/2026-11-24_10-00", "12 punktow");
  assert(CanAddMeasurement(p, why) && !RunningMeasurement(p));
  assert(p.items[0].dir == "GothicSurveying/2026-11-24_10-00" && p.items[0].summary == "12 punktow");
  SetState(p, "m1", MeasState::Aborted);                          // pusty dir/summary nie kasuje poprzednich
  assert(p.items[0].dir == "GothicSurveying/2026-11-24_10-00" && p.items[0].summary == "12 punktow");
  p.closed = true; assert(!CanAddMeasurement(p, why) && why.find("zamkniety") != std::string::npos);
}

static void roundtrip_text() {
  Project p = Fresh(); p.defaults.Str("lvl", "1");
  Measurement& m = AddMeasurement(p, "aerial", "Skaning lotniczy z drona"); m.params = "agl=80;speed=6"; m.guided = true; m.pointsFile = "pomiar_m1.csv"; const std::string mname = m.name;      // referencja traci wazność po kolejnym AddMeasurement
  SetState(p, "m1", MeasState::Done, "GothicSurveying/x", "tekst z %, = i\nnowa linia");
  AddMeasurement(p, "typ_nieznany", "Nieznany");
  const std::string text = ProjectToText(p); bool ok = false;
  const Project q = ProjectFromText(text, ok);
  assert(ok && q.name == p.name && q.world == "NEWWORLD" && q.created == p.created && !q.closed && q.defaults.GetStr("lvl") == "1");
  assert(q.items.size() == 2 && q.items[0].type == "aerial" && q.items[0].name == mname && q.items[0].params == "agl=80;speed=6" && q.items[0].guided);
  assert(q.items[0].state == MeasState::Done && q.items[0].summary == "tekst z %, = i\nnowa linia" && q.items[0].pointsFile == "pomiar_m1.csv");
  assert(q.items[1].type == "typ_nieznany" && q.items[1].state == MeasState::New);
  Project c = p; c.closed = true; bool ok2 = false; assert(ProjectFromText(ProjectToText(c), ok2).closed && ok2);
}

static void corrupt_manifest_rejected() {
  bool ok = true;
  ProjectFromText("", ok); assert(!ok);
  ProjectFromText("name=X\nworld=W\n", ok); assert(!ok);                              // brak ver
  ProjectFromText("ver=2\nname=X\n", ok); assert(!ok);                                // obca wersja
  ProjectFromText("ver=1\nname=a/b\n", ok); assert(!ok);                              // zla nazwa
  ProjectFromText("ver=1\n", ok); assert(!ok);                                        // uciety przed nazwa
  ProjectFromText("\xFF\xFE\x00garbage", ok); assert(!ok);
  Project p = Fresh(); AddMeasurement(p, "tachy", "Tachimetria"); AddMeasurement(p, "tachy", "Tachimetria");
  std::string t = ProjectToText(p); const size_t k = t.find("m.2."); assert(k != std::string::npos);
  const Project q = ProjectFromText(t.substr(0, k) + "name=Kolonia_1\nver=1\n", ok);   // klucze sa alfabetyczne: uciecie przed m.2 gubi tez m.n, ale naglowek zostaje
  assert(ok && q.items.empty());          // brak m.n: lista pusta, bez wywrotki
  const Project q2 = ProjectFromText(t.substr(0, k) + "m.n=2\nname=Kolonia_1\nver=1\n", ok); assert(ok && q2.items.size() == 1);   // m.2 uciete: zostaje pierwszy pomiar
  const Project r = ProjectFromText("ver=1\nname=X\nm.n=999999999\n", ok); assert(ok && r.items.empty());   // absurdalna liczba pomiarow nie zawiesza
}

static void reconcile_marks_stale_running_aborted() {
  Project p = Fresh(); AddMeasurement(p, "tachy", "Tachimetria"); SetState(p, "m1", MeasState::Running);
  AddMeasurement(p, "tachy", "Tachimetria"); SetState(p, "m2", MeasState::Done);
  Reconcile(p, "m1"); assert(p.items[0].state == MeasState::Running && p.items[1].state == MeasState::Done);
  Reconcile(p, "");   assert(p.items[0].state == MeasState::Aborted && p.items[1].state == MeasState::Done);   // wczytano starszy zapis: nie ma aktywnej roboty
  std::string why; assert(CanAddMeasurement(p, why));
}

static void close_refused_while_running() {
  Project p = Fresh(); AddMeasurement(p, "gnss", "Pomiar GNSS (RTK)"); SetState(p, "m1", MeasState::Running);
  std::string why; assert(!CloseProject(p, why) && !p.closed && !why.empty());
  SetState(p, "m1", MeasState::Done); assert(CloseProject(p, why) && p.closed);
  ReopenProject(p); assert(!p.closed);
}

static void save_load_list_on_temp_dir() {
  const std::string root = (std::filesystem::temp_directory_path() / "gs_project_test").string(); std::filesystem::remove_all(root);
  assert(ListProjects(root).empty());
  Project p = Fresh(); AddMeasurement(p, "tachy", "Tachimetria");
  assert(SaveProject(p, root));
  Project q = Fresh(); q.name = "Aaa"; assert(SaveProject(q, root));
  Project bad = Fresh(); bad.name = "../zly"; assert(!SaveProject(bad, root));
  const auto names = ListProjects(root); assert(names.size() == 2 && names[0] == "Aaa" && names[1] == "Kolonia_1");
  Project r; assert(LoadProject(root, "Kolonia_1", r) && r.items.size() == 1 && r.items[0].name == "Tachimetria 1");
  assert(!LoadProject(root, "Nie_ma", r) && !LoadProject(root, "..", r));
  { std::ofstream f(root + "/projekty/Aaa/projekt.txt", std::ios::trunc); f << "to nie jest manifest"; }   // uszkodzony plik
  assert(!LoadProject(root, "Aaa", r));
  assert(!std::filesystem::exists(root + "/projekty/Kolonia_1/projekt.txt.tmp"));
  std::filesystem::remove_all(root);
}

static void settings_roundtrip() {
  UiSettings u; u.lastByWorld["newworld"] = "Kolonia_1"; u.cursor["home"] = 3; u.cursor["job:aerial"] = 12;
  const UiSettings r = UiSettingsFromText(UiSettingsToText(u));
  assert(r.lastByWorld.at("newworld") == "Kolonia_1" && r.cursor.size() == 2 && r.cursor.at("home") == 3 && r.cursor.at("job:aerial") == 12);
}

static void settings_garbage_gives_defaults() {
  for (const char* g : {"", "to nie jest ustawienie", "c.home=abc\nlast=\n", "\xFF\xFE\x01"}) {
    const UiSettings r = UiSettingsFromText(g);
    for (const auto& kv : r.lastByWorld) assert(ProjectNameOk(kv.second));
    for (const auto& kv : r.cursor) assert(kv.second >= 0 && kv.second < 1000);
  }
  UiSettings bad = UiSettingsFromText("last=../zly\nc.home=-5\nc.x=99999\n");
  assert(bad.lastByWorld.empty() && bad.cursor.at("home") == 0 && bad.cursor.at("x") == 999);      // zla nazwa odrzucona, kursory przycinane do 0..999
}

static void attach_restored_reuses_unfinished_but_not_done() {
  Project p = Fresh(); AddMeasurement(p, "traverse", "Ciag niwelacyjny"); SetState(p, "m1", MeasState::Running);
  assert(AttachRestored(p, "m1", "traverse", "Ciag niwelacyjny") == "m1" && p.items.size() == 1 && p.items[0].state == MeasState::Running);
  SetState(p, "m1", MeasState::Done, "GothicSurveying/x");                       // zapis sprzed zakonczenia: wynik Done nie moze wrocic do "w toku"
  const std::string id = AttachRestored(p, "m1", "traverse", "Ciag niwelacyjny");
  assert(id != "m1" && p.items.size() == 2 && p.items[0].state == MeasState::Done && p.items[0].dir == "GothicSurveying/x" && p.items[1].state == MeasState::Running);
  assert(AttachRestored(p, "nie_ma", "tachy", "Tachimetria") != "nie_ma" && p.items.size() == 3);           // nieznany id: nowy pomiar
  assert(!AttachRestored(p, "", "tachy", "Tachimetria").empty() && p.items.size() == 4);                      // brak id (stary zapis): nowy pomiar
}

static void safe_result_dir_and_untrusted_manifest_fields() {
  assert(SafeResultDir("GothicSurveying/2026-11-24_10-00-00") && SafeResultDir("GothicSurveying/2026-11-24_10-00-00_2"));
  assert(!SafeResultDir("") && !SafeResultDir("C:/Windows/System32") && !SafeResultDir("/etc") && !SafeResultDir("\\\\serwer\\udzial") && !SafeResultDir("GothicSurveying/../system") && !SafeResultDir("..") && !SafeResultDir("x.exe"));
  bool ok = false;
  const Project q = ProjectFromText("ver=1\nname=X\nm.n=3\nm.1.id=m1\nm.1.points=..\\evil.csv\nm.2.id=../zly\nm.3.id=m3\nm.3.points=pomiar_m3.csv\n", ok);
  assert(ok && q.items.size() == 2 && q.items[0].id == "m1" && q.items[0].pointsFile.empty() && q.items[1].id == "m3" && q.items[1].pointsFile == "pomiar_m3.csv");   // id z ukosnikiem pomijany, points ze sciezka czyszczony
}

static void settings_last_project_is_per_world() {
  UiSettings u; u.lastByWorld["newworld"] = "Kolonia_1"; u.lastByWorld["oldworld"] = "Dolina_2";
  const UiSettings r = UiSettingsFromText(UiSettingsToText(u));
  assert(r.lastByWorld.size() == 2 && r.lastByWorld.at("newworld") == "Kolonia_1" && r.lastByWorld.at("oldworld") == "Dolina_2");
  assert(UiSettingsFromText("w.newworld=../zly\nw.x=Dobry\n").lastByWorld.count("newworld") == 0);               // zla nazwa odrzucona, dobra zostaje
}

int main() {
  name_validation(); add_numbers_per_type(); cannot_add_while_running_or_closed(); roundtrip_text(); corrupt_manifest_rejected();
  reconcile_marks_stale_running_aborted(); close_refused_while_running(); save_load_list_on_temp_dir();
  settings_roundtrip(); settings_garbage_gives_defaults(); attach_restored_reuses_unfinished_but_not_done(); safe_result_dir_and_untrusted_manifest_fields(); settings_last_project_is_per_world();
  std::puts("test_project OK");
  return 0;
}
