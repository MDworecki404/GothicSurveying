#include <cassert>
#include <cstdio>
#include "calc/TexColors.hpp"

static std::string Entry(const std::string& name, int w, int h, const std::vector<unsigned char>& px) {
  std::string s; s += (char)name.size(); s += name; s += (char)w; s += (char)h; s.append(reinterpret_cast<const char*>(px.data()), px.size()); return s;
}

int main() {
  assert(TexColorKey("OW_MISC_WOOD_01.TGA") == "OW_MISC_WOOD_01" && TexColorKey("ow_misc_wood_01-c.tex") == "OW_MISC_WOOD_01" && TexColorKey("_work\\Data\\Textures\\Wall-C.TEX") == "WALL");
  assert(TexColorKey("") == "" && TexColorKey("A-C") == "A" && TexColorKey("NOEXT") == "NOEXT");
  // plik: dwie tekstury: 2x1 (czerwony | niebieski) i 1x1 zielona
  std::string d = "GSTC"; const std::uint32_t n = 2; d.append(reinterpret_cast<const char*>(&n), 4);
  d += Entry("TWO", 2, 1, {255, 0, 0, 0, 0, 255}); d += Entry("GREEN", 1, 1, {0, 200, 0});
  std::map<std::string, TexThumb> m;
  assert(TexColorsParse(d, m) == 2 && m.count("TWO") && m["TWO"].w == 2 && m["GREEN"].px[1] == 200);
  assert(TexColorsParse("XXXX", m) == 0 && TexColorsParse(d.substr(0, 20), m) <= 1);                 // urwany plik: nie zawiesza sie
  const TexThumb& t = m["TWO"];
  assert(TexThumbSample(t, 0.25, 0.5) == 0xFF0000 && TexThumbSample(t, 0.75, 0.5) == 0x0000FF);        // srodki tekseli
  const unsigned mid = TexThumbSample(t, 0.5, 0.5);                                                    // granica: mieszanka
  assert(((mid >> 16) & 255) > 100 && (mid & 255) > 100);
  assert(TexThumbSample(t, 1.25, 3.5) == 0xFF0000 && TexThumbSample(t, -0.25, 0.0) == 0x0000FF);       // zawijanie
  assert(TexThumbAverage(t) == 0x7F007F || TexThumbAverage(t) == 0x80007F);
  std::puts("test_texcolors OK");
  return 0;
}
