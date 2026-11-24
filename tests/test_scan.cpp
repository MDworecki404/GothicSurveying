#include <cassert>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include "calc/Scan.hpp"

static bool near(double a, double b, double eps = 1e-9) { return std::fabs(a - b) <= eps; }

static void grid_counts_and_directions() {
  const ScanGrid g = ScanGridFor(1.0);
  assert(g.nHz == 360 && g.nV == 126 && g.Total() == 360LL * 126);
  double az, v; ScanDirection(g, 0, az, v); assert(az == 0 && v == 0);
  ScanDirection(g, 126, az, v); assert(near(az, 1.0) && v == 0);           // druga kolumna
  ScanDirection(g, 125, az, v); assert(az == 0 && near(v, 125.0));          // ostatni punkt pierwszej kolumny
  assert(ScanGridFor(0.1).Total() == 3600LL * 1251);
}

static void polar_to_local_axes() {
  const ScanLocal n = ScanToLocal(0, 90, 10, 7); assert(near(n.x, 0, 1e-5) && near(n.y, 10, 1e-5) && near(n.z, 0, 1e-5) && n.i == 7);   // poziomo na polnoc
  const ScanLocal e = ScanToLocal(90, 90, 10, 0); assert(near(e.x, 10, 1e-5) && near(e.y, 0, 1e-5));
  const ScanLocal up = ScanToLocal(33, 0, 10, 0); assert(near(up.z, 10, 1e-5) && near(up.x, 0, 1e-5));
}

static void noise_and_intensity() {
  assert(near(ScanNoiseSigmaM(100.0), 0.0025, 1e-12));
  assert(ScanIntensity(5, 1.0, false) > ScanIntensity(5, 0.2, false) && ScanIntensity(5, 1.0, false) > ScanIntensity(100, 1.0, false));
  assert(ScanIntensity(5, 1.0, true) >= ScanIntensity(5, 1.0, false) && ScanIntensity(1000, 0.0, false) >= 4);
}

static void convert_writes_pts_and_las() {
  namespace fs = std::filesystem; const fs::path dir = fs::temp_directory_path() / "gs_scan_calc"; fs::create_directories(dir);
  ScanRawWriter w; assert(w.Open((dir / "r.tmp").string()));
  w.Append(ScanToLocal(0, 90, 1.0, 10, 0x102030)); w.Append({-1.5f, 0.0f, -2.0f, 200, 255, 0, 0}); w.Append({0.0f, 5.0f, 0.5f, 77, 1, 2, 3}); w.Close();
  const ScanStats st = ScanConvert((dir / "r.tmp").string(), 5500000.0, 6500000.0, 100.0, 0.0, (dir / "o.pts").string(), (dir / "o.las").string());
  assert(w.Count() == 3 && st.count == 3 && st.ok && near(st.minX, 6499998.5, 1e-4) && near(st.maxY, 5500005.0, 1e-4) && near(st.minZ, 98.0, 1e-4));
  std::ifstream pts(dir / "o.pts"); std::string first; std::getline(pts, first); assert(first == "3");
  double x, y, z; int i, cr, cg, cb; pts >> x >> y >> z >> i >> cr >> cg >> cb; assert(near(x, 6500000.0, 1e-3) && near(y, 5500001.0, 1e-3) && near(z, 100.0, 1e-3) && i == 10 && cr == 0x10 && cg == 0x20 && cb == 0x30);   // x = E, y = N, z = H
  std::ifstream las(dir / "o.las", std::ios::binary); std::string d((std::istreambuf_iterator<char>(las)), std::istreambuf_iterator<char>());
  assert(d.size() == 227 + 3 * 26 && d.substr(0, 4) == "LASF" && (unsigned char)d[24] == 1 && (unsigned char)d[25] == 2);   // wersja 1.2
  auto u16 = [&](size_t o) { return (unsigned)((unsigned char)d[o] | ((unsigned char)d[o + 1] << 8)); };
  auto u32 = [&](size_t o) { return (unsigned)(u16(o) | (u16(o + 2) << 16)); };
  assert(u16(94) == 227 && u32(96) == 227 && (unsigned char)d[104] == 2 && u16(105) == 26 && u32(107) == 3);           // naglowek, offset, format 0, rekord 20, liczba punktow
  double scale; std::memcpy(&scale, d.data() + 131, 8); assert(near(scale, 0.001));
  double offX; std::memcpy(&offX, d.data() + 155, 8); assert(near(offX, 6499998.0));
  const int32_t X0 = (int32_t)u32(227); assert(X0 == (int32_t)std::llround((6500000.0 - offX) / 0.001) && u16(227 + 12) == 10 * 257 && u16(227 + 20) == 0x10 * 257 && u16(227 + 24) == 0x30 * 257);   // RGB po 16 bit
}

static void convert_applies_rotation() {
  namespace fs = std::filesystem; const fs::path dir = fs::temp_directory_path() / "gs_scan_rot"; fs::create_directories(dir);
  ScanRawWriter w; assert(w.Open((dir / "r.tmp").string()));
  w.Append({0.0f, 10.0f, 0.0f, 50});                                         // 10 m na polnoc w ukladzie lokalnym
  w.Close();
  const ScanStats st = ScanConvert((dir / "r.tmp").string(), 5500000.0, 6500000.0, 100.0, 90.0, (dir / "o.pts").string(), (dir / "o.las").string());
  assert(st.ok && near(st.minX, 6500010.0, 1e-4) && near(st.minY, 5500000.0, 1e-4));       // obrot o 90 st. w prawo: polnoc -> wschod
}

int main() {
  grid_counts_and_directions(); polar_to_local_axes(); noise_and_intensity(); convert_writes_pts_and_las(); convert_applies_rotation();
  std::puts("test_scan OK");
  return 0;
}
