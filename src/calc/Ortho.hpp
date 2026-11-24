// Ortofotomapa ze zdjec lotniczych (bez Gothic API): NMPT z punktow (interpolacja ważona odległością), potem dla kazdego piksela ortofotomapy wybor zdjecia o najmniejszym odchyleniu od nadiru
// i probkowanie jego koloru przez poprawiona poze (model otworkowy z Camera.hpp). Zdjecia czytane po kolei z dysku (po jednym), praca w krokach (Step z budzetem czasu).
// Wspolrzedne lokalne: x = E, y = N, z = H wzgledem punktu odniesienia; plik pozycjonujacy (.pgw) w ukladzie siatki (z originE, originN).
#pragma once
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <functional>
#include <string>
#include <vector>
#include "calc/Camera.hpp"
#include "calc/Flight.hpp"
#include "calc/Png.hpp"

struct OrthoCam { double p[3] = {0, 0, 0}; double a[3] = {0, 0, 0}; };      // poprawiona poza zdjecia: E, N, H [m] i roll, pitch, yaw [st.] (lokalnie)
struct OrthoParams {
  double f = 200, cx = 160, cy = 120; int imgW = 320, imgH = 240; Vec3 lever{0, 0, 0}; Rot3 Rb = RotFromRpy(0, 0, 0);   // kamera
  double gsd = 0.2, minE = 0, maxE = 1, minN = 0, maxN = 1;                  // piksel i zakres ortofotomapy (lokalnie, E x N)
  std::vector<PlanPt> polygon;                                               // wielokat obszaru (n, e lokalne); pusty = caly prostokat
  double originE = 0, originN = 0; size_t maxCells = 6000000;                // przesuniecie do ukladu siatki (plik .pgw) i limit liczby pikseli
};
struct OrthoResult { bool ok = false; std::string msg; int w = 0, h = 0, used = 0; double gsd = 0, coverage = 0; };

// Blad polozenia punktu na ortofotomapie: promien przez piksel (u, v) poprawiona poza przeciety z plaszczyzna H punktu znanego; roznica wzgledem polozenia znanego.
inline bool OrthoPointError(const OrthoParams& p, const OrthoCam& c, double u, double v, const Vec3& known, double& dE, double& dN) {
  TrajSample ps{}; ps.pos = {c.p[0], c.p[1], c.p[2]}; ps.rollDeg = c.a[0]; ps.pitchDeg = c.a[1]; ps.yawDeg = c.a[2];
  const AlsRay r = PixelRay(ps, p.Rb, p.lever, u, v, p.f, p.cx, p.cy); if (r.dir.z >= -1e-9) return false;
  const double t = (known.z - r.origin.z) / r.dir.z; if (t <= 0) return false;
  dE = r.origin.x + t * r.dir.x - known.x; dN = r.origin.y + t * r.dir.y - known.y; return true;
}

class Orthorectifier {
 public:
  using Loader = std::function<bool(int cam, std::vector<unsigned char>& rgb)>;
  Orthorectifier(OrthoParams p, std::vector<OrthoCam> cams, std::vector<Vec3> dsmPts, Loader load, std::string pngPath, std::string pgwPath)
      : p_(std::move(p)), cams_(std::move(cams)), pts_(std::move(dsmPts)), load_(std::move(load)), png_(std::move(pngPath)), pgw_(std::move(pgwPath)) {}

  bool Step(int budgetMs) {                                                   // true = zakonczone (wynik: Result().ok)
    if (phase_ == Phase::Done) return true;
    const auto t0 = std::chrono::steady_clock::now();
    auto over = [&]() { return std::chrono::steady_clock::now() - t0 > std::chrono::milliseconds(budgetMs); };
    switch (phase_) {
      case Phase::Start: Begin(); break;
      case Phase::Dsm: BuildDsm(); phase_ = Phase::Assign; break;
      case Phase::Assign: StepAssign(over); break;
      case Phase::Paint: StepPaint(over); break;
      case Phase::Write: WriteOut(); break;
      default: break;
    }
    return phase_ == Phase::Done;
  }
  const OrthoResult& Result() const { return res_; }
  std::string Progress() const {
    static const char* names[] = {"start", "NMPT", "wybor zdjec", "kolory", "zapis", "koniec"}; char b[96]; const int i = (int)phase_;
    if (i == 2) std::snprintf(b, sizeof b, "%s %d%%", names[i], res_.h ? 100 * row_ / res_.h : 0); else if (i == 3) std::snprintf(b, sizeof b, "%s %d/%d zdjec", names[i], ci_, (int)cams_.size()); else std::snprintf(b, sizeof b, "%s", names[i]);
    return b;
  }

 private:
  enum class Phase { Start, Dsm, Assign, Paint, Write, Done };
  struct CamM { double m[9], off[3], pos[3]; };
  void Fail(const std::string& m) { res_.ok = false; res_.msg = m; phase_ = Phase::Done; }

  void Begin() {
    if (cams_.empty()) { Fail("Brak zdjec do ortofotomapy"); return; }
    if (pts_.empty()) { Fail("Brak punktow do NMPT (potrzebne wyrownanie wiazki)"); return; }
    double gsd = p_.gsd; int w = (int)std::ceil((p_.maxE - p_.minE) / gsd), h = (int)std::ceil((p_.maxN - p_.minN) / gsd);
    if (w < 1 || h < 1) { Fail("Pusty obszar ortofotomapy"); return; }
    if ((double)w * h > (double)p_.maxCells) { gsd *= std::sqrt((double)w * h / (double)p_.maxCells); w = (int)std::ceil((p_.maxE - p_.minE) / gsd); h = (int)std::ceil((p_.maxN - p_.minN) / gsd); }
    res_.w = w; res_.h = h; res_.gsd = gsd;
    for (const OrthoCam& c : cams_) {
      const Rot3 M = Transpose(MulR(RotFromRpy(c.a[0], c.a[1], c.a[2]), p_.Rb)); const Vec3 off = Mul(Transpose(p_.Rb), p_.lever); CamM k{};
      for (int i = 0; i < 9; i++) k.m[i] = M.m[i];
      k.off[0] = off.x; k.off[1] = off.y; k.off[2] = off.z; k.pos[0] = c.p[0]; k.pos[1] = c.p[1]; k.pos[2] = c.p[2]; cm_.push_back(k);
    }
    as_.assign((size_t)w * h, -1); out_.assign((size_t)w * h * 3, 0x20); phase_ = Phase::Dsm;
  }

  void BuildDsm() {                                                          // wezly co dc: srednia wazona 1/d^2 z punktow w otoczeniu (promien rosnie, gdy za malo punktow)
    dc_ = (std::max)(2.0, 4.0 * res_.gsd); nx_ = (int)std::floor((p_.maxE - p_.minE) / dc_) + 2; ny_ = (int)std::floor((p_.maxN - p_.minN) / dc_) + 2;
    std::vector<std::vector<int>> bucket((size_t)nx_ * ny_); double mean = 0;
    auto cellOf = [&](double e, double n, int& ix, int& iy) { ix = (std::max)(0, (std::min)(nx_ - 1, (int)std::floor((e - p_.minE) / dc_))); iy = (std::max)(0, (std::min)(ny_ - 1, (int)std::floor((n - p_.minN) / dc_))); };
    for (size_t i = 0; i < pts_.size(); i++) { int ix, iy; cellOf(pts_[i].x, pts_[i].y, ix, iy); bucket[(size_t)iy * nx_ + ix].push_back((int)i); mean += pts_[i].z; }
    mean /= (double)pts_.size(); dsm_.assign((size_t)nx_ * ny_, (float)mean);
    for (int j = 0; j < ny_; j++) for (int i = 0; i < nx_; i++) {
      const double e = p_.minE + i * dc_, n = p_.minN + j * dc_; double sw = 0, sz = 0; int cnt = 0;
      for (int k = 3; k <= 24 && cnt < 3; k *= 2) {
        sw = sz = 0; cnt = 0; int cx, cy; cellOf(e, n, cx, cy);
        for (int y = (std::max)(0, cy - k); y <= (std::min)(ny_ - 1, cy + k); y++) for (int x = (std::max)(0, cx - k); x <= (std::min)(nx_ - 1, cx + k); x++)
          for (int id : bucket[(size_t)y * nx_ + x]) { const double d2 = std::pow(pts_[id].x - e, 2) + std::pow(pts_[id].y - n, 2) + 1e-3; sw += 1.0 / d2; sz += pts_[id].z / d2; cnt++; }
      }
      if (cnt > 0) dsm_[(size_t)j * nx_ + i] = (float)(sz / sw);
    }
  }
  double DsmAt(double e, double n) const {
    const double fx = (std::max)(0.0, (std::min)((double)(nx_ - 1) - 1e-9, (e - p_.minE) / dc_)), fy = (std::max)(0.0, (std::min)((double)(ny_ - 1) - 1e-9, (n - p_.minN) / dc_));
    const int i = (int)fx, j = (int)fy; const double tx = fx - i, ty = fy - j; auto g = [&](int a, int b) { return (double)dsm_[(size_t)b * nx_ + a]; };
    return (g(i, j) * (1 - tx) + g(i + 1, j) * tx) * (1 - ty) + (g(i, j + 1) * (1 - tx) + g(i + 1, j + 1) * tx) * ty;
  }
  bool Inside(double n, double e) const {
    if (p_.polygon.empty()) return true;
    bool in = false; const auto& a = p_.polygon;
    for (size_t i = 0, j = a.size() - 1; i < a.size(); j = i++) if (((a[i].n > n) != (a[j].n > n)) && (e < (a[j].e - a[i].e) * (n - a[i].n) / (a[j].n - a[i].n) + a[i].e)) in = !in;
    return in;
  }
  bool Project(int c, double e, double n, double h, double& u, double& v, double& metric) const {
    const CamM& k = cm_[c]; const double dx = e - k.pos[0], dy = n - k.pos[1], dz = h - k.pos[2];
    const double x = k.m[0] * dx + k.m[1] * dy + k.m[2] * dz - k.off[0], y = k.m[3] * dx + k.m[4] * dy + k.m[5] * dz - k.off[1], z = k.m[6] * dx + k.m[7] * dy + k.m[8] * dz - k.off[2];
    if (z > -1e-6) return false;
    const double inv = -1.0 / z; u = p_.cx + p_.f * x * inv; v = p_.cy - p_.f * y * inv; metric = (x * x + y * y) * inv * inv; return true;
  }

  template <class Over> void StepAssign(Over over) {                          // dla kazdego piksela w wielokacie: zdjecie najblizej nadiru, ktore go obejmuje
    while (row_ < res_.h) {
      for (int x = 0; x < res_.w; x++) {
        const double e = p_.minE + (x + 0.5) * res_.gsd, n = p_.maxN - (row_ + 0.5) * res_.gsd; if (!Inside(n, e)) continue; inside_++;
        const double h = DsmAt(e, n); double best = 1e300; int bi = -1;
        for (size_t c = 0; c < cm_.size(); c++) { double u, v, m; if (!Project((int)c, e, n, h, u, v, m)) continue; if (u < 1 || v < 1 || u > p_.imgW - 2 || v > p_.imgH - 2) continue; if (m < best) { best = m; bi = (int)c; } }
        as_[(size_t)row_ * res_.w + x] = (short)bi;
      }
      row_++; if (over()) return;
    }
    ci_ = 0; phase_ = Phase::Paint;
  }

  template <class Over> void StepPaint(Over over) {                           // zdjecia po kolei: wczytanie z dysku i kolory pikseli, ktore do niego naleza (interpolacja dwuliniowa)
    while (ci_ < (int)cams_.size()) {
      bool any = false; for (short a : as_) if (a == ci_) { any = true; break; }
      if (any) {
        std::vector<unsigned char> img; if (!load_(ci_, img) || img.size() != (size_t)p_.imgW * p_.imgH * 3) { Fail("Nie mozna wczytac zdjecia nr " + std::to_string(ci_ + 1)); return; }
        for (int y = 0; y < res_.h; y++) for (int x = 0; x < res_.w; x++) {
          const size_t idx = (size_t)y * res_.w + x; if (as_[idx] != ci_) continue;
          const double e = p_.minE + (x + 0.5) * res_.gsd, n = p_.maxN - (y + 0.5) * res_.gsd; double u, v, m; if (!Project(ci_, e, n, DsmAt(e, n), u, v, m)) continue;
          const double px = u - 0.5, py = v - 0.5; const int x0 = (int)std::floor(px), y0 = (int)std::floor(py); const double tx = px - x0, ty = py - y0;
          for (int ch = 0; ch < 3; ch++) {
            auto s = [&](int a, int b) { return (double)img[((size_t)(std::max)(0, (std::min)(p_.imgH - 1, b)) * p_.imgW + (std::max)(0, (std::min)(p_.imgW - 1, a))) * 3 + ch]; };
            const double c = (s(x0, y0) * (1 - tx) + s(x0 + 1, y0) * tx) * (1 - ty) + (s(x0, y0 + 1) * (1 - tx) + s(x0 + 1, y0 + 1) * tx) * ty;
            out_[idx * 3 + ch] = (unsigned char)(std::max)(0.0, (std::min)(255.0, c + 0.5));
          }
          painted_++;
        }
        res_.used++;
      }
      ci_++; if (over()) return;
    }
    phase_ = Phase::Write;
  }

  void WriteOut() {
    for (size_t i = 0; i < as_.size(); i++) if (as_[i] < 0) { const int x = (int)(i % res_.w), y = (int)(i / res_.w); const double e = p_.minE + (x + 0.5) * res_.gsd, n = p_.maxN - (y + 0.5) * res_.gsd; if (Inside(n, e)) for (int ch = 0; ch < 3; ch++) out_[i * 3 + ch] = 0x60; }
    res_.coverage = inside_ ? (double)painted_ / (double)inside_ : 0.0;
    if (!WritePng(png_, res_.w, res_.h, out_)) { Fail("Nie mozna zapisac ortofotomapy"); return; }
    std::ofstream f(pgw_); char b[200];
    std::snprintf(b, sizeof b, "%.6f\n0\n0\n%.6f\n%.6f\n%.6f\n", res_.gsd, -res_.gsd, p_.originE + p_.minE + res_.gsd / 2.0, p_.originN + p_.maxN - res_.gsd / 2.0); f << b;
    res_.ok = f.good(); if (!res_.ok) res_.msg = "Nie mozna zapisac pliku pozycjonujacego"; phase_ = Phase::Done;
  }

  OrthoParams p_; std::vector<OrthoCam> cams_; std::vector<Vec3> pts_; Loader load_; std::string png_, pgw_; OrthoResult res_; Phase phase_ = Phase::Start;
  std::vector<CamM> cm_; std::vector<short> as_; std::vector<unsigned char> out_; std::vector<float> dsm_; double dc_ = 2.0; int nx_ = 0, ny_ = 0, row_ = 0, ci_ = 0; long long inside_ = 0, painted_ = 0;
};
