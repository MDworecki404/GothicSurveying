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
      assert(p.world == w && p.solution.rfind("osnowa wzorcowa: ", 0) == 0);
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

int main() {
  known_worlds_have_sets_and_unknown_do_not(); points_are_consistent(); merge_is_idempotent_and_keeps_user_points();
  std::puts("test_worldcontrol OK");
  return 0;
}
