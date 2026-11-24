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

struct StripShift { int strip = 0; double dE = 0, dN = 0, dH = 0, sigE = 0, sigN = 0, sigH = 0; bool detH = false, detEN = false; };   // poprawka szeregu [m]

struct AdjustResult {
  bool ok = false; std::string msg;
  double bore[3] = {0, 0, 0}, sigma[3] = {0, 0, 0}; bool determined[3] = {false, false, false};   // roll, pitch, yaw boresightu [st.], niepewnosc [st.]
  double rmsBefore = 0, rmsCalibrated = 0, rmsAfter = 0; int cells = 0, strips = 0;                // rms roznic wysokosci w nakladkach [m] (bez odstajacych): przed, po kalibracji, po wyrownaniu szeregow
  std::vector<StripShift> shifts; std::vector<ControlQc> ctrlBefore, ctrlAfter;                    // przesuniecia szeregow; odchylki na punktach osnowy przed i po
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
      case Phase::Write: StepWrite(over); break;
      case Phase::Check: StepCheck(); break;
      default: break;
    }
    return phase_ == Phase::Done;
  }
  const AdjustResult& Result() const { return res_; }
  std::string Progress() const {
    static const char* names[] = {"start", "indeksowanie nakladek", "zbieranie punktow", "kalibracja i wyrownanie szeregow", "zapis wyniku", "kontrola osnowy", "koniec"};
    char b[96]; const int idx = (int)phase_;
    if (idx == 1 || idx == 2 || idx == 4) std::snprintf(b, sizeof b, "%s %d%%", names[idx], total_ ? (int)(100.0 * pos_ / total_) : 0); else std::snprintf(b, sizeof b, "%s", names[idx]);
    return b;
  }

 private:
  enum class Phase { Start, Index, Collect, Calibrate, Write, Check, Done };
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

  Tie MakeTie(const AlsPoint& p, int cell) const {
    const EstSample e = AlsEstAt(traj_, p.gps - ap_.gpsT0); const Rot3 R = RotFromRpy(e.roll, e.pitch, e.yaw);
    Tie t; t.cell = cell; t.strip = p.strip; t.px = (float)(e.pos.x - oE_); t.py = (float)(e.pos.y - oN_); t.pz = (float)(e.pos.z - oH_);
    for (int i = 0; i < 9; i++) t.R[i] = (float)R.m[i];
    const double th = (double)p.theta * 3.14159265358979323846 / 180.0; t.bx = (float)(p.range * std::sin(th)); t.bz = (float)(-p.range * std::cos(th));
    return t;
  }
  void AddTie(const AlsPoint& p, long long key) {
    for (size_t k = 0; k < cps_.size(); k++) {                                       // punkty w promieniu 3 m od punktu osnowy (co 3., max 80 na szereg)
      const double dx = p.x - (cps_[k].e - oE_), dy = p.y - (cps_[k].n - oN_);
      if (dx * dx + dy * dy > 9.0) continue;
      int& cs = cseen_[((long long)k << 16) | p.strip]; if (cs % 3 == 0 && cs / 3 < 80) cties_.push_back(MakeTie(p, (int)k)); cs++;
    }
    auto ci = cellIdx_.find(key); if (ci == cellIdx_.end()) return;
    const auto& cnt = counts_[key]; int total = 0; for (const auto& e : cnt) if (e.first == p.strip) total = e.second;
    if (total < ap_.minPerStrip) return;
    const int step = (std::max)(1, total / ap_.maxPerCell); int& seen = seen_[((long long)ci->second << 16) | p.strip];
    const bool take = (seen % step) == 0 && seen / step < ap_.maxPerCell; seen++;
    if (take) ties_.push_back(MakeTie(p, ci->second));
  }

  void EndCollect() {
    std::stable_sort(ties_.begin(), ties_.end(), [](const Tie& a, const Tie& b) { return a.cell != b.cell ? a.cell < b.cell : a.strip < b.strip; });
    for (int i = 0; i < (int)ties_.size();) { int j = i; while (j < (int)ties_.size() && ties_[j].cell == ties_[i].cell && ties_[j].strip == ties_[i].strip) j++; groups_.push_back({ties_[i].cell, i, j, ties_[i].strip}); i = j; }
    for (const Group& g : groups_) ids_.push_back(g.strip);
    std::sort(ids_.begin(), ids_.end()); ids_.erase(std::unique(ids_.begin(), ids_.end()), ids_.end());
    for (const Group& g : groups_) gIdx_.push_back(StripIdx(g.strip));
    u_.assign(3 * ids_.size(), 0.0);
    std::stable_sort(cties_.begin(), cties_.end(), [](const Tie& a, const Tie& b) { return a.cell != b.cell ? a.cell < b.cell : a.strip < b.strip; });
    for (int i = 0; i < (int)cties_.size();) { int j = i; while (j < (int)cties_.size() && cties_[j].cell == cties_[i].cell && cties_[j].strip == cties_[i].strip) j++; cgroups_.push_back({cties_[i].cell, i, j, cties_[i].strip}); i = j; }
    counts_.clear(); seen_.clear(); cellIdx_.clear(); cseen_.clear();
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

  // Obserwacja: roznica wysokosci kolejnych szeregow w komorce albo wiez osnowy (sj = -1). b, c = nachylenia plaszczyzn (E, N); r = reszta z uwzglednieniem przesuniec u.
  struct Obs { int si, sj; double bi, ci, bj, cj, r; };
  // Wszystkie obserwacje przy montazu Rb i przesunieciach u (3 na szereg: dE, dN, dH): kolejnosc stala, niepoliczalne = NaN.
  // Przesuniecie szeregu o s zmienia jego powierzchnie w punkcie o (-b sE - c sN + sH).
  void Eval(const Rot3& Rb, const std::vector<double>& u, std::vector<Obs>& obs) const {
    std::vector<std::array<double, 3>> fit; std::vector<char> ok; FitGroups(Rb, fit, ok); obs.clear();
    auto sh = [&](int si, double b, double c) { return si < 0 ? 0.0 : -b * u[3 * si] - c * u[3 * si + 1] + u[3 * si + 2]; };
    for (size_t g = 0; g + 1 < groups_.size(); g++) {
      if (groups_[g].cell != groups_[g + 1].cell) continue;
      Obs o{gIdx_[g], gIdx_[g + 1], fit[g][1], fit[g][2], fit[g + 1][1], fit[g + 1][2], std::nan("")};
      if (ok[g] && ok[g + 1]) o.r = (fit[g][0] + sh(o.si, o.bi, o.ci)) - (fit[g + 1][0] + sh(o.sj, o.bj, o.cj));
      obs.push_back(o);
    }
    if (cgroups_.empty()) return;
    std::vector<Vec3> P(cties_.size()); for (size_t i = 0; i < cties_.size(); i++) P[i] = Georef(cties_[i], Rb);
    for (const Group& g : cgroups_) {
      const int k = StripIdx(g.strip); Obs o{k, -1, 0, 0, 0, 0, std::nan("")};
      if (k >= 0) {
        alsadj::Plane pl; const double cpx = cps_[g.cell].e - oE_, cpy = cps_[g.cell].n - oN_;
        for (int i = g.begin; i < g.end; i++) pl.Add(P[i].x - cpx, P[i].y - cpy, P[i].z);
        double a, b, c; if (pl.Fit(a, b, c, 0.3)) { o.bi = b; o.ci = c; o.r = a + sh(k, b, c) - (cps_[g.cell].h - oH_); }
      }
      obs.push_back(o);
    }
  }
  int StripIdx(int strip) const { auto it = std::lower_bound(ids_.begin(), ids_.end(), strip); return (it != ids_.end() && *it == strip) ? (int)(it - ids_.begin()) : -1; }

  // Jedna iteracja Gaussa-Newtona po lacznym wektorze [3 katy boresightu, 3 poprawki na szereg]: katy z pochodnych numerycznych (+-0,01 st.), poprawki analitycznie;
  // odstajace obserwacje odrzucane (4 skale odporne); slaby warunek sredniej poprawek = 0; potem zawezenie do wyznaczalnych katow.
  bool CalibrateIteration() {
    const double h = 0.01; const int S = (int)ids_.size(), D = 3 + 3 * S;
    std::vector<Obs> o0, op, om; Eval(RotFromRpy(cal_[0], cal_[1], cal_[2]), u_, o0);
    const size_t m = o0.size(); std::vector<std::vector<double>> Jp(3, std::vector<double>(m, 0.0));
    for (int k = 0; k < 3; k++) {
      if (!active_[k]) continue;
      double a[3] = {cal_[0], cal_[1], cal_[2]}; a[k] += h; Eval(RotFromRpy(a[0], a[1], a[2]), u_, op); a[k] -= 2 * h; Eval(RotFromRpy(a[0], a[1], a[2]), u_, om);
      for (size_t i = 0; i < m; i++) Jp[k][i] = (op[i].r - om[i].r) / (2 * h);
    }
    std::vector<double> rr; for (const Obs& o : o0) rr.push_back(o.r); const double scale = RobustScale(rr);
    std::vector<double> N((size_t)D * D, 0.0), g(D, 0.0); double rss = 0; int used = 0;
    for (size_t i = 0; i < m; i++) {
      const Obs& o = o0[i]; bool good = std::isfinite(o.r) && (o.sj < 0 || std::fabs(o.r) <= 4 * scale);   // wiezy osnowy nie sa odrzucane jako odstajace for (int k = 0; k < 3; k++) if (active_[k] && !std::isfinite(Jp[k][i])) good = false;
      if (!good) continue;
      int col[9]; double val[9]; int n = 0;
      for (int k = 0; k < 3; k++) if (active_[k]) { col[n] = k; val[n++] = Jp[k][i]; }
      col[n] = 3 + 3 * o.si; val[n++] = -o.bi; col[n] = 3 + 3 * o.si + 1; val[n++] = -o.ci; col[n] = 3 + 3 * o.si + 2; val[n++] = 1;
      if (o.sj >= 0) { col[n] = 3 + 3 * o.sj; val[n++] = o.bj; col[n] = 3 + 3 * o.sj + 1; val[n++] = o.cj; col[n] = 3 + 3 * o.sj + 2; val[n++] = -1; }
      used++; rss += o.r * o.r; const double w = o.sj < 0 ? 10.0 : 1.0;                  // wiez osnowy: waga 100 (w = pierwiastek z wagi)
      for (int a = 0; a < n; a++) { g[col[a]] += w * w * val[a] * o.r; for (int b = 0; b < n; b++) N[(size_t)col[a] * D + col[b]] += w * w * val[a] * val[b]; }
    }
    if (used < 10) return Err("Za malo wiarygodnych nakladek do kalibracji");
    for (int k = 0; k < 3; k++) {                                                    // warunek sredniej: sum poprawek = 0 (waga 0,01; wiezy osnowy maja wage 100)
      double sum = 0; for (int i = 0; i < S; i++) sum += u_[3 * i + k];
      for (int i = 0; i < S; i++) { g[3 + 3 * i + k] += 0.01 * sum; for (int j = 0; j < S; j++) N[(size_t)(3 + 3 * i + k) * D + 3 + 3 * j + k] += 0.01; }
    }
    double dmax = 0; for (int k = 0; k < 3; k++) dmax = (std::max)(dmax, N[(size_t)k * D + k]);
    for (int i = 0; i < S; i++) { g[3 + 3 * i] += 1e-4 * u_[3 * i]; g[4 + 3 * i] += 1e-4 * u_[3 * i + 1]; g[5 + 3 * i] += 1e-9 * u_[3 * i + 2]; }   // slaby a priori: poprawki ~ 0 (spojny z regularyzacja nizej)
    std::vector<double> A = N, rhs(D); for (int i = 0; i < D; i++) rhs[i] = -g[i];
    for (int k = 0; k < 3; k++) {
      if (active_[k]) A[(size_t)k * D + k] += 1e-3 * N[(size_t)k * D + k] + 1e-12 * dmax;
      else { for (int j = 0; j < D; j++) { A[(size_t)k * D + j] = 0; A[(size_t)j * D + k] = 0; } A[(size_t)k * D + k] = 1; rhs[k] = 0; }
    }
    for (int i = 0; i < S; i++) { A[(size_t)(3 + 3 * i) * D + 3 + 3 * i] += 1e-4; A[(size_t)(4 + 3 * i) * D + 4 + 3 * i] += 1e-4; A[(size_t)(5 + 3 * i) * D + 5 + 3 * i] += 1e-9; }   // poziome poprawki bez nachylenia zostaja ~0
    std::vector<double> d = rhs; if (!alsadj::Solve(A, d, D)) return Err("Kalibracja: uklad osobliwy");
    double stepA = 0, stepU = 0;
    for (int k = 0; k < 3; k++) if (active_[k]) { cal_[k] += d[k]; stepA = (std::max)(stepA, std::fabs(d[k])); }
    for (int i = 0; i < 3 * S; i++) { u_[i] += d[3 + i]; stepU = (std::max)(stepU, std::fabs(d[3 + i])); }
    calIter_++;
    if (!((stepA < 2e-4 && stepU < 2e-4) || calIter_ >= (stage_ == 0 ? 12 : 8))) return true;
    // zbieznosc: niepewnosci z odwrotnosci macierzy normalnej
    int na = 0; for (int k = 0; k < 3; k++) if (active_[k]) na++;
    const double s0 = (std::max)(0.002, std::sqrt(rss / (std::max)(1.0, (double)used - na - 3.0 * S))); std::vector<double> inv;
    if (!alsadj::Invert(A, D, inv)) return Err("Kalibracja: nie mozna wyznaczyc niepewnosci");
    bool anyDrop = false;
    for (int k = 0; k < 3; k++) {
      if (active_[k]) res_.sigma[k] = s0 * std::sqrt((std::max)(0.0, inv[(size_t)k * D + k]));
      const bool det = active_[k] && res_.sigma[k] <= 0.05 && N[(size_t)k * D + k] > 1e-9 * (std::max)(dmax, 1e-30);
      if (stage_ == 0 && !det) { anyDrop = true; res_.sigma[k] = (std::max)(res_.sigma[k], 1.0); active_[k] = false; cal_[k] = 0; }
      res_.determined[k] = det;
    }
    if (stage_ == 0 && anyDrop) { stage_ = 1; calIter_ = 0; return true; }           // druga runda: tylko wyznaczalne katy, reszta 0
    for (int k = 0; k < 3; k++) { res_.bore[k] = res_.determined[k] ? cal_[k] : 0.0; if (!res_.determined[k]) cal_[k] = 0; }
    res_.shifts.clear();
    for (int i = 0; i < S; i++) {
      StripShift sh; sh.strip = ids_[i]; sh.dE = u_[3 * i]; sh.dN = u_[3 * i + 1]; sh.dH = u_[3 * i + 2];
      sh.sigE = s0 * std::sqrt((std::max)(0.0, inv[(size_t)(3 + 3 * i) * D + 3 + 3 * i])); sh.sigN = s0 * std::sqrt((std::max)(0.0, inv[(size_t)(4 + 3 * i) * D + 4 + 3 * i])); sh.sigH = s0 * std::sqrt((std::max)(0.0, inv[(size_t)(5 + 3 * i) * D + 5 + 3 * i]));
      sh.detH = sh.sigH <= 0.05; sh.detEN = sh.sigE <= 0.1 && sh.sigN <= 0.1;
      if (!sh.detEN) { sh.dE = 0; sh.dN = 0; u_[3 * i] = 0; u_[3 * i + 1] = 0; }
      if (!sh.detH) { sh.dH = 0; u_[3 * i + 2] = 0; }
      res_.shifts.push_back(sh);
    }
    const Rot3 Rb = RotFromRpy(res_.bore[0], res_.bore[1], res_.bore[2]); std::vector<Obs> oz, of; Eval(Rb, std::vector<double>(3 * S, 0.0), oz); Eval(Rb, u_, of);
    std::vector<double> ra; for (const Obs& o : oz) if (o.sj >= 0) ra.push_back(o.r); res_.rmsCalibrated = Rms(ra);
    ra.clear(); for (const Obs& o : of) if (o.sj >= 0) ra.push_back(o.r); res_.rmsAfter = Rms(ra);
    shiftOf_.clear(); for (const StripShift& sh : res_.shifts) shiftOf_[sh.strip] = Vec3{sh.dE, sh.dN, sh.dH};
    in_.close(); in_.clear(); in_.open(raw_, std::ios::binary); pos_ = 0;
    if (!wr_.Open(out_)) return Err("Nie mozna zapisac wyniku wyrownania");
    phase_ = Phase::Write; return true;
  }

  template <class Over> void StepWrite(Over over) {                              // ponowne georeferowanie wszystkich punktow z kalibracja i przesunieciami szeregow
    const Rot3 Rb = RotFromRpy(res_.bore[0], res_.bore[1], res_.bore[2]); AlsPoint p; int n = 0;
    while (pos_ < total_) {
      if (!alsdetail::ReadRec(in_, p)) { pos_ = total_; break; }
      pos_++;
      const Tie t = MakeTie(p, 0); const Vec3 g = Georef(t, Rb); Vec3 sh{0, 0, 0}; auto it = shiftOf_.find(p.strip); if (it != shiftOf_.end()) sh = it->second;
      p.x = (float)(g.x + sh.x); p.y = (float)(g.y + sh.y); p.z = (float)(g.z + sh.z); wr_.Append(p);
      if ((++n & 4095) == 0 && over()) return;
    }
    wr_.Close(); in_.close(); chk_ = cps_.empty() ? 2 : 0; phase_ = Phase::Check;
  }

  void StepCheck() {                                                              // odchylki na punktach osnowy: surowa chmura i wynik
    if (chk_ == 0) { res_.ctrlBefore = AlsControlCheck(raw_, oN_, oE_, oH_, cps_); chk_ = 1; return; }
    if (chk_ == 1) { res_.ctrlAfter = AlsControlCheck(out_, oN_, oE_, oH_, cps_); chk_ = 2; return; }
    res_.ok = true; phase_ = Phase::Done;
  }

  std::string raw_, csv_, out_; double oN_, oE_, oH_; std::vector<ControlPoint> cps_; AdjustParams ap_;
  AdjustResult res_; Phase phase_ = Phase::Start; std::ifstream in_; long long total_ = 0, pos_ = 0;
  std::vector<EstSample> traj_; std::set<int> stripsSeen_;
  std::unordered_map<long long, std::vector<std::pair<int, int>>> counts_; std::unordered_map<long long, int> cellIdx_, seen_;
  std::vector<Tie> ties_, cties_; std::vector<Group> groups_, cgroups_; std::unordered_map<long long, int> cseen_; std::map<int, Vec3> shiftOf_; AlsRawWriter wr_; int chk_ = 0; std::vector<int> ids_, gIdx_; std::vector<double> u_;
  double cal_[3] = {0, 0, 0}; bool active_[3] = {true, true, true}; int calIter_ = 0, stage_ = 0;
};
