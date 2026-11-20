#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>
#include "calc/ManualCheck.hpp"

static const double NaN = std::numeric_limits<double>::quiet_NaN();
static bool has(const std::string& s, const std::string& part) { return s.find(part) != std::string::npos; }

static std::vector<Stand> S3() { return { {1500, 1200, 3000, 3000}, {1400, 1650, 3000, 3000}, {1300, 1100, 3000, 3000} }; }
static FieldBook Book(TraverseKind k = TraverseKind::Anchored) {
  return BuildFieldBook(k, CalcMode::Manual, S3(), {"1", "2", "B"}, 100.000, 100.240, true);
}
static PlayerEntries Correct(const FieldBook& fb) {                  // wpisy równe poprawnym
  PlayerEntries p;
  for (const FbRow& r : fb.rows) { p.dhMm.push_back(r.dhMm); p.vMm.push_back(r.vMm); p.heightM.push_back(r.heightM); }
  p.sumDhMm = fb.sum.sumDhMm; p.fhMm = fb.sum.fhMm;
  return p;
}

static void all_correct_entries_pass() {
  FieldBook fb = Book(); ManualVerdict v = CheckManual(fb, Correct(fb));
  assert(v.total == 11 && v.ok == 11);                               // 3 dh + 3 v + 3 H + suma + fh
}

static void within_tolerance() {
  FieldBook fb = Book(); PlayerEntries p = Correct(fb);
  p.dhMm[0] += 0.9; assert(CheckManual(fb, p).dh[0].ok);
  p.dhMm[0] = fb.rows[0].dhMm + 1.1; ManualVerdict v = CheckManual(fb, p);
  assert(!v.dh[0].ok && v.ok == 10 && std::fabs(v.dh[0].diff - 1.1) < 1e-9);
}

static void missing_cells_count_as_not_entered() {
  FieldBook fb = Book(); PlayerEntries p;                            // puste wektory i NaN
  p.sumDhMm = NaN; p.fhMm = NaN;
  ManualVerdict v = CheckManual(fb, p);
  assert(v.total == 11 && v.ok == 0 && !v.dh[0].entered && !v.sumDh.entered && !v.fh.entered);
}

static void height_compared_in_mm() {
  FieldBook fb = Book(); PlayerEntries p = Correct(fb);
  p.heightM[2] += 0.0005; assert(CheckManual(fb, p).height[2].ok);   // 0,5 mm
  p.heightM[2] = fb.rows[2].heightM + 0.002; ManualVerdict v = CheckManual(fb, p);
  assert(!v.height[2].ok && std::fabs(v.height[2].diff - 2.0) < 1e-6);   // diff w mm
}

static void open_traverse_has_no_v_and_fh_cells() {
  FieldBook fb = Book(TraverseKind::Open); ManualVerdict v = CheckManual(fb, Correct(fb));
  assert(v.total == 7 && v.ok == 7 && v.v.empty());                  // 3 dh + 3 H + suma
}

static void csv_with_verdict_has_player_and_diff_columns() {
  FieldBook fb = Book(); PlayerEntries p = Correct(fb); p.dhMm[0] = 301.1;
  std::string csv = ToCsvWithVerdict(fb, CheckManual(fb, p));
  assert(has(csv, ToCsv(fb)));                                       // zwykły dziennik zostaje bez zmian
  assert(has(csv, "komorka;poprawna;twoja;roznica;ok"));
  assert(has(csv, "dh_1_mm;300.0;301.1;1.1;nie"));
  assert(has(csv, "dh_2_mm;-250.0;-250.0;0.0;tak"));
  assert(has(csv, "wynik_sprawdzenia;10/11"));
}

static void csv_with_verdict_marks_missing() {
  FieldBook fb = Book(); PlayerEntries p; p.sumDhMm = NaN; p.fhMm = NaN;
  assert(has(ToCsvWithVerdict(fb, CheckManual(fb, p)), "dh_1_mm;300.0;;;nie"));
}

int main() {
  all_correct_entries_pass(); within_tolerance(); missing_cells_count_as_not_entered(); height_compared_in_mm();
  open_traverse_has_no_v_and_fh_cells(); csv_with_verdict_has_player_and_diff_columns(); csv_with_verdict_marks_missing();
  std::puts("test_manual_check OK");
  return 0;
}
