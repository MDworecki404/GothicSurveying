#include <cassert>
#include <cstdio>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>
#include "calc/Png.hpp"

static std::uint32_t BE32(const std::vector<unsigned char>& b, size_t o) { return ((std::uint32_t)b[o] << 24) | ((std::uint32_t)b[o + 1] << 16) | ((std::uint32_t)b[o + 2] << 8) | b[o + 3]; }

// Dekoduje PNG zapisany przez WritePng (RGB 8 bit, filtr 0, deflate "stored") i sprawdza sumy CRC i Adler32; zwraca piksele.
static bool Decode(const std::string& path, int& w, int& h, std::vector<unsigned char>& rgb) {
  std::ifstream f(path, std::ios::binary); std::vector<unsigned char> b((std::istreambuf_iterator<char>(f)), {});
  static const unsigned char sig[8] = {137, 80, 78, 71, 13, 10, 26, 10}; if (b.size() < 8 || !std::equal(sig, sig + 8, b.begin())) return false;
  size_t o = 8; std::vector<unsigned char> z; bool end = false;
  while (o + 12 <= b.size() && !end) {
    const std::uint32_t len = BE32(b, o); const std::string type(b.begin() + o + 4, b.begin() + o + 8);
    if (o + 12 + len > b.size()) return false;
    if (pngdetail::Crc32(&b[o + 4], len + 4) != BE32(b, o + 8 + len)) return false;                                        // CRC typu i danych
    if (type == "IHDR") { w = (int)BE32(b, o + 8); h = (int)BE32(b, o + 12); if (b[o + 16] != 8 || b[o + 17] != 2) return false; }
    else if (type == "IDAT") z.insert(z.end(), b.begin() + o + 8, b.begin() + o + 8 + len);
    else if (type == "IEND") end = true;
    o += 12 + len;
  }
  if (!end || z.size() < 6 || z[0] != 0x78) return false;
  std::vector<unsigned char> raw; size_t p = 2;
  for (;;) {                                                                                                                  // bloki "stored"
    const bool fin = z[p] & 1; if (((z[p] >> 1) & 3) != 0) return false;
    const unsigned len = z[p + 1] | (z[p + 2] << 8), nlen = z[p + 3] | (z[p + 4] << 8); if ((len ^ 0xFFFF) != nlen) return false;
    raw.insert(raw.end(), z.begin() + p + 5, z.begin() + p + 5 + len); p += 5 + len; if (fin) break;
  }
  if (pngdetail::Adler32(raw.data(), raw.size()) != BE32(z, p)) return false;
  rgb.clear(); for (int y = 0; y < h; y++) { if (raw[(size_t)y * (1 + 3 * w)] != 0) return false; rgb.insert(rgb.end(), raw.begin() + (size_t)y * (1 + 3 * w) + 1, raw.begin() + (size_t)(y + 1) * (1 + 3 * w)); }
  return rgb.size() == (size_t)w * h * 3;
}

static void known_checksums() {                                                // wartosci wzorcowe: CRC-32("123456789") = CBF43926, Adler-32("Wikipedia") = 11E60398
  const unsigned char s[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'}; assert(pngdetail::Crc32(s, 9) == 0xCBF43926u);
  const unsigned char w[] = {'W', 'i', 'k', 'i', 'p', 'e', 'd', 'i', 'a'}; assert(pngdetail::Adler32(w, 9) == 0x11E60398u);
}

static void roundtrip_small_and_large() {
  for (auto wh : {std::pair<int, int>{7, 5}, std::pair<int, int>{320, 240}, std::pair<int, int>{1024, 768}}) {                // 1024x768: > 65535 bajtow, wiele blokow
    const int w = wh.first, h = wh.second; std::vector<unsigned char> img((size_t)w * h * 3);
    for (size_t i = 0; i < img.size(); i++) img[i] = (unsigned char)((i * 7 + i / 31) & 255);
    const std::string path = (std::filesystem::temp_directory_path() / ("png_t_" + std::to_string(w) + ".png")).string();
    assert(WritePng(path, w, h, img));
    int rw = 0, rh = 0; std::vector<unsigned char> back; assert(Decode(path, rw, rh, back) && rw == w && rh == h && back == img);
  }
}

static void rejects_bad_input() {
  const std::vector<unsigned char> img(10);
  assert(!WritePng((std::filesystem::temp_directory_path() / "png_bad.png").string(), 4, 4, img));                            // za malo danych
  assert(!WritePng((std::filesystem::temp_directory_path() / "nie_ma_takiego_katalogu_xyz" / "a.png").string(), 1, 1, std::vector<unsigned char>(3)));
}

int main() {
  known_checksums(); roundtrip_small_and_large(); rejects_bad_input();
  std::puts("test_png OK"); return 0;
}
