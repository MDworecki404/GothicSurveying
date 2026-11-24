#include <cassert>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include "job/Jobs.hpp"

static bool has(const std::string& s, const std::string& part) { return s.find(part) != std::string::npos; }
static bool near(double a, double b, double eps) { return std::fabs(a - b) <= eps; }

// Swiat: podloga 1,5 m pod okiem skanera (grunt H = 100) i sciana 10 m na polnoc od stanowiska.
struct FakeScan : IWorld {
  std::vector<std::string> said; std::string dir; bool ground = true; bool controlNear = false;
  GridPos station{5500000.0, 6500000.0, 100.0};
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
  bool PlaceTachymeter() override { return ground; }
  GridPos StationGrid() const override { return station; }
  std::vector<ControlPoint> ControlNearHero(double) const override {
    if (!controlNear) return {};
    ControlPoint c; c.id = "G1"; c.n = station.n + 0.012; c.e = station.e - 0.008; c.h = station.h + 0.02; return {c};
  }
  std::string ScanOutDir() override { std::filesystem::create_directories(dir); return dir; }
  bool ScanRay(double az, double elev, double& dist, double& cosInc, bool& isVob) const override {
    const double pi = 3.14159265358979323846, a = az * pi / 180.0, e = elev * pi / 180.0, h = 1.5;
    double best = 1e9; isVob = false; cosInc = 1.0;
    if (e < -1e-6) { best = h / std::sin(-e); cosInc = std::sin(-e); }                       // podloga
    const double north = std::cos(e) * std::cos(a);
    if (north > 1e-6) { const double t = 10.0 / north; if (t < best) { best = t; isVob = true; cosInc = north; } }   // sciana (vob)
    if (best > 1e8) return false;
    dist = best; return true;
  }
};

static int gFx = 0;
struct Fx {
  FakeScan w; std::unique_ptr<Job> job;
  Fx() {
    w.dir = (std::filesystem::temp_directory_path() / ("gs_scan_job_" + std::to_string(++gFx))).string();
    std::filesystem::remove_all(w.dir);
    job = CreateScanJob(w, JobOptions{TraverseKind::Open, CalcMode::Auto});
  }
  bool enabled(const std::string& id) const { for (auto& m : job->Menu()) if (m.id == id) return m.enabled; return false; }
  void res(int times) { for (int i = 0; i < times; i++) job->OnMenu("res"); }              // preset: 1 (sredni) -> 2 -> 3 -> 0 (gruby) -> 1 ...
  void runScan() { job->OnMenu("scan"); for (int i = 0; i < 100000 && !enabled("station"); i++) job->Tick(); }
};

static void registry_and_menu_flow() {
  bool found = false; for (const JobType& t : JobTypes()) if (t.id == "scan") found = t.scan && t.name == "Skaner laserowy";
  assert(found);
  Fx f; assert(f.job->Kind() == JobKind::Scan && !f.enabled("scan") && !f.enabled("cancel") && !f.enabled("finish") && f.enabled("station") && f.enabled("res"));
  f.job->OnMenu("station"); assert(f.enabled("scan") && !f.job->CanFinish());
  f.res(4);                                                                                  // cztery presety: z powrotem na ten sam
  assert(has(f.job->Hud(), "skan"));
}

static void full_scan_writes_cloud_of_floor_and_wall() {
  Fx f; f.job->OnMenu("station"); f.res(3);                                                 // gruby krok 1 st.
  f.job->OnMenu("scan"); assert(f.enabled("cancel") && !f.enabled("station") && !f.enabled("finish") && has(f.job->Hud(), "%"));
  for (int i = 0; i < 100000 && !f.enabled("station"); i++) f.job->Tick();
  assert(f.enabled("finish") && f.job->CanFinish());
  const std::string base = f.w.dir + "/skan01";
  std::ifstream pts(base + ".pts"); std::string first; std::getline(pts, first); const long long n = std::stoll(first);
  assert(n > 10000 && n <= 360LL * 126 && std::filesystem::exists(base + ".las") && std::filesystem::exists(base + "_opis.txt") && !std::filesystem::exists(base + ".tmp"));
  double x, y, z; int in; long long wall = 0, floorN = 0, other = 0, k = 0;
  while (pts >> x >> y >> z >> in) {
    k++;
    if (std::fabs(y - (5500000.0 + 10.0)) < 0.02) wall++;                                   // sciana: N = 10 m od stanowiska (szum mm, zaokraglenie pts)
    else if (std::fabs(z - 100.0) < 0.02) floorN++;                                          // podloga: H = grunt
    else other++;
  }
  assert(k == n && wall > 1000 && floorN > 1000 && other < n / 100);
  const std::vector<std::string> sum = f.job->SummaryLines();
  assert(sum.size() == 2 && has(sum[1], "skan01") && has(sum[1], "dowolne stanowisko"));
  f.job->OnMenu("station"); f.runScan();                                                    // drugi skan: osobne pliki
  assert(std::filesystem::exists(f.w.dir + "/skan02.pts") && f.job->SummaryLines().size() == 3);
}

static void station_on_control_point_uses_its_coordinates() {
  Fx f; f.w.controlNear = true; f.job->OnMenu("station"); f.res(3);
  f.runScan();
  const std::vector<std::string> sum = f.job->SummaryLines();
  assert(has(sum[1], "znak osnowy G1") && has(sum[1], "N 5500000.012"));
  std::ifstream pts(f.w.dir + "/skan01.pts"); std::string first; std::getline(pts, first); double x, y, z; int in; pts >> x >> y >> z >> in;
  assert(near(y, 5500000.012, 12.0));                                                       // wspolrzedne punktow wzgledem znaku osnowy
}

static void cancel_removes_temp_file_and_state_roundtrip() {
  Fx f; f.job->OnMenu("station"); f.res(2);                                                 // 0,1 st.: ponad 4 mln promieni, w 3 klatkach sie nie skonczy
  f.job->OnMenu("scan"); for (int i = 0; i < 3; i++) f.job->Tick();
  assert(f.enabled("cancel")); f.job->OnMenu("cancel");
  assert(!f.enabled("cancel") && f.enabled("scan") && !f.job->CanFinish());
  for (auto& e : std::filesystem::directory_iterator(f.w.dir)) assert(e.path().extension() != ".tmp");
  f.res(1); f.runScan();                                                                      // z 0,1 na gruby i pelny skan
  StateKV st; assert(f.job->CanSaveState()); f.job->SaveState(st);
  Fx g; assert(g.job->LoadState(StateKV::FromText(st.ToText())));
  assert(g.job->SummaryLines() == f.job->SummaryLines() && g.enabled("finish") && g.enabled("scan"));
}

struct LvlWorld : FakeScan { bool InstrumentLevelingEnabled() const override { return true; } };
static void leveling_blocks_scan() {
  LvlWorld w; w.dir = (std::filesystem::temp_directory_path() / "gs_scan_job_lvl").string();
  auto job = CreateScanJob(w, JobOptions{TraverseKind::Open, CalcMode::Auto});
  job->OnMenu("station");
  bool scanOn = false; for (auto& m : job->Menu()) if (m.id == "scan") scanOn = m.enabled;
  assert(!scanOn && has(job->InstrumentBlock(), "niewypoziomowany"));
  job->GetLibella()->x = 0; job->GetLibella()->y = 0;
  for (auto& m : job->Menu()) if (m.id == "scan") scanOn = m.enabled;
  assert(scanOn && job->InstrumentBlock().empty());
}

int main() {
  registry_and_menu_flow(); full_scan_writes_cloud_of_floor_and_wall(); station_on_control_point_uses_its_coordinates(); cancel_removes_temp_file_and_state_roundtrip(); leveling_blocks_scan();
  std::puts("test_scan_job OK");
  return 0;
}
