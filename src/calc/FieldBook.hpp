// Dziennik niwelacji: model + eksport CSV (bez Gothic API).
#pragma once
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include "Traverse.hpp"

enum class CalcMode { Auto, Manual };

struct FbRow {
  int standNo = 0; std::string pointName;          // punkt wprzód tego stanowiska
  int backMm = 0, foreMm = 0;
  double dhMm = 0, vMm = 0, dhAdjMm = 0, heightM = 0;
  std::string note;
  double sightBackCm = 0, sightForeCm = 0;
  double stadiaBackCm = -1, stadiaForeCm = -1;     // z dalmierza nitkowego; < 0 = brak
};

struct FieldBook {
  TraverseKind kind = TraverseKind::Anchored; CalcMode mode = CalcMode::Auto; bool complete = true;
  std::string startName = "A"; double startHeightM = 0;
  bool hasEnd = false; double endHeightM = 0;      // wysokość reperu końcowego (dowiązany: B, zamknięty: A); otwarty: brak
  std::vector<FbRow> rows;
  TraverseResult sum;
};

inline FieldBook BuildFieldBook(TraverseKind kind, CalcMode mode, const std::vector<Stand>& stands,
                                const std::vector<std::string>& forePointNames, double hStartM, double hEndM, bool complete) {
  FieldBook fb; fb.kind = kind; fb.mode = mode; fb.complete = complete; fb.startHeightM = hStartM;
  fb.sum = ComputeTraverse(kind, stands, hStartM, hEndM);
  fb.hasEnd = kind != TraverseKind::Open; fb.endHeightM = kind == TraverseKind::Anchored ? hEndM : hStartM;
  for (size_t i = 0; i < stands.size(); i++) {
    FbRow r; r.standNo = (int)i + 1;
    r.pointName = i < forePointNames.size() ? forePointNames[i] : std::to_string(i + 1);
    r.backMm = stands[i].backMm; r.foreMm = stands[i].foreMm;
    r.dhMm = fb.sum.dhMm[i]; r.vMm = fb.sum.vMm[i]; r.dhAdjMm = fb.sum.dhAdjMm[i]; r.heightM = fb.sum.heightAdjM[i + 1];
    r.note = i + 1 == stands.size() ? (kind == TraverseKind::Open ? "punkt koncowy" : "reper") : "zabka";
    r.sightBackCm = stands[i].sightBackCm; r.sightForeCm = stands[i].sightForeCm;
    r.stadiaBackCm = stands[i].stadiaBackCm; r.stadiaForeCm = stands[i].stadiaForeCm;
    fb.rows.push_back(r);
  }
  return fb;
}

inline std::string KindName(TraverseKind k) {
  return k == TraverseKind::Anchored ? "ciąg dowiązany" : k == TraverseKind::Closed ? "ciąg zamknięty" : "ciąg otwarty";
}
inline std::string ModeName(CalcMode m) { return m == CalcMode::Auto ? "automatyczne" : "samodzielne"; }

namespace fbdetail {
inline std::string Num(double v, int decimals) {
  char b[64]; std::snprintf(b, sizeof b, "%.*f", decimals, v + 0.0);   // +0.0 zamienia -0.0 na 0.0
  return b;
}
inline std::string Csv(const std::string& s) {
  if (s.find_first_of(";\"\n") == std::string::npos) return s;
  std::string o = "\"";
  for (char c : s) { if (c == '"') o += '"'; o += c; }
  return o + "\"";
}
}  // namespace fbdetail

inline std::string ToCsv(const FieldBook& fb) {
  using namespace fbdetail;
  const bool closure = fb.sum.hasClosure;
  std::string o;
  if (!fb.complete) o += "# UWAGA: dziennik niekompletny (robota przerwana)\n";
  o += "nr_stanowiska;punkt;odczyt_wstecz_mm;odczyt_wprzod_mm;dh_mm;poprawka_mm;dh_wyrown_mm;H_m;uwagi;celowa_wstecz_m;celowa_wprzod_m\n";
  o += "0;" + Csv(fb.startName) + ";;;;;;" + Num(fb.startHeightM, 3) + ";reper;;\n";
  for (const FbRow& r : fb.rows) {
    o += std::to_string(r.standNo) + ";" + Csv(r.pointName) + ";" + std::to_string(r.backMm) + ";" + std::to_string(r.foreMm) + ";" +
         Num(r.dhMm, 1) + ";" + (closure ? Num(r.vMm, 1) : "") + ";" + (closure ? Num(r.dhAdjMm, 1) : "") + ";" +
         Num(r.heightM, 3) + ";" + Csv(r.note) + ";" + (r.stadiaBackCm >= 0 ? Num(r.stadiaBackCm / 100.0, 1) : "") + ";" +
         (r.stadiaForeCm >= 0 ? Num(r.stadiaForeCm / 100.0, 1) : "") + "\n";
  }
  o += "\n";
  o += "suma_wstecz_mm;" + Num(fb.sum.sumBackMm, 1) + "\nsuma_wprzod_mm;" + Num(fb.sum.sumForeMm, 1) + "\nsuma_dh_mm;" + Num(fb.sum.sumDhMm, 1) + "\n";
  o += "fh_mm;" + (closure ? Num(fb.sum.fhMm, 1) : "") + "\nfh_dop_mm;" + (closure ? Num(fb.sum.fhDopMm, 1) : "") + "\n";
  o += "dlugosc_m;" + Num(fb.sum.lengthM, 1) + "\n";
  o += "h_start_m;" + Num(fb.startHeightM, 3) + "\nh_koniec_m;" + (fb.hasEnd ? Num(fb.endHeightM, 3) : "") + "\n";
  o += std::string("wynik;") + (!closure ? "brak kontroli" : fb.sum.withinTolerance ? "w tolerancji" : "przekroczony") + "\n";
  return o;
}
