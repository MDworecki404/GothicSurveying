// Odczyt pojedynczego piksela z bufora tekstury silnika (bez Gothic API): formaty DXT1/DXT3/DXT5 (S3TC), 16- i 32-bitowe. Numery formatow jak zTRnd_TextureFormat:
// 0 ARGB_8888, 1 ABGR_8888, 2 RGBA_8888, 3 BGRA_8888, 4 RGB_888, 5 BGR_888, 6 ARGB_4444, 7 ARGB_1555, 8 RGB_565, 9 PAL_8, 10 DXT1, 11 DXT2, 12 DXT3, 13 DXT4, 14 DXT5.
// Nazwy formatow traktujemy jak w Direct3D: ARGB = slowo 0xAARRGGBB (w pamieci B, G, R, A). Palety (PAL_8) nie obslugujemy.
#pragma once
#include <cstdint>

enum TexFmt { TEXF_ARGB8888 = 0, TEXF_ABGR8888, TEXF_RGBA8888, TEXF_BGRA8888, TEXF_RGB888, TEXF_BGR888, TEXF_ARGB4444, TEXF_ARGB1555, TEXF_RGB565, TEXF_PAL8, TEXF_DXT1, TEXF_DXT2, TEXF_DXT3, TEXF_DXT4, TEXF_DXT5 };

namespace texdecode {
inline void Rgb565(std::uint16_t c, int& r, int& g, int& b) { r = ((c >> 11) & 31) * 255 / 31; g = ((c >> 5) & 63) * 255 / 63; b = (c & 31) * 255 / 31; }
}

// Szerokosc poziomu z pitch silnika: pitch = szerokosc * bajty na piksel (takze dla DXT: 0,5 B/px dla DXT1 i 1 B/px dla DXT2-5; nie rozmiar wiersza blokow).
// Sprawdzone w grze: tekstura 128 px DXT1 ma pitch 64 na poziomie 0, potem 32, 16, 8, 4. 0 gdy nie da sie ustalic.
inline int TexEffectiveWidth(int format, int pitch) {
  if (pitch <= 0) return 0;
  if (format == TEXF_DXT1) return pitch * 2;
  if (format >= TEXF_DXT2 && format <= TEXF_DXT5) return pitch;
  const int bpp = (format <= TEXF_BGRA8888) ? 4 : (format <= TEXF_BGR888) ? 3 : (format <= TEXF_RGB565) ? 2 : 0;
  return bpp ? pitch / bpp : 0;
}

// Piksel (x, y) tekstury w = sizeX, h = sizeY; buf = poczatek poziomu 0, pitch = bajty w wierszu (dla DXT ignorowany). false = format nieobslugiwany albo poza zakresem.
inline bool TexDecodeTexel(int format, const unsigned char* buf, int pitch, int w, int h, int x, int y, unsigned char rgb[3]) {
  if (!buf || w <= 0 || h <= 0) return false;
  x = ((x % w) + w) % w; y = ((y % h) + h) % h;                          // zawijanie tekstury
  auto set = [&](int r, int g, int b) { rgb[0] = (unsigned char)r; rgb[1] = (unsigned char)g; rgb[2] = (unsigned char)b; return true; };
  if (format >= TEXF_DXT1 && format <= TEXF_DXT5) {
    const int blockSize = format == TEXF_DXT1 ? 8 : 16, bw = (w + 3) / 4;
    const unsigned char* blk = buf + ((size_t)(y / 4) * bw + (size_t)(x / 4)) * blockSize + (blockSize == 16 ? 8 : 0);   // blok koloru (dla DXT2-5 po 8 bajtach alfy)
    bool zero = true; for (int k = 0; k < 8; k++) if (blk[k]) zero = false;
    if (zero) return false;                                              // blok samych zer: dane tekstury nie sa w pamieci (nie czern)
    const std::uint16_t c0 = (std::uint16_t)(blk[0] | (blk[1] << 8)), c1 = (std::uint16_t)(blk[2] | (blk[3] << 8));
    int r[4], g[4], b[4];
    texdecode::Rgb565(c0, r[0], g[0], b[0]); texdecode::Rgb565(c1, r[1], g[1], b[1]);
    if (c0 > c1 || format != TEXF_DXT1) {                                // 4 kolory
      for (int k = 0; k < 3; k++) { int* c[3] = {r, g, b}; c[k][2] = (2 * c[k][0] + c[k][1]) / 3; c[k][3] = (c[k][0] + 2 * c[k][1]) / 3; }
    } else {                                                             // DXT1 3 kolory + przezroczysty (czarny)
      r[2] = (r[0] + r[1]) / 2; g[2] = (g[0] + g[1]) / 2; b[2] = (b[0] + b[1]) / 2; r[3] = g[3] = b[3] = 0;
    }
    const std::uint32_t bits = (std::uint32_t)blk[4] | ((std::uint32_t)blk[5] << 8) | ((std::uint32_t)blk[6] << 16) | ((std::uint32_t)blk[7] << 24);
    const int idx = (bits >> (2 * ((y % 4) * 4 + (x % 4)))) & 3;
    if (format == TEXF_DXT1 && c0 <= c1 && idx == 3) return false;        // przezroczysty tekstel (tryb 3-kolorowy): brak koloru powierzchni
    return set(r[idx], g[idx], b[idx]);
  }
  const int bpp = (format <= TEXF_BGRA8888) ? 4 : (format <= TEXF_BGR888) ? 3 : (format <= TEXF_RGB565) ? 2 : 0;
  if (bpp == 0 || pitch < w * bpp) return false;
  const unsigned char* p = buf + (size_t)y * pitch + (size_t)x * bpp;
  switch (format) {
    case TEXF_ARGB8888: return set(p[2], p[1], p[0]);
    case TEXF_ABGR8888: return set(p[0], p[1], p[2]);
    case TEXF_RGBA8888: return set(p[3], p[2], p[1]);
    case TEXF_BGRA8888: return set(p[1], p[2], p[3]);
    case TEXF_RGB888:   return set(p[0], p[1], p[2]);
    case TEXF_BGR888:   return set(p[2], p[1], p[0]);
    case TEXF_ARGB4444: { const unsigned v = p[0] | (p[1] << 8); return set(((v >> 8) & 15) * 17, ((v >> 4) & 15) * 17, (v & 15) * 17); }
    case TEXF_ARGB1555: { const unsigned v = p[0] | (p[1] << 8); return set(((v >> 10) & 31) * 255 / 31, ((v >> 5) & 31) * 255 / 31, (v & 31) * 255 / 31); }
    case TEXF_RGB565:   { int r2, g2, b2; texdecode::Rgb565((std::uint16_t)(p[0] | (p[1] << 8)), r2, g2, b2); return set(r2, g2, b2); }
  }
  return false;
}
