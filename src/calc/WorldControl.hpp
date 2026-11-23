// Wzorcowa osnowa geodezyjna swiatow Gothic II (bez Gothic API): dane z WorldControlData.hpp (assets/make_world_control.py).
// Wspolrzedne punktow wzorcowych sa "znane" (bez bledu pomiaru): n,e,h = polozenie znaku w ukladzie lokalnym wtyczki.
#pragma once
#include <string>
#include <vector>
#include "Control.hpp"
#include "Gnss.hpp"
#include "WorldControlData.hpp"

// Punkty wzorcowe danego swiata (klucz swiata jak w ControlWorldKey); pusta lista, gdy dla swiata nie ma zestawu.
inline std::vector<ControlPoint> WorldControlFor(const std::string& world) {
  std::vector<ControlPoint> out;
  const std::string key = ControlWorldKey(world);
  if (key.empty()) return out;
  for (int i = 0; i < WORLD_CONTROL_COUNT; i++) {
    const WorldControlRow& r = WORLD_CONTROL[i];
    if (key != r.world) continue;
    ControlPoint p; p.id = r.id; p.world = r.world;
    p.n = GNSS_N0 + r.n; p.e = GNSS_E0 + r.e; p.h = r.h + GNSS_H0;
    p.x = r.e * 100.0; p.y = r.h * 100.0; p.z = r.n * 100.0;          // E = x/100, N = z/100, H = y/100
    p.solution = std::string("osnowa wzorcowa: ") + r.place;
    out.push_back(p);
  }
  return out;
}

// Dopisuje do osnowy brakujace punkty wzorcowe swiata (po id i kluczu swiata); zwraca liczbe dopisanych. Nic nie usuwa.
inline int ControlMergeWorldSet(std::vector<ControlPoint>& pts, const std::string& world) {
  int added = 0;
  for (const ControlPoint& wp : WorldControlFor(world)) {
    bool exists = false;
    for (const ControlPoint& p : pts) if (p.id == wp.id && ControlWorldKey(p.world) == ControlWorldKey(wp.world)) { exists = true; break; }
    if (!exists) { pts.push_back(wp); added++; }
  }
  return added;
}
