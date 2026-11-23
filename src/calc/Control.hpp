// Osnowa pomiarowa zalozona GNSS-em (bez Gothic API): punkt, CSV, nastepny numer, filtr po odleglosci.
// Wspolrzedne n,e,h to wynik pomiaru GNSS (z bledem); x,y,z to prawdziwe polozenie znaku w grze [cm].
#pragma once
#include <algorithm>
#include <cmath>
#include <cctype>
#include <cstdlib>
#include <string>
#include <vector>
#include "FieldBook.hpp"

struct ControlPoint {
  std::string id, world;
  double n = 0, e = 0, h = 0;
  double x = 0, y = 0, z = 0;
  std::string solution;
  double pdop = 0; int epochs = 0;
  std::string timeText;
};

inline std::string ControlCsvHeader() { return "id;swiat;N_m;E_m;H_m;x_cm;y_cm;z_cm;rozwiazanie;PDOP;epoki;czas"; }

inline std::string ControlCsvRow(const ControlPoint& p) {
  using fbdetail::Num; using fbdetail::Csv;
  return Csv(p.id) + ";" + Csv(p.world) + ";" + Num(p.n, 3) + ";" + Num(p.e, 3) + ";" + Num(p.h, 3) + ";" + Num(p.x, 1) + ";" + Num(p.y, 1) + ";" +
         Num(p.z, 1) + ";" + Csv(p.solution) + ";" + Num(p.pdop, 1) + ";" + std::to_string(p.epochs) + ";" + Csv(p.timeText);
}

inline std::string ControlToCsv(const std::vector<ControlPoint>& pts) {
  std::string o = ControlCsvHeader() + "\n";
  for (const ControlPoint& p : pts) o += ControlCsvRow(p) + "\n";
  return o;
}

namespace controldetail {
// Dzieli wiersz po ';' z uwzglednieniem cudzyslowow (podwojony cudzyslow = cudzyslow).
inline std::vector<std::string> SplitCsv(const std::string& line) {
  std::vector<std::string> out; std::string cur; bool quoted = false;
  for (size_t i = 0; i < line.size(); i++) {
    const char c = line[i];
    if (quoted) {
      if (c == '"') { if (i + 1 < line.size() && line[i + 1] == '"') { cur += '"'; i++; } else quoted = false; }
      else cur += c;
    } else if (c == '"') quoted = true;
    else if (c == ';') { out.push_back(cur); cur.clear(); }
    else if (c != '\r') cur += c;
  }
  out.push_back(cur);
  return out;
}
inline bool ToDouble(const std::string& s, double& v) {
  if (s.empty()) return false;
  char* end = nullptr; v = std::strtod(s.c_str(), &end);
  return end && *end == '\0' && std::isfinite(v);
}
}  // namespace controldetail

// Wczytuje osnowe; pomija naglowek, puste i wadliwe wiersze, nie rzuca wyjatkow.
inline std::vector<ControlPoint> ControlFromCsv(const std::string& text) {
  std::vector<ControlPoint> out; size_t i = 0;
  while (i < text.size()) {
    size_t j = text.find('\n', i); if (j == std::string::npos) j = text.size();
    const std::vector<std::string> f = controldetail::SplitCsv(text.substr(i, j - i));
    i = j + 1;
    if (f.size() < 12 || f[0].empty() || f[0] == "id") continue;
    ControlPoint p; double pdop = 0, ep = 0;
    if (!controldetail::ToDouble(f[2], p.n) || !controldetail::ToDouble(f[3], p.e) || !controldetail::ToDouble(f[4], p.h) ||
        !controldetail::ToDouble(f[5], p.x) || !controldetail::ToDouble(f[6], p.y) || !controldetail::ToDouble(f[7], p.z) ||
        !controldetail::ToDouble(f[9], pdop) || !controldetail::ToDouble(f[10], ep)) continue;
    p.id = f[0]; p.world = f[1]; p.solution = f[8]; p.pdop = pdop; p.epochs = (int)ep; p.timeText = f[11];
    out.push_back(p);
  }
  return out;
}

// "O<liczba>" o najwiekszej liczbie + 1 (O1 dla pustej osnowy); inne id sa ignorowane.
inline std::string ControlNextId(const std::vector<ControlPoint>& pts) {
  long best = 0;
  for (const ControlPoint& p : pts) {
    if (p.id.size() < 2 || p.id[0] != 'O') continue;
    char* end = nullptr; const long v = std::strtol(p.id.c_str() + 1, &end, 10);
    if (end && *end == '\0' && v > best) best = v;
  }
  return "O" + std::to_string(best + 1);
}

// Klucz swiata: nazwa pliku bez sciezki i rozszerzenia, wielkimi literami (NewWorld\NewWorld.zen -> NEWWORLD); "" gdy pusta.
inline std::string ControlWorldKey(const std::string& name) {
  size_t start = name.find_last_of("/\\");
  std::string s = name.substr(start == std::string::npos ? 0 : start + 1);
  const size_t dot = s.find_last_of('.');
  if (dot != std::string::npos) s.erase(dot);
  for (char& c : s) c = (char)std::toupper((unsigned char)c);
  return s;
}

// Kierunek swiata (N, NE, ...) wektora (dx = wschod, dz = polnoc).
inline const char* ControlCompass(double dx, double dz) {
  static const char* d[8] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
  return d[((int)std::floor(std::atan2(dx, dz) * 180.0 / 3.14159265358979 / 45.0 + 0.5) % 8 + 8) % 8];
}

// Opis rozwiazania do wyswietlania: punkty wzorcowe zapisane starsza wersja maja w opisie nazwe miejsca ("osnowa wzorcowa: <miejsce>") - ucinamy ja.
inline std::string ControlDisplaySolution(const ControlPoint& c) {
  static const std::string pre = "osnowa wzorcowa";
  return c.solution.compare(0, pre.size(), pre) == 0 ? pre : c.solution;
}

// Kolejnosc wg id: litera (G przed O), potem numer rosnaco.
inline void ControlSortById(std::vector<ControlPoint>& v) {
  auto num = [](const std::string& s) { return s.size() > 1 ? std::atoi(s.c_str() + 1) : 0; };
  std::sort(v.begin(), v.end(), [&](const ControlPoint& a, const ControlPoint& b) {
    const char la = a.id.empty() ? 0 : a.id[0], lb = b.id.empty() ? 0 : b.id[0];
    return la != lb ? la < lb : num(a.id) < num(b.id);
  });
}

// Usuwa z osnowy punkty swiata o podanych id; zwraca liczbe usunietych.
inline int ControlRemove(std::vector<ControlPoint>& pts, const std::string& world, const std::vector<std::string>& ids) {
  const std::string key = ControlWorldKey(world); int n = 0;
  for (size_t i = pts.size(); i-- > 0;) {
    if (ControlWorldKey(pts[i].world) != key) continue;
    for (const std::string& id : ids) if (pts[i].id == id) { pts.erase(pts.begin() + (long)i); n++; break; }
  }
  return n;
}

// Punkty z danego swiata w poziomej odleglosci [minM, maxM] od (xCm, zCm), rosnaco wg odleglosci.
inline std::vector<ControlPoint> ControlNear(const std::vector<ControlPoint>& pts, const std::string& world, double xCm, double zCm,
                                             double minM, double maxM) {
  std::vector<std::pair<double, ControlPoint>> hit;
  const std::string key = ControlWorldKey(world);
  for (const ControlPoint& p : pts) {
    if (ControlWorldKey(p.world) != key) continue;
    const double d = std::hypot(p.x - xCm, p.z - zCm) / 100.0;
    if (d >= minM && d <= maxM) hit.push_back({d, p});
  }
  std::sort(hit.begin(), hit.end(), [](const std::pair<double, ControlPoint>& a, const std::pair<double, ControlPoint>& b) { return a.first < b.first; });
  std::vector<ControlPoint> out;
  for (auto& h : hit) out.push_back(h.second);
  return out;
}
