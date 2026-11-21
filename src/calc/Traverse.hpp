// Obliczenia ciągu niwelacyjnego (bez Gothic API). Jednostki: odczyty/przewyższenia w mm, wysokości w m, celowe w cm.
#pragma once
#include <cmath>
#include <vector>

enum class TraverseKind { Anchored, Closed, Open };   // dowiązany, zamknięty, otwarty

// stadiaCm: odległość z dalmierza nitkowego [cm] (< 0 tylko w danych ręcznych/testach).
struct Stand { int backMm; int foreMm; double sightBackCm; double sightForeCm; double stadiaBackCm = -1, stadiaForeCm = -1; };

struct TraverseResult {
  std::vector<double> dhMm, vMm, dhAdjMm;          // po jednym na stanowisko
  std::vector<double> heightRawM, heightAdjM;      // n+1 punktów, [0] = start
  double sumBackMm = 0, sumForeMm = 0, sumDhMm = 0, fhMm = 0, lengthM = 0, fhDopMm = 0;
  bool hasClosure = false, withinTolerance = true;
};

// Zamknięty ignoruje hEndM (koniec = start). Otwarty: bez fh i poprawek.
inline TraverseResult ComputeTraverse(TraverseKind kind, const std::vector<Stand>& stands,
                                      double hStartM, double hEndM, double fhDopCoeffMm = 20.0) {
  TraverseResult r;
  const size_t n = stands.size();
  if (kind == TraverseKind::Closed) hEndM = hStartM;
  r.hasClosure = kind != TraverseKind::Open;
  double sightsCm = 0;
  for (const Stand& s : stands) {
    r.dhMm.push_back(s.backMm - s.foreMm);
    r.sumBackMm += s.backMm; r.sumForeMm += s.foreMm;
    sightsCm += s.sightBackCm + s.sightForeCm;
  }
  r.sumDhMm = r.sumBackMm - r.sumForeMm;
  r.lengthM = sightsCm / 100.0;
  if (r.hasClosure && n > 0) r.fhMm = r.sumDhMm - (hEndM - hStartM) * 1000.0;
  if (r.hasClosure) r.fhDopMm = fhDopCoeffMm * std::sqrt(r.lengthM / 1000.0);
  r.withinTolerance = !r.hasClosure || std::fabs(r.fhMm) <= r.fhDopMm + 1e-9;
  const double v = (r.hasClosure && n > 0) ? -r.fhMm / (double)n : 0.0;
  r.heightRawM.push_back(hStartM); r.heightAdjM.push_back(hStartM);
  for (size_t i = 0; i < n; i++) {
    r.vMm.push_back(v);
    r.dhAdjMm.push_back(r.dhMm[i] + v);
    r.heightRawM.push_back(r.heightRawM.back() + r.dhMm[i] / 1000.0);
    r.heightAdjM.push_back(r.heightAdjM.back() + r.dhAdjMm[i] / 1000.0);
  }
  return r;
}
