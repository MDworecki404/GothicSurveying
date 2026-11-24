#include <cassert>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <cstring>
#include <iterator>
#include <set>
#include <string>
#include <vector>
#include "fake_aerial.hpp"

static int gFx = 0;
struct Fx {
  FakeAerial w; std::unique_ptr<Job> job;
  Fx() {
    w.dir = (std::filesystem::temp_directory_path() / ("gs_aerial_job_" + std::to_string(++gFx))).string();
    std::filesystem::remove_all(w.dir);
    job = CreateAerialJob(w, JobOptions{TraverseKind::Open, CalcMode::Auto});
  }
  void Vertex(double n, double e) { w.hero = {n, e, 100.0}; job->OnMenu("area_here"); }
  void Square(double side) { Vertex(N0, E0); Vertex(N0, E0 + side); Vertex(N0 + side, E0 + side); Vertex(N0 + side, E0); }
  bool enabled(const std::string& id) const { for (auto& m : job->Menu()) if (m.id == id) return m.enabled; return false; }
  int Lines(int color) const { int n = 0; for (const MapLine& l : job->MapOverlay()) if (l.color == color) n++; return n; }
};

static void registry_and_kind() {
  bool found = false; for (const JobType& t : JobTypes()) if (t.id == "aerial") found = t.aerial && !t.scan && t.name.find("lotniczy") != std::string::npos;
  assert(found);
  Fx f; assert(f.job->Kind() == JobKind::Aerial && f.enabled("area_here") && f.enabled("launch") && !f.job->CanFinish());
}

static void vertices_and_plan() {
  Fx f; f.Square(100);
  assert(f.Lines(0) == 4 && f.Lines(1) == 3);
  assert(has(f.job->Hud(), "szeregow: 3"));
}

static void launch_needs_area() {
  Fx f; f.job->OnMenu("launch");
  assert(f.w.Said("obszar") && f.w.placed == 0);
  f.Vertex(N0, E0); f.Vertex(N0 + 10, E0 + 10); f.w.said.clear(); f.job->OnMenu("launch");
  assert(f.w.Said("obszar") && f.w.placed == 0);                         // 2 wierzcholki nie wystarcza
}


static void area_clear_and_vertex_limit() {
  Fx f;
  for (int i = 0; i < 12; i++) f.Vertex(N0 + 3 * i, E0 + 5 * (i % 3));
  f.w.said.clear(); f.Vertex(N0 + 100, E0 + 100);
  assert(f.w.Said("limit"));
  f.job->OnMenu("area_clear"); assert(f.Lines(0) == 0 && f.Lines(1) == 0);
  f.Square(100); assert(f.Lines(0) == 4);                                   // po czyszczeniu znow dziala
}

static void area_rect_from_two_corners() {                                    // zaznaczenie na mapie: dwa przeciwlegle narozniki w dowolnej kolejnosci
  Fx f; assert(f.enabled("area_map"));
  char id[160]; std::snprintf(id, sizeof id, "area_rect:%.3f:%.3f:%.3f:%.3f", N0 + 100, E0 + 100, N0, E0);   // gorny prawy pierwszy
  f.job->OnMenu(id);
  assert(f.Lines(0) == 4 && f.Lines(1) == 3 && has(f.job->Hud(), "szeregow: 3"));
  f.job->OnMenu("area_here");                                                // reczny wierzcholek dochodzi do prostokata (5 wierzcholkow)
  assert(f.Lines(0) == 5);
  f.job->OnMenu(id); assert(f.Lines(0) == 4);                                // nowe zaznaczenie zastepuje stary obszar
  f.w.said.clear(); f.job->OnMenu("area_rect:5500000.000:6500000.000:5500000.400:6500100.000");
  assert(f.w.Said("za maly") && f.Lines(0) == 4);                            // zbyt waski: odmowa, obszar bez zmian
  f.job->OnMenu("area_rect:abc"); assert(f.Lines(0) == 4);                   // zly zapis: ignorowany
}

static void ctrl_vertex_from_list() {
  Fx f; assert(f.job->NeedsChoice("area_ctrl"));
  const std::vector<MenuItem> ch = f.job->TachyChoices("area_ctrl"); assert(ch.size() == 1 && ch[0].id == "area_ctrl:G1");
  f.job->OnMenu("area_ctrl:G1"); f.job->OnMenu("area_ctrl:G1");
  assert(f.Lines(0) == 1);                                                  // dwa wierzcholki = jeden bok
}

static void params_change_plan() {
  Fx f; f.Square(100);
  f.job->OnMenu("agl");                                                      // 50 -> 80 m: szerszy pas, mniej szeregow
  assert(has(f.job->Hud(), "szeregow: 2"));
  f.job->OnMenu("follow"); f.job->OnMenu("battery"); f.job->OnMenu("profile"); f.job->OnMenu("preset"); f.job->OnMenu("speed"); f.job->OnMenu("sidelap"); f.job->OnMenu("azimuth");
  assert(f.Lines(0) == 4);
}

static void area_rect_rotated() {                                               // prostokat obrocony: dwa narozniki + kat, szeregi wzdluz dluzszego boku
  Fx f; const double a = 30.0 * 3.14159265358979323846 / 180.0;
  char id[200]; std::snprintf(id, sizeof id, "area_rect:%.3f:%.3f:%.3f:%.3f:30.000", N0, E0, N0 + 100 * std::cos(a) - 60 * std::sin(a), E0 + 100 * std::sin(a) + 60 * std::cos(a));
  f.job->OnMenu(id);
  std::vector<double> sides; for (const MapLine& l : f.job->MapOverlay()) if (l.color == 0) sides.push_back(std::hypot(l.n2 - l.n1, l.e2 - l.e1));
  std::sort(sides.begin(), sides.end()); assert(sides.size() == 4 && std::fabs(sides[0] - 60) < 0.01 && std::fabs(sides[1] - 60) < 0.01 && std::fabs(sides[2] - 100) < 0.01 && std::fabs(sides[3] - 100) < 0.01);
  for (const MapLine& l : f.job->MapOverlay()) if (l.color == 1) {
    const double d = std::hypot(l.n2 - l.n1, l.e2 - l.e1), dot = ((l.n2 - l.n1) * std::cos(a) + (l.e2 - l.e1) * std::sin(a)) / d;
    assert(std::fabs(std::fabs(dot) - 1.0) < 1e-3); break;                                  // szeregi rownolegle do dluzszego boku (100 m)
  }
  f.w.said.clear(); f.job->OnMenu("area_rect:5500000.000:6500000.000:5500000.500:6500000.500:45.000");   // po obrocie 45 st. punkty leza na jednej osi: szerokosc 0 m, odmowa
  assert(f.w.Said("za maly"));
}

static void calibration_strips_menu_and_state() {
  Fx f; f.Square(100); assert(has(f.job->Hud(), "szeregow: 3"));
  f.job->OnMenu("calib"); assert(has(f.job->Hud(), "szeregow: 6") && f.Lines(1) == 6);
  StateKV s; f.job->SaveState(s);
  std::unique_ptr<Job> j2 = CreateAerialJob(f.w, JobOptions{TraverseKind::Open, CalcMode::Auto}); assert(j2->LoadState(s));
  assert(has(j2->Hud(), "szeregow: 6"));
  f.job->OnMenu("calib"); assert(has(f.job->Hud(), "szeregow: 3"));              // przelacznik wraca
}

static void plan_csv_written() {
  Fx f; f.Square(100); f.job->OnMenu("plan_csv");
  std::ifstream in(f.w.dir + "/plan_nalotu.csv"); std::string line; std::getline(in, line);
  assert(line == "szereg;N_a;E_a;N_b;E_b;dlugosc_m");
  int n = 0; while (std::getline(in, line)) n++;
  assert(n == 3);
}

// ---------- lot ----------
static std::uint32_t U(const std::vector<unsigned char>& b, size_t o, int n) { std::uint32_t v = 0; for (int i = 0; i < n; i++) v |= (std::uint32_t)b[o + i] << (8 * i); return v; }
static std::vector<unsigned char> Slurp(const std::string& p) { std::ifstream f(p, std::ios::binary); return std::vector<unsigned char>(std::istreambuf_iterator<char>(f), {}); }
static std::string Text(const std::string& p) { std::ifstream f(p, std::ios::binary); std::stringstream ss; ss << f.rdbuf(); return ss.str(); }
static AerialJob* Air(Fx& f) { return static_cast<AerialJob*>(f.job.get()); }
static long long LasCount(const std::string& dir) { const auto b = Slurp(dir + "/lot.las"); return b.size() < 227 ? -1 : (long long)U(b, 107, 4); }

static void Ready(Fx& f, int cap = 5000) {            // kwadrat 100 m, najrzadsze impulsy (2 kHz), limit impulsow na klatke
  f.Square(100); f.job->OnMenu("preset"); f.job->OnMenu("preset"); f.job->OnMenu("preset"); Air(f)->TestHooks(cap, nullptr, true);
}
static void Fly(Fx& f) { for (int i = 0; i < 100000 && f.enabled("cancel"); i++) f.job->Tick(); assert(!f.enabled("cancel")); }

static void flight_runs_to_end() {
  Fx f; Ready(f); assert(!f.enabled("cancel")); f.job->OnMenu("launch");
  assert(f.w.placed == 1 && f.enabled("cancel") && !f.enabled("area_here") && !f.job->CanFinish());
  Fly(f);
  assert(f.w.removed == 1 && f.w.poses > 0 && f.job->CanFinish());
  for (const char* n : {"lot.las", "lot.pts", "trajektoria.csv", "lot_opis.txt"}) assert(std::filesystem::exists(f.w.dir + "/" + n));
  assert(std::filesystem::exists(f.w.dir + "/lot_surowy.bin") && !std::filesystem::exists(f.w.dir + "/lot.raw"));   // surowe dane zostaja do wyrownania
  { std::ifstream in(f.w.dir + "/lot_surowy.bin", std::ios::binary); AlsPoint q{}; assert(alsdetail::ReadRec(in, q));
    assert(q.range > 49.0f && q.range < 60.0f && std::fabs(q.theta) <= 30.0f + 1e-3f); }                            // AGL 50 m, FOV 60 st.
  const long long c = LasCount(f.w.dir); assert(c > 100000);
  std::ifstream pts(f.w.dir + "/lot.pts"); long long n = 0; pts >> n; assert(n == c);
  assert(!f.job->SummaryLines().empty());
}

static void laser_beams_are_drawn_and_cleared() {                               // wiazki drona: co kilkadziesiat impulsow, po locie usuniete
  Fx f; Ready(f, 20000); f.job->OnMenu("launch"); Fly(f);
  const long long pts = LasCount(f.w.dir);
  assert(f.w.beams > 20 && f.w.beams < pts / 20 && f.w.lastBeamCleared && f.w.beamClears >= 1);
}

static void zero_error_profile_hits_truth() {
  const ErrorProfile zero{"zero", 0, 0, 0, 0, 0, 0};
  Fx f; f.Square(100); f.job->OnMenu("preset"); f.job->OnMenu("preset"); f.job->OnMenu("preset"); Air(f)->TestHooks(20000, &zero, false);
  f.job->OnMenu("launch"); Fly(f);
  std::ifstream in(f.w.dir + "/lot.pts"); std::string line; std::getline(in, line); double e, nn, h; int i, r, g, b; long long cnt = 0;
  while (std::getline(in, line)) { std::sscanf(line.c_str(), "%lf %lf %lf %d %d %d %d", &e, &nn, &h, &i, &r, &g, &b); assert(std::fabs(h - 100.0) < 1e-3); cnt++; }
  assert(cnt > 100000);
  Fx s; s.Square(100); s.job->OnMenu("preset"); s.job->OnMenu("preset"); s.job->OnMenu("preset"); s.job->OnMenu("profile"); s.job->OnMenu("profile"); Air(s)->TestHooks(20000, nullptr, true);
  s.job->OnMenu("launch"); Fly(s);
  std::ifstream in2(s.w.dir + "/lot.pts"); std::getline(in2, line); double sum = 0, sum2 = 0; long long c2 = 0;
  while (std::getline(in2, line)) { std::sscanf(line.c_str(), "%lf %lf %lf", &e, &nn, &h); sum += h; sum2 += h * h; c2++; }
  const double sd = std::sqrt(sum2 / c2 - (sum / c2) * (sum / c2)); assert(sd > 0.01);   // profil "slaby": blad trajektorii widac w H
}

static void points_have_strip_ids_and_time() {
  Fx f; Ready(f, 20000); f.job->OnMenu("launch"); Fly(f);
  const auto b = Slurp(f.w.dir + "/lot.las"); const long long n = (long long)U(b, 107, 4); std::set<int> strips; double prev = 0, first = -1;
  for (long long k = 0; k < n; k++) {
    const size_t o = 227 + (size_t)k * 34; strips.insert((int)U(b, o + 18, 2)); double g; std::memcpy(&g, &b[o + 20], 8);
    if (first < 0) first = g;
    assert(g >= prev - 1e-9); prev = g;
  }
  assert(strips == (std::set<int>{0, 1, 2}) && first >= 345600.0);
}

static void rays_miss_no_crash() {
  Fx f; Ready(f); f.w.rays = false; f.job->OnMenu("launch"); Fly(f);
  assert(LasCount(f.w.dir) == 0 && std::filesystem::file_size(f.w.dir + "/lot.las") == 227 && f.w.Said("brak punktow") && f.w.removed == 1);
}

static void ground_height_unavailable_falls_back() {
  Fx f; Ready(f, 20000); f.w.groundOk = false; f.job->OnMenu("launch"); Fly(f);
  assert(f.w.Count("wysokosci terenu") == 1 && LasCount(f.w.dir) > 100000);
}

static long long Pts(Fx& f) { long long n = 0; std::sscanf(f.job->Hud().c_str(), "Lot ALS: szereg %*d/%*d, %*d%%, %lld punktow", &n); return n; }
static void TickUntilPts(Fx& f, long long n) { for (int i = 0; i < 100000 && Pts(f) < n; i++) f.job->Tick(); assert(Pts(f) >= n); }
static void TickUntilStrip2(Fx& f) { for (int i = 0; i < 100000 && !has(f.job->Hud(), "szereg 2/3"); i++) f.job->Tick(); assert(has(f.job->Hud(), "szereg 2/3")); f.job->Tick(); f.job->Tick(); }

static void abort_midflight_writes_partial() {
  Fx f; Ready(f); f.job->OnMenu("launch");
  TickUntilPts(f, 1000);
  f.job->OnMenu("cancel");
  assert(!f.enabled("cancel") && f.w.removed == 1);
  const long long c = LasCount(f.w.dir); assert(c > 0 && c < 100000);
  assert(has(Text(f.w.dir + "/lot_opis.txt"), "przerwany") && !std::filesystem::exists(f.w.dir + "/lot.raw") && std::filesystem::exists(f.w.dir + "/lot_surowy.bin"));
}

static void idle_phases_are_flown() {                                           // wznios i dolot trwaja w czasie lotu, dron nie przeskakuje na szereg
  Fx f; Ready(f); f.job->OnMenu("launch");
  for (int i = 0; i < 3; i++) f.job->Tick();
  assert(f.w.poses == 3 && f.w.lastPose.h > 100.5 && f.w.lastPose.h < 149.0 && Pts(f) == 0);
}

static void finish_false_midflight_converts_and_keeps_raw() {                    // kontroler (przerwanie roboty, zmiana swiata, wczytanie zapisu) wola Finish(false)
  Fx f; Ready(f); f.job->OnMenu("launch");
  TickUntilPts(f, 1000);
  f.job->Finish(false);
  assert(!f.enabled("cancel") && f.w.removed == 1 && LasCount(f.w.dir) > 0 && std::filesystem::exists(f.w.dir + "/lot.raw") && std::filesystem::exists(f.w.dir + "/lot_opis.txt"));
}

static void state_roundtrip() {
  Fx f; Ready(f); f.job->OnMenu("launch");
  TickUntilStrip2(f);                                                           // szereg 1 w toku
  StateKV s; f.job->SaveState(s);
  f.job.reset();                                                                // jak po zamknieciu gry
  std::unique_ptr<Job> j2 = CreateAerialJob(f.w, JobOptions{TraverseKind::Open, CalcMode::Auto});
  assert(!j2->LoadState(StateKV{}));                                           // obcy stan
  assert(j2->LoadState(s)); f.job = std::move(j2);
  assert(f.w.placed == 2 && f.enabled("cancel") && has(f.job->Hud(), "Lot ALS: szereg 2/3") && f.w.outCalls == 1);
  Air(f)->TestHooks(5000, nullptr, true); Fly(f);
  Fx full; Ready(full); full.job->OnMenu("launch"); Fly(full);
  const long long a = LasCount(f.w.dir), b = LasCount(full.w.dir); assert(a > 0 && std::llabs(a - b) < b / 50);
  const auto las = Slurp(f.w.dir + "/lot.las"); std::set<int> st; for (long long k = 0; k < a; k++) st.insert((int)U(las, 227 + (size_t)k * 34 + 18, 2));
  assert(st == (std::set<int>{0, 1, 2}));
}

static void resume_after_finished_flight_keeps_old_result() {                   // zapis A w locie, lot dokonczony, wczytanie A: pelny wynik nie moze zostac nadpisany
  Fx f; Ready(f); f.job->OnMenu("launch");
  TickUntilStrip2(f);
  StateKV s; f.job->SaveState(s);
  Fly(f); const long long done = LasCount(f.w.dir); const auto before = Slurp(f.w.dir + "/lot.las"); assert(done > 100000);
  std::unique_ptr<Job> j2 = CreateAerialJob(f.w, JobOptions{TraverseKind::Open, CalcMode::Auto});
  assert(j2->LoadState(s)); f.job = std::move(j2);
  assert(f.w.outCalls == 2 && f.w.Said("nowego folderu"));
  assert(Slurp(f.w.dir + "/lot.las") == before);                               // stary wynik nietkniety
  Air(f)->TestHooks(5000, nullptr, true); Fly(f);
  const long long rest = LasCount(f.w.dir + "_2"); assert(rest > 0 && rest < done);
}

static void resume_with_short_raw_uses_new_folder() {                           // plik roboczy krotszy niz licznik ze stanu: bez dopelniania zerami
  Fx f; Ready(f); f.job->OnMenu("launch");
  TickUntilStrip2(f);
  StateKV s; f.job->SaveState(s); f.job.reset();
  std::filesystem::resize_file(f.w.dir + "/lot.raw", 100);
  std::unique_ptr<Job> j2 = CreateAerialJob(f.w, JobOptions{TraverseKind::Open, CalcMode::Auto});
  assert(j2->LoadState(s)); f.job = std::move(j2);
  assert(f.w.outCalls == 2 && f.w.Said("nowego folderu") && std::filesystem::file_size(f.w.dir + "/lot.raw") == 100);
}

static void open_failure_refuses_launch() {
  Fx f; Ready(f); f.w.dirOk = false; f.w.dir = (std::filesystem::temp_directory_path() / "gs_missing_dir_xyz_1" / "sub").string();
  f.job->OnMenu("launch");
  assert(!f.enabled("cancel") && f.w.placed == 0 && f.w.Said("otworzyc"));
}

static void long_plan_flies_with_battery_swaps() {                             // dlugi plan: bez odmowy; dron wraca, laduje i leci dalej
  Fx f; f.Square(300);
  for (int i = 0; i < 3; i++) f.job->OnMenu("agl");                              // 50 -> 80 -> 100 -> 150 m (3 szeregi po 300 m)
  for (int i = 0; i < 3; i++) f.job->OnMenu("speed");                            // 5 -> 6 -> 8 -> 3 m/s (szereg 100 s)
  for (int i = 0; i < 2; i++) f.job->OnMenu("battery");                          // 25 -> 40 -> 5 min (225 s uzytecznych)
  for (int i = 0; i < 3; i++) f.job->OnMenu("preset");                           // najrzadsze impulsy
  assert(has(f.job->Hud(), "szeregow: 3") && has(f.job->Hud(), "3 loty"));
  Air(f)->TestHooks(20000, nullptr, true); f.job->OnMenu("launch");
  assert(f.w.placed == 1 && f.enabled("cancel") && f.w.Said("wymian"));
  Fly(f);
  assert(f.w.landings == 2 && f.w.removed == 1 && LasCount(f.w.dir) > 100000);   // 2 wymiany w locie (ladowanie koncowe: dron zdejmowany ze swiata bez pozy)
  const auto las = Slurp(f.w.dir + "/lot.las"); std::set<int> st; const long long n = LasCount(f.w.dir);
  for (long long k = 0; k < n; k++) st.insert((int)U(las, 227 + (size_t)k * 34 + 18, 2));
  assert(st == (std::set<int>{0, 1, 2}));
}

static void adjust_after_flight_pipeline() {
  Fx f; Ready(f, 20000);
  assert(!f.enabled("adjust") && !f.enabled("adjust_cancel"));                  // przed lotem: brak danych
  f.job->OnMenu("launch"); assert(!f.enabled("adjust")); Fly(f);                // w trakcie lotu: niedostepne
  assert(f.enabled("adjust") && !f.enabled("adjust_cancel"));
  f.job->OnMenu("adjust"); assert(f.enabled("adjust_cancel") && !f.enabled("adjust") && has(f.job->Hud(), "Wyrownanie"));
  for (int i = 0; i < 100000 && f.enabled("adjust_cancel"); i++) f.job->Tick();
  assert(!f.enabled("adjust_cancel") && f.w.Said("Wyrownanie zakonczone"));
  for (const char* n : {"lot_wyrownany.las", "lot_wyrownany.pts", "lot_wyrownanie.txt"}) assert(std::filesystem::exists(f.w.dir + "/" + n));
  assert(!std::filesystem::exists(f.w.dir + "/lot_wyrownany.raw"));
  const auto adj = Slurp(f.w.dir + "/lot_wyrownany.las"); assert(adj.size() > 227 && (long long)U(adj, 107, 4) == LasCount(f.w.dir));   // te same punkty
  const std::string t = Text(f.w.dir + "/lot_wyrownanie.txt");
  assert(has(t, "Boresight") && has(t, "nieokreslony") && has(t, "rms") && has(t, "G1") && has(t, "Przesuniecia szeregow") && !has(t, "Prawdziwy"));   // plaski teren: pitch i yaw nieokreslone
}

static void products_after_flight_and_adjust() {
  Fx f; Ready(f, 20000);
  assert(!f.enabled("products"));                                                    // przed lotem brak chmury
  f.job->OnMenu("launch"); Fly(f); assert(f.enabled("products") && !f.enabled("products_cancel"));
  f.job->OnMenu("adjust"); for (int i = 0; i < 100000 && f.enabled("adjust_cancel"); i++) f.job->Tick();
  assert(std::filesystem::exists(f.w.dir + "/lot_wyrownany.bin"));                  // wyrownana chmura zostaje do produktow
  f.job->OnMenu("products"); assert(f.enabled("products_cancel") && !f.enabled("products") && has(f.job->Hud(), "Produkty"));
  for (int i = 0; i < 100000 && f.enabled("products_cancel"); i++) f.job->Tick();
  assert(f.w.Said("Produkty gotowe"));
  for (const char* n : {"nmt.asc", "nmpt.asc", "warstwice.csv", "warstwice.dxf", "lot_klasyfikacja.las", "produkty.txt"}) assert(std::filesystem::exists(f.w.dir + "/produkty/" + n));
  const std::string t = Text(f.w.dir + "/produkty/produkty.txt"); assert(has(t, "grunt") && has(t, "Objetosci"));
  const auto las = Slurp(f.w.dir + "/produkty/lot_klasyfikacja.las"); assert((long long)U(las, 107, 4) == LasCount(f.w.dir) && las[227 + 15] == 2);   // plaski teren: pierwszy punkt to grunt (klasa 2)
  f.job->OnMenu("products"); f.job->Tick(); f.job->OnMenu("products_cancel"); assert(f.enabled("products") && !f.enabled("products_cancel"));   // przerwanie bez awarii
}

static void adjust_cancel_and_failures() {
  Fx f; Ready(f, 20000); f.job->OnMenu("launch"); Fly(f);
  f.job->OnMenu("adjust"); f.job->Tick(); f.job->OnMenu("adjust_cancel");
  assert(f.enabled("adjust") && !f.enabled("adjust_cancel") && !std::filesystem::exists(f.w.dir + "/lot_wyrownany.las") && !std::filesystem::exists(f.w.dir + "/lot_wyrownany.raw"));
  std::filesystem::remove(f.w.dir + "/trajektoria.csv");                          // brak trajektorii: komunikat, bez awarii i bez plikow wyniku
  f.w.said.clear(); f.job->OnMenu("adjust"); for (int i = 0; i < 1000 && f.enabled("adjust_cancel"); i++) f.job->Tick();
  assert(f.w.Said("trajektorii") && !f.enabled("adjust_cancel") && !std::filesystem::exists(f.w.dir + "/lot_wyrownany.las"));
}

static void adjust_true_error_report_and_state() {
  Fx f; f.job = CreateAerialJob(f.w, JobOptions{TraverseKind::Open, CalcMode::Auto, true}); Ready(f, 20000); f.job->OnMenu("launch"); Fly(f);
  StateKV st; f.job->SaveState(st); std::unique_ptr<Job> j2 = CreateAerialJob(f.w, JobOptions{TraverseKind::Open, CalcMode::Auto, true});
  assert(j2->LoadState(st)); f.job = std::move(j2); assert(f.enabled("adjust"));   // po wczytaniu zapisu wyrownanie nadal dostepne (pliki w folderze lotu)
  f.job->OnMenu("adjust"); for (int i = 0; i < 100000 && f.enabled("adjust_cancel"); i++) f.job->Tick();
  const std::string t = Text(f.w.dir + "/lot_wyrownanie.txt"); assert(has(t, "Prawdziwy blad boresightu") && has(t, "bias IMU"));
}

static void route_blocked_warns_but_flies() {
  Fx f; Ready(f); f.job->OnMenu("launch");
  assert(f.w.routeChecks == 3 && !f.w.Said("przeszkod") && f.enabled("cancel"));       // czysta trasa: 3 szeregi sprawdzone, bez ostrzezenia
  Fx g; Ready(g); g.w.blocked = true; g.job->OnMenu("launch");
  assert(g.w.Said("3 z 3 szeregow") && g.w.Said("przeszkod") && g.enabled("cancel"));       // ostrzezenie, lot sie zaczyna
}

static void strip_and_control_report() {
  Fx f; Ready(f, 20000); f.job->OnMenu("launch"); Fly(f);
  const std::string t = Text(f.w.dir + "/lot_opis.txt");
  assert(has(t, "meanDz") && has(t, "G1") && has(t, "dh") && has(t, "RTK dobry"));
}

int main() {
  flight_runs_to_end(); laser_beams_are_drawn_and_cleared(); zero_error_profile_hits_truth(); points_have_strip_ids_and_time(); rays_miss_no_crash(); ground_height_unavailable_falls_back();
  abort_midflight_writes_partial(); idle_phases_are_flown(); finish_false_midflight_converts_and_keeps_raw(); state_roundtrip();
  resume_after_finished_flight_keeps_old_result(); resume_with_short_raw_uses_new_folder(); open_failure_refuses_launch(); strip_and_control_report(); route_blocked_warns_but_flies();
  adjust_after_flight_pipeline(); products_after_flight_and_adjust(); adjust_cancel_and_failures(); adjust_true_error_report_and_state();
  registry_and_kind(); vertices_and_plan(); launch_needs_area(); long_plan_flies_with_battery_swaps(); area_clear_and_vertex_limit();
  area_rect_from_two_corners(); area_rect_rotated(); calibration_strips_menu_and_state(); ctrl_vertex_from_list(); params_change_plan(); plan_csv_written();
  std::puts("test_aerial_job OK"); return 0;
}
