#include <cassert>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>
#include "calc/AlsProducts.hpp"

static bool near(double a, double b, double eps = 1e-9) { return std::fabs(a - b) <= eps; }
static const double N0 = 5500000.0, E0 = 6500000.0, H0 = 100.0;

struct PScene { std::string raw, dir; std::vector<std::uint8_t> expect; long long n = 0; };

// Chmura 40 x 40 m co 0,5 m; wysokosc z funkcji (x, y) w metrach wzgledem poczatku, klasa oczekiwana z funkcji.
static PScene Make(const std::string& name, const std::function<double(double, double)>& z, const std::function<int(double, double)>& cls) {
  PScene s; s.dir = (std::filesystem::temp_directory_path() / name).string(); std::filesystem::remove_all(s.dir); std::filesystem::create_directories(s.dir);
  s.raw = s.dir + "/in.bin"; AlsRawWriter w; w.Open(s.raw);
  for (double y = 0; y < 40; y += 0.5) for (double x = 0; x < 40; x += 0.5) {
    AlsPoint p{}; p.x = (float)x; p.y = (float)y; p.z = (float)(z(x, y) - H0 + 0.0); p.gps = 345600.0 + s.n * 1e-3; p.strip = 0; p.i = 100; p.r = p.g = p.b = 128; w.Append(p);
    s.expect.push_back((std::uint8_t)cls(x, y)); s.n++;
  }
  w.Close(); return s;
}
static ProductsResult Run(const PScene& s, ProductsParams pp = ProductsParams()) {
  AlsProducts pr(s.raw, s.dir, N0, E0, H0, pp);
  for (int i = 0; i < 100000 && !pr.Step(50); i++) {}
  return pr.Result();
}
static std::string Text(const std::string& p) { std::ifstream f(p); std::stringstream ss; ss << f.rdbuf(); return ss.str(); }

static void classification_and_dtm() {
  auto roof = [](double x, double y) { return x >= 15 && x < 25 && y >= 15 && y < 25; };
  auto bush = [](double x, double y) { return x >= 30 && x < 35 && y >= 30 && y < 35; };
  auto ground = [](double x, double) { return 100.0 + 0.05 * x; };
  const PScene s = Make("als_prod_cls", [&](double x, double y) { return ground(x, y) + (roof(x, y) ? 6.0 : bush(x, y) ? 0.6 : 0.0); },
                        [&](double x, double y) { return roof(x, y) ? 5 : bush(x, y) ? 3 : 2; });
  const ProductsResult r = Run(s); assert(r.ok && r.count == s.n && r.classes.size() == (size_t)s.n);
  long long bad = 0; for (size_t i = 0; i < s.expect.size(); i++) if (r.classes[i] != s.expect[i]) bad++;
  assert(bad <= s.n / 50);                                                                      // klasyfikacja: najwyzej 2% niezgodnosci (krawedzie)
  assert(r.cls[2] > 5000 && r.cls[3] >= 90 && r.cls[5] >= 390 && r.cls[2] + r.cls[3] + r.cls[4] + r.cls[5] == r.count);
  const std::string asc = Text(s.dir + "/nmt.asc"); std::istringstream in(asc); std::string key; int nc = 0, nr = 0; double xll = 0, yll = 0, cs = 0, nd = 0;
  in >> key >> nc >> key >> nr >> key >> xll >> key >> yll >> key >> cs >> key >> nd; assert(nc >= 39 && nr >= 39 && near(cs, 1.0) && near(nd, -9999));
  std::vector<double> v((size_t)nc * nr); for (double& d : v) in >> d;                          // wiersze od polnocy
  auto at = [&](double e, double n) { const int c = (int)((e - xll) / cs), rr = nr - 1 - (int)((n - yll) / cs); return v[(size_t)rr * nc + c]; };
  assert(near(at(E0 + 20.0, N0 + 20.0), 100.0 + 0.05 * 20.0, 0.15));                           // NMT pod dachem = teren
  assert(near(at(E0 + 32.0, N0 + 32.0), 100.0 + 0.05 * 32.0, 0.15));                           // pod krzakiem tez
  assert(std::filesystem::exists(s.dir + "/nmpt.asc") && std::filesystem::exists(s.dir + "/lot_klasyfikacja.las") && std::filesystem::exists(s.dir + "/produkty.txt"));
}

static void volumes_of_pit() {                                                                   // plaski teren 100 m z dolem 10 x 10 m, glebokosc 1 m
  auto pit = [](double x, double y) { return x >= 15 && x < 25 && y >= 15 && y < 25; };
  const PScene s = Make("als_prod_pit", [&](double x, double y) { return pit(x, y) ? 99.0 : 100.0; }, [](double, double) { return 2; });
  const ProductsResult r = Run(s); assert(r.ok);
  const double ref = 100.0 - 100.0 / 1600.0;                                                     // srednia NMT
  assert(near(r.refLevel, ref, 0.02));
  assert(near(r.volCut, 1500.0 * (100.0 - ref), 8.0) && near(r.volFill, 100.0 * (ref - 99.0), 8.0));   // wykop nad poziomem = nasyp pod poziomem
  assert(near(r.volCut, r.volFill, 6.0));
}

static void contours_of_slope() {
  const PScene s = Make("als_prod_slope", [](double x, double) { return 100.0 + 0.05 * x; }, [](double, double) { return 2; });
  ProductsParams pp; pp.contourStepM = 1.0; const ProductsResult r = Run(s, pp); assert(r.ok && r.contours > 20);
  std::ifstream in(s.dir + "/warstwice.csv"); std::string line; std::getline(in, line); assert(line == "H_m;E1;N1;E2;N2");
  int n = 0; while (std::getline(in, line)) {
    double h, e1, n1, e2, n2; assert(std::sscanf(line.c_str(), "%lf;%lf;%lf;%lf;%lf", &h, &e1, &n1, &e2, &n2) == 5);
    assert(near(h, std::round(h), 1e-6) && h >= 100.0 && h <= 102.0);
    assert(near(e1, E0 + (h - 100.0) / 0.05, 0.6) && near(e2, E0 + (h - 100.0) / 0.05, 0.6)); n++;     // izolinia 101 m przy x = 20 m, 102 m przy x = 40 m
  }
  assert(n == r.contours);
  const std::string dxf = Text(s.dir + "/warstwice.dxf"); assert(dxf.find("SECTION") != std::string::npos && dxf.find("LINE") != std::string::npos && dxf.find("EOF") != std::string::npos);
}

static void bad_input() {
  PScene s; s.dir = (std::filesystem::temp_directory_path() / "als_prod_bad").string(); std::filesystem::create_directories(s.dir); s.raw = s.dir + "/nie_ma.bin";
  const ProductsResult r = Run(s); assert(!r.ok && !r.msg.empty());
  s.raw = s.dir + "/pusty.bin"; { AlsRawWriter w; w.Open(s.raw); w.Close(); }
  const ProductsResult e = Run(s); assert(!e.ok && !e.msg.empty());
}

int main() {
  classification_and_dtm(); volumes_of_pit(); contours_of_slope(); bad_input();
  std::puts("test_alsproducts OK"); return 0;
}
