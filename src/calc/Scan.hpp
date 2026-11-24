// Skaner laserowy naziemny (bez Gothic API): siatka kierunkow skanu, szum i intensywnosc, zapis chmury punktow (surowy plik tymczasowy -> .pts i .las).
// Uklad skanera: x = wschod, y = polnoc, z = w gore; azymut 0 = polnoc, rosnie ku wschodowi; V = kat zenitalny (0 = w gore, 90 = poziomo).
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

struct ScanPreset { const char* name; double stepDeg; };
inline const ScanPreset kScanPresets[4] = {
  {"gruby: krok 1,0 st. (ok. 175 mm na 10 m)", 1.0},
  {"sredni: krok 0,5 st. (ok. 87 mm na 10 m)", 0.5},
  {"dokladny: krok 0,25 st. (ok. 44 mm na 10 m)", 0.25},
  {"bardzo dokladny: krok 0,1 st. (ok. 17 mm na 10 m)", 0.1},
};
inline const int kScanPresetCount = 4;
inline const double kScanMinRangeM = 0.5, kScanMaxRangeM = 120.0;   // zasieg skanera
inline const double kScanVMaxDeg = 125.0;                           // martwe pole pod statywem: V od 0 (zenit) do 125 st. (35 st. ponizej poziomu)

struct ScanGrid { int nHz = 0, nV = 0; double stepDeg = 1.0; long long Total() const { return (long long)nHz * nV; } };
inline ScanGrid ScanGridFor(double stepDeg) {
  ScanGrid g; g.stepDeg = stepDeg;
  g.nHz = (int)std::floor(360.0 / stepDeg + 0.5); g.nV = (int)std::floor(kScanVMaxDeg / stepDeg + 0.5) + 1;
  return g;
}
// Kierunek punktu o numerze idx: kolumnami (jak obrot glowicy): dla kazdego azymutu pelny przeciag od zenitu w dol.
inline void ScanDirection(const ScanGrid& g, long long idx, double& azDeg, double& vDeg) {
  azDeg = (double)(idx / g.nV) * g.stepDeg; vDeg = (double)(idx % g.nV) * g.stepDeg;
}

// Szum odleglosci (1 sigma) [m]: 1,5 mm + 10 ppm odleglosci.
inline double ScanNoiseSigmaM(double distM) { return 0.0015 + 1e-5 * distM; }
// Intensywnosc 0..255: cos kata padania (powierzchnia prostopadla = jasna) i spadek z odlegloscia.
inline std::uint8_t ScanIntensity(double distM, double cosInc, bool isVob) {
  const double c = (std::min)(1.0, (std::max)(0.05, cosInc)), f = 1.0 / (1.0 + (distM / 60.0) * (distM / 60.0)), base = isVob ? 0.9 : 0.75;
  return (std::uint8_t)(std::min)(250.0, (std::max)(4.0, 255.0 * base * c * (0.4 + 0.6 * f)));     // 255 zarezerwowane dla kul referencyjnych (retrorefleksja)
}

struct ScanLocal { float x, y, z; std::uint8_t i; };
// Odczytany kierunek i odleglosc -> punkt w ukladzie skanera.
inline ScanLocal ScanToLocal(double azDeg, double vDeg, double rangeM, std::uint8_t intensity) {
  const double pi = 3.14159265358979323846, az = azDeg * pi / 180.0, v = vDeg * pi / 180.0, s = std::sin(v);
  return {(float)(rangeM * s * std::sin(az)), (float)(rangeM * s * std::cos(az)), (float)(rangeM * std::cos(v)), intensity};
}

// Surowy plik tymczasowy: rekordy 13 bajtow (3 x float + uint8). Stanowisko dodaje sie przy konwersji.
class ScanRawWriter {
 public:
  bool Open(const std::string& path) { path_ = path; out_.open(path, std::ios::binary | std::ios::trunc); count_ = 0; return out_.good(); }
  void Append(const ScanLocal& p) {
    out_.write(reinterpret_cast<const char*>(&p.x), 4); out_.write(reinterpret_cast<const char*>(&p.y), 4); out_.write(reinterpret_cast<const char*>(&p.z), 4);
    out_.write(reinterpret_cast<const char*>(&p.i), 1); count_++;
  }
  void Close() { if (out_.is_open()) out_.close(); }
  long long Count() const { return count_; }
  const std::string& Path() const { return path_; }
 private:
  std::ofstream out_; std::string path_; long long count_ = 0;
};

struct ScanStats { long long count = 0; double minX = 0, minY = 0, minZ = 0, maxX = 0, maxY = 0, maxZ = 0; bool ok = false; };

namespace scandetail {
inline void PutLE(std::string& s, std::uint64_t v, int bytes) { for (int i = 0; i < bytes; i++) s += (char)((v >> (8 * i)) & 0xFF); }
inline void PutD(std::string& s, double d) { std::uint64_t u; std::memcpy(&u, &d, 8); PutLE(s, u, 8); }
inline bool ReadRec(std::ifstream& in, ScanLocal& p) {
  in.read(reinterpret_cast<char*>(&p.x), 4); in.read(reinterpret_cast<char*>(&p.y), 4); in.read(reinterpret_cast<char*>(&p.z), 4); in.read(reinterpret_cast<char*>(&p.i), 1);
  return in.good();
}
}  // namespace scandetail

// Konwersja surowego pliku do .pts (x y z intensywnosc) i .las 1.2 (format punktu 0): wspolrzedne siatki E (X), N (Y), H (Z) = stanowisko + wektor lokalny.
// Zwraca statystyki (bounding box w ukladzie siatki). Dwa przebiegi: granice, potem zapis (bez trzymania chmury w pamieci).
// rotDeg: obrot ukladu lokalnego skanera wzgledem siatki (po rejestracji); 0 = lokalne osie zgodne z siatka.
inline ScanStats ScanConvert(const std::string& rawPath, double stN, double stE, double stH, double rotDeg, const std::string& ptsPath, const std::string& lasPath) {
  const double th = rotDeg * 3.14159265358979323846 / 180.0, cth = std::cos(th), sth = std::sin(th);
  ScanStats st;
  {
    std::ifstream in(rawPath, std::ios::binary); ScanLocal p;
    while (scandetail::ReadRec(in, p)) {
      const double X = stE + p.y * sth + p.x * cth, Y = stN + p.y * cth - p.x * sth, Z = stH + p.z;
      if (!st.ok) { st.minX = st.maxX = X; st.minY = st.maxY = Y; st.minZ = st.maxZ = Z; st.ok = true; }
      st.minX = (std::min)(st.minX, X); st.maxX = (std::max)(st.maxX, X); st.minY = (std::min)(st.minY, Y); st.maxY = (std::max)(st.maxY, Y); st.minZ = (std::min)(st.minZ, Z); st.maxZ = (std::max)(st.maxZ, Z);
      st.count++;
    }
  }
  const double scale = 0.001, offX = std::floor(st.minX), offY = std::floor(st.minY), offZ = std::floor(st.minZ);
  std::ofstream pts(ptsPath, std::ios::binary | std::ios::trunc), las(lasPath, std::ios::binary | std::ios::trunc);
  pts << st.count << "\n";
  std::string h;                                                       // naglowek LAS 1.2 (227 bajtow)
  h += "LASF"; scandetail::PutLE(h, 0, 2); scandetail::PutLE(h, 0, 2); scandetail::PutLE(h, 0, 4); scandetail::PutLE(h, 0, 2); scandetail::PutLE(h, 0, 2); scandetail::PutLE(h, 0, 8);
  h += (char)1; h += (char)2;                                          // wersja 1.2
  { char sys[32] = {0}, sw[32] = {0}; std::snprintf(sys, sizeof sys, "GothicSurveying"); std::snprintf(sw, sizeof sw, "GothicSurveying scanner"); h.append(sys, 32); h.append(sw, 32); }
  scandetail::PutLE(h, 1, 2); scandetail::PutLE(h, 2025, 2);           // dzien i rok utworzenia (nieistotne)
  scandetail::PutLE(h, 227, 2); scandetail::PutLE(h, 227, 4); scandetail::PutLE(h, 0, 4);   // rozmiar naglowka, offset danych, liczba VLR
  h += (char)0; scandetail::PutLE(h, 20, 2);                           // format punktu 0, dlugosc rekordu 20
  scandetail::PutLE(h, (std::uint64_t)st.count, 4); for (int i = 0; i < 5; i++) scandetail::PutLE(h, i == 0 ? (std::uint64_t)st.count : 0, 4);
  for (int i = 0; i < 3; i++) scandetail::PutD(h, scale);
  scandetail::PutD(h, offX); scandetail::PutD(h, offY); scandetail::PutD(h, offZ);
  scandetail::PutD(h, st.maxX); scandetail::PutD(h, st.minX); scandetail::PutD(h, st.maxY); scandetail::PutD(h, st.minY); scandetail::PutD(h, st.maxZ); scandetail::PutD(h, st.minZ);
  las.write(h.data(), (std::streamsize)h.size());
  std::ifstream in(rawPath, std::ios::binary); ScanLocal p; std::string rec; char line[96];
  while (scandetail::ReadRec(in, p)) {
    const double X = stE + p.y * sth + p.x * cth, Y = stN + p.y * cth - p.x * sth, Z = stH + p.z;
    std::snprintf(line, sizeof line, "%.3f %.3f %.3f %d\n", X, Y, Z, (int)p.i); pts << line;
    rec.clear();
    scandetail::PutLE(rec, (std::uint32_t)(std::int32_t)std::llround((X - offX) / scale), 4); scandetail::PutLE(rec, (std::uint32_t)(std::int32_t)std::llround((Y - offY) / scale), 4);
    scandetail::PutLE(rec, (std::uint32_t)(std::int32_t)std::llround((Z - offZ) / scale), 4);
    scandetail::PutLE(rec, (std::uint64_t)p.i * 257u, 2);              // intensywnosc 8 -> 16 bit
    rec += (char)0x09; rec += (char)0; rec += (char)0; rec += (char)0; scandetail::PutLE(rec, 0, 2);   // flagi (1 zwrot z 1), klasyfikacja, kat, dane uzytkownika, zrodlo
    las.write(rec.data(), (std::streamsize)rec.size());
  }
  return st;
}
