#include <cassert>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
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
  bool GroundHeightM(double, double, double& h) const override { h = 100.0; return true; }
  bool PlaceDrone(const GridPos&) override { placed++; return true; }
  void RemoveDrone() override { removed++; }
  void SetDronePose(const GridPos&, double, double, double, double) override { poses++; }
  std::string AerialOutDir() override { std::filesystem::create_directories(dir); return dir; }
  std::vector<ControlPoint> ControlNearHero(double) const override { ControlPoint c; c.id = "G1"; c.n = N0 + 5; c.e = E0 + 7; c.h = 100.0; return {c}; }
  bool Said(const std::string& part) const { for (const std::string& s : said) if (has(s, part)) return true; return false; }
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

int main() {
  registry_and_kind(); vertices_and_plan(); launch_needs_area(); launch_refused_when_battery(); area_clear_and_vertex_limit();
  ctrl_vertex_from_list(); params_change_plan(); plan_csv_written();
  std::puts("test_aerial_job OK"); return 0;
}
