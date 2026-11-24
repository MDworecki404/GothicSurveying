// Miniaturki kolorow tekstur (bez Gothic API): plik GothicSurveying/texcolors.bin z assets/make_texture_colors.py. Kolor punktu skanu = probka miniaturki w miejscu trafienia
// (UV), niezalezna od pamieci silnika. Format pliku: "GSTC", uint32 liczba tekstur, potem [uint8 dlugosc nazwy, nazwa, uint8 w, uint8 h, w*h*3 bajtow RGB].
#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>

struct TexThumb { int w = 0, h = 0; std::vector<unsigned char> px; };   // RGB, wiersz po wierszu

// Klucz tekstury: wielkie litery, bez sciezki i rozszerzenia, bez przyrostka "-C" ("OW_MISC_WOOD_01.TGA" i "OW_MISC_WOOD_01-C.TEX" -> "OW_MISC_WOOD_01").
inline std::string TexColorKey(const std::string& name) {
  size_t start = name.find_last_of("/\\"); std::string s = name.substr(start == std::string::npos ? 0 : start + 1);
  for (char& c : s) c = (char)std::toupper((unsigned char)c);
  const size_t dot = s.find_last_of('.'); if (dot != std::string::npos) s.erase(dot);
  if (s.size() > 2 && s.compare(s.size() - 2, 2, "-C") == 0) s.erase(s.size() - 2);
  return s;
}

// Parsuje zawartosc pliku; zwraca liczbe wczytanych tekstur (0 przy blednym pliku).
inline size_t TexColorsParse(const std::string& d, std::map<std::string, TexThumb>& out) {
  if (d.size() < 8 || d.compare(0, 4, "GSTC") != 0) return 0;
  std::uint32_t n; std::memcpy(&n, d.data() + 4, 4);
  size_t pos = 8, count = 0;
  for (std::uint32_t i = 0; i < n && pos < d.size(); i++) {
    const size_t len = (unsigned char)d[pos++];
    if (pos + len + 2 > d.size()) break;
    const std::string name = d.substr(pos, len); pos += len;
    TexThumb t; t.w = (unsigned char)d[pos++]; t.h = (unsigned char)d[pos++];
    const size_t bytes = (size_t)t.w * t.h * 3;
    if (t.w == 0 || t.h == 0 || pos + bytes > d.size()) break;
    t.px.assign(reinterpret_cast<const unsigned char*>(d.data()) + pos, reinterpret_cast<const unsigned char*>(d.data()) + pos + bytes); pos += bytes;
    out[name] = t; count++;
  }
  return count;
}

// Probka z interpolacja dwuliniowa i zawijaniem (u, v dowolne rzeczywiste; srodek tekstela = (i + 0,5) / w). Zwraca 0xRRGGBB.
inline unsigned TexThumbSample(const TexThumb& t, double u, double v) {
  if (t.w <= 0 || t.h <= 0) return 0x808080;
  u -= std::floor(u); v -= std::floor(v);
  const double fx = u * t.w - 0.5, fy = v * t.h - 0.5, x0f = std::floor(fx), y0f = std::floor(fy), ax = fx - x0f, ay = fy - y0f;
  auto px = [&](long x, long y, int k) { x = ((x % t.w) + t.w) % t.w; y = ((y % t.h) + t.h) % t.h; return (double)t.px[((size_t)y * t.w + x) * 3 + k]; };
  unsigned c[3];
  for (int k = 0; k < 3; k++) {
    const double a = px((long)x0f, (long)y0f, k) * (1 - ax) + px((long)x0f + 1, (long)y0f, k) * ax, b = px((long)x0f, (long)y0f + 1, k) * (1 - ax) + px((long)x0f + 1, (long)y0f + 1, k) * ax;
    c[k] = (unsigned)(a * (1 - ay) + b * ay + 0.5);
  }
  return (c[0] << 16) | (c[1] << 8) | c[2];
}

inline unsigned TexThumbAverage(const TexThumb& t) {
  if (t.px.empty()) return 0x808080;
  unsigned long s[3] = {0, 0, 0}; const size_t n = (size_t)t.w * t.h;
  for (size_t i = 0; i < n; i++) for (int k = 0; k < 3; k++) s[k] += t.px[i * 3 + k];
  return ((unsigned)(s[0] / n) << 16) | ((unsigned)(s[1] / n) << 8) | (unsigned)(s[2] / n);
}
