// Uklad wierszy menu (bez Gothic API): naglowki sekcji, okno przewijania wokol kursora i zawijanie podpisu.
// Silnik (engine/Menu.hpp) tylko rysuje to, co tu policzone.
#pragma once
#include <algorithm>
#include <cctype>
#include <string>
#include <vector>
#include "Job.hpp"

struct MenuRow { bool header; int item; std::string text; };     // item = indeks w items, -1 dla naglowka
struct MenuLayout { std::vector<MenuRow> rows; int first = 0, count = 0, cursorRow = -1; bool moreAbove = false, moreBelow = false; };

// Wiersze: naglowek (wielkimi literami) przy kazdej zmianie section (pusta section = bez naglowkow); okno maxRows wierszy z kursorem w srodku.
inline MenuLayout BuildLayout(const std::vector<MenuItem>& items, int cursor, int maxRows) {
  MenuLayout l; std::string prev;
  for (size_t i = 0; i < items.size(); i++) {
    const std::string& sec = items[i].section;
    if (!sec.empty() && sec != prev) { std::string up = sec; for (char& c : up) c = (char)std::toupper((unsigned char)c); l.rows.push_back({ true, -1, up }); }
    prev = sec;
    if ((int)i == cursor) l.cursorRow = (int)l.rows.size();
    l.rows.push_back({ false, (int)i, items[i].label });
  }
  const int total = (int)l.rows.size();
  l.count = (std::min)(total, (std::max)(1, maxRows));
  if (total == 0) l.count = 0;
  int first = 0;
  if (total > l.count) {
    first = (std::max)(0, (std::min)(total - l.count, (l.cursorRow < 0 ? 0 : l.cursorRow) - l.count / 2));
    if (l.cursorRow > 0 && l.count >= 2 && l.rows[l.cursorRow - 1].header && l.cursorRow - 1 < first) first = l.cursorRow - 1;   // naglowek sekcji kursora zostaje widoczny
  }
  l.first = first; l.moreAbove = first > 0; l.moreBelow = first + l.count < total;
  return l;
}

// Kursor chodzi po wszystkich pozycjach, takze niedostepnych (podpis podaje powod); -1 gdy brak pozycji.
inline int MoveCursor(const std::vector<MenuItem>& items, int cursor, int dir) {
  const int n = (int)items.size();
  if (n == 0) return -1;
  return ((cursor + dir) % n + n) % n;
}

// Zawija do width znakow (po spacji, a bez spacji twardo), co najwyzej maxLines linii; reszta jest ucinana.
inline std::vector<std::string> WrapText(const std::string& text, size_t width, size_t maxLines) {
  std::vector<std::string> out; std::string rest = text;
  while (!rest.empty() && out.size() < maxLines) {
    size_t cut = rest.size();
    if (cut > width) { cut = rest.rfind(' ', width); if (cut == std::string::npos || cut == 0) cut = width; }
    out.push_back(rest.substr(0, cut));
    rest = cut < rest.size() ? rest.substr(cut + (rest[cut] == ' ' ? 1 : 0)) : "";
  }
  return out;
}
