#include <cassert>
#include <cmath>
#include <cstdio>
#include "calc/Libella.hpp"

static bool near(double a, double b, double eps = 1e-9) { return std::fabs(a - b) <= eps; }

static void disabled_is_always_level() {
  Libella l; l.Reset(0.3, 0.7);
  assert(l.Mag() == 0 && l.Within(0.0));
  l.Turn(0, 5.0); assert(l.Mag() == 0);
}

static void reset_range_and_within() {
  Libella l; l.enabled = true;
  for (double u1 : {0.0, 0.25, 0.5, 0.9}) for (double u2 : {0.0, 0.5, 0.99}) {
    l.Reset(u1, u2); assert(l.Mag() >= 9.0 - 1e-9 && l.Mag() <= 16.0 + 1e-9 && !l.Within(kLevelBubbleArcmin));
  }
}

static void screws_move_bubble_toward_them_and_can_level() {
  Libella l; l.enabled = true;
  l.Turn(0, 3.0); assert(near(l.x, 0.0, 1e-9) && near(l.y, 3.0, 1e-9));              // sruba 1 (gora): babelek do gory
  Libella m; m.enabled = true; m.Turn(1, 2.0); assert(m.x < 0 && m.y < 0);            // sruba 2 (lewy dol)
  Libella n; n.enabled = true; n.Turn(2, 2.0); assert(n.x > 0 && n.y < 0);            // sruba 3 (prawy dol)
  l.Reset(0.2, 0.5);                                                                   // dowolne pochylenie da sie zdjac dwiema srubami: rozklad na kierunki sruby 2 i 3
  const double tx = l.x, ty = l.y;
  double u2x, u2y, u3x, u3y; Libella::ScrewDir(1, u2x, u2y); Libella::ScrewDir(2, u3x, u3y);
  const double det = u2x * u3y - u2y * u3x, a = (-tx * u3y + ty * u3x) / det, b = (-u2x * ty + u2y * tx) / det;
  l.Turn(1, a); l.Turn(2, b);
  assert(l.Mag() < 1e-6 && l.Within(kTachyLevelArcmin));
}

static void clamp() {
  Libella l; l.enabled = true;
  for (int i = 0; i < 50; i++) l.Turn(0, 5.0);
  assert(l.y <= Libella::kMax + 1e-9);
}

int main() {
  disabled_is_always_level(); reset_range_and_within(); screws_move_bubble_toward_them_and_can_level(); clamp();
  std::puts("test_libella OK");
  return 0;
}
