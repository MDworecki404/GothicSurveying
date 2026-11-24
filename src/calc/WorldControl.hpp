// Wzorcowa osnowa geodezyjna swiatow Gothic II (bez Gothic API): dane z WorldControlData.hpp (assets/make_world_control.py).
// Wspolrzedne punktow wzorcowych sa "znane" (bez bledu pomiaru): n,e,h = polozenie znaku w ukladzie lokalnym wtyczki.
#pragma once
#include <string>
#include <vector>
#include "Control.hpp"
#include "Gnss.hpp"
#include "WorldControlData.hpp"

// Zakres mapy swiata (tekstura LVL_MAP_<KLUCZ>.TGA, polnoc u gory): lewy dolny rog (E0, N0) i bok kwadratu [m] w ukladzie lokalnym (bez N0/E0 wtyczki).
struct WorldMapView { double e0 = 0, n0 = 0, w = 1, h = 1; };   // lewy dolny rog (E0, N0) i wymiary mapy [m]; wygenerowane mapy sa kwadratowe (w = h)

inline bool WorldMapFor(const std::string& world, WorldMapView& out) {
  const std::string key = ControlWorldKey(world);
  for (int i = 0; i < WORLD_MAPS_COUNT; i++)
    if (key == WORLD_MAPS[i].world) { out.e0 = WORLD_MAPS[i].e0; out.n0 = WORLD_MAPS[i].n0; out.w = out.h = WORLD_MAPS[i].side; return true; }
  return false;
}

// Mapa z gry (rysunek na pergaminie MAP_<swiat>.TGA, 1024x768): granice swiata z Doc_SetLevelCoords(lewo, gora, prawo, dol) skryptow Gothica II [cm gry],
// potwierdzone na punktach osnowy (leza na drogach, farmach, w porcie i przy zamku) i proporcja 4:3. Mapa jest rysowana recznie, wiec niedokladna rzedu metrow.
struct GameMapInfo { const char* world; double left, top, right, bottom; };
inline const GameMapInfo GAME_MAPS[] = {
  {"NEWWORLD", -28000, 50500, 95500, -42500},
  {"OLDWORLD", -78500, 47500, 54000, -53000},
  {"ADDONWORLD", -47783, 36300, 43949, -32300},
};
inline bool GameMapFor(const std::string& world, WorldMapView& out) {
  const std::string key = ControlWorldKey(world);
  for (const GameMapInfo& g : GAME_MAPS)
    if (key == g.world) { out.e0 = g.left / 100.0; out.n0 = g.bottom / 100.0; out.w = (g.right - g.left) / 100.0; out.h = (g.top - g.bottom) / 100.0; return true; }
  return false;
}

// Polozenie (E, N) w metrach -> wspolrzedne tekstury u (w prawo) i v (w dol), 0..1.
inline void MapUnit(const WorldMapView& m, double e, double n, double& u, double& v) { u = (e - m.e0) / m.w; v = 1.0 - (n - m.n0) / m.h; }
// Odwrotnie: punkt mapy (u w prawo, v w dol, 0..1) -> polozenie (E, N) w metrach.
inline void MapFromUnit(const WorldMapView& m, double u, double v, double& e, double& n) { e = m.e0 + u * m.w; n = m.n0 + (1.0 - v) * m.h; }

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
