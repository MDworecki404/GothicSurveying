// Zapis obrazu RGB 8 bit do PNG bez kompresji (deflate w blokach "stored", filtr 0) - bez zewnetrznych bibliotek; plik jest wiekszy niz po kompresji, ale czytany przez kazdy program.
#pragma once
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

namespace pngdetail {
inline std::uint32_t Crc32(const unsigned char* d, size_t n) {
  static std::uint32_t table[256]; static bool init = false;
  if (!init) { for (std::uint32_t i = 0; i < 256; i++) { std::uint32_t c = i; for (int k = 0; k < 8; k++) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1; table[i] = c; } init = true; }
  std::uint32_t c = 0xFFFFFFFFu; for (size_t i = 0; i < n; i++) c = table[(c ^ d[i]) & 255] ^ (c >> 8); return c ^ 0xFFFFFFFFu;
}
inline std::uint32_t Adler32(const unsigned char* d, size_t n) {
  std::uint32_t a = 1, b = 0; for (size_t i = 0; i < n; i++) { a = (a + d[i]) % 65521; b = (b + a) % 65521; } return (b << 16) | a;
}
inline void Be32(std::vector<unsigned char>& v, std::uint32_t x) { v.push_back((unsigned char)(x >> 24)); v.push_back((unsigned char)(x >> 16)); v.push_back((unsigned char)(x >> 8)); v.push_back((unsigned char)x); }
inline void Chunk(std::ofstream& f, const char* type, const std::vector<unsigned char>& data) {
  std::vector<unsigned char> b; Be32(b, (std::uint32_t)data.size()); const size_t t0 = b.size(); b.insert(b.end(), type, type + 4); b.insert(b.end(), data.begin(), data.end());
  Be32(b, Crc32(b.data() + t0, b.size() - t0)); f.write(reinterpret_cast<const char*>(b.data()), (std::streamsize)b.size());
}
}  // namespace pngdetail

// rgb: w*h*3 bajtow, wiersze od gory. false = bledne dane albo brak mozliwosci zapisu.
inline bool WritePng(const std::string& path, int w, int h, const std::vector<unsigned char>& rgb) {
  if (w <= 0 || h <= 0 || rgb.size() != (size_t)w * h * 3) return false;
  std::ofstream f(path, std::ios::binary | std::ios::trunc); if (!f.good()) return false;
  static const unsigned char sig[8] = {137, 80, 78, 71, 13, 10, 26, 10}; f.write(reinterpret_cast<const char*>(sig), 8);
  std::vector<unsigned char> ihdr; pngdetail::Be32(ihdr, (std::uint32_t)w); pngdetail::Be32(ihdr, (std::uint32_t)h); ihdr.insert(ihdr.end(), {8, 2, 0, 0, 0}); pngdetail::Chunk(f, "IHDR", ihdr);
  std::vector<unsigned char> raw; raw.reserve((size_t)h * (1 + 3 * w));
  for (int y = 0; y < h; y++) { raw.push_back(0); raw.insert(raw.end(), rgb.begin() + (size_t)y * 3 * w, rgb.begin() + (size_t)(y + 1) * 3 * w); }
  std::vector<unsigned char> z = {0x78, 0x01};
  for (size_t p = 0; p < raw.size() || p == 0;) {
    const size_t len = (std::min)((size_t)65535, raw.size() - p); const bool fin = p + len >= raw.size();
    z.push_back(fin ? 1 : 0); z.push_back((unsigned char)(len & 255)); z.push_back((unsigned char)(len >> 8)); z.push_back((unsigned char)(~len & 255)); z.push_back((unsigned char)((~len >> 8) & 255));
    z.insert(z.end(), raw.begin() + p, raw.begin() + p + len); p += len; if (fin) break;
  }
  pngdetail::Be32(z, pngdetail::Adler32(raw.data(), raw.size())); pngdetail::Chunk(f, "IDAT", z);
  pngdetail::Chunk(f, "IEND", {});
  return f.good();
}
