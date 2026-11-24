#include <cassert>
#include <cmath>
#include <cstdio>
#include "calc/Camera.hpp"

static bool near(double a, double b, double eps = 1e-9) { return std::fabs(a - b) <= eps; }
static const double PI = 3.14159265358979323846;

static void geometry_values() {
  const PhotoGeom g = PhotoGeometry(kCameras[0], kImageRes[0], 50.0, 80.0, 60.0, 5.0);                  // 84 st., 320x240, AGL 50 m, zakladki 80% / 60%, 5 m/s
  const double sw = 2 * 50.0 * std::tan(42.0 * PI / 180.0);
  assert(near(g.focalPx, 160.0 / std::tan(42.0 * PI / 180.0), 1e-9));
  assert(near(g.swathM, sw, 1e-9) && near(g.alongM, sw * 0.75, 1e-9));                                    // 4:3: wzdluz toru 3/4 szerokosci pasa
  assert(near(g.gsdM, sw / 320.0, 1e-9) && near(g.gsdRealM, sw / 5472.0, 1e-9));                         // renderowane i odpowiednik matrycy 20 MP (5472 px)
  assert(near(g.spacingM, sw * 0.4, 1e-9) && near(g.baseM, sw * 0.75 * 0.2, 1e-9) && near(g.intervalS, g.baseM / 5.0, 1e-9));
  const PhotoGeom t = PhotoGeometry(kCameras[2], kImageRes[1], 100.0, 70.0, 50.0, 8.0);                  // tele 40 st., 640x480: wezszy pas, wiekszy odstep czasowy
  assert(t.swathM < PhotoGeometry(kCameras[0], kImageRes[1], 100.0, 70.0, 50.0, 8.0).swathM && t.focalPx > 640.0 / 2 / std::tan(42.0 * PI / 180.0));
}

static void pixel_rays_and_projection() {
  const double f = 200.0, cx = 160.0, cy = 120.0;
  const Vec3 c = PixelRayBody(cx, cy, f, cx, cy);                                                         // srodek obrazu = prosto w dol
  assert(near(c.x, 0, 1e-12) && near(c.y, 0, 1e-12) && near(c.z, -1.0, 1e-12));
  const Vec3 top = PixelRayBody(cx, 0, f, cx, cy); assert(top.y > 0 && near(top.x, 0, 1e-12));              // gorna krawedz = przod drona
  const Vec3 right = PixelRayBody(320, cy, f, cx, cy); assert(right.x > 0 && near(right.y, 0, 1e-12));      // prawa krawedz = prawe skrzydlo
  for (double u : {10.0, 160.0, 300.0}) for (double v : {5.0, 120.0, 230.0}) {                              // rzutowanie jest odwrotnoscia promienia
    const Vec3 d = PixelRayBody(u, v, f, cx, cy); double pu, pv; assert(ProjectBody({d.x * 50, d.y * 50, d.z * 50}, f, cx, cy, pu, pv) && near(pu, u, 1e-9) && near(pv, v, 1e-9));
  }
  double pu, pv; assert(!ProjectBody({0, 0, 5.0}, f, cx, cy, pu, pv));                                      // punkt za kamera (nad dronem) nie jest widoczny
}

static void world_projection_roundtrip() {                                                                // punkt terenu -> piksel -> promien w swiecie trafia w ten sam punkt
  TrajSample s{}; s.pos = {10, 20, 150}; s.rollDeg = 2; s.pitchDeg = -3; s.yawDeg = 40;
  const Vec3 lever{0, 0, -0.15}; const Rot3 Rb = RotFromRpy(0.1, -0.05, 0.2); const double f = 180.0, cx = 160.0, cy = 120.0;
  const Vec3 P{22.0, 31.0, 100.0}; double u, v; assert(ProjectWorld(P, s, Rb, lever, f, cx, cy, u, v));
  const AlsRay ray = PixelRay(s, Rb, lever, u, v, f, cx, cy);
  const double t = (P.z - ray.origin.z) / ray.dir.z;                                                       // przeciecie z plaszczyzna z = 100
  assert(near(ray.origin.x + t * ray.dir.x, P.x, 1e-6) && near(ray.origin.y + t * ray.dir.y, P.y, 1e-6));
  assert(!ProjectWorld(Vec3{10, 20, 200.0}, s, Rb, lever, f, cx, cy, u, v));                                // punkt nad dronem
  assert(!ProjectWorld(Vec3{500, 20, 100.0}, s, Rb, lever, f, cx, cy, u, v) || u < 0 || u > 320 || v < 0 || v > 240);   // daleko poza kadrem: poza obrazem
}

int main() {
  geometry_values(); pixel_rays_and_projection(); world_projection_roundtrip();
  std::puts("test_camera OK"); return 0;
}
