// Rachunek ciagu poligonowego (bez Gothic API), wzory jak w przewodniku (docs/przewodnik-pomiary-geodezyjne.pdf, sekcja 5):
// katy poziome lewe w gradach, azymuty, odchylki katowa i liniowa, poprawki, wspolrzedne (N = X, E = Y). Trzy rodzaje: nawiazany obustronnie
// (Anchored), zamkniety (Closed), wiszacy (Open). Limity odchylek to zalozenie symulatora (z dokladnosci instrumentu), nie wartosci z instrukcji.
#pragma once
#include <cmath>
#include <string>
#include <vector>
#include "Tachy.hpp"
#include "Traverse.hpp"

inline double PolyWrapSigned(double g) { g = NormGon(g); return g > 200.0 ? g - 400.0 : g; }     // do (-200, 200]

inline double AzimuthGon(double n1, double e1, double n2, double e2) { return NormGon(std::atan2(e2 - e1, n2 - n1) * 200.0 / TACHY_PI); }

// Kat lewy beta = K_przod - K_tyl, usredniony z KL i KP (kierunki KP w ukladzie KL maja +200g, ktore sie skraca przy roznicy). twoC = beta_KP - beta_KL (rozbieznosc polozen).
inline double PolyAngleGon(double klBack, double klFore, double kpBack, double kpFore, double* twoCGon = nullptr) {
  const double b1 = NormGon(klFore - klBack), b2 = NormGon(kpFore - kpBack), d = PolyWrapSigned(b2 - b1);
  if (twoCGon) *twoCGon = d;
  return NormGon(b1 + d / 2.0);
}

struct PolyInput {
  TraverseKind kind = TraverseKind::Anchored;            // Anchored = nawiazany obustronnie, Closed = zamkniety, Open = wiszacy
  double nN1 = 0, eN1 = 0;                               // N1: nawiazanie poczatkowe (Anchored, Open) albo punkt orientacji (Closed)
  double nA = 0, eA = 0;                                 // A: pierwszy punkt ciagu (znany)
  double nB = 0, eB = 0, nN2 = 0, eN2 = 0;               // Anchored: ostatni punkt ciagu B i nawiazanie koncowe N2
  std::vector<double> betaGon;                           // katy lewe: Anchored n (A..B), Closed n (kolejno od A), Open m (A..przedostatni)
  double orientBetaGon = 0;                              // Closed: kat przy A od N1 do P1 (K_P1 - K_N1)
  std::vector<double> sideM;                             // boki poziome: Anchored n-1, Closed n (ostatni = P_{n-1} -> A), Open m
  double sigmaArcsec = 5.0, distMm = 2.0, distPpm = 2.0; // szum instrumentu (limity)
};

struct PolyResult {
  bool valid = false; std::string error; TraverseKind kind = TraverseKind::Anchored; int n = 0; bool clockwise = false;   // clockwise: tylko Closed
  double aStartGon = 0, aEndGon = 0;                     // azymuty nawiazania (Anchored), aStart takze Open
  bool hasAngleCheck = false, hasLinearCheck = false, angleOk = true, linearOk = true;
  double fBetaGon = 0, vBetaGon = 0, fBetaLimGon = 0;
  std::vector<double> betaCorrGon, azimuthGon, dX, dY, vx, vy, x, y;   // x, y: wspolrzedne punktow P0..Pn (P0 = A)
  double fx = 0, fy = 0, fd = 0, fdLim = 0, sumD = 0;
};

namespace polydetail {
inline double Rad(double gon) { return gon * TACHY_PI / 200.0; }
}

inline PolyResult PolygonCompute(const PolyInput& in) {
  using namespace polydetail;
  PolyResult r; r.kind = in.kind;
  auto fail = [&](const char* m) { r.valid = false; r.error = m; return r; };
  const size_t nb = in.betaGon.size(), ns = in.sideM.size();
  if (in.kind == TraverseKind::Anchored) {
    if (nb < 2) return fail("ciag nawiazany: potrzebne co najmniej 2 katy (A i B)");
    if (ns != nb - 1) return fail("ciag nawiazany: bokow musi byc o 1 mniej niz katow");
  } else if (in.kind == TraverseKind::Closed) {
    if (nb < 3) return fail("poligon: potrzebne co najmniej 3 katy");
    if (ns != nb) return fail("poligon: liczba bokow musi rownac sie liczbie katow");
  } else {
    if (nb < 1) return fail("ciag wiszacy: potrzebny co najmniej 1 kat");
    if (ns != nb) return fail("ciag wiszacy: liczba bokow musi rownac sie liczbie katow");
  }
  for (double d : in.sideM) if (!(d > 0.0) || !std::isfinite(d)) return fail("dlugosc boku musi byc dodatnia");
  std::vector<double> b; for (double v : in.betaGon) { if (!std::isfinite(v)) return fail("niepoprawny kat"); b.push_back(NormGon(v)); }

  const int n = (int)nb; r.n = n;
  const bool checked = in.kind != TraverseKind::Open;
  r.hasAngleCheck = r.hasLinearCheck = checked;
  const double mBeta = in.sigmaArcsec / 3600.0 * 10.0 / 9.0;              // gon: kat = roznica dwoch kierunkow, srednia z dwoch polozen -> sigma kierunku
  for (double s : in.sideM) r.sumD += s;

  // azymut pierwszego boku i kontrola katowa
  double sumBeta = 0; for (double v : b) sumBeta += v;
  double aFirst = 0;                                                       // azymut "boku" poprzedzajacego kat 0 (Anchored/Open: N1->A)
  if (in.kind == TraverseKind::Closed) {
    // kierunek obchodu: ze znaku pola poligonu zbudowanego z surowych katow (azymuty zalezne tylko od orientacji i katow)
    std::vector<double> ax(n), ay(n); double az = NormGon(AzimuthGon(in.nA, in.eA, in.nN1, in.eN1) + in.orientBetaGon);
    ax[0] = in.nA; ay[0] = in.eA;
    for (int i = 0; i + 1 < n; i++) {
      if (i > 0) az = NormGon(az + b[i] - 200.0);
      ax[i + 1] = ax[i] + in.sideM[i] * std::cos(Rad(az)); ay[i + 1] = ay[i] + in.sideM[i] * std::sin(Rad(az));
    }
    double area = 0; for (int i = 0; i < n; i++) { const int j = (i + 1) % n; area += ax[i] * ay[j] - ax[j] * ay[i]; }
    r.clockwise = area > 0.0;                                              // w ukladzie (N, E) dodatnie pole = obchod zgodny ze wskazowkami zegara
    r.fBetaGon = PolyWrapSigned(sumBeta - (n + (r.clockwise ? 2 : -2)) * 200.0);
  } else {
    aFirst = AzimuthGon(in.nN1, in.eN1, in.nA, in.eA); r.aStartGon = aFirst;
    if (in.kind == TraverseKind::Anchored) {
      r.aEndGon = AzimuthGon(in.nB, in.eB, in.nN2, in.eN2);
      r.fBetaGon = PolyWrapSigned(aFirst + sumBeta - n * 200.0 - r.aEndGon);
    }
  }
  if (checked) { r.vBetaGon = -r.fBetaGon / n; r.fBetaLimGon = 2.0 * mBeta * std::sqrt((double)n); r.angleOk = std::fabs(r.fBetaGon) <= r.fBetaLimGon + 1e-12; }
  for (double v : b) r.betaCorrGon.push_back(v + r.vBetaGon);

  // azymuty z katow poprawionych
  const int sides = (int)ns;
  double prev = in.kind == TraverseKind::Closed ? NormGon(AzimuthGon(in.nA, in.eA, in.nN1, in.eN1) + in.orientBetaGon) : aFirst;
  for (int i = 0; i < sides; i++) {
    double a = prev;
    if (in.kind == TraverseKind::Closed) { if (i > 0) a = NormGon(prev + r.betaCorrGon[i] - 200.0); }
    else a = NormGon(prev + r.betaCorrGon[i] - 200.0);
    r.azimuthGon.push_back(a); prev = a;
  }

  // przyrosty, odchylki liniowe i poprawki
  double sdx = 0, sdy = 0;
  for (int i = 0; i < sides; i++) {
    r.dX.push_back(in.sideM[i] * std::cos(Rad(r.azimuthGon[i]))); r.dY.push_back(in.sideM[i] * std::sin(Rad(r.azimuthGon[i])));
    sdx += r.dX.back(); sdy += r.dY.back();
  }
  if (in.kind == TraverseKind::Anchored) { r.fx = in.nA + sdx - in.nB; r.fy = in.eA + sdy - in.eB; }
  else if (in.kind == TraverseKind::Closed) { r.fx = sdx; r.fy = sdy; }
  r.fd = std::hypot(r.fx, r.fy);
  r.x.push_back(in.nA); r.y.push_back(in.eA);
  for (int i = 0; i < sides; i++) {
    const double vxi = checked ? -r.fx * in.sideM[i] / r.sumD : 0.0, vyi = checked ? -r.fy * in.sideM[i] / r.sumD : 0.0;
    r.vx.push_back(vxi); r.vy.push_back(vyi);
    r.x.push_back(r.x.back() + r.dX[i] + vxi); r.y.push_back(r.y.back() + r.dY[i] + vyi);
  }

  // limit liniowy: m_d,i z dwoch pomiarow boku; wplyw katow przez odleglosc stanowisk od punktu koncowego (Anchored) albo srodka ciezkosci (Closed)
  if (checked) {
    double refN = 0, refE = 0;
    if (in.kind == TraverseKind::Anchored) { refN = in.nB; refE = in.eB; }
    else { for (int i = 0; i < n; i++) { refN += r.x[i]; refE += r.y[i]; } refN /= n; refE /= n; }
    double sumMd2 = 0, sumR2 = 0;
    for (double s : in.sideM) { const double md = (in.distMm / 1000.0 + in.distPpm * 1e-6 * s) / std::sqrt(2.0); sumMd2 += md * md; }
    for (int i = 0; i < n; i++) { const double d = std::hypot(r.x[i] - refN, r.y[i] - refE); sumR2 += d * d; }
    const double mb = Rad(mBeta);
    r.fdLim = 2.0 * std::sqrt(sumMd2 + mb * mb * sumR2);
    r.linearOk = r.fd <= r.fdLim + 1e-12;
  }
  r.valid = true;
  return r;
}
