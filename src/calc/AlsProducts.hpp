// Produkty z chmury ALS (bez Gothic API): klasyfikacja punktow (grunt / roslinnosc niska, srednia i wysoka), NMT i NMPT (siatka ESRI ASCII), warstwice (CSV i DXF)
// oraz objetosci wzgledem poziomu odniesienia. Wejscie: surowy plik rekordow 34 B (AlsRawWriter). Praca w krokach (Step z budzetem czasu), strumieniowe przebiegi po pliku.
// Klasyfikacja: powierzchnia gruntu = otwarcie morfologiczne (filtr minimum, potem maksimum, okno 2*openRadius+1 komorek) na siatce minimow; wysokosc nad nia
// <= groundTol = grunt (2), do 1 m = roslinnosc niska (3), do 5 m = srednia (4), wyzej = wysoka / obiekty (5). Budynki nie sa rozpoznawane osobno.
#pragma once
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <limits>
#include <string>
#include <vector>
#include "calc/AlsLas.hpp"

struct ProductsParams { double cellM = 1.0, groundTol = 0.3, contourStepM = 1.0; int openRadius = 10; };

struct ProductsResult {
  bool ok = false; std::string msg; long long count = 0, cls[6] = {0, 0, 0, 0, 0, 0};          // liczba punktow; licznosc klas 2..5 (ASPRS)
  int ncols = 0, nrows = 0; double refLevel = 0, volCut = 0, volFill = 0, volAboveMin = 0, minZ = 0, maxZ = 0; int contours = 0;
  std::vector<std::uint8_t> classes;                                                          // klasa kazdego punktu w kolejnosci pliku
};

class AlsProducts {
 public:
  AlsProducts(const std::string& rawPath, const std::string& outDir, double originN, double originE, double originH, ProductsParams pp = ProductsParams())
      : raw_(rawPath), dir_(outDir), oN_(originN), oE_(originE), oH_(originH), pp_(pp) {}

  bool Step(int budgetMs) {                                                                    // true = zakonczone (wynik: Result().ok)
    if (phase_ == Phase::Done) return true;
    const auto t0 = std::chrono::steady_clock::now();
    auto over = [&]() { return std::chrono::steady_clock::now() - t0 > std::chrono::milliseconds(budgetMs); };
    switch (phase_) {
      case Phase::Start: Begin(); break;
      case Phase::Bounds: Scan(over, 0); break;
      case Phase::Grid: Scan(over, 1); break;
      case Phase::Classify: Scan(over, 2); break;
      case Phase::Final: Finish(); break;
      default: break;
    }
    return phase_ == Phase::Done;
  }
  const ProductsResult& Result() const { return res_; }
  std::string Progress() const {
    static const char* names[] = {"start", "zakres chmury", "siatka terenu", "klasyfikacja", "NMT, warstwice, objetosci", "koniec"};
    char b[96]; const int i = (int)phase_; if (i >= 1 && i <= 3) std::snprintf(b, sizeof b, "%s %d%%", names[i], total_ ? (int)(100.0 * pos_ / total_) : 0); else std::snprintf(b, sizeof b, "%s", names[i]); return b;
  }

 private:
  enum class Phase { Start, Bounds, Grid, Classify, Final, Done };
  void Fail(const std::string& m) { res_.ok = false; res_.msg = m; phase_ = Phase::Done; }
  void Reopen(Phase next) { in_.close(); in_.clear(); in_.open(raw_, std::ios::binary); pos_ = 0; phase_ = next; }

  void Begin() {
    std::error_code ec; const auto sz = std::filesystem::file_size(raw_, ec);
    if (ec || sz < (std::uintmax_t)kAlsRawBytes) { Fail("Brak chmury punktow (plik surowy lotu) - nie mozna policzyc produktow"); return; }
    total_ = (long long)(sz / kAlsRawBytes); res_.count = total_; Reopen(Phase::Bounds);
  }

  template <class Over> void Scan(Over over, int pass) {
    AlsPoint p; int n = 0;
    while (pos_ < total_) {
      if (!alsdetail::ReadRec(in_, p)) { pos_ = total_; break; }
      if (pass == 0) { if (pos_ == 0) { minX_ = maxX_ = p.x; minY_ = maxY_ = p.y; } minX_ = (std::min)(minX_, (double)p.x); maxX_ = (std::max)(maxX_, (double)p.x); minY_ = (std::min)(minY_, (double)p.y); maxY_ = (std::max)(maxY_, (double)p.y); }
      else if (pass == 1) { const long long c = Cell(p); if (c >= 0) { gmin_[c] = (std::min)(gmin_[c], p.z); gmax_[c] = (std::max)(gmax_[c], p.z); } }
      else Classify(p);
      pos_++;
      if ((++n & 4095) == 0 && over()) return;
    }
    if (pass == 0) EndBounds(); else if (pass == 1) EndGrid(); else { Reopen(Phase::Final); }
  }

  long long Cell(const AlsPoint& p) const {
    const int c = (int)std::floor((p.x - minX_) / cs_), r = (int)std::floor((p.y - minY_) / cs_);
    return (c < 0 || r < 0 || c >= nc_ || r >= nr_) ? -1 : (long long)r * nc_ + c;
  }

  void EndBounds() {
    cs_ = pp_.cellM;                                                                            // siatka nie wieksza niz ok. 4 mln komorek
    while (((maxX_ - minX_) / cs_ + 1) * ((maxY_ - minY_) / cs_ + 1) > 4e6) cs_ *= 2;
    nc_ = (int)std::floor((maxX_ - minX_) / cs_) + 1; nr_ = (int)std::floor((maxY_ - minY_) / cs_) + 1; res_.ncols = nc_; res_.nrows = nr_;
    const size_t n = (size_t)nc_ * nr_; const float inf = std::numeric_limits<float>::infinity();
    gmin_.assign(n, inf); gmax_.assign(n, -inf); gsum_.assign(n, 0.0); gcnt_.assign(n, 0); res_.classes.reserve((size_t)total_);
    Reopen(Phase::Grid);
  }

  // Filtr minimum / maksimum o oknie (2r+1)^2 (rozdzielnie); komorki puste pomijane.
  static void Filter(std::vector<float>& g, int nc, int nr, int r, bool wantMin) {
    const float none = wantMin ? std::numeric_limits<float>::infinity() : -std::numeric_limits<float>::infinity();
    auto better = [&](float a, float b) { return wantMin ? a < b : a > b; };
    std::vector<float> t(g.size(), none);
    for (int y = 0; y < nr; y++) for (int x = 0; x < nc; x++) { float v = none; for (int k = (std::max)(0, x - r); k <= (std::min)(nc - 1, x + r); k++) if (better(g[(size_t)y * nc + k], v)) v = g[(size_t)y * nc + k]; t[(size_t)y * nc + x] = v; }
    for (int y = 0; y < nr; y++) for (int x = 0; x < nc; x++) { float v = none; for (int k = (std::max)(0, y - r); k <= (std::min)(nr - 1, y + r); k++) if (better(t[(size_t)k * nc + x], v)) v = t[(size_t)k * nc + x]; g[(size_t)y * nc + x] = v; }
  }

  // Siatka (nc x nr) dopelniona o r komorek z kazdej strony odbiciem punktowym wzgledem brzegu: z(-t) = 2 z(0) - z(t) (puste = +inf zostaja puste).
  static std::vector<float> PadPoint(const std::vector<float>& g, int nc, int nr, int r) {
    const float inf = std::numeric_limits<float>::infinity(); const int pw = nc + 2 * r, ph = nr + 2 * r; std::vector<float> p((size_t)pw * ph, inf);
    for (int y = 0; y < nr; y++) for (int x = 0; x < nc; x++) p[(size_t)(y + r) * pw + x + r] = g[(size_t)y * nc + x];
    auto refl = [&](float edge, float inner) { return (std::isfinite(edge) && std::isfinite(inner)) ? 2.f * edge - inner : inf; };
    for (int y = r; y < r + nr; y++) for (int t = 1; t <= r; t++) {
      p[(size_t)y * pw + r - t] = refl(p[(size_t)y * pw + r], p[(size_t)y * pw + r + (std::min)(t, nc - 1)]);
      p[(size_t)y * pw + r + nc - 1 + t] = refl(p[(size_t)y * pw + r + nc - 1], p[(size_t)y * pw + r + nc - 1 - (std::min)(t, nc - 1)]);
    }
    for (int x = 0; x < pw; x++) for (int t = 1; t <= r; t++) {
      p[(size_t)(r - t) * pw + x] = refl(p[(size_t)r * pw + x], p[(size_t)(r + (std::min)(t, nr - 1)) * pw + x]);
      p[(size_t)(r + nr - 1 + t) * pw + x] = refl(p[(size_t)(r + nr - 1) * pw + x], p[(size_t)(r + nr - 1 - (std::min)(t, nr - 1)) * pw + x]);
    }
    return p;
  }

  void EndGrid() {
    {                                                                                           // otwarcie morfologiczne: powierzchnia gruntu; siatka dopelniona odbiciem punktowym (zachowuje trend, brzeg nie "spoznia sie" na stoku)
      const int r = pp_.openRadius, pw = nc_ + 2 * r, ph = nr_ + 2 * r; std::vector<float> pad = PadPoint(gmin_, nc_, nr_, r);
      Filter(pad, pw, ph, r, true); Filter(pad, pw, ph, r, false);
      surf_.assign((size_t)nc_ * nr_, 0.f); for (int y = 0; y < nr_; y++) for (int x = 0; x < nc_; x++) surf_[(size_t)y * nc_ + x] = pad[(size_t)(y + r) * pw + x + r];
    }
    const float inf = std::numeric_limits<float>::infinity();                                    // komorki bez powierzchni: srednia sasiadow (iteracyjnie)
    for (int it = 0; it < 60; it++) {
      bool any = false; std::vector<float> nx = surf_;
      for (int y = 0; y < nr_; y++) for (int x = 0; x < nc_; x++) {
        if (std::isfinite(surf_[(size_t)y * nc_ + x])) continue;
        double s = 0; int c = 0;
        for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) { const int xx = x + dx, yy = y + dy; if (xx < 0 || yy < 0 || xx >= nc_ || yy >= nr_) continue; const float v = surf_[(size_t)yy * nc_ + xx]; if (std::isfinite(v)) { s += v; c++; } }
        if (c) nx[(size_t)y * nc_ + x] = (float)(s / c); else any = true;
      }
      surf_.swap(nx); if (!any) break;
    }
    (void)inf; Reopen(Phase::Classify);
  }

  void Classify(const AlsPoint& p) {
    const long long c = Cell(p); std::uint8_t k = 2;
    if (c >= 0) {
      const double h = p.z - surf_[c];
      k = h <= pp_.groundTol ? 2 : h <= 1.0 ? 3 : h <= 5.0 ? 4 : 5;
      if (k == 2) { gsum_[c] += p.z; gcnt_[c]++; }
    }
    res_.classes.push_back(k); res_.cls[k]++;
  }

  void Finish() {
    const size_t n = (size_t)nc_ * nr_; dtm_.assign(n, std::nanf("")); dsm_.assign(n, std::nanf(""));
    double zmin = 1e18, zmax = -1e18, sum = 0; size_t cnt = 0;
    for (size_t i = 0; i < n; i++) {
      if (gcnt_[i] > 0) dtm_[i] = (float)(gsum_[i] / gcnt_[i]); else if (std::isfinite(surf_[i])) dtm_[i] = surf_[i];
      if (std::isfinite(gmax_[i])) dsm_[i] = gmax_[i];
      if (std::isfinite(dtm_[i])) { zmin = (std::min)(zmin, (double)dtm_[i]); zmax = (std::max)(zmax, (double)dtm_[i]); sum += dtm_[i]; cnt++; }
    }
    if (!cnt) { Fail("Brak powierzchni terenu - za malo punktow"); return; }
    const double A = cs_ * cs_; res_.refLevel = sum / cnt + oH_; res_.minZ = zmin + oH_; res_.maxZ = zmax + oH_;
    for (size_t i = 0; i < n; i++) if (std::isfinite(dtm_[i])) { const double z = dtm_[i] + oH_; res_.volAboveMin += (z - res_.minZ) * A; if (z > res_.refLevel) res_.volCut += (z - res_.refLevel) * A; else res_.volFill += (res_.refLevel - z) * A; }
    WriteGrid(dir_ + "/nmt.asc", dtm_); WriteGrid(dir_ + "/nmpt.asc", dsm_); WriteContours();
    AlsConvert(raw_, oN_, oE_, oH_, "", dir_ + "/lot_klasyfikacja.las", &res_.classes, false);
    WriteReport(); res_.ok = true; phase_ = Phase::Done;
  }

  void WriteGrid(const std::string& path, const std::vector<float>& g) const {                  // ESRI ASCII, wiersze od polnocy; wspolrzedne siatki (E, N)
    std::ofstream f(path); char b[160];
    std::snprintf(b, sizeof b, "ncols %d\nnrows %d\nxllcorner %.3f\nyllcorner %.3f\ncellsize %.3f\nNODATA_value -9999\n", nc_, nr_, oE_ + minX_, oN_ + minY_, cs_); f << b;
    for (int r = nr_ - 1; r >= 0; r--) { for (int c = 0; c < nc_; c++) { const float v = g[(size_t)r * nc_ + c]; if (std::isfinite(v)) std::snprintf(b, sizeof b, "%.3f", v + oH_); else std::snprintf(b, sizeof b, "-9999"); f << b << (c + 1 < nc_ ? " " : ""); } f << "\n"; }
  }

  // Warstwice z NMT: marszowanie kwadratow miedzy srodkami komorek; CSV (H;E1;N1;E2;N2) i DXF (LINE na warstwie WARSTWICE).
  void WriteContours() {
    std::ofstream csv(dir_ + "/warstwice.csv"), dxf(dir_ + "/warstwice.dxf"); csv << "H_m;E1;N1;E2;N2\n"; dxf << "0\nSECTION\n2\nENTITIES\n";
    const double st = pp_.contourStepM; char b[256]; int count = 0;
    auto node = [&](int x, int y, double& e, double& nn) { e = oE_ + minX_ + (x + 0.5) * cs_; nn = oN_ + minY_ + (y + 0.5) * cs_; };
    for (int y = 0; y + 1 < nr_; y++) for (int x = 0; x + 1 < nc_; x++) {
      const int cx[4] = {x, x + 1, x + 1, x}, cy[4] = {y, y, y + 1, y + 1}; double z[4]; bool ok = true;
      for (int k = 0; k < 4; k++) { z[k] = dtm_[(size_t)cy[k] * nc_ + cx[k]] + oH_; if (!std::isfinite(z[k])) ok = false; }
      if (!ok) continue;
      const double lo = (std::min)((std::min)(z[0], z[1]), (std::min)(z[2], z[3])), hi = (std::max)((std::max)(z[0], z[1]), (std::max)(z[2], z[3]));
      for (double L = std::ceil(lo / st) * st; L <= hi + 1e-9; L += st) {
        double pe[4], pn[4]; int m = 0;
        for (int k = 0; k < 4; k++) {                                                            // krawedzie k -> (k+1)%4
          const int j = (k + 1) % 4; if ((z[k] < L) == (z[j] < L)) continue;
          const double t = (L - z[k]) / (z[j] - z[k]); double e1, n1, e2, n2; node(cx[k], cy[k], e1, n1); node(cx[j], cy[j], e2, n2); pe[m] = e1 + (e2 - e1) * t; pn[m] = n1 + (n2 - n1) * t; m++;
        }
        for (int s = 0; s + 1 < m; s += 2) {
          std::snprintf(b, sizeof b, "%.3f;%.3f;%.3f;%.3f;%.3f\n", L, pe[s], pn[s], pe[s + 1], pn[s + 1]); csv << b;
          std::snprintf(b, sizeof b, "0\nLINE\n8\nWARSTWICE\n10\n%.3f\n20\n%.3f\n30\n%.3f\n11\n%.3f\n21\n%.3f\n31\n%.3f\n", pe[s], pn[s], L, pe[s + 1], pn[s + 1], L); dxf << b; count++;
        }
      }
    }
    dxf << "0\nENDSEC\n0\nEOF\n"; res_.contours = count;
  }

  void WriteReport() {
    std::ofstream f(dir_ + "/produkty.txt"); char b[300];
    f << "Produkty z chmury ALS\n";
    std::snprintf(b, sizeof b, "Punktow: %lld; klasy ASPRS: 2 grunt %lld, 3 roslinnosc niska (do 1 m) %lld, 4 srednia (do 5 m) %lld, 5 wysoka lub obiekty %lld\n", res_.count, res_.cls[2], res_.cls[3], res_.cls[4], res_.cls[5]); f << b;
    std::snprintf(b, sizeof b, "NMT i NMPT: siatka %d x %d, komorka %.2f m (nmt.asc, nmpt.asc, uklad siatki N/E, ESRI ASCII)\n", nc_, nr_, cs_); f << b;
    std::snprintf(b, sizeof b, "Warstwice co %.2f m: %d odcinkow (warstwice.csv, warstwice.dxf)\n", pp_.contourStepM, res_.contours); f << b;
    std::snprintf(b, sizeof b, "Wysokosc terenu: min %.3f, max %.3f m; poziom odniesienia (srednia NMT) %.3f m\n", res_.minZ, res_.maxZ, res_.refLevel); f << b;
    std::snprintf(b, sizeof b, "Objetosci wzgledem poziomu odniesienia: wykop (teren powyzej) %.1f m3, nasyp (teren ponizej) %.1f m3; objetosc nad minimum terenu %.1f m3\n", res_.volCut, res_.volFill, res_.volAboveMin); f << b;
    f << "Klasyfikacja: powierzchnia gruntu z otwarcia morfologicznego siatki minimow; budynki nie sa rozpoznawane osobno (klasa 5).\n";
  }

  std::string raw_, dir_; double oN_, oE_, oH_; ProductsParams pp_; ProductsResult res_; Phase phase_ = Phase::Start;
  std::ifstream in_; long long total_ = 0, pos_ = 0; double minX_ = 0, maxX_ = 0, minY_ = 0, maxY_ = 0, cs_ = 1.0; int nc_ = 0, nr_ = 0;
  std::vector<float> gmin_, gmax_, surf_, dtm_, dsm_; std::vector<double> gsum_; std::vector<int> gcnt_;
};
