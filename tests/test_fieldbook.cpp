#include <cassert>
#include <cmath>
#include <cstdio>
#include <sstream>
#include <string>
#include <vector>
#include "calc/FieldBook.hpp"

static bool near(double a, double b, double eps = 1e-6) { return std::fabs(a - b) <= eps; }
static bool has(const std::string& s, const std::string& part) { return s.find(part) != std::string::npos; }

static std::vector<std::string> lines(const std::string& csv) {
  std::vector<std::string> out; std::stringstream ss(csv); std::string l;
  while (std::getline(ss, l)) out.push_back(l);
  return out;
}

static std::vector<Stand> S3() { return { {1500, 1200, 3000, 3000}, {1400, 1650, 3000, 3000}, {1300, 1100, 3000, 3000} }; }
static const std::vector<std::string> NAMES = {"1", "2", "B"};

static void rows_match_traverse() {
  FieldBook fb = BuildFieldBook(TraverseKind::Anchored, CalcMode::Auto, S3(), NAMES, 100.000, 100.240, true);
  assert(fb.rows.size() == 3 && fb.rows[2].pointName == "B");
  assert(near(fb.rows[2].heightM, 100.240) && near(fb.rows[0].dhAdjMm, 296.667, 1e-3));
  assert(fb.rows[0].standNo == 1 && fb.rows[0].backMm == 1500 && fb.rows[0].foreMm == 1200);
}

static void csv_has_header_start_row_and_summary() {
  FieldBook fb = BuildFieldBook(TraverseKind::Anchored, CalcMode::Auto, S3(), NAMES, 100.000, 100.240, true);
  auto l = lines(ToCsv(fb));
  assert(l[0] == "nr_stanowiska;punkt;odczyt_wstecz_mm;odczyt_wprzod_mm;dh_mm;poprawka_mm;dh_wyrown_mm;H_m;uwagi;celowa_wstecz_m;celowa_wprzod_m");
  assert(l[1] == "0;A;;;;;;100.000;reper;;");
  assert(l[2] == "1;1;1500;1200;300.0;-3.3;296.7;100.297;zabka;;");
  std::string csv = ToCsv(fb);
  assert(has(csv, "\nfh_mm;10.0\n") && has(csv, "\nwynik;przekroczony"));
  assert(has(csv, "\nsuma_dh_mm;250.0\n") && has(csv, "\ndlugosc_m;180.0\n"));
}

static void csv_escapes_special_fields() {
  std::vector<std::string> names = {"x;\"y\"", "2", "B"};
  FieldBook fb = BuildFieldBook(TraverseKind::Anchored, CalcMode::Auto, S3(), names, 100.000, 100.240, true);
  assert(has(ToCsv(fb), "1;\"x;\"\"y\"\"\";1500;1200;"));
}

static void incomplete_book_is_flagged() {
  FieldBook fb = BuildFieldBook(TraverseKind::Anchored, CalcMode::Auto, S3(), NAMES, 100.000, 100.240, false);
  assert(lines(ToCsv(fb))[0] == "# UWAGA: dziennik niekompletny (robota przerwana)");
  assert(lines(ToCsv(fb))[1].rfind("nr_stanowiska;", 0) == 0);
}

static void open_book_has_no_closure_rows() {
  FieldBook fb = BuildFieldBook(TraverseKind::Open, CalcMode::Auto, S3(), {"1", "2", "3"}, 100.000, 0.0, true);
  auto l = lines(ToCsv(fb));
  assert(l[2] == "1;1;1500;1200;300.0;;;100.300;zabka;;");
  assert(has(ToCsv(fb), "\nwynik;brak kontroli"));
}

static void names_are_polish_utf8() {
  assert(KindName(TraverseKind::Anchored) == "ciąg dowiązany");
  assert(KindName(TraverseKind::Closed) == "ciąg zamknięty");
  assert(KindName(TraverseKind::Open) == "ciąg otwarty");
  assert(ModeName(CalcMode::Auto) == "automatyczne" && ModeName(CalcMode::Manual) == "samodzielne");
}

static void stadia_columns_filled_when_entered() {
  std::vector<Stand> s = S3(); s[0].stadiaBackCm = 2000.0; s[0].stadiaForeCm = 2010.0;
  FieldBook fb = BuildFieldBook(TraverseKind::Anchored, CalcMode::Auto, s, NAMES, 100.000, 100.240, true);
  assert(near(fb.rows[0].stadiaBackCm, 2000.0) && fb.rows[1].stadiaBackCm < 0);
  assert(lines(ToCsv(fb))[2] == "1;1;1500;1200;300.0;-3.3;296.7;100.297;zabka;20.0;20.1");
}

static void end_height_is_kept_and_exported() {
  FieldBook a = BuildFieldBook(TraverseKind::Anchored, CalcMode::Auto, S3(), NAMES, 100.000, 100.240, true);
  assert(a.hasEnd && near(a.endHeightM, 100.240));
  assert(has(ToCsv(a), "\nh_start_m;100.000\n") && has(ToCsv(a), "\nh_koniec_m;100.240\n"));
  FieldBook c = BuildFieldBook(TraverseKind::Closed, CalcMode::Auto, S3(), NAMES, 100.000, 0.0, true);
  assert(c.hasEnd && near(c.endHeightM, 100.000));
  FieldBook o = BuildFieldBook(TraverseKind::Open, CalcMode::Auto, S3(), NAMES, 100.000, 0.0, true);
  assert(!o.hasEnd && has(ToCsv(o), "\nh_koniec_m;\n"));
}

int main() {
  rows_match_traverse(); csv_has_header_start_row_and_summary(); csv_escapes_special_fields();
  incomplete_book_is_flagged(); open_book_has_no_closure_rows(); names_are_polish_utf8();
  stadia_columns_filled_when_entered(); end_height_is_kept_and_exported();
  std::puts("test_fieldbook OK");
  return 0;
}
