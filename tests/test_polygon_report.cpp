#include <cassert>
#include <cstdio>
#include <string>
#include "calc/PolygonReport.hpp"

static bool has(const std::string& s, const std::string& part) { return s.find(part) != std::string::npos; }

static PolygonReport Guide(bool trueErr) {
  PolygonReport rep; rep.kind = TraverseKind::Anchored; rep.dateText = "2026-11-24"; rep.showTrueError = trueErr;
  rep.in.kind = TraverseKind::Anchored; rep.in.nN1 = 1000; rep.in.eN1 = 1000; rep.in.nA = 1100; rep.in.eA = 1200; rep.in.nB = 1460; rep.in.eB = 1560; rep.in.nN2 = 1600; rep.in.eN2 = 1640;
  rep.in.betaGon = {184.8109, 203.7398, 172.4127, 201.6052}; rep.in.sideM = {170.306, 199.991, 147.662};
  rep.res = PolygonCompute(rep.in);
  rep.names = {"A", "1", "2", "B"};
  const char* nm[4] = {"A", "1", "2", "B"}; const char* bk[4] = {"N1", "A", "1", "2"}; const char* fr[4] = {"1", "2", "B", "N2"};
  for (int i = 0; i < 4; i++) { PolyStationRec s; s.name = nm[i]; s.back = bk[i]; s.fore = fr[i]; s.betaGon = rep.in.betaGon[i]; s.klBack = 10; s.klFore = 10 + s.betaGon; s.kpBack = 210; s.kpFore = 210 + s.betaGon; s.twoCGon = 0.0004; rep.stations.push_back(s); }
  for (int i = 0; i < 3; i++) { PolySideRec s; s.from = nm[i]; s.to = nm[i + 1]; s.dFwdM = rep.in.sideM[i]; s.dBackM = rep.in.sideM[i] + 0.002; s.dMeanM = rep.in.sideM[i] + 0.001; s.hasFwd = s.hasBack = true; rep.sides.push_back(s); }
  if (trueErr) { rep.trueN = {1100, 1210.003, 1329.990, 1460}; rep.trueE = {1200, 1330.005, 1489.999, 1560}; }
  return rep;
}

static void csv_has_sections_and_numbers() {
  const std::string csv = PolygonCsv(Guide(false));
  assert(has(csv, "Ciag poligonowy") && has(csv, "nawiazany obustronnie") && has(csv, "zalozenie symulatora"));
  assert(has(csv, "Katy poziome") && has(csv, "Boki") && has(csv, "Rachunek") && has(csv, "Wspolrzedne"));
  assert(has(csv, "184.8109") && has(csv, "fbeta") && has(csv, "fd") && has(csv, "1460.000") && has(csv, "1560.000"));
  assert(has(csv, "w granicach") && !has(csv, "blad_N_m"));
}

static void csv_true_error_columns() {
  const std::string csv = PolygonCsv(Guide(true));
  assert(has(csv, "blad_N_m") && has(csv, "blad_E_m") && has(csv, "blad_2D_m"));
}

static void hanging_has_no_checks() {
  PolygonReport rep; rep.kind = TraverseKind::Open; rep.in.kind = TraverseKind::Open; rep.in.nN1 = 1000; rep.in.eN1 = 1000; rep.in.nA = 1100; rep.in.eA = 1200;
  rep.in.betaGon = {184.8109, 203.7398}; rep.in.sideM = {170.306, 199.991}; rep.res = PolygonCompute(rep.in); rep.names = {"A", "1", "2"};
  const std::string csv = PolygonCsv(rep);
  assert(has(csv, "wiszacy") && has(csv, "brak kontroli"));
  const auto lines = PolygonSummaryLines(rep);
  bool noCheck = false; for (const std::string& l : lines) if (has(l, "brak kontroli")) noCheck = true; assert(noCheck);
}

static void summary_lines_report_verdict() {
  PolygonReport ok = Guide(false);
  bool angle = false, lin = false;
  for (const std::string& l : PolygonSummaryLines(ok)) { if (has(l, "f_beta") && has(l, "w granicach")) angle = true; if (has(l, "f_d") && has(l, "w granicach")) lin = true; }
  assert(angle && lin);
  PolygonReport bad = Guide(false); bad.in.betaGon[1] += 0.05; bad.res = PolygonCompute(bad.in);
  bool exceeded = false; for (const std::string& l : PolygonSummaryLines(bad)) if (has(l, "f_beta") && has(l, "PRZEKROCZONE")) exceeded = true; assert(exceeded);
}

static void docx_is_zip_with_content() {
  const std::string z = PolygonDocx(Guide(true));
  assert(z.size() > 200 && z[0] == 'P' && z[1] == 'K' && has(z, "word/document.xml") && has(z, "Ciag poligonowy") && has(z, "zalozenie symulatora") && has(z, "1460.000"));
}

static void invalid_result_is_reported() {
  PolygonReport rep; rep.kind = TraverseKind::Anchored; rep.in.kind = TraverseKind::Anchored; rep.res = PolygonCompute(rep.in);        // brak danych
  assert(!rep.res.valid);
  assert(has(PolygonCsv(rep), "brak rachunku") && !PolygonSummaryLines(rep).empty());
}

int main() {
  csv_has_sections_and_numbers(); csv_true_error_columns(); hanging_has_no_checks(); summary_lines_report_verdict(); docx_is_zip_with_content(); invalid_result_is_reported();
  std::puts("test_polygon_report OK");
  return 0;
}
