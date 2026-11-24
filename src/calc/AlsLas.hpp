// Zapis chmury ALS (bez Gothic API): surowy plik tymczasowy -> .pts i LAS 1.2 (format punktu 3: RGB + czas GPS), kontrola szeregow i osnowy.
// Rekord surowy 26 B: x, y, z (float, E/N/H wzgledem punktu odniesienia), czas GPS (double), numer szeregu (u16), intensywnosc, R, G, B.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <string>
#include <utility>
#include <vector>
#include "calc/Control.hpp"
#include "calc/Scan.hpp"   // scandetail::PutLE / PutD

struct AlsPoint { float x, y, z; double gps; std::uint16_t strip; std::uint8_t i, r, g, b; };

class AlsRawWriter {
 public:
  bool Open(const std::string& path) { out_.open(path, std::ios::binary | std::ios::trunc); count_ = 0; return out_.good(); }
  void Append(const AlsPoint& p) {
    out_.write(reinterpret_cast<const char*>(&p.x), 4); out_.write(reinterpret_cast<const char*>(&p.y), 4); out_.write(reinterpret_cast<const char*>(&p.z), 4);
    out_.write(reinterpret_cast<const char*>(&p.gps), 8); out_.write(reinterpret_cast<const char*>(&p.strip), 2);
    const std::uint8_t t[4] = {p.i, p.r, p.g, p.b}; out_.write(reinterpret_cast<const char*>(t), 4); count_++;
  }
  void Close() { if (out_.is_open()) out_.close(); }
  long long Count() const { return count_; }
 private:
  std::ofstream out_; long long count_ = 0;
};

struct AlsStats { long long count = 0; double minE = 0, minN = 0, minH = 0, maxE = 0, maxN = 0, maxH = 0; bool ok = false; };

namespace alsdetail {
inline bool ReadRec(std::ifstream& in, AlsPoint& p) {
  in.read(reinterpret_cast<char*>(&p.x), 4); in.read(reinterpret_cast<char*>(&p.y), 4); in.read(reinterpret_cast<char*>(&p.z), 4);
  in.read(reinterpret_cast<char*>(&p.gps), 8); in.read(reinterpret_cast<char*>(&p.strip), 2);
  std::uint8_t t[4]; in.read(reinterpret_cast<char*>(t), 4); p.i = t[0]; p.r = t[1]; p.g = t[2]; p.b = t[3];
  return in.good();
}
}  // namespace alsdetail

inline AlsStats AlsConvert(const std::string& rawPath, double originN, double originE, double originH, const std::string& ptsPath, const std::string& lasPath) {
  AlsStats st; AlsPoint p;
  {
    std::ifstream in(rawPath, std::ios::binary);
    while (alsdetail::ReadRec(in, p)) {
      const double E = originE + p.x, N = originN + p.y, H = originH + p.z;
      if (!st.ok) { st.minE = st.maxE = E; st.minN = st.maxN = N; st.minH = st.maxH = H; st.ok = true; }
      st.minE = (std::min)(st.minE, E); st.maxE = (std::max)(st.maxE, E); st.minN = (std::min)(st.minN, N); st.maxN = (std::max)(st.maxN, N); st.minH = (std::min)(st.minH, H); st.maxH = (std::max)(st.maxH, H);
      st.count++;
    }
  }
  const double scale = 0.001, offX = std::floor(st.minE), offY = std::floor(st.minN), offZ = std::floor(st.minH);
  std::ofstream pts(ptsPath, std::ios::binary | std::ios::trunc), las(lasPath, std::ios::binary | std::ios::trunc);
  pts << st.count << "\n";
  std::string h;                                                       // naglowek LAS 1.2 (227 bajtow), format punktu 3, rekord 34 B
  h += "LASF"; scandetail::PutLE(h, 0, 2); scandetail::PutLE(h, 0, 2); scandetail::PutLE(h, 0, 4); scandetail::PutLE(h, 0, 2); scandetail::PutLE(h, 0, 2); scandetail::PutLE(h, 0, 8);
  h += (char)1; h += (char)2;
  { char sys[32] = {0}, sw[32] = {0}; std::snprintf(sys, sizeof sys, "GothicSurveying"); std::snprintf(sw, sizeof sw, "GothicSurveying ALS"); h.append(sys, 32); h.append(sw, 32); }
  scandetail::PutLE(h, 1, 2); scandetail::PutLE(h, 2025, 2);
  scandetail::PutLE(h, 227, 2); scandetail::PutLE(h, 227, 4); scandetail::PutLE(h, 0, 4);
  h += (char)3; scandetail::PutLE(h, 34, 2);
  scandetail::PutLE(h, (std::uint64_t)st.count, 4); for (int i = 0; i < 5; i++) scandetail::PutLE(h, i == 0 ? (std::uint64_t)st.count : 0, 4);
  for (int i = 0; i < 3; i++) scandetail::PutD(h, scale);
  scandetail::PutD(h, offX); scandetail::PutD(h, offY); scandetail::PutD(h, offZ);
  scandetail::PutD(h, st.maxE); scandetail::PutD(h, st.minE); scandetail::PutD(h, st.maxN); scandetail::PutD(h, st.minN); scandetail::PutD(h, st.maxH); scandetail::PutD(h, st.minH);
  las.write(h.data(), (std::streamsize)h.size());
  std::ifstream in(rawPath, std::ios::binary); std::string rec; char line[128];
  while (alsdetail::ReadRec(in, p)) {
    const double E = originE + p.x, N = originN + p.y, H = originH + p.z;
    std::snprintf(line, sizeof line, "%.3f %.3f %.3f %d %d %d %d\n", E, N, H, (int)p.i, (int)p.r, (int)p.g, (int)p.b); pts << line;
    rec.clear();
    scandetail::PutLE(rec, (std::uint32_t)(std::int32_t)std::llround((E - offX) / scale), 4); scandetail::PutLE(rec, (std::uint32_t)(std::int32_t)std::llround((N - offY) / scale), 4);
    scandetail::PutLE(rec, (std::uint32_t)(std::int32_t)std::llround((H - offZ) / scale), 4);
    scandetail::PutLE(rec, (std::uint64_t)p.i * 257u, 2);
    rec += (char)0x09; rec += (char)0; rec += (char)0; rec += (char)0; scandetail::PutLE(rec, p.strip, 2);   // flagi (1 zwrot z 1), klasyfikacja, kat, dane uzytkownika, numer szeregu
    scandetail::PutD(rec, p.gps);
    scandetail::PutLE(rec, (std::uint64_t)p.r * 257u, 2); scandetail::PutLE(rec, (std::uint64_t)p.g * 257u, 2); scandetail::PutLE(rec, (std::uint64_t)p.b * 257u, 2);
    las.write(rec.data(), (std::streamsize)rec.size());
  }
  return st;
}

struct StripQc { int a, b, cells; double meanDzM, rmsDzM; };
// Kolejne szeregi (wg numerow): roznica srednich H w wspolnych komorkach (b - a).
inline std::vector<StripQc> AlsStripCheck(const std::string& rawPath, double cellM = 2.0) {
  using Cell = std::pair<int, int>;
  std::map<int, std::map<Cell, std::pair<double, int>>> g; AlsPoint p; std::ifstream in(rawPath, std::ios::binary);
  while (alsdetail::ReadRec(in, p)) { auto& c = g[p.strip][{(int)std::floor(p.x / cellM), (int)std::floor(p.y / cellM)}]; c.first += p.z; c.second++; }
  std::vector<StripQc> out; int prev = -1; bool have = false;
  for (const auto& s : g) {
    if (have) {
      StripQc q{prev, s.first, 0, 0, 0}; double sum = 0, sum2 = 0;
      for (const auto& c : s.second) { auto it = g[prev].find(c.first); if (it == g[prev].end()) continue; const double d = c.second.first / c.second.second - it->second.first / it->second.second; sum += d; sum2 += d * d; q.cells++; }
      if (q.cells > 0) { q.meanDzM = sum / q.cells; q.rmsDzM = std::sqrt(sum2 / q.cells); }
      out.push_back(q);
    }
    prev = s.first; have = true;
  }
  return out;
}

struct ControlQc { std::string id; double dhM; int pts; };
// shortcut: mediana dolnej cwiartki H w promieniu (omija nadbudowe znaku); przy gestej roslinnosci obnizy wynik, wtedy zmienic na filtr klasyfikacji gruntu.
inline std::vector<ControlQc> AlsControlCheck(const std::string& rawPath, double originN, double originE, double originH, const std::vector<ControlPoint>& cps, double radiusM = 2.0) {
  std::vector<std::vector<double>> hs(cps.size()); AlsPoint p; std::ifstream in(rawPath, std::ios::binary);
  while (alsdetail::ReadRec(in, p)) {
    for (size_t k = 0; k < cps.size(); k++) if (std::hypot(originE + p.x - cps[k].e, originN + p.y - cps[k].n) <= radiusM) hs[k].push_back(originH + p.z);
  }
  std::vector<ControlQc> out;
  for (size_t k = 0; k < cps.size(); k++) {
    ControlQc q{cps[k].id, 0, (int)hs[k].size()};
    if (!hs[k].empty()) {
      std::sort(hs[k].begin(), hs[k].end()); const size_t m = (std::max<size_t>)(1, hs[k].size() / 4);
      q.dhM = hs[k][m / 2] - cps[k].h;
    }
    out.push_back(q);
  }
  return out;
}
