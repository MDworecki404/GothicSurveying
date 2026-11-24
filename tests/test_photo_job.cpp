#include <cassert>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include "fake_aerial.hpp"
#include "calc/Png.hpp"

static std::string Text(const std::string& p) { std::ifstream f(p); std::stringstream ss; ss << f.rdbuf(); return ss.str(); }
static std::vector<unsigned char> Slurp(const std::string& p) { std::ifstream f(p, std::ios::binary); return std::vector<unsigned char>(std::istreambuf_iterator<char>(f), {}); }

static int gFx = 0;
struct Fx {
  FakeAerial w; std::unique_ptr<Job> job;
  explicit Fx(bool trueErr = true) {
    w.dir = (std::filesystem::temp_directory_path() / ("gs_photo_job_" + std::to_string(++gFx))).string(); std::filesystem::remove_all(w.dir);
    job = CreatePhotoJob(w, JobOptions{TraverseKind::Open, CalcMode::Auto, trueErr});
  }
  PhotoJob* P() { return static_cast<PhotoJob*>(job.get()); }
  void Vertex(double n, double e) { w.hero = {n, e, 100.0}; job->OnMenu("area_here"); }
  void Square(double s) { Vertex(N0, E0); Vertex(N0, E0 + s); Vertex(N0 + s, E0 + s); Vertex(N0 + s, E0); }
  bool enabled(const std::string& id) const { for (auto& m : job->Menu()) if (m.id == id) return m.enabled; return false; }
  bool listed(const std::string& id) const { for (auto& m : job->Menu()) if (m.id == id) return true; return false; }
  void Controls() {                                                                   // 4 punkty osnowy w obszarze (prawdziwa pozycja x, y, z w cm gry i wspolrzedne siatki)
    int k = 1; for (double n : {20.0, 80.0}) for (double e : {20.0, 80.0}) {
      ControlPoint c; c.id = "G" + std::to_string(k++); c.n = N0 + n; c.e = E0 + e; c.h = 100.0; c.x = e * 100.0; c.z = n * 100.0; c.y = 100.0 * 100.0; w.ctrlList.push_back(c);
    }
  }
  void Fly() { for (int i = 0; i < 100000 && enabled("cancel"); i++) job->Tick(); assert(!enabled("cancel")); }
};

static void registry_menu_and_hud() {
  bool found = false; for (const JobType& t : JobTypes()) if (t.id == "photo") found = t.aerial && t.name.find("Fotogrametria") != std::string::npos;
  assert(found);
  Fx f; assert(f.job->Kind() == JobKind::Photo && f.job->Name().find("Fotogrametria") != std::string::npos);
  for (const char* id : {"cam", "res", "fwd", "side", "area_here", "area_map", "launch", "battery"}) assert(f.listed(id));
  for (const char* id : {"preset", "calib", "adjust", "products", "sidelap"}) assert(!f.listed(id));                       // pozycje LiDAR nie sa w tej robocie
  f.Square(100);
  const std::string h0 = f.job->Hud(); assert(has(h0, "zdjec") && has(h0, "GSD") && has(h0, "szeregow:"));
  f.job->OnMenu("cam"); f.job->OnMenu("cam"); const std::string h1 = f.job->Hud(); assert(h1 != h0);                          // kamera zmienia plan
  f.job->OnMenu("res"); assert(f.job->Hud() != h1);                                                                          // rozdzielczosc zmienia GSD
  f.job->OnMenu("preset"); f.job->OnMenu("adjust"); f.job->OnMenu("products");                                                // ignorowane
  StateKV s; f.job->SaveState(s); Fx g; assert(g.job->LoadState(s) && g.job->Hud() == f.job->Hud());                        // zapis stanu zachowuje kamere i zakladki
}

static void flight_photos_csv_and_bundle() {
  Fx f; f.Controls(); f.Square(100); f.P()->TestHooks(1, nullptr, true);
  f.job->OnMenu("launch"); assert(f.enabled("cancel")); f.Fly();
  const PhotoStats& st = f.P()->Stats();
  assert(st.photos >= 20 && st.ties > 100 && st.adjusted && st.gcp == 2 && st.checks == 2);                                  // 4 punkty osnowy: 2 GCP + 2 kontrolne
  assert(st.rmsAfter < st.rmsBefore && st.rmsAfter < 1.2 && st.rmsAfter > 0.2);                                              // reprojekcja zmniejszona do poziomu szumu pomiaru
  assert(st.checkRmse3d < 0.6);
  const std::string dir = f.w.dir + "/zdjecia";
  int pngs = 0; for (const auto& e : std::filesystem::directory_iterator(dir)) if (e.path().extension() == ".png") pngs++;
  assert(pngs == st.photos);
  const std::vector<unsigned char> b = Slurp(dir + "/IMG_0001.png"); assert(b.size() > 8 + 25 && b[1] == 'P' && b[18] == 1 && b[19] == 64 && b[23] == 240);   // PNG 320 x 240
  assert(std::filesystem::exists(f.w.dir + "/zdjecia.csv") && std::filesystem::exists(f.w.dir + "/fotogrametria_raport.txt") && std::filesystem::exists(f.w.dir + "/fotogrametria_wyrownanie.csv"));
  const std::string csv = Text(f.w.dir + "/zdjecia.csv"); int lines = 0; for (char c : csv) if (c == '\n') lines++;
  assert(lines == st.photos + 1 && has(csv, "plik") && has(csv, "E_prawdziwe"));
  const std::string rep = Text(f.w.dir + "/fotogrametria_raport.txt");
  for (const char* k : {"GSD", "rms reprojekcji", "punkty kontrolne", "Boresight kamery", "Zakladka"}) assert(has(rep, k));
  assert(f.w.Said("Fotogrametria zakonczona") && f.job->CanFinish() && f.w.removed == 1);
}

static void image_pixels_come_from_the_world() {                                                                             // plaski teren: kazdy piksel trafia w teren, kolor z AerialRay
  Fx f(false); f.Square(60); f.P()->TestHooks(1, nullptr, true); f.job->OnMenu("launch"); f.Fly();
  const std::vector<unsigned char> b = Slurp(f.w.dir + "/zdjecia/IMG_0001.png"); assert(b.size() > 300);
  const std::string csv = Text(f.w.dir + "/zdjecia.csv"); assert(!has(csv, "E_prawdziwe"));                                   // bez trybu prawdziwego bledu brak kolumn prawdy
  const auto& st = f.P()->Stats(); assert(st.photos >= 6);
}

static void ortho_after_adjustment() {
  Fx f; f.Controls(); f.Square(100); f.P()->TestHooks(1, nullptr, true);
  assert(!f.enabled("ortho") && !f.enabled("ortho_cancel"));                                                                  // przed lotem brak wyrownania
  f.job->OnMenu("launch"); f.Fly();
  assert(f.enabled("ortho") && !f.enabled("ortho_cancel"));
  f.job->OnMenu("ortho"); assert(f.enabled("ortho_cancel") && !f.enabled("ortho") && has(f.job->Hud(), "Ortofotomapa") && !f.job->CanFinish());
  for (int i = 0; i < 100000 && f.enabled("ortho_cancel"); i++) f.job->Tick();
  assert(!f.enabled("ortho_cancel") && f.w.Said("Ortofotomapa gotowa") && f.job->CanFinish());
  for (const char* n : {"ortofotomapa.png", "ortofotomapa.pgw", "ortofotomapa_raport.txt"}) assert(std::filesystem::exists(f.w.dir + "/" + n));
  int w = 0, h = 0; std::vector<unsigned char> rgb; assert(ReadPng(f.w.dir + "/ortofotomapa.png", w, h, rgb) && w > 100 && h > 100);
  const OrthoInfo& oi = f.P()->OrthoStats(); assert(oi.w == w && oi.h == h && oi.coverage > 0.9 && oi.used >= 6);
  const size_t c = ((size_t)(h / 2) * w + w / 2) * 3; assert(rgb[c] == 0x64 && rgb[c + 1] == 0xA0 && rgb[c + 2] == 0x3C);            // plaski teren: kolor z AerialRay
  assert(oi.checkN == 2 && oi.gcpN == 2 && oi.rmseCheck < 0.6);                                                               // blad planimetryczny na punktach osnowy
  const std::string rep = Text(f.w.dir + "/ortofotomapa_raport.txt"); for (const char* k : {"GSD", "pokrycie", "RMSE planimetryczne", "kontrolnych"}) assert(has(rep, k));
  std::istringstream pgw(Text(f.w.dir + "/ortofotomapa.pgw")); double a, d, b, e, ex, ey; pgw >> a >> d >> b >> e >> ex >> ey; assert(a > 0 && e == -a && ex > E0 && ex < E0 + 5 && ey > N0 + 90 && ey < N0 + 101);   // lewy gorny piksel: siatka N/E
}

static void ortho_cancel_and_unavailable_after_load() {
  Fx f; f.Controls(); f.Square(100); f.P()->TestHooks(1, nullptr, true); f.job->OnMenu("launch"); f.Fly();
  f.job->OnMenu("ortho"); f.job->Tick(); f.job->OnMenu("ortho_cancel");
  assert(f.enabled("ortho") && !f.enabled("ortho_cancel") && !std::filesystem::exists(f.w.dir + "/ortofotomapa.png"));
  StateKV s; f.job->SaveState(s); Fx g; g.Controls(); assert(g.job->LoadState(s) && !g.enabled("ortho"));                      // po wczytaniu zapisu brak danych wyrownania w pamieci
}

static void cancel_midflight_and_too_few_photos() {
  Fx f; f.Square(100); f.P()->TestHooks(1, nullptr, true); f.job->OnMenu("launch");
  for (int i = 0; i < 13; i++) f.job->Tick();                                                                                // wznios + pare zdjec
  f.job->OnMenu("cancel"); assert(!f.enabled("cancel") && f.w.removed == 1);
  const PhotoStats& st = f.P()->Stats(); assert(st.photos >= 1 && st.photos < 20);
  assert(std::filesystem::exists(f.w.dir + "/zdjecia.csv") && std::filesystem::exists(f.w.dir + "/fotogrametria_raport.txt"));
  if (st.photos < 3) assert(!st.adjusted && has(Text(f.w.dir + "/fotogrametria_raport.txt"), "za malo"));
}

int main() {
  registry_menu_and_hud(); flight_photos_csv_and_bundle(); image_pixels_come_from_the_world(); ortho_after_adjustment(); ortho_cancel_and_unavailable_after_load(); cancel_midflight_and_too_few_photos();
  std::puts("test_photo_job OK"); return 0;
}
