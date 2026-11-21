#include <cassert>
#include <cmath>
#include <cstdio>
#include "calc/Tachy.hpp"

static bool near(double a, double b, double eps = 1e-9) { return std::fabs(a - b) <= eps; }
static const TachyNoise NO_NOISE{0, 0, 0};

static void conversions() {
  assert(near(GonToDeg(100), 90) && near(DegToGon(90), 100));
  assert(near(NormDeg(-90), 270) && near(NormGon(-100), 300) && near(NormGon(400), 0));
}

static void azimuth_directions() {
  assert(near(TachyAzimuthDeg(0, 0, 1, 0), 0));
  assert(near(TachyAzimuthDeg(0, 0, 0, 1), 90));
  assert(near(TachyAzimuthDeg(0, 0, -1, 0), 180));
  assert(near(TachyAzimuthDeg(0, 0, 0, -1), 270));
}

static void polar_east_level() {
  TachyStation st{5500000.0, 6500000.0, 100.0, 1.5, 0.0, 0.0};
  TachyReading r{100.0, 100.0, 50.0};
  TachyResult a = TachyPolar(st, r, 1.5);
  assert(near(a.n, 5500000.0, 1e-6) && near(a.e, 6500050.0, 1e-6) && near(a.h, 100.0, 1e-9));
  assert(near(TachyPolar(st, r, 1.8).h, 99.7, 1e-9));              // za wysoko ustawione lustro => H nizsze
}

static void polar_sloped_north() {
  GnssRng rng(1);
  const double slope = std::sqrt(1000.0), pitch = std::atan(10.0 / 30.0) * 180.0 / 3.14159265358979323846;
  TachyReading r = TachyMeasure(0.0, pitch, 0.0, slope, NO_NOISE, rng);
  assert(near(r.vGon, 79.5167, 1e-3));
  TachyStation st{5500000.0, 6500000.0, 100.0, 1.5, 0.0, 0.0};
  TachyResult a = TachyPolar(st, r, 1.5);
  assert(near(a.n, 5500030.0, 1e-6) && near(a.e, 6500000.0, 1e-6) && near(a.h, 100.0 + 1.5 + 10.0 - 1.5, 1e-6));
}

static void orientation_zeroes_hz() {
  GnssRng rng(2);
  const double az0 = TachyOrient(37.5, NO_NOISE, rng);
  assert(near(az0, 37.5));
  assert(near(TachyMeasure(37.5, 0, az0, 10, NO_NOISE, rng).hzGon, 0));
  assert(near(TachyMeasure(127.5, 0, az0, 10, NO_NOISE, rng).hzGon, 100));
  assert(near(TachyMeasure(10.0, 0, 350.0, 10, NO_NOISE, rng).hzGon, DegToGon(20.0)));   // przez 0/360
}

static void extreme_pitch_stays_finite() {
  GnssRng rng(3);
  for (double pitch : {-90.0, 90.0, 120.0, -200.0}) {
    for (double az : {-720.0, 0.0, 359.999, 1000.0}) {
      TachyReading r = TachyMeasure(az, pitch, 10.0, 50.0, TachyNoise{}, rng);
      assert(std::isfinite(r.hzGon) && std::isfinite(r.vGon) && std::isfinite(r.slopeM));
      assert(r.hzGon >= 0 && r.hzGon < 400 && r.vGon >= 0 && r.vGon <= 200);
    }
  }
}

static void aim_error_and_limits() {
  TachyAim a = TachyAimError(0.0, 100.0, 0.0, 90.0, 0.0);          // cel 100 m na wschod, kamera na wschod, poziomo
  assert(a.ok && near(a.lateralM, 0.0, 1e-6) && near(a.distM, 100.0, 1e-6));
  assert(TachyAimError(-0.03, 100.0, 0.0, 90.0, 0.0).ok);          // 3 cm w poprzek na 100 m (tolerancja katowa 0,02 st. = 3,5 cm)
  assert(!TachyAimError(-0.05, 100.0, 0.0, 90.0, 0.0).ok);         // 5 cm w poprzek
  assert(!TachyAimError(0.0, 100.0, 0.10, 90.0, 0.0).ok);          // 10 cm wyzej
  assert(!TachyAimError(0.0, 2.0, 0.0, 90.0, 0.0).ok);             // za blisko
  assert(!TachyAimError(0.0, 301.0, 0.0, 90.0, 0.0).ok);           // za daleko
}

static void aim_tolerance_is_angular() {
  assert(TachyAimError(-0.02, 100.0, 0.0, 90.0, 0.0).ok && !TachyAimError(-0.036, 100.0, 0.0, 90.0, 0.0).ok);     // 100 m: 3,49 cm
  assert(TachyAimError(-0.01, 10.0, 0.0, 90.0, 0.0).ok && !TachyAimError(-0.02, 10.0, 0.0, 90.0, 0.0).ok);        // 10 m: dolny limit 1,5 cm
  assert(TachyAimError(-0.10, 299.0, 0.0, 90.0, 0.0).ok && !TachyAimError(-0.12, 299.0, 0.0, 90.0, 0.0).ok);      // 300 m: 10,5 cm
  assert(TachyAimError(0.0, 299.0, 0.10, 90.0, 0.0).ok && !TachyAimError(0.0, 299.0, 0.12, 90.0, 0.0).ok);        // to samo w pionie
  assert(TachyAimError(0.0, 10.0, 0.01, 90.0, 0.0).ok && !TachyAimError(0.0, 10.0, 0.02, 90.0, 0.0).ok);
}

static void angle_diff_and_circular_mean() {
  assert(near(AngleDiffDeg(10, 350), 20) && near(AngleDiffDeg(350, 10), -20) && near(AngleDiffDeg(90, 90), 0));
  assert(near(AngleDiffDeg(TachyCircularMeanDeg({350.0, 10.0}), 0.0), 0.0, 1e-9));            // 0 albo 360 - to samo (porownanie kolowe)
  assert(near(TachyCircularMeanDeg({10.0, 20.0}), 15.0, 1e-9) && near(AngleDiffDeg(TachyCircularMeanDeg({359.0, 1.0}), 0.0), 0.0, 1e-9));
  assert(near(TachyCircularMeanDeg({}), 0.0, 1e-12) && near(TachyCircularMeanDeg({90.0}), 90.0, 1e-9));
}

static void noise_statistics() {
  GnssRng rng(4); const int n = 20000;
  double sh = 0, shh = 0, sd = 0, sdd = 0;
  for (int i = 0; i < n; i++) {
    TachyReading r = TachyMeasure(45.0, 0.0, 0.0, 100.0, TachyNoise{}, rng);
    sh += r.hzGon; shh += r.hzGon * r.hzGon; sd += r.slopeM; sdd += r.slopeM * r.slopeM;
  }
  const double mh = sh / n, sdh = std::sqrt(shh / n - mh * mh), md = sd / n, sdd2 = std::sqrt(sdd / n - md * md);
  const double expHz = 5.0 / 3600.0 / 0.9, expD = 0.002 + 2e-6 * 100.0;   // 5 sekund lukowych w gradach, 2 mm + 2 ppm
  assert(sdh > 0.9 * expHz && sdh < 1.1 * expHz);
  assert(sdd2 > 0.9 * expD && sdd2 < 1.1 * expD);
}

int main() {
  conversions(); azimuth_directions(); polar_east_level(); polar_sloped_north(); orientation_zeroes_hz();
  extreme_pitch_stays_finite(); aim_error_and_limits(); aim_tolerance_is_angular(); angle_diff_and_circular_mean(); noise_statistics();
  std::puts("test_tachy OK");
  return 0;
}
