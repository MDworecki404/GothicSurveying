// Raport pomiaru GNSS: model punktów, CSV, DOCX, linie podsumowania (bez Gothic API).
#pragma once
#include <cmath>
#include <string>
#include <vector>
#include "Docx.hpp"
#include "FieldBook.hpp"
#include "Gnss.hpp"

struct GnssPoint {
  std::string name, timeText;
  double n = 0, e = 0, h = 0;
  std::string solution;                            // GnssSolName(...)
  int used = 0; double pdop = 0, hRmsMm = 0, vRmsMm = 0; int epochs = 0; double antennaM = 2.0;
  std::string note;
  bool hasTrue = false; double dNmm = 0, dEmm = 0, dHmm = 0, d2Dmm = 0;   // błąd względem prawdziwego punktu w grze
};

struct GnssReport {
  bool complete = true, showTrueError = false;
  std::string dateText;
  std::vector<GnssPoint> points;
};

namespace gnssdetail {
inline std::vector<std::string> Cells(const GnssPoint& p, int nr, bool withTrue) {
  using fbdetail::Num;
  std::vector<std::string> c = {std::to_string(nr), p.name, p.timeText, Num(p.n, 3), Num(p.e, 3), Num(p.h, 3), p.solution, std::to_string(p.used),
                                Num(p.pdop, 1), Num(p.hRmsMm, 0), Num(p.vRmsMm, 0), std::to_string(p.epochs), Num(p.antennaM, 3), p.note};
  if (withTrue) {
    if (p.hasTrue) for (double v : {p.dNmm, p.dEmm, p.dHmm, p.d2Dmm}) c.push_back(Num(v, 1));
    else for (int i = 0; i < 4; i++) c.push_back("");
  }
  return c;
}
}  // namespace gnssdetail

inline std::string GnssToCsv(const GnssReport& r) {
  std::string o;
  if (!r.complete) o += "# UWAGA: pomiar niekompletny (robota przerwana)\n";
  o += "nr;punkt;czas;N_m;E_m;H_m;rozwiazanie;satelity;PDOP;RMS_H_mm;RMS_V_mm;epoki;antena_m;uwagi";
  if (r.showTrueError) o += ";blad_N_mm;blad_E_mm;blad_H_mm;blad_2D_mm";
  o += "\n";
  int nr = 0;
  for (const GnssPoint& p : r.points) {
    std::vector<std::string> c = gnssdetail::Cells(p, ++nr, r.showTrueError);
    c[13] = fbdetail::Csv(c[13]);                  // uwagi mogą zawierać ; i "
    for (size_t i = 0; i < c.size(); i++) { if (i) o += ";"; o += c[i]; }
    o += "\n";
  }
  return o;
}

inline std::vector<std::string> GnssSummaryLines(const GnssReport& r) {
  using fbdetail::Num;
  std::vector<std::string> out;
  int fix = 0; double sumPdop = 0;
  for (const GnssPoint& p : r.points) { if (p.solution == GnssSolName(SolType::Fix)) fix++; sumPdop += p.pdop; }
  const double avg = r.points.empty() ? 0.0 : sumPdop / (double)r.points.size();
  out.push_back("Punkty: " + std::to_string(r.points.size()) + ", w fixie: " + std::to_string(fix) + ", sredni PDOP: " + Num(avg, 1));
  if (r.points.empty()) return out;
  const GnssPoint* worst = &r.points[0];
  for (const GnssPoint& p : r.points)
    if (r.showTrueError ? p.d2Dmm > worst->d2Dmm : p.pdop > worst->pdop) worst = &p;
  out.push_back(r.showTrueError ? "Najgorszy punkt: " + worst->name + " (" + Num(worst->d2Dmm, 1) + " mm)"
                                : "Najgorszy punkt: " + worst->name + " (PDOP " + Num(worst->pdop, 1) + ")");
  return out;
}

inline std::string GnssBuildDocx(const GnssReport& r) {
  using namespace docxdetail;
  std::string body = Para("Pomiar GNSS (RTK)", true, 36, true);
  body += Para("data: " + r.dateText + (r.complete ? "" : " (pomiar niekompletny - robota przerwana)"));
  std::vector<std::string> head = {"Nr", "Punkt", "Czas", "N [m]", "E [m]", "H [m]", "Rozwiązanie", "Satelity", "PDOP", "RMS H [mm]", "RMS V [mm]",
                                   "Epoki", "Antena [m]", "Uwagi"};
  if (r.showTrueError) for (const char* h : {"Błąd N [mm]", "Błąd E [mm]", "Błąd H [mm]", "Błąd 2D [mm]"}) head.push_back(h);
  std::string tbl = "<w:tbl><w:tblPr><w:tblW w:w=\"0\" w:type=\"auto\"/><w:tblBorders>";
  for (const char* b : {"top", "left", "bottom", "right", "insideH", "insideV"})
    tbl += std::string("<w:") + b + " w:val=\"single\" w:sz=\"4\" w:space=\"0\" w:color=\"000000\"/>";
  tbl += "</w:tblBorders></w:tblPr><w:tblGrid>";
  for (size_t i = 0; i < head.size(); i++) tbl += "<w:gridCol w:w=\"900\"/>";
  tbl += "</w:tblGrid>" + Row(head, true);
  int nr = 0;
  for (const GnssPoint& p : r.points) tbl += Row(gnssdetail::Cells(p, ++nr, r.showTrueError), false);
  body += tbl + "</w:tbl>";
  for (const std::string& l : GnssSummaryLines(r)) body += Para(l, true);
  return WrapDocx(body);
}
