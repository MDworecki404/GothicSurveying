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
#include "job/Jobs.hpp"

static bool has(const std::string& s, const std::string& part) { return s.find(part) != std::string::npos; }
static const double N0 = 5500000.0, E0 = 6500000.0;

// Swiat: plaski teren H = 100 m, bohater stoi tam, gdzie ustawi test.
struct FakeAerial : IWorld {
  std::vector<std::string> said; std::string dir; GridPos hero{N0, E0, 100.0}; bool heroOk = true;
  int placed = 0, removed = 0, poses = 0;
  bool PlaceMarker(int, MarkerKind) override { return true; }
  bool FeetWithinCm(int, double) const override { return true; }
  void PlaceRod(RodRole, int) override {}
  void RemoveRod(RodRole) override {}
  bool PlaceInstrument() override { return true; }
  void RemoveInstrument() override {}
  double PointHeightM(int) const override { return 0; }
  double SightDistCm(int) const override { return 0; }
  double Random01() override { return 0.5; }
  void ShiftInstrumentMm(double) override {}
  void Say(const std::string& t) override { said.push_back(t); }
  bool HeroGround(GridPos& g) const override { if (!heroOk) return false; g = hero; return true; }
  bool GroundHeightM(double, double, double& h) const override { h = 100.0; return groundOk; }
  bool PlaceDrone(const GridPos&) override { placed++; return true; }
  void RemoveDrone() override { removed++; }
  void SetDronePose(const GridPos&, double, double, double, double) override { poses++; }
  std::string AerialOutDir() override { std::filesystem::create_directories(dir); return dir; }
  std::vector<ControlPoint> ControlNearHero(double) const override { ControlPoint c; c.id = "G1"; c.n = N0 + 5; c.e = E0 + 7; c.h = 100.0; return {c}; }
  bool Said(const std::string& part) const { for (const std::string& s : said) if (has(s, part)) return true; return false; }
  int Count(const std::string& part) const { int n = 0; for (const std::string& s : said) if (has(s, part)) n++; return n; }
  bool rays = true, groundOk = true;
  bool AerialRay(const GridPos& from, double, double elevDeg, double maxM, double& dist, double& cosInc, bool& isVob, unsigned* rgb) const override {
    const double s = std::sin(elevDeg * 3.14159265358979323846 / 180.0);
    if (!rays || s > -1e-6) return false;
    dist = (from.h - 100.0) / -s; if (dist > maxM || dist < 0) return false;   // plaski teren H = 100 m
    cosInc = -s; isVob = false; if (rgb) *rgb = 0x64A03C; return true;
  }
};

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

static void launch_refused_when_battery() {
  Fx f; f.Square(600); f.job->OnMenu("launch");
  assert(f.w.Said("bateri") && f.w.placed == 0);
}

static void area_clear_and_vertex_limit() {
  Fx f;
  for (int i = 0; i < 12; i++) f.Vertex(N0 + 3 * i, E0 + 5 * (i % 3));
  f.w.said.clear(); f.Vertex(N0 + 100, E0 + 100);
  assert(f.w.Said("limit"));
  f.job->OnMenu("area_clear"); assert(f.Lines(0) == 0 && f.Lines(1) == 0);
  f.Square(100); assert(f.Lines(0) == 4);                                   // po czyszczeniu znow dziala
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
  f.job->OnMenu("follow"); f.job->OnMenu("profile"); f.job->OnMenu("preset"); f.job->OnMenu("speed"); f.job->OnMenu("sidelap"); f.job->OnMenu("azimuth");
  assert(f.Lines(0) == 4);
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
static void Fly(Fx& f) { for (int i = 0; i < 100000 && f.enabled("abort"); i++) f.job->Tick(); assert(!f.enabled("abort")); }

static void flight_runs_to_end() {
  Fx f; Ready(f); assert(!f.enabled("abort")); f.job->OnMenu("launch");
  assert(f.w.placed == 1 && f.enabled("abort") && !f.enabled("area_here") && !f.job->CanFinish());
  Fly(f);
  assert(f.w.removed == 1 && f.w.poses > 0 && f.job->CanFinish());
  for (const char* n : {"lot.las", "lot.pts", "trajektoria.csv", "lot_opis.txt"}) assert(std::filesystem::exists(f.w.dir + "/" + n));
  const long long c = LasCount(f.w.dir); assert(c > 100000);
  std::ifstream pts(f.w.dir + "/lot.pts"); long long n = 0; pts >> n; assert(n == c);
  assert(!f.job->SummaryLines().empty());
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

static void abort_midflight_writes_partial() {
  Fx f; Ready(f); f.job->OnMenu("launch");
  for (int i = 0; i < 10; i++) f.job->Tick();
  f.job->OnMenu("abort");
  assert(!f.enabled("abort") && f.w.removed == 1);
  const long long c = LasCount(f.w.dir); assert(c > 0 && c < 100000);
  assert(has(Text(f.w.dir + "/lot_opis.txt"), "przerwany"));
}

static void state_roundtrip() {
  Fx f; Ready(f); f.job->OnMenu("launch");
  for (int i = 0; i < 12; i++) f.job->Tick();                                  // szereg 1 w toku
  StateKV s; f.job->SaveState(s);
  f.job.reset();                                                                // jak po zamknieciu gry
  std::unique_ptr<Job> j2 = CreateAerialJob(f.w, JobOptions{TraverseKind::Open, CalcMode::Auto});
  assert(!j2->LoadState(StateKV{}));                                           // obcy stan
  assert(j2->LoadState(s)); f.job = std::move(j2);
  assert(f.w.placed == 2 && f.enabled("abort") && has(f.job->Hud(), "Lot ALS: szereg 2/3"));
  Air(f)->TestHooks(5000, nullptr, true); Fly(f);
  Fx full; Ready(full); full.job->OnMenu("launch"); Fly(full);
  const long long a = LasCount(f.w.dir), b = LasCount(full.w.dir); assert(a > 0 && std::llabs(a - b) < b / 50);
  const auto las = Slurp(f.w.dir + "/lot.las"); std::set<int> st; for (long long k = 0; k < a; k++) st.insert((int)U(las, 227 + (size_t)k * 34 + 18, 2));
  assert(st == (std::set<int>{0, 1, 2}));
}

static void strip_and_control_report() {
  Fx f; Ready(f, 20000); f.job->OnMenu("launch"); Fly(f);
  const std::string t = Text(f.w.dir + "/lot_opis.txt");
  assert(has(t, "meanDz") && has(t, "G1") && has(t, "dh") && has(t, "RTK dobry"));
}

int main() {
  flight_runs_to_end(); zero_error_profile_hits_truth(); points_have_strip_ids_and_time(); rays_miss_no_crash(); ground_height_unavailable_falls_back();
  abort_midflight_writes_partial(); state_roundtrip(); strip_and_control_report();
  registry_and_kind(); vertices_and_plan(); launch_needs_area(); launch_refused_when_battery(); area_clear_and_vertex_limit();
  ctrl_vertex_from_list(); params_change_plan(); plan_csv_written();
  std::puts("test_aerial_job OK"); return 0;
}
