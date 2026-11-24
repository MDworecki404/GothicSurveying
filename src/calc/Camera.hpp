// Kamera drona do fotogrametrii (bez Gothic API): presety kamer i rozdzielczosci, geometria zdjecia (GSD, pas, zakladki, baza), model otworkowy (promien piksela, rzutowanie punktu).
// Kamera patrzy w dol; obraz: u w prawo, v w dol, gorna krawedz obrazu = przod drona. Uklad ciala drona jak w Georef.hpp (x prawo, y przod, z gora); montaz kamery ma ten sam
// model boresightu co skaner: punkt P = X + R(imu) (R_b * d * s + lever), d = kierunek promienia w ukladzie kamery.
#pragma once
#include <cmath>
#include "calc/Georef.hpp"

struct CameraSpec { const char* name; double hfovDeg; };                     // poziomy kat widzenia (przez szerokosc obrazu)
inline const CameraSpec kCameras[3] = {{"szerokokatna 84 st.", 84.0}, {"standardowa 63 st.", 63.0}, {"tele 40 st.", 40.0}};
struct ImageRes { int w, h; const char* name; };                             // proporcje 4:3
inline const ImageRes kImageRes[3] = {{320, 240, "320 x 240 (szybko)"}, {640, 480, "640 x 480"}, {1024, 768, "1024 x 768 (wolno)"}};
inline const int kRealSensorWidthPx = 5472;                                  // odpowiednik rzeczywistej matrycy 20 MP do podania GSD

struct PhotoGeom { double focalPx, swathM, alongM, gsdM, gsdRealM, spacingM, baseM, intervalS; };
// agl: wysokosc nad terenem; zakladki podluzna (fwd) i poprzeczna (side) w %; speed: predkosc lotu [m/s].
inline PhotoGeom PhotoGeometry(const CameraSpec& cam, const ImageRes& res, double aglM, double fwdOverlapPct, double sideOverlapPct, double speedMps) {
  const double half = std::tan(cam.hfovDeg * 3.14159265358979323846 / 360.0); PhotoGeom g{};
  g.focalPx = (res.w / 2.0) / half; g.swathM = 2.0 * aglM * half; g.alongM = g.swathM * (double)res.h / res.w;
  g.gsdM = g.swathM / res.w; g.gsdRealM = g.swathM / kRealSensorWidthPx;
  g.spacingM = g.swathM * (1.0 - sideOverlapPct / 100.0); g.baseM = g.alongM * (1.0 - fwdOverlapPct / 100.0); g.intervalS = g.baseM / speedMps;
  return g;
}

// Kierunek promienia piksela (u, v) w ukladzie ciala drona (kamera w dol): x = (u-cx)/f, y = -(v-cy)/f, z = -1 (nieznormalizowany).
inline Vec3 PixelRayBody(double u, double v, double f, double cx, double cy) { return {(u - cx) / f, -(v - cy) / f, -1.0}; }
// Piksel punktu o wektorze d (uklad ciala, wzgledem kamery); false gdy punkt jest za kamera.
inline bool ProjectBody(const Vec3& d, double f, double cx, double cy, double& u, double& v) {
  if (d.z >= -1e-9) return false;
  u = cx + f * d.x / -d.z; v = cy - f * d.y / -d.z; return true;
}
inline Rot3 Transpose(const Rot3& r) { return Rot3{{r.m[0], r.m[3], r.m[6], r.m[1], r.m[4], r.m[7], r.m[2], r.m[5], r.m[8]}}; }
// Rzutowanie punktu swiata (E, N, H) na zdjecie wykonane z pozycji i katow `pose` przy montazu Rb i ramieniu lever; false gdy za kamera.
inline bool ProjectWorld(const Vec3& P, const TrajSample& pose, const Rot3& Rb, const Vec3& lever, double f, double cx, double cy, double& u, double& v) {
  const Rot3 R = RotFromRpy(pose.rollDeg, pose.pitchDeg, pose.yawDeg);
  const Vec3 w = Mul(Transpose(R), Vec3{P.x - pose.pos.x, P.y - pose.pos.y, P.z - pose.pos.z});
  return ProjectBody(Mul(Transpose(Rb), Vec3{w.x - lever.x, w.y - lever.y, w.z - lever.z}), f, cx, cy, u, v);
}
// Promien swiata przez piksel (u, v): poczatek = pozycja kamery, kierunek jednostkowy.
inline AlsRay PixelRay(const TrajSample& pose, const Rot3& Rb, const Vec3& lever, double u, double v, double f, double cx, double cy) {
  const Rot3 R = RotFromRpy(pose.rollDeg, pose.pitchDeg, pose.yawDeg); Vec3 d = PixelRayBody(u, v, f, cx, cy); const double l = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
  d = Mul(R, Mul(Rb, Vec3{d.x / l, d.y / l, d.z / l})); const Vec3 o = Mul(R, lever);
  return {{pose.pos.x + o.x, pose.pos.y + o.y, pose.pos.z + o.z}, d};
}
