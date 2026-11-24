// Zbiorczy raport projektu (bez Gothic API): punkty wysokosciowe pomiarow, porownanie wysokosci na wspolnych punktach, raport CSV i DOCX.
// Raport powstaje dopiero przy zamknieciu projektu; w trakcie pracy wystarczaja raporty poszczegolnych robot.
#pragma once
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>
#include "Docx.hpp"
#include "FieldBook.hpp"
#include "GnssReport.hpp"
#include "Project.hpp"
#include "TachyReport.hpp"

struct PointH { std::string pointId; double h = 0, n = 0, e = 0; bool hasNE = false; };
struct HeightRow { std::string pointId; std::vector<std::pair<std::string, double>> byMeasurement; double spanMm = 0; };   // spanMm = max - min [mm]

namespace prdetail {
inline std::string Lower(std::string s) { for (char& c : s) c = (char)std::tolower((unsigned char)c); return s; }
inline std::string Clean(std::string s) { for (char& c : s) if (c == ';' || c == '\n' || c == '\r') c = c == ';' ? (char)',' : ' '; return s; }
inline std::string Id(std::string s) { for (char& c : s) if (c == ';' || c == '\n' || c == '\r') c = '_'; return s; }   // id punktu w CSV punktow: separator -> "_"
inline std::string Num(double v, int d) { char b[40]; std::snprintf(b, sizeof b, "%.*f", d, v); return b; }
inline const char* StateText(MeasState s) { switch (s) { case MeasState::Done: return "gotowy"; case MeasState::Running: return "w toku"; case MeasState::Aborted: return "przerwany"; default: return "nowy"; } }
}  // namespace prdetail

inline std::vector<PointH> PointsOf(const GnssReport& r) {
  std::vector<PointH> v; for (const GnssPoint& p : r.points) { PointH q; q.pointId = p.name; q.n = p.n; q.e = p.e; q.h = p.h; q.hasNE = true; v.push_back(q); } return v;
}
inline std::vector<PointH> PointsOf(const TachyReport& r) {
  std::vector<PointH> v; for (const TachyPoint& p : r.points) { PointH q; q.pointId = p.name; q.n = p.n; q.e = p.e; q.h = p.h; q.hasNE = true; v.push_back(q); } return v;
}
inline std::vector<PointH> PointsOf(const FieldBook& fb) {
  std::vector<PointH> v; PointH s; s.pointId = fb.startName; s.h = fb.startHeightM; v.push_back(s);
  for (const FbRow& r : fb.rows) { PointH q; q.pointId = r.pointName; q.h = r.heightM; v.push_back(q); }
  return v;
}

inline std::string PointsCsv(const std::vector<PointH>& pts) {
  std::string o = "punkt;N;E;H;maNE\n";
  for (const PointH& p : pts) o += prdetail::Id(p.pointId) + ";" + prdetail::Num(p.n, 3) + ";" + prdetail::Num(p.e, 3) + ";" + prdetail::Num(p.h, 4) + ";" + (p.hasNE ? "1" : "0") + "\n";
  return o;
}

// Odporne na smieci: wiersze bez pieciu pol albo z pustym id sa pomijane.
inline std::vector<PointH> ParsePointsCsv(const std::string& text) {
  std::vector<PointH> out; size_t pos = 0; bool first = true;
  while (pos < text.size()) {
    size_t end = text.find('\n', pos); if (end == std::string::npos) end = text.size();
    std::string line = text.substr(pos, end - pos); pos = end + 1;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (first) { first = false; if (line.compare(0, 5, "punkt") == 0) continue; }
    std::vector<std::string> f; size_t a = 0;
    for (int i = 0; i < 4; i++) { const size_t c = line.find(';', a); if (c == std::string::npos) break; f.push_back(line.substr(a, c - a)); a = c + 1; }
    if (f.size() != 4 || f[0].empty()) continue;
    PointH p; p.pointId = f[0]; p.n = std::atof(f[1].c_str()); p.e = std::atof(f[2].c_str()); p.h = std::atof(f[3].c_str()); p.hasNE = line.substr(a) == "1";
    out.push_back(p);
  }
  return out;
}

// Punkty wspolne: te same nazwy (bez rozrozniania wielkosci liter) albo polozenie w promieniu 0.5 m (gdy oba pomiary maja N/E).
// Jeden pomiar wnosi do grupy najwyzej jedna wartosc (pierwsza). Wiersze tylko dla punktow z co najmniej dwoch pomiarow.
inline std::vector<HeightRow> CompareHeights(const std::vector<std::pair<std::string, std::vector<PointH>>>& byMeasurement) {
  struct Member { std::string meas; PointH p; };
  struct Group { std::string id; std::vector<Member> m; };
  std::vector<Group> groups;
  for (const auto& meas : byMeasurement) {
    for (const PointH& p : meas.second) {
      Group* g = nullptr;
      for (Group& c : groups) {
        bool match = prdetail::Lower(c.id) == prdetail::Lower(p.pointId);
        for (const Member& mm : c.m) if (!match && p.hasNE && mm.p.hasNE && std::hypot(p.n - mm.p.n, p.e - mm.p.e) <= 0.5) match = true;
        if (match) { g = &c; break; }
      }
      if (!g) { groups.push_back({ p.pointId, {} }); g = &groups.back(); }
      bool has = false; for (const Member& mm : g->m) if (mm.meas == meas.first) has = true;
      if (!has) g->m.push_back({ meas.first, p });
    }
  }
  std::vector<HeightRow> rows;
  for (const Group& g : groups) {
    if (g.m.size() < 2) continue;
    HeightRow r; r.pointId = g.id; double lo = g.m[0].p.h, hi = g.m[0].p.h;
    for (const Member& mm : g.m) { r.byMeasurement.push_back({ mm.meas, mm.p.h }); lo = (std::min)(lo, mm.p.h); hi = (std::max)(hi, mm.p.h); }
    r.spanMm = (hi - lo) * 1000.0; rows.push_back(r);
  }
  return rows;
}

// Pliki (bez podfolderow) w folderze wynikow, alfabetycznie; brak folderu = pusta lista.
inline std::vector<std::string> ResultFiles(const std::string& dir) {
  namespace fs = std::filesystem; std::error_code ec; std::vector<std::string> out;
  for (const auto& e : fs::directory_iterator(dir, ec)) if (e.is_regular_file(ec)) out.push_back(e.path().filename().string());
  std::sort(out.begin(), out.end());
  return out;
}

namespace prdetail {
inline std::vector<std::string> MeasColumns(const std::vector<HeightRow>& rows) {
  std::vector<std::string> cols;
  for (const HeightRow& r : rows) for (const auto& m : r.byMeasurement) if (std::find(cols.begin(), cols.end(), m.first) == cols.end()) cols.push_back(m.first);
  return cols;
}
inline std::vector<std::string> CompareCells(const HeightRow& r, const std::vector<std::string>& cols) {
  std::vector<std::string> c = { r.pointId };
  for (const std::string& col : cols) { std::string v; for (const auto& m : r.byMeasurement) if (m.first == col) v = Num(m.second, 3); c.push_back(v); }
  c.push_back(Num(r.spanMm, 1)); return c;
}
}  // namespace prdetail

inline std::string ProjectReportCsv(const Project& p, const std::vector<HeightRow>& rows) {
  using namespace prdetail;
  std::string o = "Raport projektu;" + Clean(p.name) + "\nSwiat;" + Clean(p.world) + "\nUtworzono;" + Clean(p.created) + "\n\nPomiary\nNazwa;Rodzaj;Stan;Folder wynikow;Podsumowanie\n";
  for (const Measurement& m : p.items) o += Clean(m.name) + ";" + Clean(m.type) + ";" + StateText(m.state) + ";" + Clean(m.dir) + ";" + Clean(m.summary) + "\n";
  o += "\nPorownanie wysokosci [m], rozrzut [mm]\n";
  if (rows.empty()) { o += "brak punktow do porownania\n"; return o; }
  const auto cols = MeasColumns(rows);
  o += "Punkt"; for (const std::string& c : cols) o += ";" + Clean(c); o += ";Rozrzut [mm]\n";
  for (const HeightRow& r : rows) { const auto cells = CompareCells(r, cols); for (size_t i = 0; i < cells.size(); i++) o += (i ? ";" : "") + Clean(cells[i]); o += "\n"; }
  return o;
}

inline std::string ProjectReportDocx(const Project& p, const std::vector<HeightRow>& rows, const std::string& dateText) {
  using namespace docxdetail; using namespace prdetail;
  auto table = [](const std::vector<std::vector<std::string>>& cells, int cols) {
    std::string t = "<w:tbl><w:tblPr><w:tblW w:w=\"0\" w:type=\"auto\"/><w:tblBorders>";
    for (const char* b : {"top", "left", "bottom", "right", "insideH", "insideV"}) t += std::string("<w:") + b + " w:val=\"single\" w:sz=\"4\" w:space=\"0\" w:color=\"000000\"/>";
    t += "</w:tblBorders></w:tblPr><w:tblGrid>"; for (int i = 0; i < cols; i++) t += "<w:gridCol w:w=\"2400\"/>"; t += "</w:tblGrid>";
    for (size_t i = 0; i < cells.size(); i++) t += Row(cells[i], i == 0, 2400);
    return t + "</w:tbl>";
  };
  std::string body = Para("Raport projektu", true, 36, true);
  body += Para("Projekt: " + p.name + ", swiat: " + p.world + ", data: " + dateText);
  std::vector<std::vector<std::string>> t1 = { {"Pomiar", "Rodzaj", "Stan", "Folder wynikow", "Podsumowanie"} };
  for (const Measurement& m : p.items) t1.push_back({ m.name, m.type, StateText(m.state), m.dir, m.summary });
  body += table(t1, 5);
  body += Para("Porownanie wysokosci na wspolnych punktach", true, 28);
  if (rows.empty()) body += Para("brak punktow do porownania");
  else {
    const auto cols = MeasColumns(rows);
    std::vector<std::string> head = { "Punkt" }; for (const std::string& c : cols) head.push_back(c); head.push_back("Rozrzut [mm]");
    std::vector<std::vector<std::string>> t2 = { head };
    for (const HeightRow& r : rows) t2.push_back(CompareCells(r, cols));
    body += table(t2, (int)head.size());
  }
  return WrapDocx(body);
}

// Czyta pliki punktow z projekty/<nazwa>/ i zapisuje tam projekt_raport.csv oraz projekt_raport.docx; brakujacy plik punktow jest pomijany.
inline bool WriteProjectReport(const Project& p, const std::string& root, std::string& err) {
  namespace fs = std::filesystem; std::error_code ec; err.clear();
  const std::string dir = root + "/projekty/" + p.name;
  fs::create_directories(dir, ec);
  std::vector<std::pair<std::string, std::vector<PointH>>> meas;
  for (const Measurement& m : p.items) {
    if (m.pointsFile.empty()) continue;
    std::ifstream f(dir + "/" + m.pointsFile, std::ios::binary); if (!f) continue;
    meas.push_back({ m.name, ParsePointsCsv(std::string(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>())) });
  }
  const auto rows = CompareHeights(meas);
  const std::string csv = ProjectReportCsv(p, rows), docx = ProjectReportDocx(p, rows, p.created);
  { std::ofstream f(dir + "/projekt_raport.csv", std::ios::binary | std::ios::trunc); if (!f) { err = "nie mozna zapisac projekt_raport.csv"; return false; } f.write(csv.data(), (std::streamsize)csv.size()); }
  { std::ofstream f(dir + "/projekt_raport.docx", std::ios::binary | std::ios::trunc); if (!f) { err = "nie mozna zapisac projekt_raport.docx"; return false; } f.write(docx.data(), (std::streamsize)docx.size()); }
  return true;
}
