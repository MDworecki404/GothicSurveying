#include <cassert>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include "calc/Ortho.hpp"

static bool near(double a, double b, double eps = 1e-9) { return std::fabs(a - b) <= eps; }
static const double PI = 3.14159265358979323846;

// Teren i wzor barw w ukladzie lokalnym (E, N od 0): gladki, zeby porownanie z ortofotomapa bylo jednoznaczne.
static double Terrain(double e, double n) { return 3.0 * std::sin(e / 20.0) * std::cos(n / 25.0) + 0.04 * e; }
static void Color(double e, double n, unsigned char* c) { c[0] = (unsigned char)(128 + 100 * std::sin(e / 9.0)); c[1] = (unsigned char)(128 + 100 * std::sin(n / 11.0)); c[2] = (unsigned char)(128 + 60 * std::sin((e + n) / 15.0)); }

struct Scene { OrthoParams p; std::vector<OrthoCam> cams; std::vector<Vec3> pts; std::vector<std::vector<unsigned char>> imgs; };

static bool Hit(const Vec3& o, const Vec3& d, double& e, double& n) {                      // przeciecie promienia z terenem: marsz co 0,5 m i bisekcja
  auto above = [&](double t) { return o.z + t * d.z - Terrain(o.x + t * d.x, o.y + t * d.y) > 0; };
  double t0 = 0; if (!above(0)) return false;
  for (double t = 2.0; t < 300; t += 2.0) { if (above(t)) { t0 = t; continue; } double lo = t0, hi = t; for (int i = 0; i < 40; i++) { const double m = (lo + hi) / 2; (above(m) ? lo : hi) = m; } const double r = (lo + hi) / 2; e = o.x + r * d.x; n = o.y + r * d.y; return true; }
  return false;
}

// 4 x 4 zdjecia 320 x 240 (kamera 63 st.) z wysokosci 50 m nad terenem, obszar 100 x 100 m; punkty NMPT z terenu; zdjecia renderowane analitycznie.
static Scene MakeBase() {
  Scene s; s.p.imgW = 160; s.p.imgH = 120; s.p.f = 80.0 / std::tan(31.5 * PI / 180.0); s.p.cx = 80; s.p.cy = 60; s.p.lever = Vec3{0, 0.1, -0.2}; s.p.Rb = RotFromRpy(0, 0, 0);
  s.p.gsd = 0.5; s.p.minE = 0; s.p.maxE = 100; s.p.minN = 0; s.p.maxN = 100; s.p.originE = 6500000.0; s.p.originN = 5500000.0;
  GnssRng r(99);
  for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) {
    OrthoCam c{}; c.p[0] = 12.5 + 25.0 * i; c.p[1] = 12.5 + 25.0 * j; c.p[2] = Terrain(c.p[0], c.p[1]) + 50.0; c.a[0] = (r.Uniform() - 0.5); c.a[1] = (r.Uniform() - 0.5); c.a[2] = 20.0 * j;
    s.cams.push_back(c);
    TrajSample ps{}; ps.pos = {c.p[0], c.p[1], c.p[2]}; ps.rollDeg = c.a[0]; ps.pitchDeg = c.a[1]; ps.yawDeg = c.a[2];
    std::vector<unsigned char> img((size_t)160 * 120 * 3, 0xA0);
    for (int y = 0; y < 120; y++) for (int x = 0; x < 160; x++) {
      const AlsRay ray = PixelRay(ps, s.p.Rb, s.p.lever, x + 0.5, y + 0.5, s.p.f, s.p.cx, s.p.cy); double e, n;
      if (Hit(ray.origin, ray.dir, e, n)) Color(e, n, &img[((size_t)y * 160 + x) * 3]);
    }
    s.imgs.push_back(img);
  }
  for (int k = 0; k < 800; k++) { const double e = r.Uniform() * 100, n = r.Uniform() * 100; s.pts.push_back({e, n, Terrain(e, n)}); }
  return s;
}
static Scene Make(double gsd) { static const Scene base = MakeBase(); Scene s = base; s.p.gsd = gsd; return s; }   // zdjecia renderowane raz (sa niezalezne od rozmiaru piksela ortofotomapy)
static OrthoResult Run(const Scene& s, std::vector<OrthoCam> cams, const std::string& base, std::vector<unsigned char>* out = nullptr) {
  Orthorectifier o(s.p, cams, s.pts, [&](int c, std::vector<unsigned char>& rgb) { if (c < 0 || c >= (int)s.imgs.size()) return false; rgb = s.imgs[c]; return true; }, base + ".png", base + ".pgw");
  for (int i = 0; i < 100000 && !o.Step(50); i++) {}
  if (out && o.Result().ok) { int w, h; ReadPng(base + ".png", w, h, *out); }
  return o.Result();
}
static double MeanColorError(const OrthoParams& p, const OrthoResult& r, const std::vector<unsigned char>& rgb) {
  double sum = 0; long long n = 0;
  for (int y = 4; y < r.h - 4; y++) for (int x = 4; x < r.w - 4; x++) {
    unsigned char c[3]; Color(p.minE + (x + 0.5) * r.gsd, p.maxN - (y + 0.5) * r.gsd, c); const unsigned char* q = &rgb[((size_t)y * r.w + x) * 3];
    sum += (std::abs((int)q[0] - c[0]) + std::abs((int)q[1] - c[1]) + std::abs((int)q[2] - c[2])) / 3.0; n++;
  }
  return sum / n;
}

static void ortho_matches_the_world_and_writes_files() {
  const Scene s = Make(0.5); const std::string base = (std::filesystem::temp_directory_path() / "ortho_t1").string(); std::vector<unsigned char> img;
  const OrthoResult r = Run(s, s.cams, base, &img);
  assert(r.ok && r.w == 200 && r.h == 200 && near(r.gsd, 0.5) && r.coverage > 0.97 && r.used >= 6);
  assert(img.size() == (size_t)200 * 200 * 3 && MeanColorError(s.p, r, img) < 12.0);                                  // kolory z ortofotomapy = wzor w danym punkcie terenu
  std::ifstream f(base + ".pgw"); double a, d, b, e, c, g; f >> a >> d >> b >> e >> c >> g;                           // plik pozycjonujacy: rozmiar piksela, obroty, lewy gorny piksel (wsp. siatki)
  assert(near(a, 0.5) && near(d, 0) && near(b, 0) && near(e, -0.5) && near(c, 6500000.0 + 0.25) && near(g, 5500000.0 + 100 - 0.25));
}

static void shifted_poses_make_a_worse_orthophoto() {
  const Scene s = Make(0.5); std::vector<unsigned char> good, bad; const std::string d = std::filesystem::temp_directory_path().string();
  const OrthoResult a = Run(s, s.cams, d + "/ortho_t2a", &good);
  std::vector<OrthoCam> shifted = s.cams; for (size_t i = 0; i < shifted.size(); i++) { shifted[i].p[0] += (i % 2 ? 2.5 : -2.5); shifted[i].p[1] += (i % 3 ? 1.5 : -1.5); }   // bledne pozy: szwy i przesuniecia
  const OrthoResult b = Run(s, shifted, d + "/ortho_t2b", &bad);
  assert(a.ok && b.ok && MeanColorError(s.p, b, bad) > 2.0 * MeanColorError(s.p, a, good));
}

static void polygon_and_cell_limit() {
  Scene s = Make(0.5); s.p.polygon = {{0, 0}, {0, 100}, {100, 50}};                                                    // trojkat (n, e): poza nim tlo, pokrycie liczone w obrebie wielokata
  const OrthoResult r = Run(s, s.cams, (std::filesystem::temp_directory_path() / "ortho_t3").string()); assert(r.ok && r.coverage > 0.95);
  Scene big = Make(0.5); big.p.maxCells = 10000; const OrthoResult g = Run(big, big.cams, (std::filesystem::temp_directory_path() / "ortho_t4").string());
  assert(g.ok && (long long)g.w * g.h <= 10000 && g.gsd > 0.5);                                                          // za duza siatka: wiekszy piksel
}

static void planimetric_error_of_a_point() {
  const Scene s = Make(0.5); const OrthoCam& c = s.cams[5]; const double e = 38.0, n = 33.0; const Vec3 known{e, n, Terrain(e, n)};
  TrajSample ps{}; ps.pos = {c.p[0], c.p[1], c.p[2]}; ps.rollDeg = c.a[0]; ps.pitchDeg = c.a[1]; ps.yawDeg = c.a[2];
  double u, v; assert(ProjectWorld(known, ps, s.p.Rb, s.p.lever, s.p.f, s.p.cx, s.p.cy, u, v));
  double dE, dN; assert(OrthoPointError(s.p, c, u, v, known, dE, dN) && near(dE, 0, 1e-6) && near(dN, 0, 1e-6));        // poprawna poza: punkt wraca na swoje miejsce
  OrthoCam moved = c; moved.p[0] += 1.0; assert(OrthoPointError(s.p, moved, u, v, known, dE, dN) && near(dE, 1.0, 1e-3));   // poza przesunieta o 1 m w E: blad 1 m
}

static void rejects_bad_input() {
  const Scene s = Make(1.0); const std::string base = (std::filesystem::temp_directory_path() / "ortho_t5").string();
  assert(!Run(s, {}, base).ok && !Run(s, {}, base).msg.empty());                                                       // brak zdjec
  Scene np = s; np.pts.clear(); assert(!Run(np, np.cams, base).ok);                                                    // brak punktow do NMPT
  Orthorectifier o(s.p, s.cams, s.pts, [](int, std::vector<unsigned char>&) { return false; }, base + ".png", base + ".pgw");
  for (int i = 0; i < 100000 && !o.Step(50); i++) {} assert(!o.Result().ok && !o.Result().msg.empty());                 // zdjecia nie do odczytania
}

int main() {
  ortho_matches_the_world_and_writes_files(); shifted_poses_make_a_worse_orthophoto(); polygon_and_cell_limit(); planimetric_error_of_a_point(); rejects_bad_input();
  std::puts("test_ortho OK"); return 0;
}
