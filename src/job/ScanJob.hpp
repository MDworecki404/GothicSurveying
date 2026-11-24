// Robota "Skaner laserowy naziemny": skaner na statywie skanuje siatka promieni (360 st. w poziomie, V 0..125 st.), zapisuje chmure punktow do .pts i .las
// (bez Gothic API). Skan trwa w czasie gry: w kazdej klatce Tick() puszcza tyle promieni, ile miesci sie w budzecie czasu. Szum odleglosci, zasieg 0,5-120 m,
// intensywnosc z kata padania; skanuje teren i voby ze swiata (przez IWorld::ScanRay).
#pragma once
#include <chrono>
#include <cstdio>
#include <filesystem>
#include "Job.hpp"
#include "Keys.hpp"
#include "calc/Scan.hpp"

class ScanJob : public Job {
 public:
  ScanJob(IWorld& w, const JobOptions& o) : w_(w), opt_(o), rng_((uint32_t)(w.Random01() * 4e9) + 1) { lib_.enabled = w.InstrumentLevelingEnabled(); }
  ~ScanJob() override { AbortScan(); }

  std::string Name() const override { return "Skaner laserowy"; }
  JobKind Kind() const override { return JobKind::Scan; }
  Libella* GetLibella() override { return &lib_; }
  std::string InstrumentBlock() const override { return lib_.Within(kLevelBubbleArcmin) ? "" : "Skaner niewypoziomowany (menu: Poziomuj instrument)"; }
  bool CanSaveState() const override { return true; }

  std::vector<MenuItem> Menu() const override {
    char res[160]; std::snprintf(res, sizeof res, "Rozdzielczosc: %s (zmien)", kScanPresets[preset_].name);
    const ScanGrid g = ScanGridFor(kScanPresets[preset_].stepDeg);
    char ph[96]; std::snprintf(ph, sizeof ph, "przed skanem; skan ma %lld punktow-kierunkow", g.Total());
    return {
      {"station", stationUp_ ? "Przestaw skaner (stanowisko)" : "Postaw skaner (stanowisko)", "nie w trakcie skanu", !scanning_},
      {"res", res, ph, !scanning_},
      {"scan", "Rozpocznij skan", "najpierw postaw i wypoziomuj skaner", stationUp_ && !scanning_ && lib_.Within(kLevelBubbleArcmin)},
      {"cancel", "Przerwij skan", "trwa skan", scanning_},
      {"finish", "Zakoncz robote", "najpierw wykonaj co najmniej jeden skan", !scans_.empty() && !scanning_},
    };
  }

  void OnMenu(const std::string& id) override {
    for (const MenuItem& m : Menu()) if (m.id == id && !m.enabled) return;
    if (id == "station") {
      if (!w_.PlaceTachymeter()) { w_.Say("Brak gruntu pod stopami"); return; }
      stationUp_ = true;
      if (lib_.enabled) { const double u1 = w_.Random01(), u2 = w_.Random01(); lib_.Reset(u1, u2); }
    } else if (id == "res") {
      preset_ = (preset_ + 1) % kScanPresetCount;
    } else if (id == "scan") {
      StartScan();
    } else if (id == "cancel") {
      AbortScan(); w_.Say("Skan przerwany");
    }
  }

  // Skan w czasie gry: promienie w budzecie czasu klatki.
  void Tick() override {
    if (!scanning_) return;
    const auto t0 = std::chrono::steady_clock::now();
    while (idx_ < grid_.Total()) {
      double az, v; ScanDirection(grid_, idx_++, az, v);
      double dist = 0, cosInc = 1; bool vob = false;
      if (w_.ScanRay(az, 90.0 - v, dist, cosInc, vob) && dist >= kScanMinRangeM && dist <= kScanMaxRangeM) {
        const double d = dist + ScanNoiseSigmaM(dist) * rng_.Normal();
        raw_.Append(ScanToLocal(az, v, d, ScanIntensity(dist, cosInc, vob)));
      }
      if ((idx_ & 63) == 0 && std::chrono::steady_clock::now() - t0 > std::chrono::milliseconds(kBudgetMs)) break;
    }
    if (idx_ >= grid_.Total()) FinishScan();
  }

  std::string Hud() const override {
    char b[160];
    if (scanning_) { std::snprintf(b, sizeof b, "Skan %d: %d%% (%lld punktow)", (int)scans_.size() + 1, (int)(100.0 * idx_ / grid_.Total()), raw_.Count()); return b; }
    if (!stationUp_) return std::string("Postaw skaner (") + LV_KEYNAME_MENU + " - menu)";
    if (scans_.empty()) return std::string("Wypoziomuj skaner i rozpocznij skan (") + LV_KEYNAME_MENU + " - menu)";
    return std::string("Skanow: ") + std::to_string(scans_.size()) + ". Przestaw skaner i skanuj dalej albo zakoncz robote (" + LV_KEYNAME_MENU + " - menu)";
  }

  bool CanFinish() const override { return !scans_.empty() && !scanning_; }

  std::vector<std::string> SummaryLines() const override {
    std::vector<std::string> out; char b[220];
    out.push_back("Skanow: " + std::to_string(scans_.size()) + (outDir_.empty() ? "" : ", folder: " + outDir_));
    for (size_t i = 0; i < scans_.size(); i++) {
      const Scan& s = scans_[i];
      std::snprintf(b, sizeof b, "%s: %lld punktow, stanowisko N %.3f E %.3f H %.3f (%s)", s.name.c_str(), s.points, s.n, s.e, s.h, s.source.c_str()); out.push_back(b);
    }
    return out;
  }

  void SaveState(StateKV& s) const override {                          // trwajacy skan nie jest wznawiany; zapisane skany zostaja
    s.Bool("stationUp", stationUp_); s.Int("preset", preset_); s.Str("outDir", outDir_); s.Num("lib.x", lib_.x); s.Num("lib.y", lib_.y); s.Int("scans.n", (long long)scans_.size());
    for (size_t i = 0; i < scans_.size(); i++) {
      const std::string p = "scans." + std::to_string(i); const Scan& c = scans_[i];
      s.Str(p + ".name", c.name); s.Int(p + ".pts", c.points); s.Num(p + ".n", c.n); s.Num(p + ".e", c.e); s.Num(p + ".h", c.h); s.Str(p + ".src", c.source);
    }
  }
  bool LoadState(const StateKV& s) override {
    AbortScan(); stationUp_ = s.GetBool("stationUp"); preset_ = s.GetInt("preset") % kScanPresetCount; outDir_ = s.GetStr("outDir"); lib_.x = s.GetNum("lib.x"); lib_.y = s.GetNum("lib.y");
    scans_.clear(); const int n = s.GetInt("scans.n");
    for (int i = 0; i < n; i++) {
      const std::string p = "scans." + std::to_string(i); Scan c;
      c.name = s.GetStr(p + ".name"); c.points = s.GetInt(p + ".pts"); c.n = s.GetNum(p + ".n"); c.e = s.GetNum(p + ".e"); c.h = s.GetNum(p + ".h"); c.source = s.GetStr(p + ".src");
      scans_.push_back(c);
    }
    return true;
  }

  // Interfejs niwelacji: ta robota go nie uzywa.
  std::vector<int> ActiveRodPoints() const override { return {}; }
  int RodPointFor(RodRole) const override { return -1; }
  bool OnReading(int, int) override { return false; }
  FieldBook Finish(bool) override { return FieldBook{}; }

 private:
  struct Scan { std::string name, source; long long points = 0; double n = 0, e = 0, h = 0; };
  static constexpr double kInstrH = 1.5;                                // wysokosc osi skanera nad gruntem [m] (jak tachimetr)
  static constexpr int kBudgetMs = 12;                                  // budzet czasu skanu na klatke

  void StartScan() {
    if (outDir_.empty()) outDir_ = w_.ScanOutDir();
    if (outDir_.empty()) { w_.Say("Nie mozna utworzyc folderu skanow"); return; }
    char nm[32]; std::snprintf(nm, sizeof nm, "skan%02d", (int)scans_.size() + 1); curName_ = nm;
    const std::vector<ControlPoint> ctl = w_.ControlNearHero(2.0);       // skaner na znaku osnowy: wspolrzedne znaku (z GNSS), inaczej prawdziwe
    const GridPos tru = w_.StationGrid();
    if (!ctl.empty()) { stN_ = ctl[0].n; stE_ = ctl[0].e; stH_ = ctl[0].h; source_ = "znak osnowy " + ctl[0].id; }
    else { stN_ = tru.n; stE_ = tru.e; stH_ = tru.h; source_ = "dowolne stanowisko (wspolrzedne prawdziwe)"; }
    stH_ += kInstrH;                                                      // srodek skanera = grunt + wysokosc instrumentu
    rawPath_ = outDir_ + "/" + curName_ + ".tmp";
    if (!raw_.Open(rawPath_)) { w_.Say("Blad zapisu skanu"); return; }
    grid_ = ScanGridFor(kScanPresets[preset_].stepDeg); idx_ = 0; scanning_ = true;
    w_.Say("Skan rozpoczety");
  }

  void FinishScan() {
    raw_.Close(); scanning_ = false;
    const std::string base = outDir_ + "/" + curName_;
    const ScanStats st = ScanConvert(rawPath_, stN_, stE_, stH_, base + ".pts", base + ".las");
    std::error_code ec; std::filesystem::remove(rawPath_, ec);
    Scan s; s.name = curName_; s.points = st.count; s.n = stN_; s.e = stE_; s.h = stH_ - kInstrH; s.source = source_;   // h = grunt pod skanerem
    scans_.push_back(s); rawPath_.clear();
    char b[200]; std::snprintf(b, sizeof b, "Skan %s gotowy: %lld punktow (%s.pts, %s.las)", curName_.c_str(), st.count, curName_.c_str(), curName_.c_str());
    w_.Say(b);
    WriteInfo(s);
  }

  void WriteInfo(const Scan& s) {                                        // opis skanu obok plikow
    std::ofstream f(outDir_ + "/" + s.name + "_opis.txt");
    f << "Skan " << s.name << "\nPunkty: " << s.points << "\nRozdzielczosc: " << kScanPresets[preset_].name << "\nZasieg: " << kScanMinRangeM << " - " << kScanMaxRangeM
      << " m\nSzum odleglosci: 1,5 mm + 10 ppm\nStanowisko (grunt): N " << s.n << " E " << s.e << " H " << s.h << " (" << s.source << "), os skanera " << kInstrH << " m nad gruntem\n"
      << "Uklad: X = E, Y = N, Z = H (m); intensywnosc 0-255\n";
  }

  void AbortScan() {
    if (!scanning_ && rawPath_.empty()) return;
    raw_.Close(); scanning_ = false;
    std::error_code ec; if (!rawPath_.empty()) std::filesystem::remove(rawPath_, ec);
    rawPath_.clear();
  }

  IWorld& w_; JobOptions opt_; GnssRng rng_; Libella lib_;
  bool stationUp_ = false, scanning_ = false; int preset_ = 1;
  ScanGrid grid_; long long idx_ = 0; ScanRawWriter raw_; std::string rawPath_, outDir_, curName_, source_; double stN_ = 0, stE_ = 0, stH_ = 0;
  std::vector<Scan> scans_;
};

inline std::unique_ptr<Job> CreateScanJob(IWorld& w, const JobOptions& o) { return std::make_unique<ScanJob>(w, o); }
