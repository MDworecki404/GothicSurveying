#include <cassert>
#include <cstdio>
#include <filesystem>
#include "fake_aerial.hpp"

static int gFx = 0;
struct Fx {
  FakeAerial w; std::unique_ptr<Job> job;
  explicit Fx(bool photo) {
    w.dir = (std::filesystem::temp_directory_path() / ("gs_nextstep_air_" + std::to_string(++gFx))).string(); std::filesystem::remove_all(w.dir);
    job = photo ? CreatePhotoJob(w, JobOptions{TraverseKind::Open, CalcMode::Auto, false}) : CreateAerialJob(w, JobOptions{TraverseKind::Open, CalcMode::Auto, false});
  }
  void Vertex(double n, double e) { w.hero = {n, e, 100.0}; job->OnMenu("area_here"); }
  void Square(double s) { Vertex(N0, E0); Vertex(N0, E0 + s); Vertex(N0 + s, E0 + s); Vertex(N0 + s, E0); }
  bool enabled(const std::string& id) const { for (auto& m : job->Menu()) if (m.id == id) return m.enabled; return false; }
  void Fly() { for (int i = 0; i < 100000 && enabled("cancel"); i++) job->Tick(); assert(!enabled("cancel")); }
  void Run(const char* id, const char* cancelId) { job->OnMenu(id); for (int i = 0; i < 100000 && enabled(cancelId); i++) job->Tick(); assert(!enabled(cancelId)); }
  void CheckEnabled() const {
    const std::string id = job->NextStep(); if (id.empty()) return;
    bool found = false; for (const MenuItem& m : job->Menu()) if (m.id == id) { found = true; assert(m.enabled); } assert(found);
  }
};

static void aerial_fresh_asks_for_area_then_launch() {
  Fx f(false); assert(f.job->NextStep() == "area_map"); f.CheckEnabled();
  f.Vertex(N0, E0); f.Vertex(N0, E0 + 50); assert(f.job->NextStep() == "area_map");   // 2 wierzcholki to jeszcze nie obszar
  f.Vertex(N0 + 50, E0 + 50); assert(f.job->NextStep() == "launch"); f.CheckEnabled();
}

static void aerial_after_flight_asks_for_adjust_then_products_then_finish() {
  Fx f(false); f.Square(100); for (int i = 0; i < 3; i++) f.job->OnMenu("preset");
  static_cast<AerialJob*>(f.job.get())->TestHooks(20000, nullptr, true);          // jak w test_aerial_job: gestosc i budzet impulsow na klatke
  f.job->OnMenu("launch");
  assert(f.job->NextStep().empty());                         // w locie nic nie rekomendujemy
  f.Fly();
  assert(f.job->NextStep() == "adjust"); f.CheckEnabled();
  f.Run("adjust", "adjust_cancel");
  assert(f.job->NextStep() == "products"); f.CheckEnabled();
  f.Run("products", "products_cancel");
  assert(f.job->NextStep().empty() && f.job->CanFinish());   // reszte robi linia "Zakoncz pomiar"
}

static void photo_flow_has_no_lidar_steps() {
  Fx f(true); assert(f.job->NextStep() == "area_map");
  f.Square(100); assert(f.job->NextStep() == "launch"); f.CheckEnabled();
  static_cast<PhotoJob*>(f.job.get())->TestHooks(1, nullptr, true);               // jak w test_photo_job: 1 zdjecie na klatke
  f.job->OnMenu("launch"); f.Fly();
  assert(f.job->NextStep() != "adjust" && f.job->NextStep() != "products");
  assert((f.job->NextStep() == "ortho") == f.enabled("ortho")); f.CheckEnabled();
}

struct ScanWorld : FakeAerial { bool PlaceTachymeter() override { return true; } };

static void scan_flow() {
  ScanWorld w; auto j = CreateScanJob(w, JobOptions{TraverseKind::Open, CalcMode::Auto, false});
  assert(j->NextStep() == "station");
  j->OnMenu("station"); assert(j->NextStep() == "scan");
  bool ok = false; for (const MenuItem& m : j->Menu()) if (m.id == "scan") ok = m.enabled; assert(ok);
}

int main() {
  aerial_fresh_asks_for_area_then_launch(); aerial_after_flight_asks_for_adjust_then_products_then_finish(); photo_flow_has_no_lidar_steps(); scan_flow();
  std::puts("test_nextstep_air OK");
  return 0;
}
