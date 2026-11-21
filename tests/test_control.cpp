#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include "calc/Control.hpp"

static bool near(double a, double b, double eps) { return std::fabs(a - b) <= eps; }

static ControlPoint P(const std::string& id, const std::string& world, double x, double z) {
  ControlPoint p; p.id = id; p.world = world; p.n = 5500000.0 + z / 100.0; p.e = 6500000.0 + x / 100.0; p.h = 12.345;
  p.x = x; p.y = 1234.5; p.z = z; p.solution = "fix RTK"; p.pdop = 1.84; p.epochs = 25; p.timeText = "09:15:03";
  return p;
}

static void csv_roundtrip() {
  std::vector<ControlPoint> v = {P("O1", "NEWWORLD.ZEN", 100.0, 200.0), P("O2", "a;b \"c\"", -5000.5, 7000.0)};
  const std::vector<ControlPoint> r = ControlFromCsv(ControlToCsv(v));
  assert(r.size() == 2);
  for (size_t i = 0; i < 2; i++) {
    assert(r[i].id == v[i].id && r[i].world == v[i].world && r[i].solution == v[i].solution && r[i].timeText == v[i].timeText);
    assert(near(r[i].n, v[i].n, 1e-3) && near(r[i].e, v[i].e, 1e-3) && near(r[i].h, v[i].h, 1e-3));
    assert(near(r[i].x, v[i].x, 0.1) && near(r[i].y, v[i].y, 0.1) && near(r[i].z, v[i].z, 0.1) && r[i].epochs == 25 && near(r[i].pdop, 1.8, 0.1));
  }
  assert(ControlCsvHeader() == "id;swiat;N_m;E_m;H_m;x_cm;y_cm;z_cm;rozwiazanie;PDOP;epoki;czas");
}

static void csv_tolerates_garbage() {
  assert(ControlFromCsv("").empty());
  assert(ControlFromCsv(ControlCsvHeader() + "\n").empty());
  const std::string good = ControlCsvRow(P("O1", "W", 1, 2));
  const std::string text = ControlCsvHeader() + "\n" + good + "\n\nO2;W;1;2\nO3;W;abc;2;3;4;5;6;fix;1;2;t\n;;;;;;;;;;;\n" + good + "\n";
  const std::vector<ControlPoint> r = ControlFromCsv(text);
  assert(r.size() == 2 && r[0].id == "O1");                    // wadliwe i puste wiersze pominiete; poprawne zostaja
}

static void next_id() {
  assert(ControlNextId({}) == "O1");
  assert(ControlNextId({P("O1", "W", 0, 0), P("O3", "W", 0, 0)}) == "O4");
  assert(ControlNextId({P("X5", "W", 0, 0), P("O2", "W", 0, 0)}) == "O3");     // id spoza wzorca O<liczba> ignorowane
}

static void near_filters_and_sorts() {
  const std::vector<ControlPoint> v = {P("O1", "W1", 0, 0), P("O2", "W1", 100.0 * 100.0, 0), P("O3", "W1", 400.0 * 100.0, 0),
                                       P("O4", "W2", 50.0 * 100.0, 0), P("O5", "W1", 30.0 * 100.0, 0)};
  std::vector<ControlPoint> a = ControlNear(v, "W1", 0.0, 0.0, 3.0, 300.0);
  assert(a.size() == 2 && a[0].id == "O5" && a[1].id == "O2");                 // 30 m i 100 m, rosnaco; 0 m, 400 m i inny swiat odpadaja
  std::vector<ControlPoint> b = ControlNear(v, "W1", 0.0, 0.0, 0.0, 2.0);
  assert(b.size() == 1 && b[0].id == "O1");
  assert(ControlNear(v, "W3", 0.0, 0.0, 0.0, 1e9).empty());
}

int main() {
  csv_roundtrip(); csv_tolerates_garbage(); next_id(); near_filters_and_sorts();
  std::puts("test_control OK");
  return 0;
}
