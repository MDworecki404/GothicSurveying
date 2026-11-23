// Obliczenia tachimetryczne (bez Gothic API): kąty w gradach, orientacja, odczyty z szumem, wynik biegunowy, warunek celowania.
// Azymut od północy (+Z = N) zgodnie z ruchem wskazówek zegara (+X = E). Odległości i współrzędne w metrach.
#pragma once
#include <cmath>
#include <vector>
#include "Gnss.hpp"

inline const double TACHY_PI = 3.14159265358979323846;

inline double GonToDeg(double g) { return g * 0.9; }
inline double DegToGon(double d) { return d / 0.9; }
inline double NormDeg(double d) {
  if (!std::isfinite(d)) return 0.0;
  d = std::fmod(d, 360.0); if (d < 0) d += 360.0;
  return d >= 360.0 ? 0.0 : d;
}
inline double NormGon(double g) {
  if (!std::isfinite(g)) return 0.0;
  g = std::fmod(g, 400.0); if (g < 0) g += 400.0;
  return g >= 400.0 ? 0.0 : g;
}

// Azymut z punktu 1 do punktu 2 [stopnie, 0-360).
inline double TachyAzimuthDeg(double n1, double e1, double n2, double e2) { return NormDeg(std::atan2(e2 - e1, n2 - n1) * 180.0 / TACHY_PI); }

struct TachyNoise { double angleArcsec = 5.0, distMm = 2.0, distPpm = 2.0; };
struct TachyReading { double hzGon, vGon, slopeM; };          // V = kąt zenitalny
struct TachyStation { double n, e, h; double instrH = 1.5; double azBackDeg = 0.0; double az0Deg = 0.0; };   // az0 = azymut kamery na nawiązanie

// Orientacja: gracz celuje w nawiązanie; zapamiętany azymut kamery z szumem kątowym.
inline double TachyOrient(double camAzDeg, const TachyNoise& nz, GnssRng& rng) {
  const double noise = nz.angleArcsec / 3600.0 * rng.Normal();
  return NormDeg((std::isfinite(camAzDeg) ? camAzDeg : 0.0) + noise);
}

inline TachyReading TachyMeasure(double camAzDeg, double camPitchDeg, double az0Deg, double trueSlopeM, const TachyNoise& nz, GnssRng& rng) {
  const double aNoise = nz.angleArcsec / 3600.0;
  const double az = std::isfinite(camAzDeg) ? camAzDeg : 0.0, pitch = std::isfinite(camPitchDeg) ? camPitchDeg : 0.0;
  const double hzDeg = NormDeg(az - az0Deg + aNoise * rng.Normal());
  double vDeg = 90.0 - pitch + aNoise * rng.Normal();
  vDeg = (std::min)(180.0, (std::max)(0.0, vDeg));
  const double d = (std::max)(0.0, trueSlopeM);
  const double slope = d + (nz.distMm / 1000.0 + nz.distPpm * 1e-6 * d) * rng.Normal();
  return {NormGon(DegToGon(hzDeg)), DegToGon(vDeg), (std::max)(0.0, slope)};
}

// Dwa polozenia lunety. Bledy instrumentu: kolimacji c (wplyw na Hz: c / sin V) i indeksu pionowego i (zero kola pionowego), oba w gradach.
// KL (face 1): Hz = Hz0 + c/sinV, V = V0 + i.  KP (face 2): Hz = Hz0 + 200 - c/sinV, V = 400 - V0 + i.
inline TachyReading TachyMeasureFace(double camAzDeg, double camPitchDeg, double az0Deg, double trueSlopeM, const TachyNoise& nz, GnssRng& rng,
                                     int face, double collGon, double indexGon) {
  TachyReading r = TachyMeasure(camAzDeg, camPitchDeg, az0Deg, trueSlopeM, nz, rng);
  const double sinV = (std::max)(0.05, std::sin(GonToDeg(r.vGon) * TACHY_PI / 180.0)), ch = collGon / sinV;
  if (face == 2) return {NormGon(r.hzGon + 200.0 - ch), (std::min)(400.0, (std::max)(0.0, 400.0 - r.vGon + indexGon)), r.slopeM};
  return {NormGon(r.hzGon + ch), (std::min)(400.0, (std::max)(0.0, r.vGon + indexGon)), r.slopeM};
}

struct TachyFaceMean { TachyReading r; double twoC = 0, index = 0; };   // twoC = Hz(KP) - 200 - Hz(KL) [gon] (= -2c/sinV), index = (V_KL + V_KP - 400) / 2 [gon]

// Srednia z KL i KP: usuwa kolimacje i indeks; odchylki pokazuja wielkosc bledow instrumentu.
inline TachyFaceMean TachyCombineFaces(const TachyReading& kl, const TachyReading& kp) {
  double d = NormGon(kp.hzGon - 200.0 - kl.hzGon + 200.0) - 200.0;           // do (-200, 200)
  TachyFaceMean m;
  m.r = {NormGon(kl.hzGon + d / 2.0), (kl.vGon - kp.vGon + 400.0) / 2.0, (kl.slopeM + kp.slopeM) / 2.0};
  m.twoC = d; m.index = (kl.vGon + kp.vGon - 400.0) / 2.0;
  return m;
}

struct TachyResult { double n, e, h, hDistM, azDeg; };

// Wynik biegunowy z odczytów: Az = azymut nawiązania + Hz, d = D sin V, H = Hs + ih + D cos V - wysokość lustra.
inline TachyResult TachyPolar(const TachyStation& st, const TachyReading& r, double prismSetM) {
  const double az = NormDeg(st.azBackDeg + GonToDeg(r.hzGon)), azRad = az * TACHY_PI / 180.0;
  const double vRad = GonToDeg(r.vGon) * TACHY_PI / 180.0;
  const double d = r.slopeM * std::sin(vRad);
  return {st.n + d * std::cos(azRad), st.e + d * std::sin(azRad), st.h + st.instrH + r.slopeM * std::cos(vRad) - prismSetM, d, az};
}

struct TachyAim { double lateralM, verticalM, distM; bool ok; };

// Różnica kątów a - b zawinięta do [-180, 180] stopni.
inline double AngleDiffDeg(double a, double b) {
  double d = std::fmod(a - b, 360.0);
  if (d > 180.0) d -= 360.0;
  if (d < -180.0) d += 360.0;
  return d;
}

// Średnia kołowa kierunków [stopnie] w [0,360); pusta lista daje 0.
inline double TachyCircularMeanDeg(const std::vector<double>& deg) {
  if (deg.empty()) return 0.0;
  double sx = 0, sy = 0;
  for (double d : deg) { sx += std::cos(d * TACHY_PI / 180.0); sy += std::sin(d * TACHY_PI / 180.0); }
  return NormDeg(std::atan2(sy, sx) * 180.0 / TACHY_PI);
}

// Błąd celowania: (dN, dE, dH) od oka lunety do środka celu. ok gdy kąt boczny i pionowy ≤ 0,02° (nie mniej niż 1,5 cm liniowo)
// oraz odległość skośna to 3-300 m.
inline TachyAim TachyAimError(double dN, double dE, double dH, double camAzDeg, double camPitchDeg) {
  const double hDist = std::hypot(dN, dE), slope = std::hypot(hDist, dH);
  const double diff = AngleDiffDeg(std::atan2(dE, dN) * 180.0 / TACHY_PI, camAzDeg);
  const double lateral = std::fabs(diff) < 90.0 ? hDist * std::sin(diff * TACHY_PI / 180.0) : hDist;   // za plecami: nigdy ok
  const double vdiff = (std::atan2(dH, hDist) * 180.0 / TACHY_PI - camPitchDeg) * TACHY_PI / 180.0;
  const double vertical = slope * std::sin(vdiff);
  const double tanTol = std::tan(0.02 * TACHY_PI / 180.0);
  const double tolLat = (std::max)(0.015, hDist * tanTol), tolVert = (std::max)(0.015, slope * tanTol);
  return {lateral, vertical, slope, std::fabs(lateral) <= tolLat && std::fabs(vertical) <= tolVert && slope >= 3.0 && slope <= 300.0};
}
