#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include "calc/TachyReport.hpp"

static bool has(const std::string& s, const std::string& part) { return s.find(part) != std::string::npos; }
static std::vector<std::string> lines(const std::string& s) {
  std::vector<std::string> out; size_t i = 0;
  while (i < s.size()) { size_t j = s.find('\n', i); if (j == std::string::npos) j = s.size(); out.push_back(s.substr(i, j - i)); i = j + 1; }
  return out;
}

static TachyPoint T1() {
  TachyPoint p; p.name = "T1"; p.timeText = "09:15:03"; p.hzGon = 123.45678; p.vGon = 98.76543; p.slopeM = 55.1234; p.hDistM = 55.0;
  p.dHM = 3.4567; p.n = 5500012.3456; p.e = 6500034.5; p.h = 12.3456; p.prismM = 1.5;
  return p;
}
static TachyReport R1() {
  TachyReport r; r.stationN = 5500000; r.stationE = 6500000; r.stationH = 100; r.instrH = 1.5; r.azBackGon = 0; r.points.push_back(T1());
  return r;
}

static void csv_header_and_row() {
  TachyReport r = R1();
  auto l = lines(TachyToCsv(r));                                          // l[0] = linia stanowiska, l[1] = naglowek
  assert(l[1] == "nr;punkt;czas;Hz_g;V_g;D_skosna_m;d_pozioma_m;dh_m;N_m;E_m;H_m;wys_lustra_m;uwagi");
  assert(l[2] == "1;T1;09:15:03;123.4568;98.7654;55.123;55.000;3.457;5500012.346;6500034.500;12.346;1.500;");
  r.showTrueError = true;
  assert(TachyCsvHeader(true) == "nr;punkt;czas;Hz_g;V_g;D_skosna_m;d_pozioma_m;dh_m;N_m;E_m;H_m;wys_lustra_m;uwagi;blad_N_m;blad_E_m;blad_H_m;blad_2D_m");
  assert(lines(TachyToCsv(r))[1] == TachyCsvHeader(true));
}

static void csv_station_line_and_quoting() {
  TachyReport r = R1(); r.showTrueError = true;
  TachyPoint p = T1(); p.note = "a;b \"c\""; p.hasTrue = true; p.dNm = -0.0032; p.dEm = 0.0041; p.dHm = 0.0078; p.d2Dm = 0.0052;
  r.points = {p};
  auto l = lines(TachyToCsv(r));
  assert(l[0] == "# stanowisko;5500000.000;6500000.000;100.000;1.500;0.0000");
  assert(has(l[2], ";\"a;b \"\"c\"\"\";-0.003;0.004;0.008;0.005"));
}

static void incomplete_flag() {
  TachyReport r = R1(); r.complete = false;
  assert(lines(TachyToCsv(r))[0].rfind("# UWAGA: pomiar niekompletny", 0) == 0);
}

static void docx_contains_points() {
  TachyReport r = R1(); r.dateText = "2026-11-21";
  std::string d = TachyBuildDocx(r);
  assert(d.rfind("PK", 0) == 0 && has(d, "T1") && has(d, "Tachimetria") && has(d, "w:w=\"1076\""));          // 13 kolumn: 14000 / 13
  r.showTrueError = true;
  assert(has(TachyBuildDocx(r), "w:w=\"823\""));              // 17 kolumn: 14000 / 17
}

static void summary_lines() {
  TachyReport r = R1(); TachyPoint a = T1(), b = T1(), c = T1();
  a.name = "T1"; a.slopeM = 20; b.name = "T2"; b.slopeM = 40; c.name = "T3"; c.slopeM = 60; r.points = {a, b, c};
  std::string all; for (auto& l : TachySummaryLines(r)) all += l + "\n";
  assert(has(all, "Punkty: 3") && has(all, "srednia odleglosc: 40.0 m") && has(all, "najdluzsza celowa: 60.0 m"));
  r.showTrueError = true; a.hasTrue = b.hasTrue = c.hasTrue = true; a.d2Dm = 0.004; b.d2Dm = 0.008; c.d2Dm = 0.006; r.points = {a, b, c};
  all.clear(); for (auto& l : TachySummaryLines(r)) all += l + "\n";
  assert(has(all, "Najgorszy punkt: T2 (0.008 m)"));
}

int main() {
  csv_header_and_row(); csv_station_line_and_quoting(); incomplete_flag(); docx_contains_points(); summary_lines();
  std::puts("test_tachy_report OK");
  return 0;
}
