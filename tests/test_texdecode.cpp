#include <cassert>
#include <cstdio>
#include "calc/TexDecode.hpp"

int main() {
  unsigned char rgb[3];
  // DXT1 4x4: c0 = czerwony (0xF800), c1 = niebieski (0x001F); indeksy pierwszego wiersza 0,1,2,3
  const unsigned char dxt1[8] = {0x00, 0xF8, 0x1F, 0x00, 0xE4, 0x00, 0x00, 0x00};
  assert(TexDecodeTexel(TEXF_DXT1, dxt1, 0, 4, 4, 0, 0, rgb) && rgb[0] == 255 && rgb[1] == 0 && rgb[2] == 0);
  assert(TexDecodeTexel(TEXF_DXT1, dxt1, 0, 4, 4, 1, 0, rgb) && rgb[0] == 0 && rgb[2] == 255);
  assert(TexDecodeTexel(TEXF_DXT1, dxt1, 0, 4, 4, 2, 0, rgb) && rgb[0] == 170 && rgb[2] == 85);      // 2/3 czerwonego + 1/3 niebieskiego
  assert(TexDecodeTexel(TEXF_DXT1, dxt1, 0, 4, 4, 3, 0, rgb) && rgb[0] == 85 && rgb[2] == 170);
  assert(TexDecodeTexel(TEXF_DXT1, dxt1, 0, 4, 4, 4, 4, rgb) && rgb[0] == 255);                      // zawijanie (4,4) -> (0,0)
  // DXT5 ma 8 bajtow alfy przed blokiem koloru
  unsigned char dxt5[16] = {255, 255, 0, 0, 0, 0, 0, 0, 0x00, 0xF8, 0x1F, 0x00, 0xE4, 0x00, 0x00, 0x00};
  assert(TexDecodeTexel(TEXF_DXT5, dxt5, 0, 4, 4, 1, 0, rgb) && rgb[2] == 255 && rgb[0] == 0);
  // dwa bloki w wierszu: 8x4, piksel (5,2) leży w drugim bloku
  unsigned char two[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0xE0, 0x07, 0xE0, 0x07, 0x00, 0x00, 0x00, 0x00};   // drugi blok: zielony (c0 = c1 = 0x07E0)
  assert(TexDecodeTexel(TEXF_DXT1, two, 0, 8, 4, 5, 2, rgb) && rgb[0] == 0 && rgb[1] == 255 && rgb[2] == 0);
  // formaty nieskompresowane
  const unsigned char argb[8] = {10, 20, 30, 255, 1, 2, 3, 255};                                     // B G R A: czerwony 30, zielony 20, niebieski 10
  assert(TexDecodeTexel(TEXF_ARGB8888, argb, 8, 2, 1, 0, 0, rgb) && rgb[0] == 30 && rgb[1] == 20 && rgb[2] == 10);
  const unsigned char r565[2] = {0x1F, 0x00};                                                        // niebieski
  assert(TexDecodeTexel(TEXF_RGB565, r565, 2, 1, 1, 0, 0, rgb) && rgb[2] == 255 && rgb[0] == 0);
  const unsigned char r4444[2] = {0x0F, 0xFF};                                                       // 0xFF0F: A=F R=F G=0 B=F
  assert(TexDecodeTexel(TEXF_ARGB4444, r4444, 2, 1, 1, 0, 0, rgb) && rgb[0] == 255 && rgb[1] == 0 && rgb[2] == 255);
  assert(!TexDecodeTexel(TEXF_PAL8, argb, 8, 2, 1, 0, 0, rgb) && !TexDecodeTexel(TEXF_DXT1, nullptr, 0, 4, 4, 0, 0, rgb));
  std::puts("test_texdecode OK");
  return 0;
}
