// Zapis obrazu RGB 8 bit do PNG bez kompresji (deflate w blokach "stored", filtr 0) - bez zewnetrznych bibliotek; plik jest wiekszy niz po kompresji, ale czytany przez kazdy program.
#pragma once
#include <cstdint>
#include <cstdio>
#include <algorithm>
#include <fstream>
#include <iterator>
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

// Odczyt PNG zapisanego przez WritePng (RGB 8 bit, filtr 0, deflate "stored"); sprawdza sumy CRC i Adler32. false = inny format, uszkodzony albo brak pliku.
inline bool ReadPng(const std::string& path, int& w, int& h, std::vector<unsigned char>& rgb) {
  std::ifstream f(path, std::ios::binary); if (!f.good()) return false;
  const std::vector<unsigned char> b((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  auto be32 = [&](size_t o) { return ((std::uint32_t)b[o] << 24) | ((std::uint32_t)b[o + 1] << 16) | ((std::uint32_t)b[o + 2] << 8) | b[o + 3]; };
  static const unsigned char sig[8] = {137, 80, 78, 71, 13, 10, 26, 10}; if (b.size() < 8 + 12 || !std::equal(sig, sig + 8, b.begin())) return false;
  size_t o = 8; std::vector<unsigned char> z; bool end = false, haveHdr = false;
  while (o + 12 <= b.size() && !end) {
    const std::uint32_t len = be32(o); if (o + 12 + (size_t)len > b.size()) return false;
    if (pngdetail::Crc32(&b[o + 4], (size_t)len + 4) != be32(o + 8 + len)) return false;
    const std::string type(b.begin() + o + 4, b.begin() + o + 8);
    if (type == "IHDR") { if (len < 13) return false; w = (int)be32(o + 8); h = (int)be32(o + 12); if (b[o + 16] != 8 || b[o + 17] != 2 || w <= 0 || h <= 0) return false; haveHdr = true; }
    else if (type == "IDAT") z.insert(z.end(), b.begin() + o + 8, b.begin() + o + 8 + len);
    else if (type == "IEND") end = true;
    o += 12 + (size_t)len;
  }
  if (!haveHdr || !end || z.size() < 6 || z[0] != 0x78) return false;
  std::vector<unsigned char> raw; size_t p = 2;
  for (;;) {
    if (p + 5 > z.size() || ((z[p] >> 1) & 3) != 0) return false;                       // tylko bloki "stored"
    const bool fin = z[p] & 1; const unsigned len = z[p + 1] | (z[p + 2] << 8), nlen = z[p + 3] | (z[p + 4] << 8);
    if ((len ^ 0xFFFFu) != nlen || p + 5 + len > z.size()) return false;
    raw.insert(raw.end(), z.begin() + p + 5, z.begin() + p + 5 + len); p += 5 + len; if (fin) break;
  }
  if (p + 4 > z.size() || pngdetail::Adler32(raw.data(), raw.size()) != (((std::uint32_t)z[p] << 24) | ((std::uint32_t)z[p + 1] << 16) | ((std::uint32_t)z[p + 2] << 8) | z[p + 3])) return false;
  if (raw.size() != (size_t)h * (1 + 3 * (size_t)w)) return false;
  rgb.clear(); rgb.reserve((size_t)w * h * 3);
  for (int y = 0; y < h; y++) { if (raw[(size_t)y * (1 + 3 * w)] != 0) return false; rgb.insert(rgb.end(), raw.begin() + (size_t)y * (1 + 3 * w) + 1, raw.begin() + (size_t)(y + 1) * (1 + 3 * w)); }
  return true;
}
