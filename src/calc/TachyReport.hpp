// Raport pomiaru tachimetrycznego: model punktów, CSV, DOCX, linie podsumowania (bez Gothic API).
#pragma once
#include <cmath>
#include <string>
#include <vector>
#include "Docx.hpp"
#include "FieldBook.hpp"
#include "Tachy.hpp"

struct TachyPoint {
  std::string name, timeText;
  double hzGon = 0, vGon = 0, slopeM = 0, hDistM = 0, dHM = 0;   // odczyty i różnica wysokości (wzgl. oka)
  double n = 0, e = 0, h = 0, prismM = 1.5;                      // wynik i ustawiona wysokość lustra
  std::string note;
  bool hasTrue = false; double dNm = 0, dEm = 0, dHm = 0, d2Dm = 0;   // błąd względem prawdziwego punktu w grze [m]
};

struct TachyReport {
  bool complete = true, showTrueError = false;
  std::string dateText;
  double stationN = 0, stationE = 0, stationH = 0, instrH = 1.5, azBackGon = 0;
  std::vector<TachyPoint> points;
};

namespace tachydetail {
inline std::vector<std::string> Cells(const TachyPoint& p, int nr, bool withTrue) {
  using fbdetail::Num;
  std::vector<std::string> c = {std::to_string(nr), p.name, p.timeText, Num(p.hzGon, 4), Num(p.vGon, 4), Num(p.slopeM, 3), Num(p.hDistM, 3),
                                Num(p.dHM, 3), Num(p.n, 3), Num(p.e, 3), Num(p.h, 3), Num(p.prismM, 3), p.note};
  if (withTrue) {
    if (p.hasTrue) for (double v : {p.dNm, p.dEm, p.dHm, p.d2Dm}) c.push_back(Num(v, 3));
    else for (int i = 0; i < 4; i++) c.push_back("");
  }
  return c;
}
}  // namespace tachydetail

inline std::string TachyCsvHeader(bool trueErr) {
  std::string o = "nr;punkt;czas;Hz_g;V_g;D_skosna_m;d_pozioma_m;dh_m;N_m;E_m;H_m;wys_lustra_m;uwagi";
  if (trueErr) o += ";blad_N_m;blad_E_m;blad_H_m;blad_2D_m";
  return o;
}
inline std::string TachyCsvRow(const TachyPoint& p, int nr, bool trueErr) {
  std::vector<std::string> c = tachydetail::Cells(p, nr, trueErr);
  c[12] = fbdetail::Csv(c[12]);                    // uwagi mogą zawierać ; i "
  std::string o;
  for (size_t i = 0; i < c.size(); i++) { if (i) o += ";"; o += c[i]; }
  return o;
}

inline std::string TachyToCsv(const TachyReport& r) {
  using fbdetail::Num;
  std::string o;
  if (!r.complete) o += "# UWAGA: pomiar niekompletny (robota przerwana)\n";
  o += "# stanowisko;" + Num(r.stationN, 3) + ";" + Num(r.stationE, 3) + ";" + Num(r.stationH, 3) + ";" + Num(r.instrH, 3) + ";" + Num(r.azBackGon, 4) + "\n";
  o += TachyCsvHeader(r.showTrueError) + "\n";
  int nr = 0;
  for (const TachyPoint& p : r.points) o += TachyCsvRow(p, ++nr, r.showTrueError) + "\n";
  return o;
}

inline std::vector<std::string> TachySummaryLines(const TachyReport& r) {
  using fbdetail::Num;
  std::vector<std::string> out;
  double sum = 0, longest = 0;
  for (const TachyPoint& p : r.points) { sum += p.slopeM; if (p.slopeM > longest) longest = p.slopeM; }
  const double avg = r.points.empty() ? 0.0 : sum / (double)r.points.size();
  out.push_back("Punkty: " + std::to_string(r.points.size()) + ", srednia odleglosc: " + Num(avg, 1) + " m, najdluzsza celowa: " + Num(longest, 1) + " m");
  if (r.showTrueError && !r.points.empty()) {
    const TachyPoint* worst = &r.points[0];
    for (const TachyPoint& p : r.points) if (p.d2Dm > worst->d2Dm) worst = &p;
    out.push_back("Najgorszy punkt: " + worst->name + " (" + Num(worst->d2Dm, 3) + " m)");
  }
  return out;
}

inline std::string TachyBuildDocx(const TachyReport& r) {
  using namespace docxdetail; using fbdetail::Num;
  std::string body = Para("Tachimetria (pomiar biegunowy)", true, 36, true);
  body += Para("data: " + r.dateText + (r.complete ? "" : " (pomiar niekompletny - robota przerwana)"));
  body += Para("Stanowisko: N " + Num(r.stationN, 3) + " m, E " + Num(r.stationE, 3) + " m, H " + Num(r.stationH, 3) + " m, wysokość instrumentu " +
               Num(r.instrH, 3) + " m, azymut nawiązania " + Num(r.azBackGon, 4) + " g");
  std::vector<std::string> head = {"Nr", "Punkt", "Czas", "Hz [g]", "V [g]", "D skośna [m]", "d pozioma [m]", "Δh [m]", "N [m]", "E [m]", "H [m]",
                                   "Wys. lustra [m]", "Uwagi"};
  if (r.showTrueError) for (const char* h : {"Błąd N [m]", "Błąd E [m]", "Błąd H [m]", "Błąd 2D [m]"}) head.push_back(h);
  std::string tbl = "<w:tbl><w:tblPr><w:tblW w:w=\"0\" w:type=\"auto\"/><w:tblBorders>";
  for (const char* b : {"top", "left", "bottom", "right", "insideH", "insideV"})
    tbl += std::string("<w:") + b + " w:val=\"single\" w:sz=\"4\" w:space=\"0\" w:color=\"000000\"/>";
  tbl += "</w:tblBorders></w:tblPr><w:tblGrid>";
  const int width = 14000 / (int)head.size();      // użyteczna szerokość A4 poziomej to ok. 14500 dxa
  for (size_t i = 0; i < head.size(); i++) tbl += "<w:gridCol w:w=\"" + std::to_string(width) + "\"/>";
  tbl += "</w:tblGrid>" + Row(head, true, width);
  int nr = 0;
  for (const TachyPoint& p : r.points) tbl += Row(tachydetail::Cells(p, ++nr, r.showTrueError), false, width);
  body += tbl + "</w:tbl>";
  for (const std::string& l : TachySummaryLines(r)) body += Para(l, true);
  return WrapDocx(body);
}
