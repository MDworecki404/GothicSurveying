#include <cassert>
#include <cmath>
#include <cstdio>
#include <set>
#include <string>
#include <vector>
#include "calc/WorldControl.hpp"

static bool near(double a, double b, double eps) { return std::fabs(a - b) <= eps; }

static void known_worlds_have_sets_and_unknown_do_not() {
  assert(WorldControlFor("NEWWORLD").size() >= 6);
  assert(WorldControlFor("newworld.zen").size() == WorldControlFor("NEWWORLD").size());           // klucz swiata, nie surowa nazwa
  assert(WorldControlFor("NewWorld\\NewWorld.zen").size() == WorldControlFor("NEWWORLD").size());
  assert(WorldControlFor("ADDONWORLD").size() >= 4 && WorldControlFor("OLDWORLD").size() >= 4);
  assert(WorldControlFor("").empty() && WorldControlFor("NOSUCHWORLD").empty() && WorldControlFor("DRAGONISLAND").empty());
}

static void points_are_consistent() {
  for (const char* w : {"NEWWORLD", "ADDONWORLD", "OLDWORLD"}) {
    const std::vector<ControlPoint> v = WorldControlFor(w);
    std::set<std::string> ids;
    for (const ControlPoint& p : v) {
      assert(ids.insert(p.id).second && p.id.size() == 3 && p.id[0] == 'G');                        // G01, G02, ... unikalne
      assert(p.world == w && p.solution == "osnowa wzorcowa");
      assert(near(p.n - GNSS_N0, p.z / 100.0, 1e-3) && near(p.e - GNSS_E0, p.x / 100.0, 1e-3) && near(p.h, p.y / 100.0, 1e-3));   // uklad jak w grze
      assert(std::isfinite(p.n) && std::isfinite(p.e) && std::isfinite(p.h));
    }
    for (size_t i = 0; i < v.size(); i++) for (size_t j = i + 1; j < v.size(); j++)
      assert(std::hypot(v[i].x - v[j].x, v[i].z - v[j].z) / 100.0 >= 40.0);                          // punkty nie leza na sobie (dogeszczanie dopuszcza min. 40 m)
  }
}

static void merge_is_idempotent_and_keeps_user_points() {
  ControlPoint mine; mine.id = "O1"; mine.world = "NEWWORLD"; mine.solution = "fix RTK";
  std::vector<ControlPoint> pts = {mine};
  const size_t count = WorldControlFor("NEWWORLD").size();
  assert(ControlMergeWorldSet(pts, "NEWWORLD") == (int)count && pts.size() == count + 1 && pts[0].id == "O1");
  assert(ControlMergeWorldSet(pts, "newworld.zen") == 0 && pts.size() == count + 1);                 // drugie zaladowanie nic nie dopisuje
  assert(ControlMergeWorldSet(pts, "DRAGONISLAND") == 0 && ControlMergeWorldSet(pts, "") == 0);
  const int added = ControlMergeWorldSet(pts, "ADDONWORLD");
  assert(added == (int)WorldControlFor("ADDONWORLD").size());
  assert(ControlNear(pts, "NEWWORLD", 0, 0, 0, 1e9).size() == count + 1);                            // punkty innych swiatow nie mieszaja sie
}

static void map_covers_all_points() {
  for (const char* w : {"NEWWORLD", "ADDONWORLD", "OLDWORLD"}) {
    WorldMapView m;
    assert(WorldMapFor(w, m) && m.w >= 200.0 && m.w == m.h);
    for (const ControlPoint& p : WorldControlFor(w)) {
      double u, v; MapUnit(m, p.e - GNSS_E0, p.n - GNSS_N0, u, v);
      assert(u > 0.02 && u < 0.98 && v > 0.02 && v < 0.98);                    // wszystkie punkty leza na mapie, z marginesem
    }
  }
  WorldMapView m; assert(WorldMapFor("newworld.zen", m) && !WorldMapFor("DRAGONISLAND", m) && !WorldMapFor("", m));
  WorldMapView t{100.0, 200.0, 400.0, 400.0}; double u, v;
  MapUnit(t, 100.0, 200.0, u, v); assert(u == 0.0 && v == 1.0);                // lewy dolny rog mapy: u = 0, v = 1 (polnoc u gory)
  MapUnit(t, 500.0, 600.0, u, v); assert(u == 1.0 && v == 0.0);
}

static void map_unit_inverse() {                                               // klik na mapie (u, v) -> polozenie w metrach, odwrotnosc MapUnit
  const WorldMapView t{100.0, 200.0, 400.0, 300.0}; double e, n, u, v;
  MapFromUnit(t, 0.0, 1.0, e, n); assert(e == 100.0 && n == 200.0);          // lewy dolny rog
  MapFromUnit(t, 1.0, 0.0, e, n); assert(e == 500.0 && n == 500.0);          // prawy gorny rog (mapa 400 x 300)
  MapFromUnit(t, 0.25, 0.5, e, n); MapUnit(t, e, n, u, v); assert(near(u, 0.25, 1e-12) && near(v, 0.5, 1e-12));
  MapFromUnit(t, 0.5, 0.5, e, n); assert(near(e, 300.0, 1e-9) && near(n, 350.0, 1e-9));
}

static void game_map_calibration() {                                            // mapa z gry: granice z Doc_SetLevelCoords, proporcja 4:3 i punkty osnowy w srodku mapy
  for (const char* w : {"NEWWORLD", "OLDWORLD", "ADDONWORLD"}) {
    WorldMapView m; assert(GameMapFor(w, m));
    assert(std::fabs(m.w / m.h - 4.0 / 3.0) < 0.02);                           // tekstura ma 1024 x 768
    for (const ControlPoint& p : WorldControlFor(w)) {
      double u, v; MapUnit(m, p.e - GNSS_E0, p.n - GNSS_N0, u, v);
      assert(u > 0.02 && u < 0.98 && v > 0.02 && v < 0.98);
    }
  }
  WorldMapView m; assert(GameMapFor("newworld.zen", m) && !GameMapFor("DRAGONISLAND", m));
  assert(near(m.e0, -280.0, 1e-9) && near(m.n0, -425.0, 1e-9) && near(m.w, 1235.0, 1e-9) && near(m.h, 930.0, 1e-9));
}

int main() {
  game_map_calibration();
  map_unit_inverse();
  known_worlds_have_sets_and_unknown_do_not(); points_are_consistent(); merge_is_idempotent_and_keeps_user_points(); map_covers_all_points();
  std::puts("test_worldcontrol OK");
  return 0;
}
