// Wyrownanie wiazki (bundle adjustment) zdjec lotniczych (bez Gothic API): niewiadome to poza kazdego zdjecia (E, N, H, roll, pitch, yaw) i polozenia punktow wiazacych;
// obserwacje to piksele (model otworkowy z Camera.hpp), a-priori z GNSS i IMU (wagi) oraz punkty stale (GCP, wspolrzedne znane). Gauss-Newton / Levenberg-Marquardt z dopelnieniem Schura
// (punkty wyeliminowane, rozwiazywany jest uklad dla zdjec). Pochodne numeryczne.
#pragma once
#include <array>
#include <cmath>
#include <string>
#include <vector>
#include "calc/AlsAdjust.hpp"   // alsadj::Solve
#include "calc/Camera.hpp"

struct BaCam { double p[3] = {0, 0, 0}; double a[3] = {0, 0, 0}; };      // pozycja E, N, H [m] i katy roll, pitch, yaw [st.]
struct BaPoint { Vec3 x{0, 0, 0}; bool fixed = false; };                  // fixed = punkt znany (GCP), nie podlega wyrownaniu
struct BaObs { int cam, pt; double u, v; };                               // pomiar piksela punktu pt na zdjeciu cam
struct BaParams {
  double f = 200, cx = 160, cy = 120; Vec3 lever{0, 0, 0}; Rot3 Rb = RotFromRpy(0, 0, 0);   // wewnetrzne i montaz kamery (nominalny)
  double sigmaPx = 0.5, sigmaPos = 0.5, sigmaAtt = 1.0; int maxIter = 30;                     // bledy: pomiar piksela [px], a-priori pozycji [m] i katow [st.]
};
struct BaResult { bool ok = false; std::string msg; int iters = 0, nObs = 0, nCams = 0, nPts = 0; double rmsBefore = 0, rmsAfter = 0; };   // rms reprojekcji [px]

namespace badetail {
inline bool Resid(const BaParams& bp, const BaCam& c, const Vec3& P, const BaObs& o, double& ru, double& rv) {
  TrajSample s{}; s.pos = {c.p[0], c.p[1], c.p[2]}; s.rollDeg = c.a[0]; s.pitchDeg = c.a[1]; s.yawDeg = c.a[2];
  double u, v; if (!ProjectWorld(P, s, bp.Rb, bp.lever, bp.f, bp.cx, bp.cy, u, v)) { ru = rv = 1e3; return false; }   // punkt za kamera: duza reszta
  ru = u - o.u; rv = v - o.v; return true;
}
inline void Inv3(const double* m, double* o) {
  const double a = m[0], b = m[1], c = m[2], d = m[3], e = m[4], f = m[5], g = m[6], h = m[7], i = m[8];
  const double det = a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g), id = 1.0 / det;
  o[0] = (e * i - f * h) * id; o[1] = (c * h - b * i) * id; o[2] = (b * f - c * e) * id; o[3] = (f * g - d * i) * id; o[4] = (a * i - c * g) * id; o[5] = (c * d - a * f) * id;
  o[6] = (d * h - e * g) * id; o[7] = (b * g - a * h) * id; o[8] = (a * e - b * d) * id;
}
}  // namespace badetail

// cams: startowe (a-priori z GNSS/IMU) -> wynik; prior: a-priori; pts: punkty (wolne -> wynik); obs: obserwacje pikseli.
inline BaResult BundleAdjust(std::vector<BaCam>& cams, const std::vector<BaCam>& prior, std::vector<BaPoint>& pts, const std::vector<BaObs>& obs, const BaParams& bp) {
  BaResult res; const int nc = (int)cams.size(), np = (int)pts.size();
  if (nc == 0 || prior.size() != cams.size()) { res.msg = "Brak zdjec do wyrownania"; return res; }
  if (obs.empty()) { res.msg = "Brak obserwacji punktow na zdjeciach"; return res; }
  res.nCams = nc; res.nPts = np; res.nObs = (int)obs.size();
  std::vector<int> fidx(np, -1); int nf = 0; for (int k = 0; k < np; k++) if (!pts[k].fixed) fidx[k] = nf++;
  const int nC = 6 * nc; const double wpx = 1.0 / (bp.sigmaPx * bp.sigmaPx);
  auto cost = [&](const std::vector<BaCam>& C, const std::vector<BaPoint>& P, double* rmsPx) {
    double s = 0, spx = 0;
    for (const BaObs& o : obs) { double ru, rv; badetail::Resid(bp, C[o.cam], P[o.pt].x, o, ru, rv); spx += ru * ru + rv * rv; }
    s = spx * wpx;
    for (int c = 0; c < nc; c++) for (int k = 0; k < 3; k++) { const double dp = (C[c].p[k] - prior[c].p[k]) / bp.sigmaPos, da = (C[c].a[k] - prior[c].a[k]) / bp.sigmaAtt; s += dp * dp + da * da; }
    if (rmsPx) *rmsPx = std::sqrt(spx / (2.0 * obs.size()));
    return s;
  };
  double rms = 0; double cur = cost(cams, pts, &rms); res.rmsBefore = rms; double lambda = 1e-3;
  struct Link { int cam; double H[18]; };
  for (int iter = 0; iter < bp.maxIter; iter++) {
    std::vector<double> Hcc((size_t)nC * nC, 0.0), bc(nC, 0.0), Hpp((size_t)9 * nf, 0.0), bpv((size_t)3 * nf, 0.0); std::vector<std::vector<Link>> links(nf);
    for (const BaObs& o : obs) {
      const BaCam& c = cams[o.cam]; const Vec3& P = pts[o.pt].x; double r0u, r0v; badetail::Resid(bp, c, P, o, r0u, r0v);
      double Jc[2][6], Jp[2][3] = {{0, 0, 0}, {0, 0, 0}}; const double st = 1e-3;
      for (int k = 0; k < 6; k++) { BaCam t = c; (k < 3 ? t.p[k] : t.a[k - 3]) += st; double ru, rv; badetail::Resid(bp, t, P, o, ru, rv); Jc[0][k] = (ru - r0u) / st; Jc[1][k] = (rv - r0v) / st; }
      const bool free = fidx[o.pt] >= 0;
      if (free) {
        for (int k = 0; k < 3; k++) { Vec3 q = P; (k == 0 ? q.x : k == 1 ? q.y : q.z) += st; double ru, rv; badetail::Resid(bp, c, q, o, ru, rv); Jp[0][k] = (ru - r0u) / st; Jp[1][k] = (rv - r0v) / st; }
      }
      for (int a = 0; a < 6; a++) {
        bc[(size_t)o.cam * 6 + a] += wpx * (Jc[0][a] * r0u + Jc[1][a] * r0v);
        for (int b = 0; b < 6; b++) Hcc[((size_t)o.cam * 6 + a) * nC + (size_t)o.cam * 6 + b] += wpx * (Jc[0][a] * Jc[0][b] + Jc[1][a] * Jc[1][b]);
      }
      if (free) {
        const int f = fidx[o.pt]; Link L{o.cam, {0}};
        for (int a = 0; a < 3; a++) {
          bpv[(size_t)f * 3 + a] += wpx * (Jp[0][a] * r0u + Jp[1][a] * r0v);
          for (int b = 0; b < 3; b++) Hpp[(size_t)f * 9 + a * 3 + b] += wpx * (Jp[0][a] * Jp[0][b] + Jp[1][a] * Jp[1][b]);
        }
        for (int a = 0; a < 6; a++) for (int b = 0; b < 3; b++) L.H[a * 3 + b] = wpx * (Jc[0][a] * Jp[0][b] + Jc[1][a] * Jp[1][b]);
        links[f].push_back(L);
      }
    }
    for (int c = 0; c < nc; c++) for (int k = 0; k < 6; k++) {                                        // a-priori GNSS (pozycja) i IMU (katy)
      const double sg = k < 3 ? bp.sigmaPos : bp.sigmaAtt, w = 1.0 / (sg * sg), d = (k < 3 ? cams[c].p[k] - prior[c].p[k] : cams[c].a[k - 3] - prior[c].a[k - 3]);
      Hcc[((size_t)c * 6 + k) * nC + (size_t)c * 6 + k] += w; bc[(size_t)c * 6 + k] += w * d;
    }
    bool accepted = false; double maxStep = 0;
    for (int tries = 0; tries < 10 && !accepted; tries++) {
      std::vector<double> S = Hcc, rhs(nC); for (int i = 0; i < nC; i++) { S[(size_t)i * nC + i] *= (1.0 + lambda); rhs[i] = -bc[i]; }
      std::vector<std::array<double, 9>> Ai(nf);
      for (int f = 0; f < nf; f++) {
        double A[9]; for (int i = 0; i < 9; i++) A[i] = Hpp[(size_t)f * 9 + i]; for (int i = 0; i < 3; i++) A[i * 4] = A[i * 4] * (1.0 + lambda) + 1e-9; badetail::Inv3(A, Ai[f].data());
        const std::vector<Link>& L = links[f]; std::vector<std::array<double, 18>> W(L.size());
        for (size_t l = 0; l < L.size(); l++) for (int a = 0; a < 6; a++) for (int b = 0; b < 3; b++) { double s = 0; for (int k = 0; k < 3; k++) s += L[l].H[a * 3 + k] * Ai[f][k * 3 + b]; W[l][a * 3 + b] = s; }
        for (size_t l1 = 0; l1 < L.size(); l1++) {
          for (int a = 0; a < 6; a++) { double s = 0; for (int k = 0; k < 3; k++) s += W[l1][a * 3 + k] * bpv[(size_t)f * 3 + k]; rhs[(size_t)L[l1].cam * 6 + a] += s; }
          for (size_t l2 = 0; l2 < L.size(); l2++) for (int a = 0; a < 6; a++) for (int b = 0; b < 6; b++) {
            double s = 0; for (int k = 0; k < 3; k++) s += W[l1][a * 3 + k] * L[l2].H[b * 3 + k]; S[((size_t)L[l1].cam * 6 + a) * nC + (size_t)L[l2].cam * 6 + b] -= s;
          }
        }
      }
      std::vector<double> dc = rhs; if (!alsadj::Solve(S, dc, nC)) { lambda *= 10; continue; }
      std::vector<BaCam> tc = cams; std::vector<BaPoint> tp = pts; maxStep = 0;
      for (int c = 0; c < nc; c++) for (int k = 0; k < 6; k++) { (k < 3 ? tc[c].p[k] : tc[c].a[k - 3]) += dc[(size_t)c * 6 + k]; maxStep = (std::max)(maxStep, std::fabs(dc[(size_t)c * 6 + k])); }
      for (int f = 0, k = 0; k < np; k++) {
        if (fidx[k] < 0) continue;
        f = fidx[k]; double g[3] = {bpv[(size_t)f * 3], bpv[(size_t)f * 3 + 1], bpv[(size_t)f * 3 + 2]};
        for (const Link& l : links[f]) for (int b = 0; b < 3; b++) for (int a = 0; a < 6; a++) g[b] += l.H[a * 3 + b] * dc[(size_t)l.cam * 6 + a];
        double d[3]; for (int a = 0; a < 3; a++) d[a] = -(Ai[f][a * 3] * g[0] + Ai[f][a * 3 + 1] * g[1] + Ai[f][a * 3 + 2] * g[2]);
        tp[k].x.x += d[0]; tp[k].x.y += d[1]; tp[k].x.z += d[2]; for (int a = 0; a < 3; a++) maxStep = (std::max)(maxStep, std::fabs(d[a]));
      }
      double r2; const double nc2 = cost(tc, tp, &r2);
      if (nc2 < cur) { cams.swap(tc); pts.swap(tp); cur = nc2; rms = r2; lambda = (std::max)(1e-9, lambda / 3.0); accepted = true; } else lambda *= 5.0;
    }
    res.iters = iter + 1; if (!accepted || maxStep < 1e-6) break;
  }
  res.rmsAfter = rms; res.ok = true; return res;
}
