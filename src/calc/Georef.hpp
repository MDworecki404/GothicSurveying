// Georeferencja bezposrednia ALS (bez Gothic API): obroty, wiazka skanera liniowego, prawdziwa trajektoria drona,
// szum GNSS/IMU i ukryty boresight. Uklad ENU: x = wschod (E), y = polnoc (N), z = gora (H); pozycje w metrach siatki.
// Wzor: P = X_gnss + R(imu) * (R_boresight * r_skanera + lever).
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <vector>
#include "calc/Flight.hpp"
#include "calc/Gnss.hpp"

struct Vec3 { double x, y, z; };
struct Rot3 { double m[9]; };   // wiersze

inline Vec3 Mul(const Rot3& r, const Vec3& v) {
  return {r.m[0] * v.x + r.m[1] * v.y + r.m[2] * v.z, r.m[3] * v.x + r.m[4] * v.y + r.m[5] * v.z, r.m[6] * v.x + r.m[7] * v.y + r.m[8] * v.z};
}
inline Rot3 MulR(const Rot3& a, const Rot3& b) {
  Rot3 c{};
  for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) c.m[i * 3 + j] = a.m[i * 3] * b.m[j] + a.m[i * 3 + 1] * b.m[3 + j] + a.m[i * 3 + 2] * b.m[6 + j];
  return c;
}
// Uklad ciala: x = prawe skrzydlo, y = przod, z = gora. Yaw 0 = polnoc, rosnie ku wschodowi; pitch + = nos w gore; roll + = prawe skrzydlo w dol.
inline Rot3 RotFromRpy(double rollDeg, double pitchDeg, double yawDeg) {
  const double d = 3.14159265358979323846 / 180.0, cr = std::cos(rollDeg * d), sr = std::sin(rollDeg * d), cp = std::cos(pitchDeg * d), sp = std::sin(pitchDeg * d), cy = std::cos(yawDeg * d), sy = std::sin(yawDeg * d);
  const Rot3 Ry{{cy, sy, 0, -sy, cy, 0, 0, 0, 1}}, Rp{{1, 0, 0, 0, cp, -sp, 0, sp, cp}}, Rr{{cr, 0, sr, 0, 1, 0, -sr, 0, cr}};
  return MulR(Ry, MulR(Rp, Rr));
}
// Uklad skanera: wiazka w plaszczyznie poprzecznej do toru; theta = 0 nadir, + w prawo.
inline Vec3 LineBeamDir(double thetaDeg) { const double t = thetaDeg * 3.14159265358979323846 / 180.0; return {std::sin(t), 0, -std::cos(t)}; }
// Lustro pilokszaltowe: kolejna linia co 1/lineHz, od -fov/2 do +fov/2.
inline double MirrorThetaDeg(double tSec, const SensorSpec& s) {
  const double ph = tSec * s.lineHz; return s.fovDeg * (ph - std::floor(ph) - 0.5);
}

struct TrajSample { double t = 0; Vec3 pos{0, 0, 0}; double rollDeg = 0, pitchDeg = 0, yawDeg = 0; bool scanning = false; int strip = -1; };

struct ErrorProfile { const char* name; double gnssHSigmaM, gnssVSigmaM, driftMPerMin, imuSigmaDeg, imuBiasDeg, boresightDeg; };
inline const ErrorProfile kAlsProfiles[3] = {
  {"RTK dobry", 0.02, 0.03, 0.02, 0.01, 0.01, 0.05},
  {"PPK", 0.05, 0.08, 0.1, 0.02, 0.02, 0.1},
  {"slaby", 0.3, 0.5, 0.6, 0.05, 0.05, 0.2},
};

// Prawdziwa trajektoria: wznios 3 m/s, tranzyt, zakret 5 s (zawis), szereg (scanning), powrot, ladowanie. Wysokosc: teren + AGL (wygladzony), albo dom + AGL.
class FlightPath {
 public:
  using GroundFn = std::function<double(double n, double e)>;
  FlightPath(const FlightPlan& plan, GroundFn ground, double aglM, double speedMps, bool follow, double homeGroundH, PlanPt home)
      : ground_(std::move(ground)), agl_(aglM), follow_(follow), homeGround_(homeGroundH) {
    double t = 0; PlanPt cur = home;
    auto add = [&](PlanPt a, double za, PlanPt b, double zb, double dur, bool scan, int strip, bool onTheFly) {
      if (dur <= 0) return;
      Seg s{t, dur, a, za, b, zb, Course(a, b), scan, strip, onTheFly}; segs_.push_back(s); t += dur;
    };
    const double hz = Alt(home.n, home.e);
    const PlanPt first = plan.strips.empty() ? home : plan.strips[0].a;
    const double c0 = Course(home, first);
    add(home, homeGround_, home, hz, std::fabs(hz - homeGround_) / 3.0, false, -1, false); if (!segs_.empty()) segs_.back().yaw = c0;
    double zc = hz;
    for (const Strip& s : plan.strips) {
      const double za = Alt(s.a.n, s.a.e), zb = Alt(s.b.n, s.b.e);
      add(cur, zc, s.a, za, std::hypot(s.a.n - cur.n, s.a.e - cur.e) / speedMps, false, -1, false);
      const double y = Course(s.a, s.b);
      { Seg h{t, 5.0, s.a, za, s.a, za, y, false, -1, false}; segs_.push_back(h); t += 5.0; }
      add(s.a, za, s.b, zb, s.lengthM / speedMps, true, s.id, follow_);
      cur = s.b; zc = zb;
    }
    add(cur, zc, home, hz, std::hypot(home.n - cur.n, home.e - cur.e) / speedMps, false, -1, false);
    add(home, hz, home, homeGround_, std::fabs(hz - homeGround_) / 3.0, false, -1, false);
    total_ = t;
  }
  double Duration() const { return total_; }
  double FirstScanTime() const { for (const Seg& s : segs_) if (s.scan) return s.t0; return 0; }
  TrajSample At(double t) const {
    TrajSample r; r.t = t; const double pi2 = 2.0 * 3.14159265358979323846;
    r.rollDeg = 1.0 * std::sin(pi2 * 0.2 * t); r.pitchDeg = 1.0 * std::sin(pi2 * 0.2 * t + 1.0);
    if (segs_.empty()) return r;
    const double tt = (std::max)(0.0, (std::min)(total_, t));
    size_t i = 0; while (i + 1 < segs_.size() && tt >= segs_[i].t0 + segs_[i].dur) i++;
    const Seg& s = segs_[i]; const double f = s.dur > 0 ? (std::min)(1.0, (tt - s.t0) / s.dur) : 0.0;
    const double n = s.a.n + (s.b.n - s.a.n) * f, e = s.a.e + (s.b.e - s.a.e) * f;
    r.pos = {e, n, s.onTheFly ? Alt(n, e) : s.za + (s.zb - s.za) * f};
    r.yawDeg = s.yaw; r.scanning = s.scan; r.strip = s.scan ? s.strip : -1;
    return r;
  }
 private:
  struct Seg { double t0, dur; PlanPt a; double za; PlanPt b; double zb; double yaw; bool scan; int strip; bool onTheFly; };
  static double Course(PlanPt a, PlanPt b) {
    if (std::hypot(b.n - a.n, b.e - a.e) < 1e-9) return 0;
    double d = std::atan2(b.e - a.e, b.n - a.n) * 180.0 / 3.14159265358979323846; if (d < 0) d += 360.0; return d;
  }
  double Alt(double n, double e) const {                           // wysokosc przelotu: wygladzony teren (krzyz 5 probek, +-8 m) + AGL
    if (!follow_) return homeGround_ + agl_;
    return (ground_(n, e) + ground_(n + 8, e) + ground_(n - 8, e) + ground_(n, e + 8) + ground_(n, e - 8)) / 5.0 + agl_;
  }
  GroundFn ground_; double agl_; bool follow_; double homeGround_; double total_ = 0; std::vector<Seg> segs_;
};

// Szum estymaty trajektorii: bialy + dryf (random walk) GNSS, bialy + bias IMU; prawdziwy boresight = nominalny (jednostkowy) * ukryty blad.
class TrajectoryNoise {
 public:
  TrajectoryNoise(uint32_t seed, const ErrorProfile& p) : rng_(seed), p_(p) {
    bias_[0] = (rng_.Uniform() * 2 - 1) * p.imuBiasDeg; bias_[1] = (rng_.Uniform() * 2 - 1) * p.imuBiasDeg; bias_[2] = (rng_.Uniform() * 2 - 1) * p.imuBiasDeg;
    bore_[0] = (rng_.Uniform() * 2 - 1) * p.boresightDeg; bore_[1] = (rng_.Uniform() * 2 - 1) * p.boresightDeg; bore_[2] = (rng_.Uniform() * 2 - 1) * p.boresightDeg;
  }
  TrajSample Estimate(const TrajSample& truth) {
    const double dt = first_ ? 0.0 : (std::max)(0.0, truth.t - last_); first_ = false; last_ = truth.t;
    const double step = p_.driftMPerMin * std::sqrt(dt / 60.0);   // random walk: sigma po minucie = driftMPerMin
    for (int k = 0; k < 3; k++) drift_[k] += rng_.Normal() * step;
    TrajSample e = truth;
    e.pos.x += drift_[0] + rng_.Normal() * p_.gnssHSigmaM; e.pos.y += drift_[1] + rng_.Normal() * p_.gnssHSigmaM; e.pos.z += drift_[2] + rng_.Normal() * p_.gnssVSigmaM;
    e.rollDeg += bias_[0] + rng_.Normal() * p_.imuSigmaDeg; e.pitchDeg += bias_[1] + rng_.Normal() * p_.imuSigmaDeg; e.yawDeg += bias_[2] + rng_.Normal() * p_.imuSigmaDeg;
    return e;
  }
  Rot3 TrueBoresight() const { return RotFromRpy(bore_[0], bore_[1], bore_[2]); }
  void BoresightDeg(double& r, double& p, double& y) const { r = bore_[0]; p = bore_[1]; y = bore_[2]; }
 private:
  GnssRng rng_; ErrorProfile p_; double bias_[3], bore_[3], drift_[3] = {0, 0, 0}, last_ = 0; bool first_ = true;
};

struct AlsRay { Vec3 origin, dir; };
// Promien rzeczywisty (z prawdziwej pozycji, z prawdziwym montazem skanera).
inline AlsRay TrueRay(const TrajSample& truth, const Rot3& boresightTrue, const Vec3& lever, double thetaDeg) {
  const Rot3 R = RotFromRpy(truth.rollDeg, truth.pitchDeg, truth.yawDeg); const Vec3 l = Mul(R, lever);
  return {{truth.pos.x + l.x, truth.pos.y + l.y, truth.pos.z + l.z}, Mul(R, Mul(boresightTrue, LineBeamDir(thetaDeg)))};
}
// Punkt liczony z estymaty trajektorii i nominalnego montazu: P = X + R(imu) * (R_bore * r + lever).
inline Vec3 GeorefPoint(const TrajSample& est, const Rot3& boresightNominal, const Vec3& lever, double thetaDeg, double rangeM) {
  const Rot3 R = RotFromRpy(est.rollDeg, est.pitchDeg, est.yawDeg); const Vec3 b = LineBeamDir(thetaDeg);
  const Vec3 r = Mul(boresightNominal, {b.x * rangeM, b.y * rangeM, b.z * rangeM}), v = Mul(R, {r.x + lever.x, r.y + lever.y, r.z + lever.z});
  return {est.pos.x + v.x, est.pos.y + v.y, est.pos.z + v.z};
}
