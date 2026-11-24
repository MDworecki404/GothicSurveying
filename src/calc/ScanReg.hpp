// Rejestracja skanow na kulach referencyjnych (bez Gothic API): grupowanie jasnych punktow (kule sa retrorefleksyjne, intensywnosc 255), dopasowanie kuli
// metoda najmniejszych kwadratow i wyznaczenie stanowiska skanera (obrot wokol pionu + przesuniecie) z kul o znanych wspolrzednych.
// Skaner jest wypoziomowany, wiec poziomo to obrot i przesuniecie (Procrustes, skala 1), a wysokosc to samo przesuniecie pionowe.
#pragma once
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

struct P3 { double x = 0, y = 0, z = 0; };
inline const double kScanTargetRadiusM = 0.145;           // kula referencyjna o srednicy 29 cm
inline const double kScanTargetUpM = 0.5;                 // srodek kuli nad znakiem punktu osnowy [m]

// Grupuje punkty: kazdy trafia do pierwszej grupy ze srodkiem bli zej niz radiusM, inaczej tworzy nowa. Zwraca grupy z co najmniej minPts punktow.
inline std::vector<std::vector<P3>> ScanClusterBright(const std::vector<P3>& pts, double radiusM, size_t minPts) {
  struct C { std::vector<P3> p; double sx = 0, sy = 0, sz = 0; };
  std::vector<C> cl;
  for (const P3& q : pts) {
    C* hit = nullptr;
    for (C& c : cl) {
      const double n = (double)c.p.size();
      if (std::hypot(std::hypot(c.sx / n - q.x, c.sy / n - q.y), c.sz / n - q.z) <= radiusM) { hit = &c; break; }
    }
    if (!hit) { cl.push_back(C()); hit = &cl.back(); }
    hit->p.push_back(q); hit->sx += q.x; hit->sy += q.y; hit->sz += q.z;
  }
  std::vector<std::vector<P3>> out;
  for (C& c : cl) if (c.p.size() >= minPts) out.push_back(std::move(c.p));
  return out;
}

// Kula z punktow (czesc powierzchni, widoczna od strony skanera): liniowe najmniejsze kwadraty x^2+y^2+z^2 = 2ax + 2by + 2cz + d. false gdy zbyt malo punktow,
// zdegenerowany uklad albo promien rozni sie od oczekiwanego o ponad tolRadiusM.
inline bool ScanFitSphere(const std::vector<P3>& p, double radius, double tolRadiusM, P3& center, double& rmsM) {
  const size_t n = p.size();
  if (n < 8) return false;
  P3 m; for (const P3& q : p) { m.x += q.x; m.y += q.y; m.z += q.z; } m.x /= n; m.y /= n; m.z /= n;      // srodkowanie poprawia uwarunkowanie
  double A[4][5] = {};
  for (const P3& q : p) {
    const double x = q.x - m.x, y = q.y - m.y, z = q.z - m.z, row[4] = {2 * x, 2 * y, 2 * z, 1.0}, rhs = x * x + y * y + z * z;
    for (int i = 0; i < 4; i++) { for (int j = 0; j < 4; j++) A[i][j] += row[i] * row[j]; A[i][4] += row[i] * rhs; }
  }
  for (int c = 0; c < 4; c++) {                                          // eliminacja Gaussa z wyborem elementu glownego
    int piv = c; for (int r = c + 1; r < 4; r++) if (std::fabs(A[r][c]) > std::fabs(A[piv][c])) piv = r;
    if (std::fabs(A[piv][c]) < 1e-12) return false;
    for (int j = 0; j < 5; j++) std::swap(A[c][j], A[piv][j]);
    for (int r = 0; r < 4; r++) if (r != c) { const double f = A[r][c] / A[c][c]; for (int j = c; j < 5; j++) A[r][j] -= f * A[c][j]; }
  }
  const double a = A[0][4] / A[0][0], b = A[1][4] / A[1][1], c = A[2][4] / A[2][2], d = A[3][4] / A[3][3];
  const double r2 = d + a * a + b * b + c * c;
  if (r2 <= 0) return false;
  const double r = std::sqrt(r2);
  if (std::fabs(r - radius) > tolRadiusM) return false;
  center = {m.x + a, m.y + b, m.z + c};
  double q = 0; for (const P3& s : p) { const double e = std::hypot(std::hypot(s.x - center.x, s.y - center.y), s.z - center.z) - r; q += e * e; }
  rmsM = std::sqrt(q / (double)n);
  return true;
}

struct ScanRef { std::string id; double n = 0, e = 0, h = 0; };         // kula o znanych wspolrzednych srodka (siatka)
struct ScanReg {
  bool ok = false; std::string msg;
  double n = 0, e = 0, h = 0, rotDeg = 0, rmsM = 0;                      // srodek skanera w siatce, obrot lokalnego ukladu (az prawdziwy = az lokalny + rot), rms dopasowania
  std::vector<std::string> ids; std::vector<double> residM;              // dopasowane kule (kolejnosc jak wykryte) i reszty
};

namespace scanregdetail {
struct Fit { bool ok = false; double n = 0, e = 0, h = 0, rotDeg = 0, rms = 1e9; std::vector<double> resid; std::vector<size_t> idx; };
// Dopasowanie obrotu + przesuniecia: a_i = (y, x) lokalne (polnoc, wschod), b_i = (N, E) siatki; wysokosc: stale przesuniecie.
inline Fit Procrustes(const std::vector<P3>& loc, const std::vector<const ScanRef*>& ref) {
  Fit f; const size_t k = loc.size();
  double cn = 0, ce = 0, cz = 0, bn = 0, be = 0, bh = 0;
  for (size_t i = 0; i < k; i++) { cn += loc[i].y; ce += loc[i].x; cz += loc[i].z; bn += ref[i]->n; be += ref[i]->e; bh += ref[i]->h; }
  cn /= k; ce /= k; cz /= k; bn /= k; be /= k; bh /= k;
  double sc = 0, sd = 0;
  for (size_t i = 0; i < k; i++) {
    const double pn = loc[i].y - cn, pe = loc[i].x - ce, qn = ref[i]->n - bn, qe = ref[i]->e - be;
    sc += pn * qe - pe * qn; sd += pn * qn + pe * qe;
  }
  if (std::fabs(sc) + std::fabs(sd) < 1e-9) return f;
  const double th = std::atan2(sc, sd), c = std::cos(th), s = std::sin(th);
  f.n = bn - (cn * c - ce * s); f.e = be - (cn * s + ce * c); f.h = bh - cz;
  f.rotDeg = std::fmod(th * 180.0 / 3.14159265358979323846 + 360.0, 360.0);
  double q = 0;
  for (size_t i = 0; i < k; i++) {
    const double fn = f.n + loc[i].y * c - loc[i].x * s, fe = f.e + loc[i].y * s + loc[i].x * c, fh = f.h + loc[i].z;
    const double r = std::sqrt((ref[i]->n - fn) * (ref[i]->n - fn) + (ref[i]->e - fe) * (ref[i]->e - fe) + (ref[i]->h - fh) * (ref[i]->h - fh));
    f.resid.push_back(r); q += r * r;
  }
  f.rms = std::sqrt(q / (double)k); f.ok = true;
  return f;
}
inline void Search(const std::vector<P3>& det, const std::vector<ScanRef>& refs, size_t i, std::vector<size_t>& cur, std::vector<char>& used,
                   Fit& best, Fit& second) {
  if (i == det.size()) {
    std::vector<const ScanRef*> p; for (size_t k : cur) p.push_back(&refs[k]);
    Fit f = Procrustes(det, p); if (!f.ok) return;
    f.idx = cur;
    if (f.rms < best.rms) { second = best; best = f; } else if (f.rms < second.rms) second = f;
    return;
  }
  for (size_t j = 0; j < refs.size(); j++) {
    if (used[j]) continue;
    used[j] = 1; cur.push_back(j); Search(det, refs, i + 1, cur, used, best, second); cur.pop_back(); used[j] = 0;
  }
}
}  // namespace scanregdetail

// Dopasowuje wykryte srodki kul (uklad lokalny skanera) do kul o znanych wspolrzednych: probuje wszystkich przyporzadkowan i bierze najlepsze (najmniejszy rms);
// wynik niejednoznaczny (drugie przyporzadkowanie niewiele gorsze) odrzuca. Potrzebne co najmniej 3 kule.
inline ScanReg ScanRegister(const std::vector<P3>& det, const std::vector<ScanRef>& refs, double maxRmsM = 0.05) {
  ScanReg r;
  if (det.size() < 3) { r.msg = "wykryto " + std::to_string(det.size()) + " kul, potrzeba co najmniej 3"; return r; }
  if (refs.size() < det.size() || det.size() > 6) { r.msg = "za malo kul referencyjnych w robocie"; return r; }
  scanregdetail::Fit best, second; std::vector<size_t> cur; std::vector<char> used(refs.size(), 0);
  scanregdetail::Search(det, refs, 0, cur, used, best, second);
  if (!best.ok) { r.msg = "uklad kul zdegenerowany"; return r; }
  if (best.rms > maxRmsM) { r.msg = "kule nie pasuja do wspolrzednych (rms " + std::to_string((int)(best.rms * 1000)) + " mm)"; return r; }
  if (second.rms < 2.0 * best.rms + 0.005) { r.msg = "przyporzadkowanie kul niejednoznaczne (ustaw kule asymetrycznie)"; return r; }
  r.ok = true; r.n = best.n; r.e = best.e; r.h = best.h; r.rotDeg = best.rotDeg; r.rmsM = best.rms; r.residM = best.resid;
  for (size_t k : best.idx) r.ids.push_back(refs[k].id);
  return r;
}

// Okna doskanowania kul: z jasnych punktow przegladowego skanu (kierunek az/v i odleglosc) wybiera obszary katowe do gestego skanowania.
struct ScanBrightHit { double azDeg, vDeg, distM; };
struct ScanWindow { double azCenter, vCenter, halfDeg, stepDeg; };
inline std::vector<ScanWindow> ScanTargetWindows(const std::vector<ScanBrightHit>& hits) {
  struct G { double saz = 0, sv = 0, sd = 0; int n = 0; };
  std::vector<G> gs;
  for (const ScanBrightHit& h : hits) {
    G* g = nullptr;
    for (G& c : gs) {
      const double da = std::fabs(std::fmod(h.azDeg - c.saz / c.n + 1260.0, 360.0) - 180.0);
      if (da < 4.0 && std::fabs(h.vDeg - c.sv / c.n) < 4.0 && std::fabs(h.distM - c.sd / c.n) < 1.0) { g = &c; break; }
    }
    if (!g) { gs.push_back(G()); g = &gs.back(); }
    double az = h.azDeg; if (g->n > 0) { const double mean = g->saz / g->n; while (az - mean > 180.0) az -= 360.0; while (az - mean < -180.0) az += 360.0; }
    g->saz += az; g->sv += h.vDeg; g->sd += h.distM; g->n++;
  }
  std::vector<ScanWindow> out;
  for (const G& g : gs) {
    const double dist = g.sd / g.n, half = 0.75 + std::atan2(kScanTargetRadiusM + 0.1, (std::max)(dist, 0.5)) * 180.0 / 3.14159265358979323846;
    out.push_back({std::fmod(g.saz / g.n + 720.0, 360.0), g.sv / g.n, half, (std::max)(0.02, 2.0 * half / 200.0)});
  }
  return out;
}
