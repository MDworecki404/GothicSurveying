#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <vector>
#include "calc/AlsLas.hpp"

static bool near(double a, double b, double eps = 1e-9) { return std::fabs(a - b) <= eps; }
static std::string Tmp(const char* n) { return (std::filesystem::temp_directory_path() / n).string(); }
static std::vector<unsigned char> Slurp(const std::string& p) { std::ifstream f(p, std::ios::binary); return std::vector<unsigned char>(std::istreambuf_iterator<char>(f), {}); }
static std::uint32_t U(const std::vector<unsigned char>& b, size_t o, int n) { std::uint32_t v = 0; for (int i = 0; i < n; i++) v |= (std::uint32_t)b[o + i] << (8 * i); return v; }
static double D(const std::vector<unsigned char>& b, size_t o) { double d; std::memcpy(&d, &b[o], 8); return d; }

static AlsPoint P(float x, float y, float z, double gps, int strip, int i = 100, int r = 10, int g = 20, int b = 30) {
  AlsPoint p{}; p.x = x; p.y = y; p.z = z; p.gps = gps; p.strip = (std::uint16_t)strip; p.i = (std::uint8_t)i; p.r = (std::uint8_t)r; p.g = (std::uint8_t)g; p.b = (std::uint8_t)b; return p;
}

static void las_header_and_record() {
  const std::string raw = Tmp("als_t1.raw"), pts = Tmp("als_t1.pts"), las = Tmp("als_t1.las");
  AlsRawWriter w; assert(w.Open(raw));
  w.Append(P(123.456f, 78.9f, 12.345f, 345600.5, 2)); w.Append(P(1.f, 2.f, 3.f, 345601.0, 3)); w.Append(P(4.f, 5.f, 6.f, 345602.0, 3)); w.Close();
  assert(w.Count() == 3);
  const AlsStats st = AlsConvert(raw, 5500000.0, 6500000.0, 0.0, pts, las); assert(st.ok && st.count == 3);
  const std::vector<unsigned char> b = Slurp(las);
  assert(b.size() == 227 + 3 * 34);
  assert(std::memcmp(b.data(), "LASF", 4) == 0 && b[24] == 1 && b[25] == 2);
  assert(U(b, 94, 2) == 227 && U(b, 96, 4) == 227 && b[104] == 3 && U(b, 105, 2) == 34 && U(b, 107, 4) == 3);
  assert(near(D(b, 131), 0.001) && near(D(b, 139), 0.001) && near(D(b, 147), 0.001));
  const double offX = D(b, 155), offY = D(b, 163), offZ = D(b, 171);
  assert(near(offX, std::floor(st.minE)) && near(offY, std::floor(st.minN)));
  const size_t r0 = 227;
  assert(near((std::int32_t)U(b, r0, 4) * 0.001 + offX, 6500000.0 + (double)123.456f, 0.0005));      // X = E z offsetem
  assert(near((std::int32_t)U(b, r0 + 4, 4) * 0.001 + offY, 5500000.0 + (double)78.9f, 0.0005));      // Y = N
  assert(near((std::int32_t)U(b, r0 + 8, 4) * 0.001 + offZ, (double)12.345f, 0.0005));                // Z = H
  assert(U(b, r0 + 12, 2) == 100 * 257u && U(b, r0 + 18, 2) == 2);                                    // intensywnosc, point source id
  assert(near(D(b, r0 + 20), 345600.5) && U(b, r0 + 28, 2) == 10 * 257u && U(b, r0 + 30, 2) == 20 * 257u && U(b, r0 + 32, 2) == 30 * 257u);
  assert(U(b, r0 + 34 + 18, 2) == 3);
}

static void raw_record_is_34_bytes_with_theta_range() {
  const std::string raw = Tmp("als_t0.raw"); AlsRawWriter w; w.Open(raw);
  AlsPoint p = P(1.f, 2.f, 3.f, 12.5, 4); p.theta = 12.5f; p.range = 77.25f; w.Append(p); w.Close();
  assert(kAlsRawBytes == 34 && std::filesystem::file_size(raw) == 34);
  std::ifstream in(raw, std::ios::binary); AlsPoint q{}; assert(alsdetail::ReadRec(in, q));
  assert(q.theta == 12.5f && q.range == 77.25f && q.strip == 4 && q.gps == 12.5 && q.z == 3.f);
  AlsRawWriter a; assert(a.Open(raw, true) && a.Count() == 1);                    // dopisywanie liczy rekordy po 34 B
}

static void pts_lines() {
  const std::string raw = Tmp("als_t2.raw"), pts = Tmp("als_t2.pts"), las = Tmp("als_t2.las");
  AlsRawWriter w; w.Open(raw); w.Append(P(1, 2, 3, 0, 0)); w.Append(P(4, 5, 6, 0, 1)); w.Close();
  AlsConvert(raw, 5500000.0, 6500000.0, 0.0, pts, las);
  std::ifstream f(pts); std::string line; std::getline(f, line); assert(line == "2");
  int n = 0; while (std::getline(f, line)) { std::istringstream is(line); double a, b, c, i, r, g, bl; assert(is >> a >> b >> c >> i >> r >> g >> bl); n++; }
  assert(n == 2);
}

static void strip_check_detects_offset() {
  const std::string raw = Tmp("als_t3.raw"); AlsRawWriter w; w.Open(raw);
  for (int s = 0; s < 2; s++) for (float x = 0; x < 20; x += 0.5f) for (float y = 0; y < 20; y += 0.5f) w.Append(P(x, y, s == 0 ? 0.f : 0.10f, 0, s));
  for (float x = 500; x < 520; x += 0.5f) for (float y = 0; y < 20; y += 0.5f) w.Append(P(x, y, 0.f, 0, 2));
  w.Close();
  const std::vector<StripQc> q = AlsStripCheck(raw);
  assert(q.size() == 2);
  assert(q[0].a == 0 && q[0].b == 1 && q[0].cells > 0 && near(q[0].meanDzM, 0.10, 1e-6) && near(q[0].rmsDzM, 0.10, 1e-6));
  assert(q[1].a == 1 && q[1].b == 2 && q[1].cells == 0);
}

static void control_check() {
  const std::string raw = Tmp("als_t4.raw"); AlsRawWriter w; w.Open(raw);
  for (float x = -2; x <= 2; x += 0.2f) for (float y = -2; y <= 2; y += 0.2f) if (std::hypot(x, y) <= 1.9) w.Append(P(x, y, 5.0f, 0, 0));
  for (int k = 0; k < 5; k++) w.Append(P(0.1f * k - 0.2f, 0.f, 5.4f, 0, 0));
  w.Close();
  ControlPoint cp; cp.id = "G01"; cp.n = 5500000.0; cp.e = 6500000.0; cp.h = 5.02;
  const std::vector<ControlQc> q = AlsControlCheck(raw, 5500000.0, 6500000.0, 0.0, {cp});
  assert(q.size() == 1 && q[0].id == "G01" && q[0].pts > 50 && near(q[0].dhM, -0.02, 1e-3));
  ControlPoint far = cp; far.id = "G02"; far.e += 400;
  assert(AlsControlCheck(raw, 5500000.0, 6500000.0, 0.0, {far})[0].pts == 0);
}

static void empty_raw() {
  const std::string raw = Tmp("als_t5.raw"), pts = Tmp("als_t5.pts"), las = Tmp("als_t5.las");
  AlsRawWriter w; w.Open(raw); w.Close();
  const AlsStats st = AlsConvert(raw, 5500000.0, 6500000.0, 0.0, pts, las);
  assert(!st.ok && st.count == 0);
  const std::vector<unsigned char> b = Slurp(las); assert(b.size() == 227 && U(b, 107, 4) == 0);
}

int main() {
  las_header_and_record(); raw_record_is_34_bytes_with_theta_range(); pts_lines(); strip_check_detects_offset(); control_check(); empty_raw();
  std::puts("test_alslas OK"); return 0;
}
