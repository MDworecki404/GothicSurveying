#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>
#include "calc/Bundle.hpp"

static double Terrain(double x, double y) { return 100.0 + 3.0 * std::sin(x / 20.0) * std::cos(y / 25.0); }
// Prosty generator liczb: stale ziarno, ten sam wynik wszedzie.
struct Rnd { GnssRng r{12345}; double N(double s) { return r.Normal() * s; } };

struct Scene { std::vector<BaCam> truth, prior; std::vector<BaPoint> pts, truthPts; std::vector<BaObs> obs; BaParams bp; };

// 4 x 3 zdjecia z wysokosci 150 m nad obszarem 100 x 100 m, 144 punkty terenu + 4 GCP w narozach i 4 punkty kontrolne; szum pikseli 0,5; a-priori: pozycje +-0,3 m, katy +-0,3 st. plus bias 0,2 st.
static Scene Make(double pixNoise = 0.5) {
  Scene s; Rnd rnd; s.bp.f = 180.0; s.bp.cx = 160.0; s.bp.cy = 120.0; s.bp.lever = Vec3{0, 0, -0.15}; s.bp.Rb = RotFromRpy(0, 0, 0);
  for (int i = 0; i < 4; i++) for (int j = 0; j < 3; j++) {
    BaCam c{}; c.p[0] = 12.5 + 25.0 * i + 12.5 * 0; c.p[1] = 16.0 + 34.0 * j; c.p[2] = 150.0; c.a[0] = rnd.N(0.5); c.a[1] = rnd.N(0.5); c.a[2] = 30.0 * j + rnd.N(2.0);
    s.truth.push_back(c); BaCam q = c; for (int k = 0; k < 3; k++) q.p[k] += rnd.N(0.3); for (int k = 0; k < 3; k++) q.a[k] += rnd.N(0.3) + 0.2; s.prior.push_back(q);
  }
  for (int i = 0; i < 12; i++) for (int j = 0; j < 12; j++) { const double x = 5 + i * 8.2, y = 5 + j * 8.2; s.truthPts.push_back({Vec3{x, y, Terrain(x, y)}, false}); }
  for (const double cx : {2.0, 98.0}) for (const double cy : {2.0, 98.0}) s.truthPts.push_back({Vec3{cx, cy, Terrain(cx, cy)}, true});                       // GCP (stale)
  for (const double cx : {30.0, 70.0}) for (const double cy : {30.0, 70.0}) s.truthPts.push_back({Vec3{cx + 0.3, cy - 0.2, Terrain(cx + 0.3, cy - 0.2)}, false});    // kontrolne: wolne, bez wiezow
  s.pts = s.truthPts; for (BaPoint& p : s.pts) if (!p.fixed) { p.x.x += rnd.N(1.0); p.x.y += rnd.N(1.0); p.x.z += rnd.N(1.0); }
  for (size_t c = 0; c < s.truth.size(); c++) for (size_t k = 0; k < s.truthPts.size(); k++) {
    TrajSample ps{}; ps.pos = {s.truth[c].p[0], s.truth[c].p[1], s.truth[c].p[2]}; ps.rollDeg = s.truth[c].a[0]; ps.pitchDeg = s.truth[c].a[1]; ps.yawDeg = s.truth[c].a[2];
    double u, v; if (!ProjectWorld(s.truthPts[k].x, ps, s.bp.Rb, s.bp.lever, s.bp.f, s.bp.cx, s.bp.cy, u, v)) continue;
    if (u < 0 || u > 320 || v < 0 || v > 240) continue;
    s.obs.push_back({(int)c, (int)k, u + rnd.N(pixNoise), v + rnd.N(pixNoise)});
  }
  s.bp.sigmaPx = pixNoise; s.bp.sigmaPos = 0.5; s.bp.sigmaAtt = 1.0; return s;
}

static double MeanPosErr(const std::vector<BaCam>& a, const std::vector<BaCam>& b) { double s = 0; for (size_t i = 0; i < a.size(); i++) s += std::sqrt(std::pow(a[i].p[0] - b[i].p[0], 2) + std::pow(a[i].p[1] - b[i].p[1], 2) + std::pow(a[i].p[2] - b[i].p[2], 2)); return s / a.size(); }

static void converges_and_checkpoints_accurate() {
  Scene s = Make(); std::vector<BaCam> cams = s.prior;
  assert(s.obs.size() > 400);                                                                                // kazdy punkt na kilku zdjeciach
  const BaResult r = BundleAdjust(cams, s.prior, s.pts, s.obs, s.bp);
  assert(r.ok && r.iters >= 2 && r.nObs == (int)s.obs.size() && r.nCams == 12);
  assert(r.rmsBefore > 2.0 && r.rmsAfter < 0.8 && r.rmsAfter > 0.3);                                         // z kilku pikseli do szumu pomiaru (0,5 px)
  assert(MeanPosErr(cams, s.truth) < 0.6 * MeanPosErr(s.prior, s.truth) && MeanPosErr(cams, s.truth) < 0.25);   // pozycje zdjec blizej prawdy niz a-priori z GNSS
  for (size_t k = 144 + 4; k < s.pts.size(); k++) {                                                          // punkty kontrolne (wolne, bez wiezow): blad 3D
    const Vec3 &a = s.pts[k].x, &t = s.truthPts[k].x; assert(std::sqrt(std::pow(a.x - t.x, 2) + std::pow(a.y - t.y, 2) + std::pow(a.z - t.z, 2)) < 0.3);
  }
  for (size_t k = 144; k < 148; k++) assert(s.pts[k].x.x == s.truthPts[k].x.x && s.pts[k].x.z == s.truthPts[k].x.z);   // GCP bez zmian
}

static void gcp_remove_systematic_gnss_error() {                                                               // systematyczny blad GNSS (0,5 m w E, -0,4 m w H): z osnowa blad pozycji zdjec jest mniejszy niz bez niej
  Scene s = Make(); for (BaCam& c : s.prior) { c.p[0] += 0.5; c.p[2] -= 0.4; }
  std::vector<BaCam> a = s.prior; std::vector<BaPoint> pa = s.pts; assert(BundleAdjust(a, s.prior, pa, s.obs, s.bp).ok);
  const double withGcp = MeanPosErr(a, s.truth);
  Scene n = Make(); for (BaCam& c : n.prior) { c.p[0] += 0.5; c.p[2] -= 0.4; } for (BaPoint& p : n.pts) p.fixed = false;      // te same dane, ale bez punktow stalych
  std::vector<BaCam> b = n.prior; std::vector<BaPoint> pb = n.pts; BundleAdjust(b, n.prior, pb, n.obs, n.bp);
  assert(withGcp < 0.8 * MeanPosErr(b, n.truth) && withGcp < MeanPosErr(s.prior, s.truth));                   // osnowa poprawia dopasowanie do prawdy (4 GCP w narozach: poprawa czesciowa, jak w praktyce)
}

static void rejects_degenerate() {
  Scene s = Make(); std::vector<BaCam> cams = s.prior; std::vector<BaObs> none;
  BaResult r = BundleAdjust(cams, s.prior, s.pts, none, s.bp); assert(!r.ok && !r.msg.empty());                // brak obserwacji
  std::vector<BaCam> empty; r = BundleAdjust(empty, empty, s.pts, s.obs, s.bp); assert(!r.ok && !r.msg.empty());   // brak zdjec
}

int main() {
  converges_and_checkpoints_accurate(); gcp_remove_systematic_gnss_error(); rejects_degenerate();
  std::puts("test_bundle OK"); return 0;
}
