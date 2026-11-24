// Kalibracja boresightu i wyrownanie szeregow ALS (bez Gothic API). Dane: surowy plik rekordow 34 B (AlsRawWriter) i trajektoria estymowana z trajektoria.csv.
// Metoda: komorki w nakladkach szeregow -> plaszczyzny z = a + b dx + c dy na szereg -> roznice wysokosci w srodkach komorek; kalibracja = Gauss-Newton po trzech katach boresightu
// (punkty liczone ponownie: P = X + R(imu) (R_b r + lever)). Praca w krokach (Step z budzetem czasu), przebiegi strumieniowe po pliku, w pamieci tylko wybrane punkty.
#pragma once
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
#include "calc/AlsLas.hpp"
#include "calc/Georef.hpp"

struct EstSample { double t = 0; Vec3 pos{0, 0, 0}; double roll = 0, pitch = 0, yaw = 0; };   // pos: x = E, y = N, z = H (siatka)

// Wiersze trajektoria.csv: t;N_prawdziwe;E;H;N_gnss;E;H;roll;pitch;yaw;roll_imu;pitch_imu;yaw_imu;szereg -> estymata (kolumny gnss i imu), posortowana po czasie.
inline std::vector<EstSample> AlsLoadTrajectory(const std::string& csv) {
  std::vector<EstSample> out; std::ifstream in(csv); std::string line;
  if (!in.good()) return out;
  std::getline(in, line);
  while (std::getline(in, line)) {
    double t, n, e, h, r, p, y;
    if (std::sscanf(line.c_str(), "%lf;%*f;%*f;%*f;%lf;%lf;%lf;%*f;%*f;%*f;%lf;%lf;%lf", &t, &n, &e, &h, &r, &p, &y) != 7) continue;
    EstSample s; s.t = t; s.pos = {e, n, h}; s.roll = r; s.pitch = p; s.yaw = y; out.push_back(s);
  }
  std::stable_sort(out.begin(), out.end(), [](const EstSample& a, const EstSample& b) { return a.t < b.t; });
  return out;
}

inline EstSample AlsEstAt(const std::vector<EstSample>& tr, double t) {
  if (tr.empty()) return EstSample{};
  if (t <= tr.front().t) return tr.front();
  if (t >= tr.back().t) return tr.back();
  const size_t hi = (size_t)(std::upper_bound(tr.begin(), tr.end(), t, [](double v, const EstSample& s) { return v < s.t; }) - tr.begin());
  const EstSample& a = tr[hi - 1]; const EstSample& b = tr[hi]; const double f = (b.t > a.t) ? (t - a.t) / (b.t - a.t) : 0.0;
  EstSample r; r.t = t;
  r.pos = {a.pos.x + (b.pos.x - a.pos.x) * f, a.pos.y + (b.pos.y - a.pos.y) * f, a.pos.z + (b.pos.z - a.pos.z) * f};
  r.roll = a.roll + (b.roll - a.roll) * f; r.pitch = a.pitch + (b.pitch - a.pitch) * f;
  double d = std::fmod(b.yaw - a.yaw, 360.0); if (d > 180) d -= 360; if (d < -180) d += 360;
  r.yaw = a.yaw + d * f; r.yaw = std::fmod(r.yaw + 360.0, 360.0);
  return r;
}

struct AdjustParams { double cellM = 4; int minPerStrip = 8, maxPerCell = 24, maxCells = 3000; Vec3 lever{0.0, 0.15, -0.30}; double gpsT0 = 345600.0; };

struct AdjustResult {
  bool ok = false; std::string msg;
  double bore[3] = {0, 0, 0}, sigma[3] = {0, 0, 0}; bool determined[3] = {false, false, false};   // roll, pitch, yaw boresightu [st.], niepewnosc [st.]
  double rmsBefore = 0, rmsCalibrated = 0; int cells = 0, strips = 0;                              // rms roznic wysokosci w nakladkach [m]
};

namespace alsadj {
// Rozwiazuje A x = b (A: n x n, wiersze); Gauss z wyborem elementu glownego; false gdy osobliwa. Wynik w b.
inline bool Solve(std::vector<double> A, std::vector<double>& b, int n) {
  for (int c = 0; c < n; c++) {
    int pr = c; for (int r = c + 1; r < n; r++) if (std::fabs(A[r * n + c]) > std::fabs(A[pr * n + c])) pr = r;
    if (std::fabs(A[pr * n + c]) < 1e-300) return false;
    if (pr != c) { for (int k = 0; k < n; k++) std::swap(A[c * n + k], A[pr * n + k]); std::swap(b[c], b[pr]); }
    for (int r = c + 1; r < n; r++) { const double f = A[r * n + c] / A[c * n + c]; if (f == 0) continue; for (int k = c; k < n; k++) A[r * n + k] -= f * A[c * n + k]; b[r] -= f * b[c]; }
  }
  for (int r = n - 1; r >= 0; r--) { double s = b[r]; for (int k = r + 1; k < n; k++) s -= A[r * n + k] * b[k]; b[r] = s / A[r * n + r]; }
  return true;
}
inline bool Invert(const std::vector<double>& A, int n, std::vector<double>& inv) {
  inv.assign((size_t)n * n, 0.0);
  for (int c = 0; c < n; c++) { std::vector<double> e(n, 0.0); e[c] = 1.0; if (!Solve(A, e, n)) return false; for (int r = 0; r < n; r++) inv[r * n + c] = e[r]; }
  return true;
}
struct Plane {                                                      // z = a + b dx + c dy (MNK)
  double n = 0, sx = 0, sy = 0, sxx = 0, sxy = 0, syy = 0, sz = 0, sxz = 0, syz = 0;
  void Add(double dx, double dy, double z) { n++; sx += dx; sy += dy; sxx += dx * dx; sxy += dx * dy; syy += dy * dy; sz += z; sxz += dx * z; syz += dy * z; }
  bool Fit(double& a, double& b, double& c, double minSigma = 0) const {
    if (n < 4) return false;
    if (minSigma > 0) {                                             // punkty zbyt zebrane w waska smuge: plaszczyzna nieokreslona, nie ekstrapolujemy
      const double mx = sx / n, my = sy / n, vx = sxx / n - mx * mx, vy = syy / n - my * my, cv = sxy / n - mx * my;
      const double lmin = (vx + vy) / 2 - std::sqrt((vx - vy) * (vx - vy) / 4 + cv * cv);
      if (lmin < minSigma * minSigma) return false;
    }
    std::vector<double> A = {n, sx, sy, sx, sxx, sxy, sy, sxy, syy}, r = {sz, sxz, syz};
    if (!Solve(A, r, 3)) return false;
    a = r[0]; b = r[1]; c = r[2]; return true;
  }
};
inline long long CellKey(int ix, int iy) { return ((long long)(ix + 1000000) << 32) | (long long)(iy + 1000000); }
}  // namespace alsadj

class AlsAdjuster {
 public:
  AlsAdjuster(const std::string& rawPath, const std::string& trajCsv, const std::string& outRaw, double originN, double originE, double originH, std::vector<ControlPoint> cps, AdjustParams ap = AdjustParams())
      : raw_(rawPath), csv_(trajCsv), out_(outRaw), oN_(originN), oE_(originE), oH_(originH), cps_(std::move(cps)), ap_(ap) {}

  // Jeden krok pracy w budzecie czasu [ms]; true = zakonczone (wynik: Result().ok).
  bool Step(int budgetMs) {
    if (phase_ == Phase::Done) return true;
    const auto t0 = std::chrono::steady_clock::now();
    auto over = [&]() { return std::chrono::steady_clock::now() - t0 > std::chrono::milliseconds(budgetMs); };
    switch (phase_) {
      case Phase::Start: if (!Begin()) return Fail(); break;
      case Phase::Index: StepScan(over, false); break;
      case Phase::Collect: StepScan(over, true); break;
      case Phase::Calibrate: if (!CalibrateIteration()) return Fail(); break;
      default: break;
    }
    return phase_ == Phase::Done;
  }
  const AdjustResult& Result() const { return res_; }
  std::string Progress() const {
    static const char* names[] = {"start", "indeksowanie nakladek", "zbieranie punktow", "kalibracja boresightu", "koniec"};
    char b[96]; const int idx = phase_ == Phase::Start ? 0 : phase_ == Phase::Index ? 1 : phase_ == Phase::Collect ? 2 : phase_ == Phase::Calibrate ? 3 : 4;
    if (idx == 1 || idx == 2) std::snprintf(b, sizeof b, "%s %d%%", names[idx], total_ ? (int)(100.0 * pos_ / total_) : 0); else std::snprintf(b, sizeof b, "%s", names[idx]);
    return b;
  }

 private:
  enum class Phase { Start, Index, Collect, Calibrate, Done };
  struct Tie { int cell; std::uint16_t strip; float px, py, pz, R[9], bx, bz; };      // punkt wiazacy: poza estymowana, obrot IMU i wektor wiazki (bez montazu)
  struct Group { int cell, begin, end; std::uint16_t strip; };

  bool Fail() { res_.ok = false; phase_ = Phase::Done; return true; }
  bool Err(const std::string& m) { res_.ok = false; res_.msg = m; return false; }

  bool Begin() {
    traj_ = AlsLoadTrajectory(csv_);
    if (traj_.empty()) return Err("Brak trajektorii (trajektoria.csv) - nie mozna wyrownac");
    std::error_code ec; const auto sz = std::filesystem::file_size(raw_, ec);
    if (ec || sz < (std::uintmax_t)kAlsRawBytes) return Err("Brak surowych danych lotu (lot_surowy.bin) - nie mozna wyrownac");
    total_ = (long long)(sz / kAlsRawBytes); in_.open(raw_, std::ios::binary); pos_ = 0; phase_ = Phase::Index; return true;
  }

  // Przebieg po pliku: indeksowanie (liczniki szeregow w komorkach) albo zbieranie decymowanych punktow wiazacych.
  template <class Over> void StepScan(Over over, bool collect) {
    AlsPoint p; int n = 0;
    while (pos_ < total_) {
      if (!alsdetail::ReadRec(in_, p)) { pos_ = total_; break; }
      pos_++;
      const int ix = (int)std::floor(p.x / ap_.cellM), iy = (int)std::floor(p.y / ap_.cellM); const long long key = alsadj::CellKey(ix, iy);
      if (!collect) { stripsSeen_.insert(p.strip); auto& v = counts_[key]; bool f = false; for (auto& e : v) if (e.first == p.strip) { e.second++; f = true; break; } if (!f) v.push_back({p.strip, 1}); }
      else AddTie(p, key);
      if ((++n & 4095) == 0 && over()) return;
    }
    if (!collect) EndIndex(); else EndCollect();
  }

  void EndIndex() {
    res_.strips = (int)stripsSeen_.size();
    if (res_.strips < 2) { Err("Jeden szereg: brak nakladek do wyrownania"); phase_ = Phase::Done; return; }
    std::vector<long long> keys;
    for (const auto& kv : counts_) { int ok = 0; for (const auto& e : kv.second) if (e.second >= ap_.minPerStrip) ok++; if (ok >= 2) keys.push_back(kv.first); }
    std::sort(keys.begin(), keys.end());
    if ((int)keys.size() > ap_.maxCells) { const double st = (double)keys.size() / ap_.maxCells; std::vector<long long> k2; for (int i = 0; i < ap_.maxCells; i++) k2.push_back(keys[(size_t)(i * st)]); keys.swap(k2); }
    if (keys.size() < 10) { Err("Za malo nakladek szeregow do wyrownania (potrzebne szeregi zachodzace na siebie)"); phase_ = Phase::Done; return; }
    for (size_t i = 0; i < keys.size(); i++) {
      cellIdx_[keys[i]] = (int)i;
    }
    res_.cells = (int)keys.size();
    in_.close(); in_.clear(); in_.open(raw_, std::ios::binary); pos_ = 0; phase_ = Phase::Collect;
  }

  void AddTie(const AlsPoint& p, long long key) {
    auto ci = cellIdx_.find(key); if (ci == cellIdx_.end()) return;
    const auto& cnt = counts_[key]; int total = 0; for (const auto& e : cnt) if (e.first == p.strip) total = e.second;
    if (total < ap_.minPerStrip) return;
    const int step = (std::max)(1, total / ap_.maxPerCell); int& seen = seen_[((long long)ci->second << 16) | p.strip];
    const bool take = (seen % step) == 0 && seen / step < ap_.maxPerCell; seen++;
    if (!take) return;
    const EstSample e = AlsEstAt(traj_, p.gps - ap_.gpsT0); const Rot3 R = RotFromRpy(e.roll, e.pitch, e.yaw);
    Tie t; t.cell = ci->second; t.strip = p.strip; t.px = (float)(e.pos.x - oE_); t.py = (float)(e.pos.y - oN_); t.pz = (float)(e.pos.z - oH_);
    for (int i = 0; i < 9; i++) t.R[i] = (float)R.m[i];
    const double th = (double)p.theta * 3.14159265358979323846 / 180.0; t.bx = (float)(p.range * std::sin(th)); t.bz = (float)(-p.range * std::cos(th));
    ties_.push_back(t);
  }

  void EndCollect() {
    std::stable_sort(ties_.begin(), ties_.end(), [](const Tie& a, const Tie& b) { return a.cell != b.cell ? a.cell < b.cell : a.strip < b.strip; });
    for (int i = 0; i < (int)ties_.size();) { int j = i; while (j < (int)ties_.size() && ties_[j].cell == ties_[i].cell && ties_[j].strip == ties_[i].strip) j++; groups_.push_back({ties_[i].cell, i, j, ties_[i].strip}); i = j; }
    counts_.clear(); seen_.clear(); cellIdx_.clear();
    Rot3 I = RotFromRpy(0, 0, 0); std::vector<double> r0; Residuals(I, r0);
    if (std::count_if(r0.begin(), r0.end(), [](double v) { return std::isfinite(v); }) < 10) { Err("Za malo nakladek szeregow do wyrownania"); phase_ = Phase::Done; return; }
    res_.rmsBefore = Rms(r0); cal_[0] = cal_[1] = cal_[2] = 0; calIter_ = 0; active_[0] = active_[1] = active_[2] = true; phase_ = Phase::Calibrate;
  }

  // Skala odporna (1,4826 * mediana |r|, min 2 mm) i rms bez odstajacych (|r| > 4 skali); NaN pomijane.
  static double RobustScale(const std::vector<double>& r) {
    std::vector<double> a; for (double v : r) if (std::isfinite(v)) a.push_back(std::fabs(v));
    if (a.empty()) return 0.002;
    std::nth_element(a.begin(), a.begin() + a.size() / 2, a.end());
    return (std::max)(0.002, 1.4826 * a[a.size() / 2]);
  }
  static double Rms(const std::vector<double>& r) {
    const double sc = RobustScale(r); double s = 0; int n = 0;
    for (double v : r) if (std::isfinite(v) && std::fabs(v) <= 4 * sc) { s += v * v; n++; }
    return n ? std::sqrt(s / n) : 0;
  }

  Vec3 Georef(const Tie& t, const Rot3& Rb) const {
    const double bx = t.bx, bz = t.bz;
    const double vx = Rb.m[0] * bx + Rb.m[2] * bz + ap_.lever.x, vy = Rb.m[3] * bx + Rb.m[5] * bz + ap_.lever.y, vz = Rb.m[6] * bx + Rb.m[8] * bz + ap_.lever.z;
    return {t.px + t.R[0] * vx + t.R[1] * vy + t.R[2] * vz, t.py + t.R[3] * vx + t.R[4] * vy + t.R[5] * vz, t.pz + t.R[6] * vx + t.R[7] * vy + t.R[8] * vz};
  }
  // Plaszczyzny wszystkich grup przy montazu Rb. Kazda komorka: wspolny punkt porownania = srodek ciezkosci punktow wszystkich szeregow w komorce (bez ekstrapolacji do srodka komorki);
  // fit[g] = {z w tym punkcie, nachylenie wzdluz E, nachylenie wzdluz N}.
  void FitGroups(const Rot3& Rb, std::vector<std::array<double, 3>>& fit, std::vector<char>& ok) const {
    fit.assign(groups_.size(), {0, 0, 0}); ok.assign(groups_.size(), 0);
    std::vector<Vec3> P(ties_.size()); for (size_t i = 0; i < ties_.size(); i++) P[i] = Georef(ties_[i], Rb);
    for (size_t g0 = 0; g0 < groups_.size();) {
      size_t g1 = g0; while (g1 < groups_.size() && groups_[g1].cell == groups_[g0].cell) g1++;
      double ux = 0, uy = 0; int n = 0;
      for (size_t g = g0; g < g1; g++) for (int i = groups_[g].begin; i < groups_[g].end; i++) { ux += P[i].x; uy += P[i].y; n++; }
      ux /= n; uy /= n;
      for (size_t g = g0; g < g1; g++) {
        alsadj::Plane pl; for (int i = groups_[g].begin; i < groups_[g].end; i++) pl.Add(P[i].x - ux, P[i].y - uy, P[i].z);
        ok[g] = pl.Fit(fit[g][0], fit[g][1], fit[g][2], 0.12 * ap_.cellM) ? 1 : 0;
      }
      g0 = g1;
    }
  }
  // Wektor reszt: roznice wysokosci (we wspolnym punkcie komorki) kolejnych szeregow; zawsze ta sama dlugosc (niepoliczalne = NaN).
  void Residuals(const Rot3& Rb, std::vector<double>& r) const {
    std::vector<std::array<double, 3>> fit; std::vector<char> ok; FitGroups(Rb, fit, ok); r.clear();
    for (size_t g = 0; g + 1 < groups_.size(); g++) if (groups_[g].cell == groups_[g + 1].cell) r.push_back(ok[g] && ok[g + 1] ? fit[g][0] - fit[g + 1][0] : std::nan(""));
  }

  // Jedna iteracja Gaussa-Newtona (pochodne numeryczne +-0,01 st.), potem zawezenie do wyznaczalnych katow.
  bool CalibrateIteration() {
    const double h = 0.01; const int na = (int)(active_[0]) + (int)(active_[1]) + (int)(active_[2]);
    std::vector<double> r0, rp, rm; Rot3 Rb = RotFromRpy(cal_[0], cal_[1], cal_[2]); Residuals(Rb, r0);
    const size_t m = r0.size(); std::vector<std::vector<double>> J(3, std::vector<double>(m, 0.0));
    for (int k = 0; k < 3; k++) {
      if (!active_[k]) continue;
      double a[3] = {cal_[0], cal_[1], cal_[2]}; a[k] += h; Residuals(RotFromRpy(a[0], a[1], a[2]), rp); a[k] -= 2 * h; Residuals(RotFromRpy(a[0], a[1], a[2]), rm);
      for (size_t i = 0; i < m; i++) J[k][i] = (rp[i] - rm[i]) / (2 * h);
    }
    const double scale = RobustScale(r0); std::vector<double> N(9, 0.0), g(3, 0.0); double rss = 0; int used = 0;
    for (size_t i = 0; i < m; i++) {
      bool good = std::isfinite(r0[i]) && std::fabs(r0[i]) <= 4 * scale; for (int k = 0; k < 3; k++) if (active_[k] && !std::isfinite(J[k][i])) good = false;
      if (!good) continue;
      used++; rss += r0[i] * r0[i];
      for (int a = 0; a < 3; a++) { g[a] += J[a][i] * r0[i]; for (int b = 0; b < 3; b++) N[a * 3 + b] += J[a][i] * J[b][i]; }
    }
    if (used < 10) return Err("Za malo wiarygodnych nakladek do kalibracji");
    double dmax = 0; for (int a = 0; a < 3; a++) dmax = (std::max)(dmax, N[a * 3 + a]);
    std::vector<double> A = N, rhs(3); for (int a = 0; a < 3; a++) { rhs[a] = -g[a]; if (active_[a]) A[a * 3 + a] += 1e-3 * N[a * 3 + a] + 1e-12 * dmax; else { for (int b = 0; b < 3; b++) { A[a * 3 + b] = 0; A[b * 3 + a] = 0; } A[a * 3 + a] = 1; rhs[a] = 0; } }
    if (!alsadj::Solve(A, rhs, 3)) return Err("Kalibracja boresightu: uklad osobliwy");
    double step = 0; for (int a = 0; a < 3; a++) { if (active_[a]) cal_[a] += rhs[a]; step = (std::max)(step, std::fabs(rhs[a])); }
    calIter_++;
    const bool conv = step < 2e-4 || calIter_ >= (stage_ == 0 ? 10 : 6);
    if (conv) {
      std::vector<double> cov; double s0 = std::sqrt(rss / (std::max)(1.0, (double)used - na));
      std::vector<double> A2(9); for (int a = 0; a < 3; a++) for (int b = 0; b < 3; b++) A2[a * 3 + b] = N[a * 3 + b]; for (int a = 0; a < 3; a++) A2[a * 3 + a] += 1e-12 * dmax + 1e-30;
      if (!alsadj::Invert(A2, 3, cov)) return Err("Kalibracja boresightu: nie mozna wyznaczyc niepewnosci");
      bool anyDrop = false;
      for (int a = 0; a < 3; a++) {
        res_.sigma[a] = active_[a] ? s0 * std::sqrt((std::max)(0.0, cov[a * 3 + a])) : res_.sigma[a];
        const bool det = active_[a] && res_.sigma[a] <= 0.05 && N[a * 3 + a] > 1e-9 * (std::max)(dmax, 1e-30);
        if (stage_ == 0 && !det) { anyDrop = true; res_.sigma[a] = (std::max)(res_.sigma[a], 1.0); active_[a] = false; cal_[a] = 0; }
        res_.determined[a] = det;
      }
      if (stage_ == 0 && anyDrop) { stage_ = 1; calIter_ = 0; return true; }       // druga runda: tylko wyznaczalne katy, reszta 0
      for (int a = 0; a < 3; a++) { res_.bore[a] = res_.determined[a] ? cal_[a] : 0.0; if (!res_.determined[a]) cal_[a] = 0; }
      Residuals(RotFromRpy(res_.bore[0], res_.bore[1], res_.bore[2]), r0); res_.rmsCalibrated = Rms(r0);
      res_.ok = true; phase_ = Phase::Done;
    }
    return true;
  }

  std::string raw_, csv_, out_; double oN_, oE_, oH_; std::vector<ControlPoint> cps_; AdjustParams ap_;
  AdjustResult res_; Phase phase_ = Phase::Start; std::ifstream in_; long long total_ = 0, pos_ = 0;
  std::vector<EstSample> traj_; std::set<int> stripsSeen_;
  std::unordered_map<long long, std::vector<std::pair<int, int>>> counts_; std::unordered_map<long long, int> cellIdx_, seen_;
  std::vector<Tie> ties_; std::vector<Group> groups_;
  double cal_[3] = {0, 0, 0}; bool active_[3] = {true, true, true}; int calIter_ = 0, stage_ = 0;
};
