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
  std::string note; int stationNo = 1;                           // numer stanowiska (wiele stanowisk w robocie)
  bool hasTrue = false; double dNm = 0, dEm = 0, dHm = 0, d2Dm = 0;   // błąd względem prawdziwego punktu w grze [m]
};

// Nawiązanie użyte do orientacji: kierunek i odległość zmierzone vs. ze współrzędnych (odchyłki kontrolne).
struct TachyBacksight {
  std::string name;
  double azGon = 0, hzGon = 0, dirResidMgon = 0, distMeasuredM = 0, distCoordM = 0, distResidMm = 0;
  bool uncertain = false;
};

// Odcinek niwelacji trygonometrycznej: pomiar tam (S->T) i z powrotem (T->S), przewyzszenie srednie i kontrola.
struct TrigLeg {
  std::string from, to;
  double distM = 0, vFwdGon = 0, vBackGon = 0, dhFwdMm = 0, dhBackMm = 0;   // dhBack = przewyzszenie T->S zmierzone z powrotem
  double sumMm = 0, meanMm = 0, tolMm = 0, hFromM = 0, hToM = 0;            // sum = tam + powrot (powinno byc ~0), mean = (tam - powrot) / 2
  bool ok = true, hasTrue = false; double errMm = 0;                          // err = mean - prawdziwe przewyzszenie
};

// Stanowisko w raporcie z wieloma stanowiskami: dane stanowiska i jego nawiazania (odchylki kontrolne).
struct TachyStationRec {
  std::string label, coordSource, stationName;
  double n = 0, e = 0, h = 0, instrH = 1.5, azBackGon = 0;
  std::vector<TachyBacksight> backsights;
};

struct TachyReport {
  bool trig = false; std::vector<TrigLeg> legs;                 // niwelacja trygonometryczna: odcinki (points = pojedyncze obserwacje)
  std::vector<TachyStationRec> stations;                   // > 1 gdy robota ma wiele stanowisk (pola ponizej = ostatnie stanowisko); puste = jedno
  std::string coordSource = "prawdziwe", stationName;      // źródło współrzędnych ("osnowa GNSS"/"prawdziwe") i nazwa stanowiska
  std::vector<TachyBacksight> backsights;
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

inline std::string TachyBacksightCsvLine(const TachyBacksight& b) {
  using fbdetail::Num;
  return "# nawiazanie;" + fbdetail::Csv(b.name) + ";" + Num(b.azGon, 4) + ";" + Num(b.hzGon, 4) + ";" + Num(b.dirResidMgon, 1) + ";" + Num(b.distMeasuredM, 3) +
         ";" + Num(b.distCoordM, 3) + ";" + Num(b.distResidMm, 1) + ";" + (b.uncertain ? "niepewne" : "ok");
}

inline std::string TrigLegsCsv(const TachyReport& r) {
  using fbdetail::Num;
  std::string o = "odcinek;od;do;d_pozioma_m;V_tam_g;V_powrot_g;dh_tam_mm;dh_powrot_mm;suma_mm;dh_sredni_mm;dopuszczalna_mm;ocena;H_od_m;H_do_m";
  if (r.showTrueError) o += ";blad_dh_mm";
  o += "\n";
  int nr = 0;
  for (const TrigLeg& l : r.legs) {
    o += std::to_string(++nr) + ";" + l.from + ";" + l.to + ";" + Num(l.distM, 3) + ";" + Num(l.vFwdGon, 4) + ";" + Num(l.vBackGon, 4) + ";" + Num(l.dhFwdMm, 1) + ";" +
         Num(l.dhBackMm, 1) + ";" + Num(l.sumMm, 1) + ";" + Num(l.meanMm, 1) + ";" + Num(l.tolMm, 1) + ";" + (l.ok ? "ok" : "PRZEKROCZONA") + ";" + Num(l.hFromM, 3) + ";" + Num(l.hToM, 3);
    if (r.showTrueError) o += ";" + (l.hasTrue ? Num(l.errMm, 1) : std::string());
    o += "\n";
  }
  return o;
}

inline std::string TachyToCsv(const TachyReport& r) {
  using fbdetail::Num;
  std::string o;
  if (!r.complete) o += "# UWAGA: pomiar niekompletny (robota przerwana)\n";
  if (r.trig) {                                        // niwelacja trygonometryczna: odcinki, potem pojedyncze obserwacje
    o += "# niwelacja trygonometryczna (tam i z powrotem, poprawka na krzywizne Ziemi i refrakcje k=0,13)\n" + TrigLegsCsv(r) + "#\n";
    o += TachyCsvHeader(false) + "\n";
    int nr = 0;
    for (const TachyPoint& p : r.points) o += TachyCsvRow(p, ++nr, false) + "\n";
    return o;
  }
  if (r.stations.size() > 1) {
    for (const TachyStationRec& s : r.stations) {
      o += "# stanowisko " + s.label + ";" + Num(s.n, 3) + ";" + Num(s.e, 3) + ";" + Num(s.h, 3) + ";" + Num(s.instrH, 3) + ";" + Num(s.azBackGon, 4) + "\n";
      o += "# zrodlo " + s.label + ";" + fbdetail::Csv(s.coordSource) + ";" + fbdetail::Csv(s.stationName) + "\n";
      for (const TachyBacksight& b : s.backsights) o += TachyBacksightCsvLine(b) + ";" + s.label + "\n";
    }
  } else {
    o += "# stanowisko;" + Num(r.stationN, 3) + ";" + Num(r.stationE, 3) + ";" + Num(r.stationH, 3) + ";" + Num(r.instrH, 3) + ";" + Num(r.azBackGon, 4) + "\n";
    o += "# zrodlo;" + fbdetail::Csv(r.coordSource) + ";" + fbdetail::Csv(r.stationName) + "\n";
    for (const TachyBacksight& b : r.backsights) o += TachyBacksightCsvLine(b) + "\n";
  }
  o += TachyCsvHeader(r.showTrueError) + "\n";
  int nr = 0;
  for (const TachyPoint& p : r.points) o += TachyCsvRow(p, ++nr, r.showTrueError) + "\n";
  return o;
}

inline std::vector<std::string> TachySummaryLines(const TachyReport& r) {
  using fbdetail::Num;
  std::vector<std::string> out;
  if (r.trig) {
    int bad = 0; for (const TrigLeg& l : r.legs) if (!l.ok) bad++;
    out.push_back("Odcinki: " + std::to_string(r.legs.size()) + (bad ? ", poza tolerancja: " + std::to_string(bad) : ", wszystkie w tolerancji"));
    for (const TrigLeg& l : r.legs)
      out.push_back(l.from + " -> " + l.to + ": dh " + Num(l.meanMm, 1) + " mm (tam " + Num(l.dhFwdMm, 1) + ", powrot " + Num(-l.dhBackMm, 1) + ", suma " + Num(l.sumMm, 1) + " / dop. " + Num(l.tolMm, 1) +
                    ")  H " + l.from + " = " + Num(l.hFromM, 3) + ", H " + l.to + " = " + Num(l.hToM, 3) + (r.showTrueError && l.hasTrue ? ", blad " + Num(l.errMm, 1) + " mm" : ""));
    return out;
  }
  double sum = 0, longest = 0;
  for (const TachyPoint& p : r.points) { sum += p.slopeM; if (p.slopeM > longest) longest = p.slopeM; }
  const double avg = r.points.empty() ? 0.0 : sum / (double)r.points.size();
  out.push_back("Punkty: " + std::to_string(r.points.size()) + ", srednia odleglosc: " + Num(avg, 1) + " m, najdluzsza celowa: " + Num(longest, 1) + " m");
  std::vector<TachyBacksight> allB = r.backsights;
  if (r.stations.size() > 1) { allB.clear(); for (const TachyStationRec& s : r.stations) allB.insert(allB.end(), s.backsights.begin(), s.backsights.end()); out.insert(out.begin(), "Stanowiska: " + std::to_string(r.stations.size())); }
  if (!allB.empty()) {
    double worst = 0;
    for (const TachyBacksight& b : allB) if (std::fabs(b.dirResidMgon) > worst) worst = std::fabs(b.dirResidMgon);
    out.push_back("Nawiazania: " + std::to_string(allB.size()) + ", najwieksza odchylka kierunku: " + Num(worst, 1) + " mgon");
  }
  if (r.showTrueError && !r.points.empty()) {
    const TachyPoint* worst = &r.points[0];
    for (const TachyPoint& p : r.points) if (p.d2Dm > worst->d2Dm) worst = &p;
    out.push_back("Najgorszy punkt: " + worst->name + " (" + Num(worst->d2Dm, 3) + " m)");
  }
  return out;
}

inline std::string TrigBuildDocx(const TachyReport& r) {
  using namespace docxdetail; using fbdetail::Num;
  std::string body = Para("Niwelacja trygonometryczna", true, 36, true);
  body += Para("data: " + r.dateText + (r.complete ? "" : " (pomiar niekompletny - robota przerwana)"));
  body += Para("Przewyższenie z odczytów kąta zenitalnego V i odległości skośnej D: Δh = D·cosV + i − t + (1−k)·d²/(2R), k = 0,13, R = 6371 km; każdy odcinek mierzony tam i z powrotem.");
  std::vector<std::string> head = {"Nr", "Od", "Do", "d [m]", "V tam [g]", "V powrót [g]", "Δh tam [mm]", "Δh powrót [mm]", "Suma [mm]", "Δh średnie [mm]", "Dop. [mm]", "Ocena", "H od [m]", "H do [m]"};
  if (r.showTrueError) head.push_back("Błąd Δh [mm]");
  std::string tbl = "<w:tbl><w:tblPr><w:tblW w:w=\"0\" w:type=\"auto\"/><w:tblBorders>";
  for (const char* b : {"top", "left", "bottom", "right", "insideH", "insideV"})
    tbl += std::string("<w:") + b + " w:val=\"single\" w:sz=\"4\" w:space=\"0\" w:color=\"000000\"/>";
  tbl += "</w:tblBorders></w:tblPr><w:tblGrid>";
  const int width = 14000 / (int)head.size();
  for (size_t i = 0; i < head.size(); i++) tbl += "<w:gridCol w:w=\"" + std::to_string(width) + "\"/>";
  tbl += "</w:tblGrid>" + Row(head, true, width);
  int nr = 0;
  for (const TrigLeg& l : r.legs) {
    std::vector<std::string> c = {std::to_string(++nr), l.from, l.to, Num(l.distM, 3), Num(l.vFwdGon, 4), Num(l.vBackGon, 4), Num(l.dhFwdMm, 1), Num(l.dhBackMm, 1), Num(l.sumMm, 1),
                                  Num(l.meanMm, 1), Num(l.tolMm, 1), l.ok ? "ok" : "PRZEKROCZONA", Num(l.hFromM, 3), Num(l.hToM, 3)};
    if (r.showTrueError) c.push_back(l.hasTrue ? Num(l.errMm, 1) : "");
    tbl += Row(c, false, width);
  }
  body += tbl + "</w:tbl>";
  for (const std::string& l : TachySummaryLines(r)) body += Para(l, true);
  return WrapDocx(body);
}

inline std::string TachyBuildDocx(const TachyReport& r) {
  using namespace docxdetail; using fbdetail::Num;
  if (r.trig) return TrigBuildDocx(r);
  std::string body = Para("Tachimetria (pomiar biegunowy)", true, 36, true);
  body += Para("data: " + r.dateText + (r.complete ? "" : " (pomiar niekompletny - robota przerwana)"));
  std::vector<TachyStationRec> recs = r.stations;
  if (recs.size() <= 1) {                              // jedno stanowisko: pola raportu
    recs.clear(); TachyStationRec s; s.n = r.stationN; s.e = r.stationE; s.h = r.stationH; s.instrH = r.instrH; s.azBackGon = r.azBackGon;
    s.coordSource = r.coordSource; s.stationName = r.stationName; s.backsights = r.backsights; recs.push_back(s);
  }
  const bool multi = r.stations.size() > 1;
  for (const TachyStationRec& st : recs) {
  body += Para((multi ? "Stanowisko " + st.label + ": N " : "Stanowisko: N ") + Num(st.n, 3) + " m, E " + Num(st.e, 3) + " m, H " + Num(st.h, 3) + " m, wysokość instrumentu " +
               Num(st.instrH, 3) + " m, azymut nawiązania " + Num(st.azBackGon, 4) + " g, źródło współrzędnych: " + st.coordSource +
               (st.stationName.empty() ? "" : " (" + st.stationName + ")"));
  if (!st.backsights.empty()) {                        // tabela nawiązań: odchyłki kierunku i odległości
    const std::vector<std::string> bh = {"Nawiązanie", "Azymut [g]", "Hz [g]", "Odchyłka kierunku [mgon]", "D zmierzona [m]", "D ze współrzędnych [m]",
                                         "Odchyłka odległości [mm]", "Ocena"};
    const int bw = 14000 / (int)bh.size();
    std::string bt = "<w:tbl><w:tblPr><w:tblW w:w=\"0\" w:type=\"auto\"/><w:tblBorders>";
    for (const char* b : {"top", "left", "bottom", "right", "insideH", "insideV"})
      bt += std::string("<w:") + b + " w:val=\"single\" w:sz=\"4\" w:space=\"0\" w:color=\"000000\"/>";
    bt += "</w:tblBorders></w:tblPr><w:tblGrid>";
    for (size_t i = 0; i < bh.size(); i++) bt += "<w:gridCol w:w=\"" + std::to_string(bw) + "\"/>";
    bt += "</w:tblGrid>" + Row(bh, true, bw);
    for (const TachyBacksight& b : st.backsights)
      bt += Row({b.name, Num(b.azGon, 4), Num(b.hzGon, 4), Num(b.dirResidMgon, 1), Num(b.distMeasuredM, 3), Num(b.distCoordM, 3), Num(b.distResidMm, 1),
                 b.uncertain ? "niepewne" : "ok"}, false, bw);
    body += Para("Nawiązania", true) + bt + "</w:tbl>" + Para("");
  }
  }
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
