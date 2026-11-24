// Raport ciagu poligonowego (bez Gothic API): model, CSV, DOCX i linie podsumowania. Limity odchylek sa zalozeniem symulatora (z dokladnosci
// instrumentu), co raport mowi wprost.
#pragma once
#include <cstdio>
#include <string>
#include <vector>
#include "Docx.hpp"
#include "Polygon.hpp"

struct PolyStationRec {
  std::string name, back, fore;
  double klBack = 0, klFore = 0, kpBack = 0, kpFore = 0, betaGon = 0, twoCGon = 0;
};
struct PolySideRec {
  std::string from, to;
  double dFwdM = 0, dBackM = 0, dMeanM = 0; bool hasFwd = false, hasBack = false;     // dFwd: z poczatku boku, dBack: z konca
};
struct PolygonReport {
  bool complete = true, showTrueError = false; std::string dateText; TraverseKind kind = TraverseKind::Anchored;
  std::vector<std::string> names;                          // nazwy punktow P0..Pn (w Closed ostatni = pierwszy)
  std::vector<PolyStationRec> stations; std::vector<PolySideRec> sides;
  PolyInput in; PolyResult res;
  std::vector<double> trueN, trueE;                        // prawdziwe pozycje znakow (gdy showTrueError), ta sama kolejnosc co names
};

namespace polyrep {
inline std::string F(double v, int d) { char b[48]; std::snprintf(b, sizeof b, "%.*f", d, v); return b; }
inline std::string Sg(double v, int d) { char b[48]; std::snprintf(b, sizeof b, "%+.*f", d, v); return b; }
inline const char* PolyKindName(TraverseKind k) { return k == TraverseKind::Anchored ? "nawiazany obustronnie" : k == TraverseKind::Closed ? "zamkniety" : "wiszacy"; }
inline const char* kAssumption = "Limity odchylek: zalozenie symulatora (z dokladnosci instrumentu: 5 sek. na kierunek, 2 mm + 2 ppm), nie wartosci z instrukcji";
inline const char* Verdict(bool ok) { return ok ? "w granicach" : "PRZEKROCZONE"; }

inline std::string NameOr(const PolygonReport& r, size_t i) { return i < r.names.size() ? r.names[i] : "P" + std::to_string(i); }
inline bool HasTrue(const PolygonReport& r, size_t i) { return r.showTrueError && i < r.trueN.size() && i < r.trueE.size(); }

// Wiersze tabel w jednej postaci dla CSV i DOCX.
inline std::vector<std::vector<std::string>> AngleRows(const PolygonReport& r) {
  std::vector<std::vector<std::string>> t = { {"Stanowisko", "Tyl", "Przod", "KL tyl [g]", "KL przod [g]", "KP tyl [g]", "KP przod [g]", "Kat sredni [g]", "KP-KL [g]"} };
  for (const PolyStationRec& s : r.stations) t.push_back({ s.name, s.back, s.fore, F(s.klBack, 4), F(s.klFore, 4), F(s.kpBack, 4), F(s.kpFore, 4), F(s.betaGon, 4), Sg(s.twoCGon, 4) });
  return t;
}
inline std::vector<std::vector<std::string>> SideRows(const PolygonReport& r) {
  std::vector<std::vector<std::string>> t = { {"Bok", "d z poczatku [m]", "d z konca [m]", "d srednia [m]"} };
  for (const PolySideRec& s : r.sides) t.push_back({ s.from + "-" + s.to, s.hasFwd ? F(s.dFwdM, 3) : "", s.hasBack ? F(s.dBackM, 3) : "", F(s.dMeanM, 3) });
  return t;
}
inline std::vector<std::vector<std::string>> CoordRows(const PolygonReport& r) {
  std::vector<std::string> head = { "Punkt", "N [m]", "E [m]" };
  if (r.showTrueError) { head.push_back("blad_N_m"); head.push_back("blad_E_m"); head.push_back("blad_2D_m"); }
  std::vector<std::vector<std::string>> t = { head };
  for (size_t i = 0; i < r.res.x.size(); i++) {
    std::vector<std::string> row = { NameOr(r, i), F(r.res.x[i], 3), F(r.res.y[i], 3) };
    if (r.showTrueError) {
      if (HasTrue(r, i)) { const double dn = r.res.x[i] - r.trueN[i], de = r.res.y[i] - r.trueE[i]; row.push_back(F(dn, 3)); row.push_back(F(de, 3)); row.push_back(F(std::hypot(dn, de), 3)); }
      else for (int k = 0; k < 3; k++) row.push_back("");
    }
    t.push_back(row);
  }
  return t;
}
inline std::vector<std::vector<std::string>> CalcRows(const PolygonReport& r) {
  std::vector<std::vector<std::string>> t;
  const PolyResult& p = r.res;
  if (!p.valid) { t.push_back({"brak rachunku", p.error}); return t; }
  t.push_back({"Liczba katow n", std::to_string(p.n)});
  if (p.hasAngleCheck) {
    t.push_back({"fbeta [g]", Sg(p.fBetaGon, 4)}); t.push_back({"v_beta [g]", Sg(p.vBetaGon, 4)});
    t.push_back({"fbeta dop. [g]", F(p.fBetaLimGon, 4)}); t.push_back({"ocena katowa", Verdict(p.angleOk)});
    t.push_back({"fx [m]", Sg(p.fx, 3)}); t.push_back({"fy [m]", Sg(p.fy, 3)}); t.push_back({"fd [m]", F(p.fd, 3)}); t.push_back({"fd dop. [m]", F(p.fdLim, 3)});
    t.push_back({"ocena liniowa", Verdict(p.linearOk)});
  } else t.push_back({"kontrola", "brak kontroli (ciag wiszacy: odchylki niedostepne)"});
  return t;
}
inline std::vector<std::vector<std::string>> BearingRows(const PolygonReport& r) {
  std::vector<std::vector<std::string>> t = { {"Bok", "Azymut [g]", "d [m]", "dX [m]", "dY [m]", "v_x [m]", "v_y [m]"} };
  const PolyResult& p = r.res;
  if (!p.valid) return t;
  for (size_t i = 0; i < p.azimuthGon.size() && i < r.sides.size(); i++)
    t.push_back({ r.sides[i].from + "-" + r.sides[i].to, F(p.azimuthGon[i], 4), F(i < r.in.sideM.size() ? r.in.sideM[i] : 0.0, 3), Sg(p.dX[i], 3), Sg(p.dY[i], 3), Sg(p.vx[i], 3), Sg(p.vy[i], 3) });
  return t;
}
}  // namespace polyrep

inline std::string PolygonCsv(const PolygonReport& r) {
  using namespace polyrep;
  std::string o = std::string("Ciag poligonowy;") + PolyKindName(r.kind) + (r.complete ? "" : " (niekompletny - pomiar przerwany)") + "\nData;" + r.dateText + "\n" + kAssumption + "\n";
  auto put = [&](const char* title, const std::vector<std::vector<std::string>>& t) {
    o += std::string("\n") + title + "\n";
    for (const auto& row : t) { for (size_t i = 0; i < row.size(); i++) o += (i ? ";" : "") + row[i]; o += "\n"; }
  };
  put("Katy poziome lewe", AngleRows(r)); put("Boki", SideRows(r)); put("Rachunek", CalcRows(r));
  if (r.res.valid) { put("Azymuty i przyrosty", BearingRows(r)); put("Wspolrzedne", CoordRows(r)); }
  return o;
}

inline std::string PolygonDocx(const PolygonReport& r) {
  using namespace docxdetail; using namespace polyrep;
  auto table = [](const std::vector<std::vector<std::string>>& cells) {
    const int cols = cells.empty() ? 1 : (int)cells[0].size();
    std::string t = "<w:tbl><w:tblPr><w:tblW w:w=\"0\" w:type=\"auto\"/><w:tblBorders>";
    for (const char* b : {"top", "left", "bottom", "right", "insideH", "insideV"}) t += std::string("<w:") + b + " w:val=\"single\" w:sz=\"4\" w:space=\"0\" w:color=\"000000\"/>";
    t += "</w:tblBorders></w:tblPr><w:tblGrid>"; for (int i = 0; i < cols; i++) t += "<w:gridCol w:w=\"1500\"/>"; t += "</w:tblGrid>";
    for (size_t i = 0; i < cells.size(); i++) t += Row(cells[i], i == 0, 1500);
    return t + "</w:tbl>";
  };
  std::string body = Para("Ciag poligonowy", true, 36, true);
  body += Para(std::string("Rodzaj: ") + PolyKindName(r.kind) + ", data: " + r.dateText + (r.complete ? "" : " (niekompletny - pomiar przerwany)"));
  body += Para(kAssumption);
  body += Para("Katy poziome lewe", true, 28); body += table(AngleRows(r));
  body += Para("Boki", true, 28); body += table(SideRows(r));
  body += Para("Rachunek", true, 28); body += table(CalcRows(r));
  if (r.res.valid) { body += Para("Azymuty i przyrosty", true, 28); body += table(BearingRows(r)); body += Para("Wspolrzedne", true, 28); body += table(CoordRows(r)); }
  return WrapDocx(body);
}

inline std::vector<std::string> PolygonSummaryLines(const PolygonReport& r) {
  using namespace polyrep;
  std::vector<std::string> out; const PolyResult& p = r.res;
  out.push_back(std::string("Ciag poligonowy (") + PolyKindName(r.kind) + "), stanowisk: " + std::to_string(r.stations.size()) + (r.complete ? "" : ", pomiar przerwany"));
  if (!p.valid) { out.push_back("Brak rachunku: " + p.error); return out; }
  if (p.hasAngleCheck) {
    out.push_back("f_beta = " + Sg(p.fBetaGon, 4) + " g (dop. " + F(p.fBetaLimGon, 4) + " g) - " + Verdict(p.angleOk));
    out.push_back("f_d = " + F(p.fd, 3) + " m (f_x " + Sg(p.fx, 3) + ", f_y " + Sg(p.fy, 3) + "; dop. " + F(p.fdLim, 3) + " m) - " + Verdict(p.linearOk));
    out.push_back("Limity: zalozenie symulatora (z dokladnosci instrumentu)");
  } else out.push_back("Ciag wiszacy: brak kontroli (odchylki niedostepne)");
  for (size_t i = 0; i < p.x.size(); i++) out.push_back(NameOr(r, i) + "  N " + F(p.x[i], 3) + "  E " + F(p.y[i], 3));
  return out;
}
