// Robota "Skaner laserowy naziemny": skaner na statywie skanuje siatka promieni (360 st. w poziomie, V 0..125 st.), zapisuje chmure punktow do .pts i .las
// (bez Gothic API). Skan trwa w czasie gry: w kazdej klatce Tick() puszcza tyle promieni, ile miesci sie w budzecie czasu. Szum odleglosci, zasieg 0,5-120 m,
// intensywnosc z kata padania; skanuje teren i voby ze swiata (przez IWorld::ScanRay).
// Rejestracja: skan w ukladzie lokalnym (zero azymutu skanera losowe), kule referencyjne na punktach osnowy (retrorefleksja = intensywnosc 255) sa wykrywane,
// doskanowywane gesto, dopasowywane (kula metoda najmniejszych kwadratow) i dopasowywane do wspolrzednych punktow: stanowisko i obrot skanera z >= 3 kul.
#pragma once
#include <chrono>
#include <cstdio>
#include <filesystem>
#include "Job.hpp"
#include "Keys.hpp"
#include "calc/Scan.hpp"
#include "calc/ScanReg.hpp"

class ScanJob : public Job {
 public:
  ScanJob(IWorld& w, const JobOptions& o) : w_(w), opt_(o), rng_((uint32_t)(w.Random01() * 4e9) + 1) {
    lib_.enabled = w.InstrumentLevelingEnabled(); regOn_ = w.ScanRegistrationByDefault();
  }
  ~ScanJob() override { AbortScan(); }

  std::string Name() const override { return "Skaner laserowy"; }
  JobKind Kind() const override { return JobKind::Scan; }
  Libella* GetLibella() override { return &lib_; }
  std::string InstrumentBlock() const override { return lib_.Within(kLevelBubbleArcmin) ? "" : "Skaner niewypoziomowany (menu: Poziomuj instrument)"; }
  bool CanSaveState() const override { return true; }
  bool NeedsChoice(const std::string& menuId) const override { return menuId == "target"; }

  std::vector<MenuItem> Menu() const override {
    char res[160]; std::snprintf(res, sizeof res, "Rozdzielczosc: %s (zmien)", kScanPresets[preset_].name);
    const ScanGrid g = ScanGridFor(kScanPresets[preset_].stepDeg);
    char ph[96]; std::snprintf(ph, sizeof ph, "przed skanem; skan ma %lld kierunkow", g.Total());
    const bool regOk = !regOn_ || targets_.size() >= 3;
    return {
      {"station", stationUp_ ? "Przestaw skaner (stanowisko)" : "Postaw skaner (stanowisko)", "nie w trakcie skanu", !scanning_},
      {"res", res, ph, !scanning_},
      {"rgb", rgbOn_ ? "Kolor RGB: wlaczony, z tekstur swiata (zmien)" : "Kolor RGB: wylaczony, szare punkty (zmien)", "kolor punktu = srednia barwa tekstury trafionej powierzchni; nie w trakcie skanu", !scanning_},
      {"reg", regOn_ ? "Rejestracja: wlaczona, kule referencyjne (zmien)" : "Rejestracja: wylaczona, siatka ze stanowiska (zmien)",
       "wlaczona: skan w ukladzie lokalnym, stanowisko z kul na punktach osnowy; nie w trakcie skanu", !scanning_},
      {"target", "Dodaj kule referencyjna (punkt osnowy)", "max 6; punkt osnowy do 150 m od ciebie, kula staje nad znakiem", !scanning_ && targets_.size() < 6},
      {"untarget", "Usun ostatnia kule", "brak kul", !scanning_ && !targets_.empty()},
      {"scan", "Rozpocznij skan", regOk ? "najpierw postaw i wypoziomuj skaner" : "rejestracja wymaga min. 3 kul (albo wylacz rejestracje)",
       stationUp_ && !scanning_ && lib_.Within(kLevelBubbleArcmin) && regOk},
      {"cancel", "Przerwij skan", "trwa skan", scanning_},
      {"finish", "Zakoncz robote", "najpierw wykonaj co najmniej jeden skan", !scans_.empty() && !scanning_},
    };
  }

  std::vector<MenuItem> TachyChoices(const std::string& menuId) const override {
    std::vector<MenuItem> out;
    if (menuId != "target") return out;
    for (const ControlPoint& c : w_.ControlNearHero(150.0)) {
      bool used = false; for (const Tgt& t : targets_) if (t.id == c.id) used = true;
      if (used) continue;
      char b[120]; std::snprintf(b, sizeof b, "%s  N %.3f E %.3f H %.3f", c.id.c_str(), c.n, c.e, c.h);
      out.push_back({"target:" + c.id, b, "", true});
    }
    return out;
  }

  void OnMenu(const std::string& id) override {
    const size_t colon = id.find(':');
    const std::string base = colon == std::string::npos ? id : id.substr(0, colon), arg = colon == std::string::npos ? "" : id.substr(colon + 1);
    for (const MenuItem& m : Menu()) if (m.id == base && !m.enabled) return;
    if (base == "station") {
      if (!w_.PlaceScanner()) { w_.Say("Brak gruntu pod stopami"); return; }
      stationUp_ = true;
      if (lib_.enabled) { const double u1 = w_.Random01(), u2 = w_.Random01(); lib_.Reset(u1, u2); }
    } else if (base == "res") {
      preset_ = (preset_ + 1) % kScanPresetCount;
    } else if (base == "rgb") {
      rgbOn_ = !rgbOn_;
    } else if (base == "reg") {
      regOn_ = !regOn_;
    } else if (base == "target") {
      for (const ControlPoint& c : w_.ControlNearHero(150.0)) {
        if (c.id != arg) continue;
        int slot = 1; for (const Tgt& t : targets_) slot = (std::max)(slot, t.slot + 1);
        if (!w_.PlaceScanTarget(slot, c)) { w_.Say("Nie mozna postawic kuli"); return; }
        targets_.push_back({c.id, c, slot}); return;
      }
      w_.Say("Brak takiego punktu osnowy w poblizu");
    } else if (base == "untarget") {
      w_.RemoveScanTarget(targets_.back().slot); targets_.pop_back();
    } else if (base == "scan") {
      StartScan();
    } else if (base == "cancel") {
      AbortScan(); w_.Say("Skan przerwany");
    }
  }

  // Skan w czasie gry: promienie w budzecie czasu klatki (faza 1: skan glowny, faza 2: gesto kule referencyjne).
  void Tick() override {
    if (!scanning_) return;
    const auto t0 = std::chrono::steady_clock::now();
    auto over = [&]() { return std::chrono::steady_clock::now() - t0 > std::chrono::milliseconds(kBudgetMs); };
    if (phase_ == 1) {
      while (idx_ < grid_.Total()) {
        double az, v; ScanDirection(grid_, idx_++, az, v);
        double dist = 0, cosInc = 1; bool vob = false, tgt = false; unsigned rgb = 0x808080;
        if (w_.ScanRay(std::fmod(az + psi_, 360.0), 90.0 - v, dist, cosInc, vob, tgt, rgbOn_ ? &rgb : nullptr) && dist >= kScanMinRangeM && dist <= kScanMaxRangeM) {
          const double d = dist + ScanNoiseSigmaM(dist) * rng_.Normal();
          const std::uint8_t inten = tgt ? (std::uint8_t)255 : ScanIntensity(dist, cosInc, vob);
          if (tgt) rgb = 0xFF8000;                                                      // kula referencyjna: pomaranczowa
          else if (!rgbOn_) rgb = (unsigned)inten * 0x010101u;                          // bez koloru: szarosc z intensywnosci
          const ScanLocal p = ScanToLocal(az, v, d, inten, rgb);
          raw_.Append(p);
          if (tgt && brightDir_.size() < 50000) { brightDir_.push_back({az, v, dist}); brightLoc_.push_back({p.x, p.y, p.z}); }
        }
        if ((idx_ & 63) == 0 && over()) return;
      }
      raw_.Close();
      if (regOn_) { windows_ = ScanTargetWindows(brightDir_); wi_ = 0; wIdx_ = 0; }
      if (regOn_ && !windows_.empty()) { phase_ = 2; return; }
      FinishScan(); return;
    }
    while (wi_ < windows_.size()) {                                         // faza 2: gesty skan okien wokol jasnych trafien
      const ScanWindow& wd = windows_[wi_];
      const long long side = (long long)std::floor(2.0 * wd.halfDeg / wd.stepDeg + 0.5) + 1, total = side * side;
      while (wIdx_ < total) {
        const long long k = wIdx_++;
        const double az = wd.azCenter - wd.halfDeg + (double)(k / side) * wd.stepDeg, v = wd.vCenter - wd.halfDeg + (double)(k % side) * wd.stepDeg;
        if (v < 0.0 || v > kScanVMaxDeg) continue;
        double dist = 0, cosInc = 1; bool vob = false, tgt = false;
        if (w_.ScanRay(std::fmod(az + 720.0 + psi_, 360.0), 90.0 - v, dist, cosInc, vob, tgt) && tgt && dist >= kScanMinRangeM && dist <= kScanMaxRangeM && brightLoc_.size() < 400000) {
          const ScanLocal p = ScanToLocal(az, v, dist + ScanNoiseSigmaM(dist) * rng_.Normal(), 255);
          brightLoc_.push_back({p.x, p.y, p.z});
        }
        if ((wIdx_ & 63) == 0 && over()) return;
      }
      wi_++; wIdx_ = 0;
    }
    FinishScan();
  }

  std::string Hud() const override {
    char b[160];
    if (scanning_) {
      if (phase_ == 2) { std::snprintf(b, sizeof b, "Skan %d: doskanowanie kul %d/%d", (int)scans_.size() + 1, (int)wi_ + 1, (int)windows_.size()); return b; }
      std::snprintf(b, sizeof b, "Skan %d: %d%% (%lld punktow)", (int)scans_.size() + 1, (int)(100.0 * idx_ / grid_.Total()), raw_.Count()); return b;
    }
    if (!stationUp_) return std::string("Postaw skaner (") + LV_KEYNAME_MENU + " - menu)";
    if (regOn_ && targets_.size() < 3) return std::string("Dodaj co najmniej 3 kule referencyjne na punktach osnowy (") + LV_KEYNAME_MENU + " - menu)";
    if (scans_.empty()) return std::string("Wypoziomuj skaner i rozpocznij skan (") + LV_KEYNAME_MENU + " - menu)";
    return std::string("Skanow: ") + std::to_string(scans_.size()) + ". Przestaw skaner i skanuj dalej albo zakoncz robote (" + LV_KEYNAME_MENU + " - menu)";
  }

  bool CanFinish() const override { return !scans_.empty() && !scanning_; }

  std::vector<std::string> SummaryLines() const override {
    std::vector<std::string> out; char b[260];
    out.push_back("Skanow: " + std::to_string(scans_.size()) + (outDir_.empty() ? "" : ", folder: " + outDir_));
    for (size_t i = 0; i < scans_.size(); i++) {
      const Scan& s = scans_[i];
      std::snprintf(b, sizeof b, "%s: %lld punktow, %s%s", s.name.c_str(), s.points, s.source.c_str(), s.local ? " [uklad lokalny]" : ""); out.push_back(b);
    }
    return out;
  }

  void SaveState(StateKV& s) const override {                          // trwajacy skan nie jest wznawiany; zapisane skany i kule zostaja
    s.Bool("stationUp", stationUp_); s.Int("preset", preset_); s.Bool("regOn", regOn_); s.Bool("rgbOn", rgbOn_); s.Str("outDir", outDir_); s.Num("lib.x", lib_.x); s.Num("lib.y", lib_.y);
    s.Int("targets.n", (long long)targets_.size());
    for (size_t i = 0; i < targets_.size(); i++) { const std::string p = "targets." + std::to_string(i); s.Str(p + ".id", targets_[i].id); s.Int(p + ".slot", targets_[i].slot); PutControl(s, p + ".cp", targets_[i].cp); }
    s.Int("scans.n", (long long)scans_.size());
    for (size_t i = 0; i < scans_.size(); i++) {
      const std::string p = "scans." + std::to_string(i); const Scan& c = scans_[i];
      s.Str(p + ".name", c.name); s.Int(p + ".pts", c.points); s.Str(p + ".src", c.source); s.Bool(p + ".reg", c.registered); s.Bool(p + ".local", c.local);
    }
  }
  bool LoadState(const StateKV& s) override {
    AbortScan(); stationUp_ = s.GetBool("stationUp"); preset_ = s.GetInt("preset") % kScanPresetCount; regOn_ = s.GetBool("regOn"); rgbOn_ = s.GetBool("rgbOn", true); outDir_ = s.GetStr("outDir");
    lib_.x = s.GetNum("lib.x"); lib_.y = s.GetNum("lib.y");
    targets_.clear(); const int nt = s.GetInt("targets.n");
    for (int i = 0; i < nt; i++) { const std::string p = "targets." + std::to_string(i); targets_.push_back({s.GetStr(p + ".id"), GetControl(s, p + ".cp"), s.GetInt(p + ".slot")}); }
    scans_.clear(); const int n = s.GetInt("scans.n");
    for (int i = 0; i < n; i++) {
      const std::string p = "scans." + std::to_string(i); Scan c;
      c.name = s.GetStr(p + ".name"); c.points = s.GetInt(p + ".pts"); c.source = s.GetStr(p + ".src"); c.registered = s.GetBool(p + ".reg"); c.local = s.GetBool(p + ".local");
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
  struct Scan { std::string name, source; long long points = 0; bool registered = false, local = false; };
  struct Tgt { std::string id; ControlPoint cp; int slot = 0; };
  static constexpr double kInstrH = 1.5;                                // wysokosc osi skanera nad gruntem [m] (jak tachimetr)
  static constexpr int kBudgetMs = 12;                                  // budzet czasu skanu na klatke

  void StartScan() {
    if (outDir_.empty()) outDir_ = w_.ScanOutDir();
    if (outDir_.empty()) { w_.Say("Nie mozna utworzyc folderu skanow"); return; }
    char nm[32]; std::snprintf(nm, sizeof nm, "skan%02d", (int)scans_.size() + 1); curName_ = nm;
    const std::vector<ControlPoint> ctl = w_.ControlNearHero(2.0);       // bez rejestracji: skaner na znaku osnowy ma jego wspolrzedne (z GNSS), inaczej prawdziwe
    const GridPos tru = w_.StationGrid();
    if (!ctl.empty()) { stN_ = ctl[0].n; stE_ = ctl[0].e; stH_ = ctl[0].h; source_ = "znak osnowy " + ctl[0].id; }
    else { stN_ = tru.n; stE_ = tru.e; stH_ = tru.h; source_ = "dowolne stanowisko (wspolrzedne prawdziwe)"; }
    stH_ += kInstrH;                                                      // srodek skanera = grunt + wysokosc instrumentu
    psi_ = regOn_ ? 360.0 * rng_.Uniform() : 0.0;                         // zero azymutu skanera (nieznane: wyznaczy rejestracja)
    brightDir_.clear(); brightLoc_.clear(); windows_.clear();
    rawPath_ = outDir_ + "/" + curName_ + ".tmp";
    if (!raw_.Open(rawPath_)) { w_.Say("Blad zapisu skanu"); return; }
    grid_ = ScanGridFor(kScanPresets[preset_].stepDeg); idx_ = 0; scanning_ = true; phase_ = 1;
    w_.Say("Skan rozpoczety");
  }

  void FinishScan() {
    raw_.Close(); scanning_ = false; phase_ = 0;
    Scan s; s.name = curName_; std::string files = curName_;
    double n = stN_, e = stE_, h = stH_, rot = 0;
    if (regOn_) {
      std::vector<P3> centers;
      for (const std::vector<P3>& cl : ScanClusterBright(brightLoc_, 0.25, 12)) {
        P3 c; double r = 0; if (ScanFitSphere(cl, kScanTargetRadiusM, 0.03, c, r)) centers.push_back(c);
      }
      std::vector<ScanRef> refs; for (const Tgt& t : targets_) refs.push_back({t.id, t.cp.n, t.cp.e, t.cp.h + kScanTargetUpM});
      const ScanReg reg = ScanRegister(centers, refs);
      char b[240];
      if (reg.ok) {
        n = reg.n; e = reg.e; h = reg.h; rot = reg.rotDeg; s.registered = true;
        std::string ids; for (const std::string& i : reg.ids) ids += (ids.empty() ? "" : ",") + i;
        std::snprintf(b, sizeof b, "rejestracja na %d kulach (%s): rms %.1f mm, stanowisko N %.3f E %.3f H %.3f, obrot %.3f st.", (int)reg.ids.size(), ids.c_str(), reg.rmsM * 1000.0, n, e, h, rot);
      } else {
        n = e = h = 0; rot = 0; s.local = true; files += "_lokalny";
        std::snprintf(b, sizeof b, "rejestracja nieudana (%s)", reg.msg.c_str());
        w_.Say(std::string("Rejestracja nieudana: ") + reg.msg + ". Zapisano w ukladzie lokalnym skanera");
      }
      s.source = b;
    } else {
      char b[200]; std::snprintf(b, sizeof b, "%s: N %.3f E %.3f H %.3f (grunt), bez rejestracji", source_.c_str(), stN_, stE_, stH_ - kInstrH); s.source = b;
    }
    const std::string base = outDir_ + "/" + files;
    const ScanStats st = ScanConvert(rawPath_, n, e, h, rot, base + ".pts", base + ".las");
    std::error_code ec; std::filesystem::remove(rawPath_, ec); rawPath_.clear();
    s.points = st.count; scans_.push_back(s);
    char b[220]; std::snprintf(b, sizeof b, "Skan %s gotowy: %lld punktow (%s.pts, %s.las)%s", curName_.c_str(), st.count, files.c_str(), files.c_str(), s.registered ? ", zarejestrowany" : "");
    w_.Say(b);
    WriteInfo(s, files);
  }

  void WriteInfo(const Scan& s, const std::string& files) {              // opis skanu obok plikow
    std::ofstream f(outDir_ + "/" + files + "_opis.txt");
    f << "Skan " << s.name << "\nPunkty: " << s.points << "\nRozdzielczosc: " << kScanPresets[preset_].name << "\nZasieg: " << kScanMinRangeM << " - " << kScanMaxRangeM
      << " m\nSzum odleglosci: 1,5 mm + 10 ppm\nStanowisko: " << s.source << "\nOs skanera " << kInstrH << " m nad gruntem\n"
      << (s.local ? "Uklad: lokalny skanera (X = wschod, Y = polnoc wg zera skanera, Z = w gore), srodek skanera w (0,0,0)\n" : "Uklad: X = E, Y = N, Z = H (m), siatka\n")
      << "Intensywnosc 0-255 (255 = kula referencyjna); kolor RGB: " << (rgbOn_ ? "srednia barwa tekstury powierzchni" : "wylaczony (szarosc z intensywnosci)") << "\n";
  }

  void AbortScan() {
    if (!scanning_ && rawPath_.empty()) return;
    raw_.Close(); scanning_ = false; phase_ = 0;
    std::error_code ec; if (!rawPath_.empty()) std::filesystem::remove(rawPath_, ec);
    rawPath_.clear();
  }

  IWorld& w_; JobOptions opt_; GnssRng rng_; Libella lib_;
  bool stationUp_ = false, scanning_ = false, regOn_ = false, rgbOn_ = true; int preset_ = 1, phase_ = 0;
  ScanGrid grid_; long long idx_ = 0; ScanRawWriter raw_; std::string rawPath_, outDir_, curName_, source_; double stN_ = 0, stE_ = 0, stH_ = 0, psi_ = 0;
  std::vector<ScanBrightHit> brightDir_; std::vector<P3> brightLoc_; std::vector<ScanWindow> windows_; size_t wi_ = 0; long long wIdx_ = 0;
  std::vector<Tgt> targets_; std::vector<Scan> scans_;
};

inline std::unique_ptr<Job> CreateScanJob(IWorld& w, const JobOptions& o) { return std::make_unique<ScanJob>(w, o); }
