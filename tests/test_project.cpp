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

static void safe_delete_dir_only_single_result_folder() {
  assert(SafeDeleteDir("GothicSurveying/2026-11-24_10-00-00") && SafeDeleteDir("GothicSurveying/2026-11-24_10-00-00_2"));
  assert(!SafeDeleteDir("GothicSurveying") && !SafeDeleteDir("GothicSurveying/") && !SafeDeleteDir("GothicSurveying/projekty") && !SafeDeleteDir("GothicSurveying/projekty/Kolonia_1"));
  assert(!SafeDeleteDir("GothicSurveying/a/b") && !SafeDeleteDir("GothicSurveying/..") && !SafeDeleteDir("GothicSurveying/../system") && !SafeDeleteDir("") && !SafeDeleteDir("C:/x"));
}

static std::string Touch(const std::string& path) { std::filesystem::create_directories(std::filesystem::path(path).parent_path()); std::ofstream f(path); f << "x"; return path; }

static void remove_measurement_with_files() {
  namespace fs = std::filesystem;
  const std::string root = (fs::temp_directory_path() / "gs_rm_test" / "GothicSurveying").string(); fs::remove_all(fs::temp_directory_path() / "gs_rm_test");
  Project p = Fresh(); p.name = "Kolonia_1";
  AddMeasurement(p, "tachy", "Tachimetria"); AddMeasurement(p, "tachy", "Tachimetria"); AddMeasurement(p, "gnss", "GNSS"); AddMeasurement(p, "gnss", "GNSS");
  p.items[0].dir = "GothicSurveying/2026-11-24_10-00-00"; p.items[0].pointsFile = "pomiar_m1.csv"; p.items[0].state = MeasState::Done;
  p.items[1].dir = "GothicSurveying/2026-11-24_11-00-00"; p.items[1].state = MeasState::Aborted;
  p.items[2].dir = "GothicSurveying/2026-11-24_11-00-00"; p.items[2].state = MeasState::Done;       // wspolny folder z m2: nie wolno go usunac
  p.items[3].state = MeasState::Running;
  Touch(root + "/2026-11-24_10-00-00/dziennik.csv"); Touch(root + "/2026-11-24_10-00-00/pod/x.txt"); Touch(root + "/2026-11-24_11-00-00/a.csv");
  Touch(root + "/projekty/Kolonia_1/pomiar_m1.csv"); Touch(root + "/projekty/Kolonia_1/projekt.txt"); Touch(root + "/osnowa.csv"); Touch(root + "/2026-11-25_09-00-00/inny.csv");
  std::string why;
  assert(!RemoveMeasurementWithFiles(p, "m4", root, why) && !why.empty() && p.items.size() == 4);            // trwajacy: odmowa
  assert(!RemoveMeasurementWithFiles(p, "nie_ma", root, why) && p.items.size() == 4);
  assert(RemoveMeasurementWithFiles(p, "m1", root, why) && p.items.size() == 3 && p.items[0].id == "m2");
  assert(!fs::exists(root + "/2026-11-24_10-00-00") && !fs::exists(root + "/projekty/Kolonia_1/pomiar_m1.csv"));             // folder wynikow z podfolderami i plik punktow zniknely
  assert(fs::exists(root + "/projekty/Kolonia_1/projekt.txt") && fs::exists(root + "/osnowa.csv") && fs::exists(root + "/2026-11-25_09-00-00/inny.csv"));   // reszta nietknieta
  assert(RemoveMeasurementWithFiles(p, "m2", root, why) && p.items.size() == 2 && fs::exists(root + "/2026-11-24_11-00-00/a.csv"));   // m3 nadal wskazuje ten folder
  assert(RemoveMeasurementWithFiles(p, "m3", root, why) && !fs::exists(root + "/2026-11-24_11-00-00"));                                  // ostatni wlasciciel: folder znika
  p.closed = true; p.items[0].state = MeasState::Done; assert(!RemoveMeasurementWithFiles(p, "m4", root, why));                          // zamkniety projekt: tylko do odczytu
  fs::remove_all(fs::temp_directory_path() / "gs_rm_test");
}

static void remove_ignores_unsafe_manifest_paths() {
  namespace fs = std::filesystem;
  const std::string base = (fs::temp_directory_path() / "gs_rm_test2").string(); fs::remove_all(base);
  const std::string root = base + "/GothicSurveying"; Touch(base + "/system/wazne.txt"); Touch(root + "/projekty/Kolonia_1/projekt.txt");
  Project p = Fresh(); AddMeasurement(p, "tachy", "Tachimetria"); AddMeasurement(p, "tachy", "Tachimetria");
  p.items[0].dir = "GothicSurveying/../system"; p.items[0].pointsFile = "../../system/wazne.txt"; p.items[1].dir = "GothicSurveying/projekty"; p.items[1].pointsFile = "projekt.txt";
  std::string why; assert(RemoveMeasurementWithFiles(p, "m1", root, why)); assert(RemoveMeasurementWithFiles(p, "m2", root, why));      // wpisy znikaja z manifestu
  assert(fs::exists(base + "/system/wazne.txt") && fs::exists(root + "/projekty/Kolonia_1/projekt.txt"));                              // ale pliki spoza folderu wynikow zostaja
  fs::remove_all(base);
}

static void name_problem_explains_why() {
  namespace fs = std::filesystem;
  const std::string root = (fs::temp_directory_path() / "gs_name_test").string(); fs::remove_all(root);
  Project p = Fresh(); p.name = "Kolonia"; assert(SaveProject(p, root));
  assert(ProjectNameProblem("Nowy_1", root).empty() && ProjectNameProblem("a-b_9", root).empty());
  assert(ProjectNameProblem("", root).find("pusta") != std::string::npos);
  assert(ProjectNameProblem(std::string(33, 'x'), root).find("32") != std::string::npos);
  assert(ProjectNameProblem("Gornicza\xC5\x82", root).find("litery") != std::string::npos && ProjectNameProblem("a/b", root).find("litery") != std::string::npos && ProjectNameProblem("..", root).find("litery") != std::string::npos);
  assert(ProjectNameProblem("Kolonia", root).find("istnieje") != std::string::npos && ProjectNameProblem("kolonia", root).find("istnieje") != std::string::npos);   // Windows: wielkosc liter bez znaczenia
  fs::remove_all(root);
}

static void rename_project_moves_folder_and_keeps_data() {
  namespace fs = std::filesystem;
  const std::string root = (fs::temp_directory_path() / "gs_rename_test").string(); fs::remove_all(root);
  Project p = Fresh(); p.name = "Stara"; AddMeasurement(p, "tachy", "Tachimetria"); SetState(p, "m1", MeasState::Done, "GothicSurveying/x", "ok"); p.items[0].pointsFile = "pomiar_m1.csv";
  assert(SaveProject(p, root)); Touch(root + "/projekty/Stara/pomiar_m1.csv"); Touch(root + "/projekty/Stara/projekt_raport.csv");
  Project other = Fresh(); other.name = "Zajeta"; assert(SaveProject(other, root));
  std::string why;
  assert(!RenameProject(p, "Zajeta", root, why) && p.name == "Stara" && !why.empty());                       // zajeta nazwa: bez zmian
  assert(!RenameProject(p, "zla/nazwa", root, why) && p.name == "Stara");
  assert(RenameProject(p, "Nowa", root, why) && p.name == "Nowa");
  assert(!fs::exists(root + "/projekty/Stara") && fs::exists(root + "/projekty/Nowa/pomiar_m1.csv") && fs::exists(root + "/projekty/Nowa/projekt_raport.csv"));
  Project q; assert(LoadProject(root, "Nowa", q) && q.name == "Nowa" && q.items.size() == 1 && q.items[0].dir == "GothicSurveying/x" && q.items[0].summary == "ok");
  assert(!RenameProject(p, "NOWA", root, why) && p.name == "Nowa");                                          // sama zmiana wielkosci liter = ta sama nazwa: odmowa, projekt cały
  Project r; assert(LoadProject(root, p.name, r) && r.items.size() == 1);
  p.closed = true; assert(!RenameProject(p, "Inna", root, why));                                           // zamkniety: tylko do odczytu
  fs::remove_all(root);
}

static void delete_project_removes_everything_of_the_project_only() {
  namespace fs = std::filesystem;
  const std::string base = (fs::temp_directory_path() / "gs_delproj").string(); fs::remove_all(base);
  const std::string root = base + "/GothicSurveying";
  Project p = Fresh(); p.name = "Kolonia_1"; AddMeasurement(p, "tachy", "Tachimetria"); AddMeasurement(p, "gnss", "GNSS");
  p.items[0].dir = "GothicSurveying/2026-11-24_10-00-00"; p.items[0].pointsFile = "pomiar_m1.csv"; p.items[0].state = MeasState::Done;
  p.items[1].dir = "GothicSurveying/2026-11-24_11-00-00"; p.items[1].state = MeasState::Aborted;
  assert(SaveProject(p, root));
  Touch(root + "/2026-11-24_10-00-00/a.csv"); Touch(root + "/2026-11-24_11-00-00/b.csv"); Touch(root + "/projekty/Kolonia_1/pomiar_m1.csv"); Touch(root + "/projekty/Kolonia_1/projekt_raport.docx");
  Touch(root + "/osnowa.csv"); Touch(root + "/2026-11-25_09-00-00/cudzy.csv");                                      // wynik pomiaru spoza projektu
  Project other = Fresh(); other.name = "Inny"; assert(SaveProject(other, root));
  std::string why;
  p.items[1].state = MeasState::Running; assert(!DeleteProjectWithFiles(p, root, why) && !why.empty() && fs::exists(root + "/2026-11-24_10-00-00/a.csv"));   // pomiar w toku: odmowa, nic nie kasujemy
  p.items[1].state = MeasState::Aborted; p.closed = true;                                                            // zamkniety projekt mozna usunac
  assert(DeleteProjectWithFiles(p, root, why));
  assert(!fs::exists(root + "/2026-11-24_10-00-00") && !fs::exists(root + "/2026-11-24_11-00-00") && !fs::exists(root + "/projekty/Kolonia_1"));
  assert(fs::exists(root + "/osnowa.csv") && fs::exists(root + "/2026-11-25_09-00-00/cudzy.csv") && fs::exists(root + "/projekty/Inny/projekt.txt"));
  Project bad = Fresh(); bad.name = "../Inny"; assert(!DeleteProjectWithFiles(bad, root, why) && fs::exists(root + "/projekty/Inny/projekt.txt"));   // zla nazwa: nic nie ruszamy
  fs::remove_all(base);
}

int main() {
  name_validation(); add_numbers_per_type(); cannot_add_while_running_or_closed(); roundtrip_text(); corrupt_manifest_rejected();
  reconcile_marks_stale_running_aborted(); close_refused_while_running(); save_load_list_on_temp_dir();
  settings_roundtrip(); settings_garbage_gives_defaults(); attach_restored_reuses_unfinished_but_not_done(); safe_result_dir_and_untrusted_manifest_fields(); settings_last_project_is_per_world(); safe_delete_dir_only_single_result_folder(); remove_measurement_with_files(); remove_ignores_unsafe_manifest_paths(); name_problem_explains_why(); rename_project_moves_folder_and_keeps_data(); delete_project_removes_everything_of_the_project_only();
  std::puts("test_project OK");
  return 0;
}
