#include <cassert>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include "calc/ProjectReport.hpp"

static bool has(const std::string& s, const std::string& part) { return s.find(part) != std::string::npos; }
static std::string Slurp(const std::string& p) { std::ifstream f(p, std::ios::binary); return std::string(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()); }
static PointH P(const std::string& id, double h) { PointH p; p.pointId = id; p.h = h; return p; }
static PointH PN(const std::string& id, double n, double e, double h) { PointH p; p.pointId = id; p.n = n; p.e = e; p.h = h; p.hasNE = true; return p; }
using Meas = std::pair<std::string, std::vector<PointH>>;

static void compare_only_shared_points() {
  const auto rows = CompareHeights({ Meas{"Niwelacja 1", {P("A", 100.000), P("B", 101.0)}}, Meas{"GNSS 1", {P("a", 100.004), P("C", 50.0)}} });
  assert(rows.size() == 1 && rows[0].pointId == "A" && rows[0].byMeasurement.size() == 2);
  assert(rows[0].byMeasurement[0].first == "Niwelacja 1" && rows[0].byMeasurement[1].first == "GNSS 1" && std::fabs(rows[0].spanMm - 4.0) < 1e-6);
}

static void compare_by_position_when_names_differ() {
  const auto rows = CompareHeights({ Meas{"Tachimetria 1", {PN("G01", 10, 20, 5.000)}}, Meas{"GNSS 1", {PN("P7", 10.2, 20.1, 5.010)}}, Meas{"GNSS 2", {PN("P9", 50, 60, 7.0)}} });
  assert(rows.size() == 1 && rows[0].byMeasurement.size() == 2 && std::fabs(rows[0].spanMm - 10.0) < 1e-6);
}

static void compare_empty_when_no_overlap() {
  assert(CompareHeights({}).empty());
  assert(CompareHeights({ Meas{"M1", {P("A", 1)}}, Meas{"M2", {P("B", 2)}} }).empty());
  assert(CompareHeights({ Meas{"M1", {P("A", 1), P("A", 2)}} }).empty());                   // powtorzenie w jednym pomiarze to nie porownanie
}

static void duplicate_name_in_one_measurement_counts_once() {
  const auto rows = CompareHeights({ Meas{"M1", {P("A", 1.000), P("A", 1.500)}}, Meas{"M2", {P("A", 1.002)}} });
  assert(rows.size() == 1 && rows[0].byMeasurement.size() == 2 && rows[0].byMeasurement[0].second == 1.000);
}

static void points_csv_roundtrip() {
  const auto back = ParsePointsCsv(PointsCsv({ PN("G;01", 1.5, 2.5, 3.5), P("A", 100.25) }));
  assert(back.size() == 2 && back[0].pointId == "G_01" && back[0].hasNE && back[0].n == 1.5 && back[0].e == 2.5 && back[0].h == 3.5);
  assert(back[1].pointId == "A" && !back[1].hasNE && back[1].h == 100.25);
  assert(ParsePointsCsv("").empty() && ParsePointsCsv("smiec bez separatorow\n;;;\n").empty());
}

static void points_of_reports() {
  GnssReport g; GnssPoint a; a.name = "P1"; a.n = 1; a.e = 2; a.h = 3; g.points.push_back(a);
  const auto pg = PointsOf(g); assert(pg.size() == 1 && pg[0].pointId == "P1" && pg[0].hasNE && pg[0].h == 3);
  TachyReport t; TachyPoint tp; tp.name = "T1"; tp.n = 4; tp.e = 5; tp.h = 6; t.points.push_back(tp);
  const auto pt = PointsOf(t); assert(pt.size() == 1 && pt[0].hasNE && pt[0].e == 5);
  FieldBook fb; fb.startName = "A"; fb.startHeightM = 100.0; FbRow r; r.pointName = "1"; r.heightM = 100.5; fb.rows.push_back(r);
  const auto pf = PointsOf(fb); assert(pf.size() == 2 && pf[0].pointId == "A" && pf[0].h == 100.0 && !pf[0].hasNE && pf[1].pointId == "1");
}

static void result_files_sorted() {
  const std::string d = (std::filesystem::temp_directory_path() / "gs_resfiles").string(); std::filesystem::remove_all(d); std::filesystem::create_directories(d + "/podfolder");
  for (const char* n : {"b.csv", "a.txt"}) { std::ofstream f(d + "/" + n); f << "x"; }
  const auto v = ResultFiles(d); assert(v.size() == 2 && v[0] == "a.txt" && v[1] == "b.csv");
  assert(ResultFiles(d + "/nie_ma").empty());
  std::filesystem::remove_all(d);
}

static Project Proj() {
  Project p; p.name = "Kolonia_1"; p.world = "NEWWORLD"; p.created = "2026-11-24_10-00-00";
  AddMeasurement(p, "traverse", "Ciag niwelacyjny"); SetState(p, "m1", MeasState::Done, "GothicSurveying/x", "fh 2.1 mm; 3 stan.");
  AddMeasurement(p, "gnss", "Pomiar GNSS (RTK)"); SetState(p, "m2", MeasState::Aborted);
  return p;
}

static void report_csv_has_row_per_measurement() {
  const Project p = Proj();
  const auto rows = CompareHeights({ Meas{"Ciag niwelacyjny 1", {P("A", 100.0)}}, Meas{"Pomiar GNSS (RTK) 1", {P("A", 100.004)}} });
  const std::string csv = ProjectReportCsv(p, rows);
  assert(has(csv, "Ciag niwelacyjny 1;traverse;gotowy") && has(csv, "Pomiar GNSS (RTK) 1;gnss;przerwany") && has(csv, "Porownanie wysokosci") && has(csv, "A;"));
  assert(has(csv, "4.0"));                                            // rozrzut [mm]
}

static void report_docx_is_zip_with_content() {
  const Project p = Proj();
  const std::string z = ProjectReportDocx(p, {}, "2026-11-24");
  assert(z.size() > 100 && z[0] == 'P' && z[1] == 'K' && has(z, "word/document.xml") && has(z, "Raport projektu") && has(z, "Kolonia_1") && has(z, "Ciag niwelacyjny 1") && has(z, "brak punktow do porownania"));
}

static void write_report_to_temp_dir() {
  const std::string root = (std::filesystem::temp_directory_path() / "gs_projreport").string(); std::filesystem::remove_all(root);
  Project p = Proj(); p.items[0].pointsFile = "pomiar_m1.csv"; p.items[1].pointsFile = "pomiar_nie_ma.csv";     // brakujacy plik punktow nie wywraca raportu
  std::filesystem::create_directories(root + "/projekty/Kolonia_1");
  { std::ofstream f(root + "/projekty/Kolonia_1/pomiar_m1.csv"); f << PointsCsv({ P("A", 100.0) }); }
  std::string err; assert(WriteProjectReport(p, root, err) && err.empty());
  assert(std::filesystem::exists(root + "/projekty/Kolonia_1/projekt_raport.csv") && std::filesystem::exists(root + "/projekty/Kolonia_1/projekt_raport.docx"));
  assert(has(Slurp(root + "/projekty/Kolonia_1/projekt_raport.csv"), "Ciag niwelacyjny 1"));
  assert(!WriteProjectReport(p, root + "/nie_ma/glebokie", err) || true);                      // zapis do nieistniejacego katalogu nie moze wywrocic (tworzy go albo zwraca blad)
  std::filesystem::remove_all(root);
}

int main() {
  compare_only_shared_points(); compare_by_position_when_names_differ(); compare_empty_when_no_overlap(); duplicate_name_in_one_measurement_counts_once();
  points_csv_roundtrip(); points_of_reports(); result_files_sorted(); report_csv_has_row_per_measurement(); report_docx_is_zip_with_content(); write_report_to_temp_dir();
  std::puts("test_projectreport OK");
  return 0;
}
