#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include "calc/GnssReport.hpp"

static bool has(const std::string& s, const std::string& part) { return s.find(part) != std::string::npos; }
static std::vector<std::string> lines(const std::string& s) {
  std::vector<std::string> out; size_t i = 0;
  while (i < s.size()) { size_t j = s.find('\n', i); if (j == std::string::npos) j = s.size(); out.push_back(s.substr(i, j - i)); i = j + 1; }
  return out;
}

static GnssPoint P1() {
  GnssPoint p; p.name = "P1"; p.timeText = "09:15:03"; p.n = 5500123.4567; p.e = 6500234.5; p.h = 12.3456;
  p.solution = "fix RTK"; p.used = 9; p.pdop = 1.84; p.hRmsMm = 9.2; p.vRmsMm = 14.6; p.epochs = 12; p.antennaM = 2.0;
  return p;
}

static void csv_header_and_row() {
  GnssReport r; r.points.push_back(P1());
  auto l = lines(GnssToCsv(r));
  assert(l[0] == "nr;punkt;czas;N_m;E_m;H_m;rozwiazanie;satelity;PDOP;RMS_H_mm;RMS_V_mm;epoki;antena_m;uwagi");
  assert(l[1] == "1;P1;09:15:03;5500123.457;6500234.500;12.346;fix RTK;9;1.8;9;15;12;2.000;");
  r.showTrueError = true;
  assert(lines(GnssToCsv(r))[0] == "nr;punkt;czas;N_m;E_m;H_m;rozwiazanie;satelity;PDOP;RMS_H_mm;RMS_V_mm;epoki;antena_m;uwagi;blad_N_mm;blad_E_mm;blad_H_mm;blad_2D_mm");
}

static void csv_quotes_note_and_appends_true_error() {
  GnssReport r; r.showTrueError = true; GnssPoint p = P1();
  p.note = "a;b \"c\""; p.hasTrue = true; p.dNmm = -3.2; p.dEmm = 4.1; p.dHmm = 7.8; p.d2Dmm = 5.2;
  r.points.push_back(p);
  std::string row = lines(GnssToCsv(r))[1];
  assert(has(row, ";\"a;b \"\"c\"\"\";-3.2;4.1;7.8;5.2"));
}

static void incomplete_report_is_flagged() {
  GnssReport r; r.complete = false; r.points.push_back(P1());
  assert(lines(GnssToCsv(r))[0].rfind("# UWAGA: pomiar niekompletny", 0) == 0);
}

static void docx_contains_points() {
  GnssReport r; r.dateText = "2026-11-21"; r.points.push_back(P1());
  std::string d = GnssBuildDocx(r);
  assert(d.rfind("PK", 0) == 0 && has(d, "P1") && has(d, "fix RTK") && has(d, "Pomiar GNSS"));
}

static void summary_lines() {
  GnssReport r; GnssPoint a = P1(), b = P1(), c = P1();
  a.name = "P1"; a.pdop = 1.5; b.name = "P2"; b.pdop = 2.0; c.name = "P3"; c.pdop = 3.0; c.solution = "float RTK";
  r.points = {a, b, c};
  std::string all; for (auto& l : GnssSummaryLines(r)) all += l + "\n";
  assert(has(all, "Punkty: 3") && has(all, "w fixie: 2") && has(all, "sredni PDOP: 2.2"));
  assert(has(all, "Najgorszy punkt: P3 (PDOP 3.0)"));
  r.showTrueError = true; a.hasTrue = b.hasTrue = c.hasTrue = true; a.d2Dmm = 4; b.d2Dmm = 7.5; c.d2Dmm = 6; r.points = {a, b, c};
  all.clear(); for (auto& l : GnssSummaryLines(r)) all += l + "\n";
  assert(has(all, "Najgorszy punkt: P2 (7.5 mm)"));
}

int main() {
  csv_header_and_row(); csv_quotes_note_and_appends_true_error(); incomplete_report_is_flagged(); docx_contains_points(); summary_lines();
  std::puts("test_gnss_report OK");
  return 0;
}
