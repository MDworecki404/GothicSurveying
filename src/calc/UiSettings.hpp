// Pamiec ustawien interfejsu (bez Gothic API): ostatni projekt i ostatnia pozycja kursora na ekranie; GothicSurveying/ustawienia.txt (StateKV).
#pragma once
#include <cstdlib>
#include <map>
#include <string>
#include "Project.hpp"

struct UiSettings { std::string lastProject; std::map<std::string, int> cursor; };

inline std::string UiSettingsToText(const UiSettings& u) {
  StateKV s; s.Int("ver", 1); s.Str("last", u.lastProject);
  for (const auto& kv : u.cursor) s.Int("c." + kv.first, kv.second);
  return s.ToText();
}

// Odporne na smieci: zla nazwa projektu jest odrzucana, kursory przycinane do 0..999.
inline UiSettings UiSettingsFromText(const std::string& text) {
  UiSettings u; const StateKV s = StateKV::FromText(text);
  if (ProjectNameOk(s.GetStr("last"))) u.lastProject = s.GetStr("last");
  for (const auto& kv : s.m) {
    if (kv.first.compare(0, 2, "c.") != 0 || kv.first.size() <= 2) continue;
    int v = std::atoi(kv.second.c_str()); v = v < 0 ? 0 : (v > 999 ? 999 : v);
    u.cursor[kv.first.substr(2)] = v;
  }
  return u;
}
