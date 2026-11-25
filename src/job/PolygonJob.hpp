// Robota "Ciag poligonowy" (bez Gothic API): tachimetr na kolejnych punktach ciagu, kat poziomy lewy w dwoch polozeniach lunety, boki mierzone z obu
// koncow, rachunek w calc/Polygon.hpp. Rodzaje: nawiazany obustronnie (Anchored), zamkniety (Closed), wiszacy (Open). Wzor: job/TrigLevel.hpp.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include "Job.hpp"
#include "Keys.hpp"
#include "calc/Polygon.hpp"
#include "calc/PolygonReport.hpp"

class PolygonJob : public Job {
 public:
  PolygonJob(IWorld& w, const JobOptions& o) : w_(w), opt_(o), kind_(o.kind), rng_((uint32_t)(w.Random01() * 4e9) + 1) { w_.SetPrismHeightM(kPrism[0]); lib_.enabled = w_.InstrumentLevelingEnabled(); }
  void SetNoise(const TachyNoise& n) { noise_ = n; }                         // hak testowy: szum pomiaru (limity zawsze z nominalnej dokladnosci)
  Libella* GetLibella() override { return &lib_; }

  std::string Name() const override { return "Ciag poligonowy"; }
  JobKind Kind() const override { return JobKind::Polygon; }
  bool NeedsChoice(const std::string& menuId) const override {
    if (menuId == "redo") return ObsStationCount() >= 2;               // jedno stanowisko z pomiarami: powtarzamy je bez pytania
    return menuId == "ref_n1" || menuId == "ref_a" || menuId == "ref_b" || menuId == "ref_n2";
  }

  std::vector<MenuItem> TachyChoices(const std::string& menuId) const override {
    std::vector<MenuItem> out;
    if (menuId == "redo") {
      const std::vector<Pt> seq = Seq();
      for (int i = 0; i < SCount(); i++) if (StnHasObs(i)) out.push_back({"redo:" + std::to_string(i), "Stanowisko " + seq[i].name + (StnDone(i) ? " (kat " + Fmt(StnBeta(i)) + " g)" : " (niekompletne)"), "", true});
      return out;
    }
    if (!NeedsChoice(menuId)) return out;
    for (const ControlPoint& c : w_.ControlNearHero(1e9)) {
      if (UsedRef(c.id, menuId)) continue;
      char b[120]; std::snprintf(b, sizeof b, "%s  N %.3f E %.3f", c.id.c_str(), c.n, c.e);
      out.push_back({menuId + ":" + c.id, b, "", true});
    }
    return out;
  }

  std::vector<MenuItem> Menu() const override {
    const bool fresh = !AnyObs(), ready = Ready(), anch = kind_ == TraverseKind::Anchored;
    std::vector<MenuItem> m;
    auto ref = [&](const char* id, const char* what, const Pt& p, bool set) {
      std::string l = set ? std::string(what) + ": " + p.name + " (zmien)" : std::string("Wybierz ") + what + " (punkt osnowy)";
      m.push_back({id, l, "punkt osnowy z listy; nie po zmierzeniu pierwszego stanowiska", fresh});
    };
    ref("ref_n1", kind_ == TraverseKind::Closed ? "punkt orientacji N1" : "nawiazanie poczatkowe N1", n1_, hasN1_);
    ref("ref_a", "pierwszy punkt ciagu A", a_, hasA_);
    if (anch) { ref("ref_b", "ostatni punkt ciagu B", b_, hasB_); ref("ref_n2", "nawiazanie koncowe N2", n2_, hasN2_); }
    m.push_back({"addpt", "Postaw punkt ciagu (pod stopami)", "najpierw nawiazania; punkty zakladasz po kolei od A; nie po zmierzeniu pierwszego stanowiska", hasA_ && fresh && mid_.size() < kMaxMid && (!anch || (hasB_ && hasN2_))});
    const int cur = Cur(), S = SCount();
    const std::string curName = ready && cur < S ? Seq()[cur].name : "?";
    m.push_back({"inst", instUp_ ? "Przestaw tachimetr na punkcie " + curName : "Postaw tachimetr na punkcie " + curName, "najpierw nawiazania i punkty ciagu; nie po zmierzeniu wszystkich stanowisk", ready && cur < S});
    if (ready && cur < S) {
      m.push_back({"prism_back", "Postaw lustro z tylu (" + BackOf(cur).name + ")", "najpierw tachimetr", instUp_});
      m.push_back({"prism_fore", "Postaw lustro z przodu (" + ForeOf(cur).name + ")", "najpierw tachimetr", instUp_});
      if (kind_ == TraverseKind::Closed && cur == 0) m.push_back({"prism_n1", "Postaw lustro na N1 (" + n1_.name + ", orientacja)", "tylko na punkcie A poligonu; najpierw tachimetr", instUp_});
    } else {
      m.push_back({"prism_back", "Postaw lustro z tylu", "najpierw tachimetr", false});
      m.push_back({"prism_fore", "Postaw lustro z przodu", "najpierw tachimetr", false});
    }
    char ph[64]; std::snprintf(ph, sizeof ph, "Wysokosc lustra: %.3f m (zmien)", kPrism[prismIdx_]);
    m.push_back({"prismh", ph, "najpierw nawiazania", ready});
    m.push_back({"view", "Patrz przez lunete (" LV_KEYNAME_SCOPE ")", "najpierw tachimetr i lustro na swoich punktach", instUp_ && prismUp_});
    m.push_back({"redo", "Powtorz stanowisko", "brak zmierzonych stanowisk", AnyObs()});
    m.push_back({"calc", "Podglad rachunku (odchylki)", "po zmierzeniu wszystkich stanowisk", AllDone()});
    m.push_back({"finish", "Zakoncz i oblicz", "po zmierzeniu wszystkich stanowisk", AllDone()});
    return m;
  }

  void OnMenu(const std::string& id) override {
    const size_t colon = id.find(':');
    const std::string base = colon == std::string::npos ? id : id.substr(0, colon), arg = colon == std::string::npos ? "" : id.substr(colon + 1);
    bool listed = false; for (const MenuItem& m : Menu()) if (m.id == base) { listed = true; if (!m.enabled) return; }
    if (!listed) return;                                                  // np. prism_n1 poza poligonem
    if (base == "ref_n1" || base == "ref_a" || base == "ref_b" || base == "ref_n2") { SetRef(base, arg); return; }
    if (base == "addpt") {
      GridPos g;
      if (!w_.HeroGround(g)) { w_.Say("Brak gruntu pod stopami"); return; }
      const int mk = (int)mid_.size() + 1;
      if (!w_.PlaceMarker(mk, MarkerKind::Turning)) { w_.Say("Brak gruntu pod stopami"); return; }
      Pt p; p.name = std::to_string(mk); p.marker = mk; p.truth = g; mid_.push_back(p); Sync();
      char b[80]; std::snprintf(b, sizeof b, "Punkt ciagu %s zalozony", p.name.c_str()); w_.Say(b);
    } else if (base == "inst") {
      const Pt s = Seq()[Cur()];
      if (!(s.control ? w_.PlaceTachymeterAt(s.cp) : w_.PlaceTachymeterOnPoint(s.marker))) { w_.Say("Nie mozna postawic tachimetru na punkcie " + s.name); return; }
      instUp_ = true; prismUp_ = false; prismRole_ = -1; face_ = 1; lastMsg_.clear(); stn_[Cur()] = Stn{};   // nowe zero kola i bledy instrumentu: poprzednie obserwacje tego stanowiska sa niewazne
      collGon_ = (rng_.Uniform() < 0.5 ? -1 : 1) * (0.003 + 0.005 * rng_.Uniform());      // ukryte bledy instrumentu: kolimacja, indeks, zero kola poziomego
      indexGon_ = (rng_.Uniform() < 0.5 ? -1 : 1) * (0.003 + 0.005 * rng_.Uniform());
      az0Deg_ = rng_.Uniform() * 360.0;
      if (lib_.enabled) { const double u1 = w_.Random01(), u2 = w_.Random01(); lib_.Reset(u1, u2); }
    } else if (base == "prism_back" || base == "prism_fore" || base == "prism_n1") {
      const int role = base == "prism_back" ? 0 : base == "prism_fore" ? 1 : 2, cur = Cur();
      const Pt t = role == 0 ? BackOf(cur) : role == 1 ? ForeOf(cur) : n1_;
      if (t.control) { if (!w_.PlaceBacksightAt(1, t.cp)) { w_.Say("Nie mozna postawic lustra na punkcie " + t.name); return; } prismKind_ = 2; }
      else { if (prismKind_ == 2) w_.RemoveBacksight(1); if (!w_.PlacePrismOnPoint(t.marker)) { w_.Say("Nie mozna postawic lustra na punkcie " + t.name); return; } prismKind_ = 1; }
      prismUp_ = true; prismRole_ = role; lastMsg_.clear();
    } else if (base == "prismh") {
      prismIdx_ = (prismIdx_ + 1) % kPrismCount; w_.SetPrismHeightM(kPrism[prismIdx_]);
    } else if (base == "redo") {
      const int cur = Cur(), S = SCount(); int target = -1;
      if (!arg.empty()) { const int k = std::atoi(arg.c_str()); if (k >= 0 && k < S && StnHasObs(k)) target = k; }
      else if (cur < S && StnHasObs(cur)) target = cur; else for (int i = (std::min)(cur, S) - 1; i >= 0; i--) if (StnHasObs(i)) { target = i; break; }
      if (target >= 0) { stn_[target] = Stn{}; w_.Say("Stanowisko " + Seq()[target].name + " do ponownego pomiaru"); }
      instUp_ = prismUp_ = false; prismRole_ = -1; lastMsg_.clear(); previewed_ = false;
    } else if (base == "calc") {
      PolygonReport r = BuildReport(true);
      const std::vector<std::string> lines = PolygonSummaryLines(r);
      for (size_t i = 0; i < lines.size() && i < 3; i++) w_.Say(lines[i]);
      previewed_ = true;
    }
    // "view": kontroler otwiera lunete; "finish": FinishPolygon(true)
  }

  std::string TachyFire(double camAzDeg, double camPitchDeg) override {
    if (!instUp_ || !prismUp_ || prismRole_ < 0) return lastMsg_ = "Najpierw tachimetr na stanowisku i lustro (menu)";
    if (!lib_.Within(kTachyLevelArcmin)) { char m[120]; std::snprintf(m, sizeof m, "Tachimetr niewypoziomowany (pochylenie %.1f'): menu - Poziomuj instrument", lib_.Mag()); return lastMsg_ = m; }
    const int i = Cur(); if (i >= SCount()) return lastMsg_ = "Wszystkie stanowiska zmierzone";
    const GridPos s = w_.StationGrid(), g = prismKind_ == 2 ? w_.PointGrid(1) : w_.PoleGroundGrid();
    const double ph = kPrism[prismIdx_];
    const TachyAim a = TachyAimError(g.n - s.n, g.e - s.e, (g.h + ph) - (s.h + kInstrH), camAzDeg, camPitchDeg);
    if (a.distM < 3.0 || a.distM > 300.0) return lastMsg_ = "Lustro poza zasiegiem (3-300 m)";
    if (!a.ok) return lastMsg_ = "Nie celujesz w lustro";
    if (!w_.LineClear(prismKind_ == 1, 1)) return lastMsg_ = "Linia celowa zaslonieta";
    Obs& ob = stn_[i].o[prismRole_][face_ - 1];
    if (ob.has) return lastMsg_ = "To lustro w tym polozeniu lunety juz zmierzone (przerzut: " LV_KEYNAME_FLIP " albo inne lustro)";
    const double hd = std::hypot(g.n - s.n, g.e - s.e), trueAz = std::atan2(g.e - s.e, g.n - s.n) * 180.0 / TACHY_PI, truePitch = std::atan2((g.h + ph) - (s.h + kInstrH), hd) * 180.0 / TACHY_PI;
    // celowanie w tolerancji jest "trafione": kierunek bierzemy prawdziwy, a bledy daje tylko instrument (inaczej bledy celowania > limity z dokladnosci instrumentu)
    const TachyReading r = TachyMeasureFace(trueAz, truePitch, az0Deg_, a.distM, noise_, rng_, face_, collGon_, indexGon_);
    ob.has = true; ob.hz = r.hzGon; ob.v = r.vGon; ob.slope = r.slopeM;
    char b[200];
    if (StnDone(i)) {
      double twoC = 0; const double beta = StnBeta(i, &twoC);
      std::snprintf(b, sizeof b, "Stanowisko %s gotowe: kat %.4f g (KP-KL %+.4f g). Przenies tachimetr na nastepne", Seq()[i].name.c_str(), beta, twoC);
      instUp_ = prismUp_ = false; prismRole_ = -1; face_ = 1;
    } else {
      int have = 0, need = 0; for (int rl = 0; rl < 3; rl++) if (Need(i, rl)) { need += 2; for (int f = 0; f < 2; f++) have += stn_[i].o[rl][f].has ? 1 : 0; }
      std::snprintf(b, sizeof b, "Stanowisko %s: %s %s zmierzone (%d z %d)", Seq()[i].name.c_str(), prismRole_ == 0 ? "tyl" : prismRole_ == 1 ? "przod" : "N1", face_ == 1 ? "KL" : "KP", have, need);
    }
    return lastMsg_ = b;
  }

  std::string TachyFlip() override { face_ = 3 - face_; return lastMsg_ = face_ == 2 ? "Polozenie II (KP): wyceluj w lustro i Enter" : "Polozenie I (KL)"; }

  std::vector<std::string> TachyPanel(double camAzDeg, double camPitchDeg) const override {
    const double az = std::isfinite(camAzDeg) ? camAzDeg : 0.0, pitch = std::isfinite(camPitchDeg) ? camPitchDeg : 0.0;
    double hz = NormGon(DegToGon(az - az0Deg_)), v = DegToGon((std::min)(180.0, (std::max)(0.0, 90.0 - pitch)));
    if (face_ == 2) { hz = NormGon(hz + 200.0); v = 400.0 - v; }
    char b[150]; std::vector<std::string> out; const int cur = Cur(), S = SCount();
    std::snprintf(b, sizeof b, "Hz: %.4f g   V: %.4f g   %s", hz, v, face_ == 2 ? "KP" : "KL"); out.push_back(b);
    if (Ready() && cur < S) {
      const char* role = prismRole_ == 0 ? "tyl" : prismRole_ == 1 ? "przod" : prismRole_ == 2 ? "N1" : "brak";
      std::snprintf(b, sizeof b, "Stanowisko %s (%d z %d)   lustro: %s", Seq()[cur].name.c_str(), cur + 1, S, role); out.push_back(b);
    }
    if (lib_.enabled) { std::snprintf(b, sizeof b, lib_.Within(kTachyLevelArcmin) ? "Instrument: poziomo (%.1f')" : "Instrument: NIEWYPOZIOMOWANY (%.1f') - menu: Poziomuj", lib_.Mag()); out.push_back(b); }
    out.push_back(lastMsg_.empty() ? "Enter - zmierz lustro, " LV_KEYNAME_FLIP " - przerzut lunety (KL/KP)" : lastMsg_);
    return out;
  }

  std::string Hud() const override {
    if (!Ready()) return std::string("Wybierz nawiazania i zaloz punkty ciagu (") + LV_KEYNAME_MENU + " - menu)";
    if (AllDone()) return std::string("Wszystkie stanowiska zmierzone: podglad rachunku albo zakoncz (") + LV_KEYNAME_MENU + " - menu)";
    const std::string n = Seq()[Cur()].name;
    if (!instUp_) return "Postaw tachimetr na punkcie " + n + " (" LV_KEYNAME_MENU " - menu)";
    if (!prismUp_) return "Postaw lustro z tylu albo z przodu (" LV_KEYNAME_MENU " - menu)";
    return std::string("Zmierz w lunecie (") + LV_KEYNAME_SCOPE + ")";
  }

  bool CanFinish() const override { return AllDone(); }

  std::string NextStep() const override {
    if (!hasN1_) return "ref_n1";
    if (!hasA_) return "ref_a";
    if (kind_ == TraverseKind::Anchored) { if (!hasB_) return "ref_b"; if (!hasN2_) return "ref_n2"; }
    if (mid_.empty() || !Ready()) return "addpt";
    if (AllDone()) return kind_ != TraverseKind::Open && !previewed_ ? "calc" : "";       // po podgladzie (albo bez kontroli) zostaje zakonczenie
    const int cur = Cur();
    if (!instUp_) return "inst";
    auto missing = [&](int role) { return Need(cur, role) && !stn_[cur].o[role][face_ - 1].has; };
    if (prismUp_ && prismRole_ >= 0 && missing(prismRole_)) return "view";
    for (int role = 0; role < 3; role++) if (missing(role)) return role == 0 ? "prism_back" : role == 1 ? "prism_fore" : "prism_n1";
    return prismUp_ ? "view" : "prism_back";                                              // komplet w tym polozeniu: przerzut lunety (K) w widoku lunety
  }

  std::vector<std::string> SummaryLines() const override { return PolygonSummaryLines(BuildReport(true)); }

  PolygonReport FinishPolygon(bool complete) override { return BuildReport(complete); }

  bool CanSaveState() const override { return true; }
  void SaveState(StateKV& s) const override {
    s.Int("kind", (int)kind_); s.Bool("hasN1", hasN1_); s.Bool("hasA", hasA_); s.Bool("hasB", hasB_); s.Bool("hasN2", hasN2_);
    PutPt(s, "n1", n1_); PutPt(s, "a", a_); PutPt(s, "b", b_); PutPt(s, "n2", n2_);
    s.Int("mid.n", (int)mid_.size()); for (size_t i = 0; i < mid_.size(); i++) PutPt(s, "mid." + std::to_string(i), mid_[i]);
    for (size_t i = 0; i < stn_.size(); i++) for (int r = 0; r < 3; r++) for (int f = 0; f < 2; f++) {
      const Obs& o = stn_[i].o[r][f]; const std::string k = "o." + std::to_string(i) + "." + std::to_string(r) + "." + std::to_string(f);
      s.Bool(k + ".has", o.has); if (o.has) { s.Num(k + ".hz", o.hz); s.Num(k + ".v", o.v); s.Num(k + ".d", o.slope); }
    }
    s.Bool("instUp", instUp_); s.Bool("prismUp", prismUp_); s.Int("role", prismRole_); s.Int("pkind", prismKind_); s.Int("face", face_); s.Int("prismIdx", prismIdx_);
    s.Num("coll", collGon_); s.Num("index", indexGon_); s.Num("az0", az0Deg_); s.Bool("previewed", previewed_); s.Num("lib.x", lib_.x); s.Num("lib.y", lib_.y);
  }
  bool LoadState(const StateKV& s) override {
    if (s.GetInt("kind", -1) != (int)kind_) return false;
    hasN1_ = s.GetBool("hasN1"); hasA_ = s.GetBool("hasA"); hasB_ = s.GetBool("hasB"); hasN2_ = s.GetBool("hasN2");
    n1_ = GetPt(s, "n1"); a_ = GetPt(s, "a"); b_ = GetPt(s, "b"); n2_ = GetPt(s, "n2");
    mid_.clear(); const int nm = (std::min)(s.GetInt("mid.n"), (int)kMaxMid); for (int i = 0; i < nm; i++) mid_.push_back(GetPt(s, "mid." + std::to_string(i)));
    stn_.assign((size_t)SCount(), Stn{});
    for (size_t i = 0; i < stn_.size(); i++) for (int r = 0; r < 3; r++) for (int f = 0; f < 2; f++) {
      const std::string k = "o." + std::to_string(i) + "." + std::to_string(r) + "." + std::to_string(f); Obs& o = stn_[i].o[r][f];
      o.has = s.GetBool(k + ".has"); if (o.has) { o.hz = s.GetNum(k + ".hz"); o.v = s.GetNum(k + ".v"); o.slope = s.GetNum(k + ".d"); }
    }
    instUp_ = s.GetBool("instUp"); prismUp_ = s.GetBool("prismUp"); prismRole_ = (std::min)(2, (std::max)(-1, s.GetInt("role", -1))); prismKind_ = s.GetInt("pkind"); face_ = s.GetInt("face", 1) == 2 ? 2 : 1;
    prismIdx_ = ((s.GetInt("prismIdx") % kPrismCount) + kPrismCount) % kPrismCount; collGon_ = s.GetNum("coll"); indexGon_ = s.GetNum("index"); az0Deg_ = s.GetNum("az0"); previewed_ = s.GetBool("previewed");
    lib_.x = s.GetNum("lib.x"); lib_.y = s.GetNum("lib.y"); lastMsg_.clear();
    w_.SetPrismHeightM(kPrism[prismIdx_]);
    return true;
  }

  // Interfejs niwelacji: ta robota go nie uzywa.
  std::vector<int> ActiveRodPoints() const override { return {}; }
  int RodPointFor(RodRole) const override { return -1; }
  bool OnReading(int, int) override { return false; }
  FieldBook Finish(bool) override { return FieldBook{}; }

 private:
  static constexpr int kPrismCount = 4;
  static constexpr double kPrism[kPrismCount] = {1.5, 1.8, 2.0, 1.3};
  static constexpr double kInstrH = 1.5;
  static constexpr size_t kMaxMid = 30;

  struct Pt { std::string name; bool control = false; ControlPoint cp; int marker = 0; GridPos truth{0, 0, 0}; double n = 0, e = 0; };
  struct Obs { bool has = false; double hz = 0, v = 0, slope = 0; };
  struct Stn { Obs o[3][2]; };                                          // [rola: 0 tyl, 1 przod, 2 N1 (orientacja)][polozenie: 0 KL, 1 KP]

  static std::string Fmt(double v) { char b[40]; std::snprintf(b, sizeof b, "%.4f", v); return b; }
  int ObsStationCount() const { int c = 0; for (size_t i = 0; i < stn_.size(); i++) if (StnHasObs((int)i)) c++; return c; }
  static void PutPt(StateKV& s, const std::string& k, const Pt& p) {
    s.Str(k + ".name", p.name); s.Bool(k + ".ctl", p.control); s.Int(k + ".mk", p.marker); s.Num(k + ".n", p.n); s.Num(k + ".e", p.e);
    s.Num(k + ".tn", p.truth.n); s.Num(k + ".te", p.truth.e); s.Num(k + ".th", p.truth.h); PutControl(s, k + ".cp", p.cp);
  }
  static Pt GetPt(const StateKV& s, const std::string& k) {
    Pt p; p.name = s.GetStr(k + ".name"); p.control = s.GetBool(k + ".ctl"); p.marker = s.GetInt(k + ".mk"); p.n = s.GetNum(k + ".n"); p.e = s.GetNum(k + ".e");
    p.truth = GridPos{s.GetNum(k + ".tn"), s.GetNum(k + ".te"), s.GetNum(k + ".th")}; p.cp = GetControl(s, k + ".cp");
    return p;
  }
  bool UsedRef(const std::string& cpId, const std::string& menuId) const {
    if (menuId != "ref_n1" && hasN1_ && n1_.name == cpId) return true;
    if (menuId != "ref_a" && hasA_ && a_.name == cpId) return true;
    if (menuId != "ref_b" && hasB_ && b_.name == cpId) return true;
    if (menuId != "ref_n2" && hasN2_ && n2_.name == cpId) return true;
    return false;
  }
  void SetRef(const std::string& which, const std::string& cpId) {
    for (const ControlPoint& c : w_.ControlNearHero(1e9)) {
      if (c.id != cpId) continue;
      if (UsedRef(cpId, which)) { w_.Say("Ten punkt osnowy jest juz uzyty w innej roli"); return; }
      Pt p; p.name = c.id; p.control = true; p.cp = c; p.n = c.n; p.e = c.e;
      if (which == "ref_n1") { n1_ = p; hasN1_ = true; } else if (which == "ref_a") { a_ = p; hasA_ = true; }
      else if (which == "ref_b") { b_ = p; hasB_ = true; } else { n2_ = p; hasN2_ = true; }
      Sync(); return;
    }
    w_.Say("Brak takiego punktu osnowy");
  }

  bool Ready() const {
    if (!hasN1_ || !hasA_) return false;
    if (kind_ == TraverseKind::Anchored) return hasB_ && hasN2_;
    return mid_.size() >= (kind_ == TraverseKind::Closed ? 2u : 1u);
  }
  int SCount() const {                                                   // liczba stanowisk (katow)
    if (!Ready()) return 0;
    return kind_ == TraverseKind::Anchored ? (int)mid_.size() + 2 : kind_ == TraverseKind::Closed ? (int)mid_.size() + 1 : (int)mid_.size();
  }
  void Sync() { stn_.resize((size_t)SCount()); if (!Ready()) stn_.clear(); }
  std::vector<Pt> Seq() const { std::vector<Pt> s; s.push_back(a_); for (const Pt& p : mid_) s.push_back(p); if (kind_ == TraverseKind::Anchored) s.push_back(b_); return s; }
  Pt BackOf(int i) const { const std::vector<Pt> s = Seq(); const int n = (int)s.size(); return kind_ == TraverseKind::Closed ? s[(i + n - 1) % n] : (i == 0 ? n1_ : s[i - 1]); }
  Pt ForeOf(int i) const { const std::vector<Pt> s = Seq(); const int n = (int)s.size(); return kind_ == TraverseKind::Anchored && i == n - 1 ? n2_ : s[(i + 1) % n]; }

  bool Need(int i, int role) const { return role < 2 || (kind_ == TraverseKind::Closed && i == 0); }
  bool StnDone(int i) const { for (int r = 0; r < 3; r++) if (Need(i, r)) for (int f = 0; f < 2; f++) if (!stn_[i].o[r][f].has) return false; return true; }
  bool StnHasObs(int i) const { for (int r = 0; r < 3; r++) for (int f = 0; f < 2; f++) if (stn_[i].o[r][f].has) return true; return false; }
  bool AnyObs() const { for (size_t i = 0; i < stn_.size(); i++) if (StnHasObs((int)i)) return true; return false; }
  int Cur() const { const int S = SCount(); for (int i = 0; i < S; i++) if (!StnDone(i)) return i; return S; }
  bool AllDone() const { const int S = SCount(); return S > 0 && Cur() == S; }

  double StnBeta(int i, double* twoC = nullptr) const {
    const Stn& s = stn_[i]; return PolyAngleGon(s.o[0][0].hz, s.o[1][0].hz, s.o[0][1].hz, s.o[1][1].hz, twoC);
  }
  bool Dist(int i, int role, double& d) const {                          // pozioma odleglosc do lustra z obu polozen (V i D usrednione)
    if (i < 0 || i >= SCount() || !stn_[i].o[role][0].has || !stn_[i].o[role][1].has) return false;
    const Obs &k = stn_[i].o[role][0], &p = stn_[i].o[role][1];
    const TachyFaceMean m = TachyCombineFaces({k.hz, k.v, k.slope}, {p.hz, p.v, p.slope});
    d = m.r.slopeM * std::sin(m.r.vGon * TACHY_PI / 200.0); return true;
  }

  PolygonReport BuildReport(bool complete) const {
    PolygonReport r; r.complete = complete; r.kind = kind_; r.showTrueError = opt_.showTrueError;
    if (!Ready()) { r.res.error = "brak nawiazan albo punktow ciagu"; return r; }
    const std::vector<Pt> seq = Seq(); const int S = SCount(), np = (int)seq.size();
    for (const Pt& p : seq) r.names.push_back(p.name);
    if (kind_ == TraverseKind::Closed) r.names.push_back(seq[0].name);
    for (const Pt& p : seq) { r.trueN.push_back(p.control ? p.n : p.truth.n); r.trueE.push_back(p.control ? p.e : p.truth.e); }
    if (kind_ == TraverseKind::Closed) { r.trueN.push_back(seq[0].n); r.trueE.push_back(seq[0].e); }
    for (int i = 0; i < S; i++) {
      if (!StnDone(i)) continue;
      PolyStationRec s; s.name = seq[i].name; s.back = BackOf(i).name; s.fore = ForeOf(i).name;
      const Stn& o = stn_[i]; s.klBack = o.o[0][0].hz; s.klFore = o.o[1][0].hz; s.kpBack = o.o[0][1].hz; s.kpFore = o.o[1][1].hz; s.betaGon = StnBeta(i, &s.twoCGon);
      r.stations.push_back(s);
    }
    const int sides = kind_ == TraverseKind::Anchored ? np - 1 : np;
    for (int i = 0; i < sides; i++) {
      double f = 0, b = 0; PolySideRec sd; sd.from = seq[i].name; sd.to = seq[(i + 1) % np].name;
      const int backStn = kind_ == TraverseKind::Closed ? (i + 1) % np : i + 1;
      sd.hasFwd = i < S && Dist(i, 1, f); sd.hasBack = backStn < S && Dist(backStn, 0, b);
      if (!sd.hasFwd && !sd.hasBack) continue;
      sd.dFwdM = f; sd.dBackM = b; sd.dMeanM = sd.hasFwd && sd.hasBack ? (f + b) / 2.0 : sd.hasFwd ? f : b;
      r.sides.push_back(sd);
    }
    if (AllDone()) {
      PolyInput in; in.kind = kind_; in.nN1 = n1_.n; in.eN1 = n1_.e; in.nA = a_.n; in.eA = a_.e;
      if (kind_ == TraverseKind::Anchored) { in.nB = b_.n; in.eB = b_.e; in.nN2 = n2_.n; in.eN2 = n2_.e; }
      for (int i = 0; i < S; i++) in.betaGon.push_back(StnBeta(i));
      for (const PolySideRec& sd : r.sides) in.sideM.push_back(sd.dMeanM);
      if (kind_ == TraverseKind::Closed) {
        const Stn& o = stn_[0]; in.orientBetaGon = PolyAngleGon(o.o[2][0].hz, o.o[1][0].hz, o.o[2][1].hz, o.o[1][1].hz);
      }
      const TachyNoise nominal; in.sigmaArcsec = nominal.angleArcsec; in.distMm = nominal.distMm; in.distPpm = nominal.distPpm;
      r.in = in; r.res = PolygonCompute(in);
    } else r.res.error = "niekompletny pomiar (nie wszystkie stanowiska)";
    return r;
  }

  IWorld& w_; JobOptions opt_; TraverseKind kind_; GnssRng rng_; TachyNoise noise_; Libella lib_;
  Pt n1_, a_, b_, n2_; bool hasN1_ = false, hasA_ = false, hasB_ = false, hasN2_ = false;
  std::vector<Pt> mid_; std::vector<Stn> stn_;
  bool instUp_ = false, prismUp_ = false; int prismRole_ = -1, prismKind_ = 0, face_ = 1, prismIdx_ = 0;   // prismKind_: 0 brak, 1 tyczka na znaku, 2 lustro nawiazania (slot 1)
  double collGon_ = 0, indexGon_ = 0, az0Deg_ = 0; bool previewed_ = false;
  std::string lastMsg_;
};

inline std::unique_ptr<Job> CreatePolygonJob(IWorld& w, const JobOptions& o) { return std::make_unique<PolygonJob>(w, o); }
