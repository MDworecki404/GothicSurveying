#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
#include "job/MenuLayout.hpp"

static std::vector<MenuItem> Items(int n, int perSection, bool enabled = true) {
  std::vector<MenuItem> v;
  for (int i = 0; i < n; i++) { MenuItem m{"id" + std::to_string(i), "Pozycja " + std::to_string(i), "", enabled}; m.section = "Sekcja" + std::to_string(i / perSection); v.push_back(m); }
  return v;
}

static void empty_menu() {
  const MenuLayout l = BuildLayout({}, 0, 16);
  assert(l.rows.empty() && l.count == 0 && l.first == 0 && l.cursorRow == -1 && !l.moreAbove && !l.moreBelow);
  assert(MoveCursor({}, 0, 1) == -1);
}

static void all_disabled_cursor_still_moves() {
  const auto it = Items(3, 3, false);
  assert(MoveCursor(it, 0, 1) == 1 && MoveCursor(it, 2, 1) == 0 && MoveCursor(it, 0, -1) == 2);
  const MenuLayout l = BuildLayout(it, 1, 16); assert(l.cursorRow >= 0 && l.rows[l.cursorRow].item == 1);
}

static void headers_inserted_on_section_change() {
  const MenuLayout l = BuildLayout(Items(4, 2), 0, 16);       // 2 sekcje po 2 pozycje -> 2 naglowki + 4 pozycje
  assert(l.rows.size() == 6 && l.rows[0].header && l.rows[0].item == -1 && l.rows[0].text == "SEKCJA0");
  assert(!l.rows[1].header && l.rows[1].item == 0 && l.rows[1].text == "Pozycja 0" && l.rows[3].header && l.rows[3].text == "SEKCJA1");
  assert(l.cursorRow == 1);
  std::vector<MenuItem> noSec = Items(2, 2); for (auto& m : noSec) m.section.clear();
  assert(BuildLayout(noSec, 0, 16).rows.size() == 2);          // bez sekcji: bez naglowkow (stare menu)
}

static void window_scrolls_40_items_cursor_last() {
  const MenuLayout l = BuildLayout(Items(40, 10), 39, 16);    // 4 naglowki + 40 pozycji = 44 wiersze
  assert(l.rows.size() == 44 && l.count == 16 && l.first == 28 && l.moreAbove && !l.moreBelow);
  assert(l.cursorRow == 43 && l.cursorRow >= l.first && l.cursorRow < l.first + l.count);
  const MenuLayout f = BuildLayout(Items(40, 10), 0, 16);
  assert(f.first == 0 && !f.moreAbove && f.moreBelow && f.cursorRow == 1);
  const MenuLayout m = BuildLayout(Items(40, 10), 20, 16);    // srodek: kursor widoczny, przewijanie w obie strony
  assert(m.moreAbove && m.moreBelow && m.cursorRow >= m.first && m.cursorRow < m.first + m.count);
}

static void cursor_item_keeps_its_section_header_visible() {
  const MenuLayout l = BuildLayout(Items(40, 10), 30, 6);     // pierwsza pozycja sekcji: naglowek tuz nad nia ma byc w oknie
  assert(l.rows[l.cursorRow - 1].header && l.cursorRow - 1 >= l.first && l.cursorRow < l.first + l.count);
}

static void window_no_scroll_when_fits() {
  const MenuLayout l = BuildLayout(Items(6, 3), 5, 16);
  assert(l.first == 0 && l.count == (int)l.rows.size() && !l.moreAbove && !l.moreBelow);
}

static void wrap_breaks_on_space_and_limits_lines() {
  const auto w = WrapText("aaa bbb ccc ddd", 7, 2);
  assert(w.size() == 2 && w[0] == "aaa bbb" && w[1] == "ccc ddd");
  assert(WrapText("aaa bbb ccc ddd eee", 7, 2).size() == 2);           // reszta ponad limit linii jest ucinana
  assert(WrapText("", 10, 2).empty());
  const auto hard = WrapText("abcdefghijklmnop", 5, 5); assert(hard.size() == 4 && hard[0] == "abcde" && hard[3] == "p");   // brak spacji: twarde ciecie
  assert(WrapText("krotki", 88, 2).size() == 1);
}

int main() {
  empty_menu(); all_disabled_cursor_still_moves(); headers_inserted_on_section_change(); window_scrolls_40_items_cursor_last();
  cursor_item_keeps_its_section_header_visible(); window_no_scroll_when_fits(); wrap_breaks_on_space_and_limits_lines();
  std::puts("test_menulayout OK");
  return 0;
}
