// Wzorcowa osnowa geodezyjna swiatow Gothic II (bez Gothic API): dane z WorldControlData.hpp (assets/make_world_control.py).
// Wspolrzedne punktow wzorcowych sa "znane" (bez bledu pomiaru): n,e,h = polozenie znaku w ukladzie lokalnym wtyczki.
#pragma once
#include <string>
#include <vector>
#include "Control.hpp"
#include "Gnss.hpp"
#include "WorldControlData.hpp"

// Zakres mapy swiata (tekstura LVL_MAP_<KLUCZ>.TGA, polnoc u gory): lewy dolny rog (E0, N0) i bok kwadratu [m] w ukladzie lokalnym (bez N0/E0 wtyczki).
struct WorldMapView { double e0 = 0, n0 = 0, side = 1; };

inline bool WorldMapFor(const std::string& world, WorldMapView& out) {
  const std::string key = ControlWorldKey(world);
  for (int i = 0; i < WORLD_MAPS_COUNT; i++)
    if (key == WORLD_MAPS[i].world) { out.e0 = WORLD_MAPS[i].e0; out.n0 = WORLD_MAPS[i].n0; out.side = WORLD_MAPS[i].side; return true; }
  return false;
}

// Polozenie (E, N) w metrach -> wspolrzedne tekstury u (w prawo) i v (w dol), 0..1.
inline void MapUnit(const WorldMapView& m, double e, double n, double& u, double& v) { u = (e - m.e0) / m.side; v = 1.0 - (n - m.n0) / m.side; }

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
    p.solution = "osnowa wzorcowa";       // r.place (nazwa lokacji z modelu) bywa bledna - nie pokazujemy jej
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
