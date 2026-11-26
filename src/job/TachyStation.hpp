// Robota "Tachimetria (pomiar biegunowy)": stanowisko (na punkcie osnowy GNSS albo o znanych wspolrzednych), do 5 nawiazan z lustrami,
// orientacja ze sredniej kierunkow z odchylkami kontrolnymi, pomiar punktow na lustro albo bezlustrowy (bez Gothic API).
// Wspolrzedne punktow licza sie z odczytanych katow i odleglosci; geometria celowania uzywa prawdziwych polozen obiektow.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdio>
#include "GnssRover.hpp"
#include "Job.hpp"
#include "Keys.hpp"

class TachyStationJob : public Job {
 public:
  TachyStationJob(IWorld& w, const JobOptions& o) : w_(w), opt_(o), rng_((uint32_t)(w.Random01() * 4e9) + 1) {
    control_ = !w_.ControlNearHero(1e9).empty();      // domyslnie osnowa GNSS, gdy w swiecie sa jej punkty
    gnss_ = CreateGnssStatic(w_, opt_);                // GNSS w robocie: zakladanie osnowy bez wychodzenia z tachimetrii
    w_.SetPrismHeightM(kPrism[0]);
    lib_.enabled = w_.InstrumentLevelingEnabled();
    if (w_.TwoFaceByDefault()) SetTwoFace(true);
  }
  Libella* GetLibella() override { return &lib_; }

  std::string Name() const override { return "Tachimetria (pomiar biegunowy)"; }
  JobKind Kind() const override { return JobKind::Tachy; }

  std::vector<MenuItem> Menu() const override {
    char ph[64]; std::snprintf(ph, sizeof ph, "Wysokosc lustra: %.3f m (zmien)", kPrism[prismIdx_]);
    const bool fresh = !PointsHere(), canSwitchSrc = !stationUp_ && (control_ || !w_.ControlNearHero(1e9).empty());
    const int n = BackCount();
    std::vector<MenuItem> items = {
      {"src", control_ ? "Zrodlo wspolrzednych: osnowa GNSS (zmien)" : "Zrodlo wspolrzednych: prawdziwe (zmien)", "przed postawieniem tachimetru; osnowa wymaga punktow", canSwitchSrc},
      {"free", free_ ? "Stanowisko: dowolne, wciecie wstecz (zmien)" : "Stanowisko: na znanym punkcie (zmien)", "przed postawieniem tachimetru; wciecie wymaga osnowy i min. 2 nawiazan", !stationUp_ && control_},
      {"newst", "Nowe stanowisko (zamknij biezace)", "najpierw zmierz punkt na biezacym stanowisku", stationUp_ && !fresh},
      {"station", stationUp_ ? "Przestaw tachimetr (stanowisko)" : "Postaw tachimetr (stanowisko)", "nie po zmierzeniu punktu", fresh},
      {"backsight", "Dodaj nawiazanie (lustro)", "najpierw tachimetr; max 5; nie po zmierzeniu punktu; najpierw zmierz poprzednie nawiazanie (stoi tylko jedno lustro)", stationUp_ && fresh && n < kMaxBack && (n == 0 || backs_.back().read)},
      {"unback", "Usun ostatnie nawiazanie", "brak nawiazan; nie po zmierzeniu punktu", n > 0 && fresh},
      {"meas", reflectorless_ ? "Rodzaj pomiaru: bezlustrowy (zmien)" : "Rodzaj pomiaru: na lustro (zmien)", "najpierw tachimetr", stationUp_},
      {"faces", twoFace_ ? "Polozenia lunety: dwa, KL+KP (zmien)" : "Polozenia lunety: jedno (zmien)", "nie po zmierzeniu punktu; dwa = kazdy cel mierzysz w KL i KP, przerzut klawiszem " LV_KEYNAME_FLIP, fresh},
      {"prism", prismUp_ ? "Przestaw tyczke z lustrem" : "Postaw tyczke z lustrem", "najpierw tachimetr; nie w trakcie pomiaru GNSS", stationUp_ && !GnssBusy()},
      {"prismh", ph, "najpierw tachimetr", stationUp_},
      {"fine", "Precyzyjne ustawienie lustra (milimetry)", "najpierw postaw tyczke z lustrem; nie w trakcie pomiaru GNSS", prismUp_ && !GnssBusy()},   // obsluguje kontroler (jak tyczka GNSS)
      {"view", "Patrz przez lunete (" LV_KEYNAME_SCOPE ")", "najpierw tachimetr i co najmniej jedno nawiazanie", stationUp_ && n >= 1},
      {"stake", stakeOn_ ? "Wytyczanie: punkt " + stake_.id + " (zmien)" : "Wytycz punkt osnowy", "najpierw orientacja i tyczka z lustrem; cel 3-300 m od stanowiska", stationUp_ && oriented_},
      {"unstake", "Zakoncz wytyczanie", "trwa wytyczanie", stakeOn_},
      {"finish", "Zakoncz robote", "najpierw zmierz co najmniej jeden punkt", CanFinish()},
    };
    for (const MenuItem& m : gnss_->Menu())            // zakladanie osnowy GNSS: te same kroki co w robocie GNSS (tyczka GNSS zajmuje miejsce tyczki z lustrem)
      if (m.id == "pole" || m.id == "antenna" || m.id == "base" || m.id == "start" || m.id == "control" || m.id == "discard")
        items.push_back({"g_" + m.id, "GNSS: " + m.label, m.hint, m.enabled});
    return items;
  }

  bool NeedsChoice(const std::string& menuId) const override { return menuId == "stake" || (control_ && ((menuId == "station" && !free_) || menuId == "backsight")); }

  std::vector<MenuItem> TachyChoices(const std::string& menuId) const override {
    std::vector<MenuItem> out; char b[120];
    if (menuId == "station") {
      for (const ControlPoint& c : w_.ControlNearHero(kStationRadiusM)) {
        std::snprintf(b, sizeof b, "%s  N %.3f E %.3f H %.3f", c.id.c_str(), c.n, c.e, c.h);
        out.push_back({"station:" + c.id, b, "", true});
      }
    } else if (menuId == "stake") {
      const GridPos s = w_.StationGrid();
      for (const ControlPoint& c : w_.ControlNearStation(3.0, 300.0)) {
        if (stationUp_ && !free_ && c.id == stationCp_.id) continue;
        std::snprintf(b, sizeof b, "%s  N %.3f E %.3f H %.3f  (%.1f m)", c.id.c_str(), c.n, c.e, c.h, std::hypot(c.n - s.n, c.e - s.e));
        out.push_back({"stake:" + c.id, b, "", true});
      }
    } else if (menuId == "backsight") {
      const GridPos s = w_.StationGrid();
      for (const ControlPoint& c : w_.ControlNearStation(3.0, 300.0)) {
        if (stationUp_ && c.id == stationCp_.id) continue;
        if (Used(c.id)) continue;
        const double d = std::hypot(c.n - s.n, c.e - s.e);
        std::snprintf(b, sizeof b, "%s  N %.3f E %.3f  (%.1f m)", c.id.c_str(), c.n, c.e, d);
        out.push_back({"backsight:" + c.id, b, "", true});
      }
    }
    return out;
  }

  void OnMenu(const std::string& id) override {
    const size_t colon = id.find(':');
    const std::string base = colon == std::string::npos ? id : id.substr(0, colon), arg = colon == std::string::npos ? "" : id.substr(colon + 1);
    for (const MenuItem& m : Menu()) if (m.id == base && !m.enabled) return;
    if (base.compare(0, 2, "g_") == 0) {               // odbiornik GNSS: delegacja do robota GNSS
      gnss_->OnMenu(base.substr(2));
      if (base == "g_pole") prismUp_ = false;          // tyczka GNSS zajela miejsce tyczki z lustrem
      return;
    }
    if (base == "src") {
      control_ = !control_;
    } else if (base == "newst") {
      done_.push_back(CurrentRec());
      w_.Say("Stanowisko " + done_.back().label + " zamkniete. Postaw tachimetr na nastepnym");
      stationNo_++; stationStart_ = points_.size(); stationUp_ = false;
      ClearBacksights(); ClearReadings(); solved_ = false;
    } else if (base == "free") {
      free_ = !free_;
    } else if (base == "stake") {
      const ControlPoint* cp = FindIn(w_.ControlNearStation(3.0, 300.0), arg);
      if (!cp) { w_.Say("Brak takiego punktu do wytyczenia"); return; }
      stake_ = *cp; stakeOn_ = true; lastMsg_ = "Wytyczanie " + cp->id + ": ustaw tyczke, celuj i Enter";
    } else if (base == "unstake") {
      stakeOn_ = false;
    } else if (base == "station") {
      if (free_ && control_) {                         // stanowisko dowolne: wspolrzedne dopiero z wciecia wstecz
        if (!w_.PlaceTachymeter()) { w_.Say("Brak gruntu pod stopami"); return; }
        ClearBacksights();
        st_.n = st_.e = st_.h = 0; solved_ = false;
        stationUp_ = true; ClearReadings(); NewSetup();
        return;
      }
      if (control_) {                                  // osnowa: stanowisko tylko z listy
        const ControlPoint* cp = FindIn(w_.ControlNearHero(kStationRadiusM), arg);
        if (!cp) { w_.Say("Brak punktu osnowy w poblizu (promien 2 m)"); return; }
        if (!w_.PlaceTachymeterAt(*cp)) { w_.Say("Nie mozna postawic tachimetru"); return; }
        ClearBacksights();
        stationCp_ = *cp; st_.n = cp->n; st_.e = cp->e; st_.h = cp->h;
      } else {
        if (!w_.PlaceTachymeter()) { w_.Say("Brak gruntu pod stopami"); return; }
        ClearBacksights();
        const GridPos s = w_.StationGrid(); st_.n = Mm(s.n); st_.e = Mm(s.e); st_.h = Mm(s.h);
      }
      stationUp_ = true; ClearReadings(); NewSetup();
    } else if (base == "backsight") {
      const int slot = BackCount() + 1;
      Back b;
      if (control_) {
        const ControlPoint* cp = FindIn(w_.ControlNearStation(3.0, 300.0), arg);
        if (!cp || (stationUp_ && cp->id == stationCp_.id) || Used(cp->id)) { w_.Say("Brak takiego punktu osnowy do nawiazania"); return; }
        if (!w_.PlaceBacksightAt(slot, *cp)) { w_.Say("Nie mozna postawic nawiazania"); return; }
        b.cp = *cp; b.name = cp->id;
      } else {
        if (!w_.PlaceBacksight(slot)) { w_.Say("Brak gruntu pod stopami"); return; }
        const GridPos g = w_.PointGrid(slot);
        b.cp.n = Mm(g.n); b.cp.e = Mm(g.e); b.cp.h = Mm(g.h); b.name = "N" + std::to_string(slot);
      }
      backs_.push_back(b); lastMsg_.clear(); half_.valid = false; face_ = 1;   // zmierzone wczesniej nawiazania zostaja (stoi tylko jedno lustro, mierzy sie je po kolei)
    } else if (base == "unback") {
      w_.RemoveBacksight(BackCount()); backs_.pop_back(); ClearReadings();
    } else if (base == "prism") {
      if (!w_.PlacePrism()) { w_.Say("Brak gruntu pod stopami"); return; }
      prismUp_ = true;
      gnss_ = CreateGnssStatic(w_, opt_);               // tyczka GNSS zniknela: odbiornik zaczyna od nowa
    } else if (base == "prismh") {
      prismIdx_ = (prismIdx_ + 1) % kPrismCount;
      w_.SetPrismHeightM(kPrism[prismIdx_]);          // modele lustra (tyczka i nawiazania) maja od teraz te wysokosc
    } else if (base == "meas") {
      reflectorless_ = !reflectorless_;
    } else if (base == "faces") {
      SetTwoFace(!twoFace_);
      ClearReadings();
    }
    // "view": kontroler otwiera luneta; "finish": kontroler wywoluje FinishTachy(true)
  }

  std::string TachyFire(double camAzDeg, double camPitchDeg) override {
    if (!stationUp_) return "Najpierw postaw tachimetr";
    if (!lib_.Within(kTachyLevelArcmin)) { char m[120]; std::snprintf(m, sizeof m, "Tachimetr niewypoziomowany (pochylenie %.1f'): menu - Poziomuj instrument", lib_.Mag()); return lastMsg_ = m; }
    const std::string bs = TryBacksight(camAzDeg, camPitchDeg);      // celowanie w lustro nawiazania ma pierwszenstwo
    if (!bs.empty()) return bs;
    if (!oriented_) return lastMsg_ = "Najpierw zmierz nawiazanie (celuj w lustro nawiazania)";
    if (reflectorless_) return MeasureReflectorless(camAzDeg, camPitchDeg);
    if (!prismUp_) return "Najpierw postaw tyczke z lustrem";
    return MeasurePrism(camAzDeg, camPitchDeg);
  }

  std::string TachyFlip() override {
    if (!twoFace_) return "Dwa polozenia lunety wlacz w menu (Polozenia lunety)";
    face_ = 3 - face_;
    return face_ == 2 ? "Polozenie II (KP): wyceluj w ten sam cel i Enter" : "Polozenie I (KL)";
  }

  std::vector<std::string> TachyPanel(double camAzDeg, double camPitchDeg) const override {
    const double az = std::isfinite(camAzDeg) ? camAzDeg : 0.0, pitch = std::isfinite(camPitchDeg) ? camPitchDeg : 0.0;
    double hz = NormGon(DegToGon(az - (oriented_ ? az0_ : 0.0)));
    double v = DegToGon((std::min)(180.0, (std::max)(0.0, 90.0 - pitch)));
    if (face_ == 2) { hz = NormGon(hz + 200.0); v = 400.0 - v; }          // KP: kolo poziome +200 g, pionowe 400 - V
    char b[110]; std::vector<std::string> out;
    std::snprintf(b, sizeof b, "Hz: %.4f g   V: %.4f g  %s%s", hz, v, twoFace_ ? (face_ == 2 ? "KP" : "KL") : "", oriented_ ? "" : "  (przed orientacja)"); out.push_back(b);
    std::snprintf(b, sizeof b, "Nawiazania: %d z %d zmierzone   orientacja: %s", ReadCount(), BackCount(), oriented_ ? "tak" : "nie"); out.push_back(b);
    if (reflectorless_) std::snprintf(b, sizeof b, "Pomiar bezlustrowy   Punkty: %d", (int)points_.size());
    else std::snprintf(b, sizeof b, "Pomiar na lustro %.3f m   Punkty: %d", kPrism[prismIdx_], (int)points_.size());
    out.push_back(b);
    if (free_ && stationUp_) { std::snprintf(b, sizeof b, solved_ ? "Stanowisko dowolne: N %.3f E %.3f (rms %.1f mm)" : "Stanowisko dowolne: zmierz min. 2 nawiazania", st_.n, st_.e, freeRms_ * 1000.0); out.push_back(b); }
    if (lib_.enabled) { std::snprintf(b, sizeof b, lib_.Within(kTachyLevelArcmin) ? "Instrument: poziomo (%.1f')" : "Instrument: NIEWYPOZIOMOWANY (%.1f') - menu: Poziomuj", lib_.Mag()); out.push_back(b); }
    out.push_back(lastMsg_.empty() ? (twoFace_ ? "Enter - zmierz w KL, " LV_KEYNAME_FLIP " - przerzut, Enter - w KP" : "Enter - zmierz (lustro nawiazania albo punkt)") : lastMsg_);
    return out;
  }

  std::string Hud() const override {
    if (!stationUp_) return std::string("Postaw tachimetr (") + LV_KEYNAME_MENU + " - menu)";
    if (BackCount() == 0) return "Dodaj nawiazanie z lustrem";
    if (!oriented_) return std::string("Zmierz nawiazania w lunecie (") + LV_KEYNAME_SCOPE + ")";
    if (!reflectorless_ && !prismUp_) return "Postaw tyczke z lustrem albo wybierz pomiar bezlustrowy";
    return std::string("Mierz punkty (") + LV_KEYNAME_SCOPE + " - luneta), potem zakoncz robote";
  }

  bool CanFinish() const override { return !points_.empty(); }
  std::string NextStep() const override {
    if (!stationUp_) return "station";
    if (BackCount() < 1) return "backsight";
    if (!oriented_) return "view";                     // orientacja: pomiar nawiazania w lunecie
    if (!reflectorless_ && !prismUp_) return "prism";
    return "view";
  }
  void Tick() override { gnss_->Tick(); }                                 // epoki odbiornika GNSS plyna w czasie gry
  std::vector<std::string> StatusLines() const override { return gnss_->StatusLines(); }   // panel odbiornika, gdy tyczka GNSS stoi

  TachyReport FinishTachy(bool complete) override {
    TachyReport r; r.complete = complete; r.showTrueError = opt_.showTrueError; r.points = points_;
    r.stationN = st_.n; r.stationE = st_.e; r.stationH = st_.h; r.instrH = st_.instrH;
    if (!done_.empty()) {                                  // wiele stanowisk: zamkniete + biezace (jesli cos na nim zrobiono)
      r.stations = done_;
      if (stationUp_ && (ReadCount() > 0 || PointsHere())) r.stations.push_back(CurrentRec());
      if (r.stations.size() > 1) {
        const TachyStationRec& last = r.stations.back();
        r.stationN = last.n; r.stationE = last.e; r.stationH = last.h; r.instrH = last.instrH; r.azBackGon = last.azBackGon;
        for (TachyPoint& pt : r.points) if (pt.note.find("stanowisko S") == std::string::npos) pt.note += (pt.note.empty() ? "" : "; ") + std::string("stanowisko S") + std::to_string(pt.stationNo);
        r.coordSource = last.coordSource; r.stationName = last.stationName; r.backsights = last.backsights;
        return r;
      }
      r.stations.clear();
    }
    r.coordSource = control_ ? (free_ ? "osnowa GNSS, stanowisko dowolne (wciecie wstecz)" : "osnowa GNSS") : "prawdziwe"; r.stationName = control_ ? (free_ ? "dowolne" : stationCp_.id) : "";
    for (const Back& b : backs_) {
      if (!b.read || (free_ && !solved_)) continue;
      r.backsights.push_back(Residuals(b));
    }
    if (!backs_.empty()) r.azBackGon = DegToGon(TachyAzimuthDeg(st_.n, st_.e, backs_[0].cp.n, backs_[0].cp.e));
    return r;
  }

  // Interfejs niwelacji: ta robota go nie uzywa.
  std::vector<int> ActiveRodPoints() const override { return {}; }
  int RodPointFor(RodRole) const override { return -1; }
  bool OnReading(int, int) override { return false; }
  FieldBook Finish(bool) override { return FieldBook{}; }

 public:
  bool CanSaveState() const override { return true; }
  void SaveState(StateKV& s) const override {
    s.Bool("control", control_); s.Bool("free", free_); s.Bool("solved", solved_); s.Bool("stakeOn", stakeOn_); s.Bool("stationUp", stationUp_); s.Bool("prismUp", prismUp_);
    s.Bool("oriented", oriented_); s.Bool("reflectorless", reflectorless_); s.Bool("twoFace", twoFace_); s.Bool("errDrawn", errDrawn_); s.Int("face", face_);
    s.Num("coll", collGon_); s.Num("index", indexGon_); s.Int("prismIdx", prismIdx_); s.Num("az0", az0_); s.Num("freeRms", freeRms_);
    s.Num("st.n", st_.n); s.Num("st.e", st_.e); s.Num("st.h", st_.h); s.Num("st.ih", st_.instrH); s.Num("st.azb", st_.azBackDeg); s.Num("st.az0", st_.az0Deg);
    PutControl(s, "stationCp", stationCp_); PutControl(s, "stake", stake_);
    s.Int("backs.n", (long long)backs_.size());
    for (size_t i = 0; i < backs_.size(); i++) {
      const std::string p = "backs." + std::to_string(i); const Back& b = backs_[i];
      s.Str(p + ".name", b.name); PutControl(s, p + ".cp", b.cp); s.Bool(p + ".read", b.read); s.Num(p + ".az", b.azMeasDeg); s.Num(p + ".d", b.distMeasM); s.Num(p + ".hd", b.hdMeasM); s.Num(p + ".dh", b.dHMeasM);
    }
    PutVec(s, "points", points_, PutTachyPoint); PutVec(s, "done", done_, PutTachyStationRec);
    s.Int("stationNo", stationNo_); s.Int("stationStart", (long long)stationStart_); s.Num("lib.x", lib_.x); s.Num("lib.y", lib_.y);
    { StateKV g; gnss_->SaveState(g); s.Str("gnss", g.ToText()); }                 // zagniezdzony odbiornik GNSS (zakladanie osnowy w robocie)
  }
  bool LoadState(const StateKV& s) override {
    control_ = s.GetBool("control"); free_ = s.GetBool("free"); solved_ = s.GetBool("solved"); stakeOn_ = s.GetBool("stakeOn"); stationUp_ = s.GetBool("stationUp"); prismUp_ = s.GetBool("prismUp");
    oriented_ = s.GetBool("oriented"); reflectorless_ = s.GetBool("reflectorless"); twoFace_ = s.GetBool("twoFace"); errDrawn_ = s.GetBool("errDrawn"); face_ = s.GetInt("face", 1);
    collGon_ = s.GetNum("coll"); indexGon_ = s.GetNum("index"); prismIdx_ = s.GetInt("prismIdx") % kPrismCount; az0_ = s.GetNum("az0"); freeRms_ = s.GetNum("freeRms");
    st_.n = s.GetNum("st.n"); st_.e = s.GetNum("st.e"); st_.h = s.GetNum("st.h"); st_.instrH = s.GetNum("st.ih", 1.5); st_.azBackDeg = s.GetNum("st.azb"); st_.az0Deg = s.GetNum("st.az0");
    stationCp_ = GetControl(s, "stationCp"); stake_ = GetControl(s, "stake");
    backs_.clear(); const int nb = s.GetInt("backs.n");
    for (int i = 0; i < nb; i++) {
      const std::string p = "backs." + std::to_string(i); Back b;
      b.name = s.GetStr(p + ".name"); b.cp = GetControl(s, p + ".cp"); b.read = s.GetBool(p + ".read"); b.azMeasDeg = s.GetNum(p + ".az"); b.distMeasM = s.GetNum(p + ".d"); b.hdMeasM = s.GetNum(p + ".hd"); b.dHMeasM = s.GetNum(p + ".dh");
      backs_.push_back(b);
    }
    points_ = GetVec<TachyPoint>(s, "points", GetTachyPoint); done_ = GetVec<TachyStationRec>(s, "done", GetTachyStationRec);
    stationNo_ = s.GetInt("stationNo", 1); stationStart_ = (size_t)s.GetInt("stationStart"); lib_.x = s.GetNum("lib.x"); lib_.y = s.GetNum("lib.y");
    half_.valid = false; lastMsg_.clear();
    w_.SetPrismHeightM(kPrism[prismIdx_]);
    return gnss_->LoadState(StateKV::FromText(s.GetStr("gnss")));
  }

 private:
  struct Back { std::string name; ControlPoint cp; bool read = false; double azMeasDeg = 0, distMeasM = 0, hdMeasM = 0, dHMeasM = 0; };   // cp.n,e,h = wspolrzedne uzywane do obliczen
  static constexpr int kMaxBack = 5;
  static constexpr int kPrismCount = 4;
  static constexpr double kPrism[kPrismCount] = {1.5, 1.8, 2.0, 1.3};   // wysokosc srodka lustra nad gruntem [m]; tyczki maja model o tej wysokosci
  static constexpr double kMinSightM = 1.5;                             // minimalna odleglosc pomiaru bezlustrowego
  static constexpr double kStationRadiusM = 2.0;                        // znak osnowy dla stanowiska max 2 m od bohatera

  void SetTwoFace(bool on) {
    twoFace_ = on;
    if (on && !errDrawn_) {                            // ukryte bledy instrumentu: kolimacja i indeks, 3-8 mgon ze znakiem; losowane przy pierwszym wlaczeniu
      errDrawn_ = true;
      collGon_ = (rng_.Uniform() < 0.5 ? -1 : 1) * (0.003 + 0.005 * rng_.Uniform());
      indexGon_ = (rng_.Uniform() < 0.5 ? -1 : 1) * (0.003 + 0.005 * rng_.Uniform());
    }
  }
  void NewSetup() { if (!lib_.enabled) return; const double u1 = w_.Random01(), u2 = w_.Random01(); lib_.Reset(u1, u2); }   // nowe stanowisko: instrument krzywo na statywie
  bool PointsHere() const { return points_.size() > stationStart_; }       // czy na biezacym stanowisku zmierzono juz punkt

  TachyStationRec CurrentRec() const {
    TachyStationRec s; s.label = "S" + std::to_string(stationNo_);
    s.n = st_.n; s.e = st_.e; s.h = st_.h; s.instrH = st_.instrH;
    s.coordSource = control_ ? (free_ ? "osnowa GNSS, stanowisko dowolne (wciecie wstecz)" : "osnowa GNSS") : "prawdziwe";
    s.stationName = control_ ? (free_ ? "dowolne" : stationCp_.id) : "";
    for (const Back& b : backs_) if (b.read && !(free_ && !solved_)) s.backsights.push_back(Residuals(b));
    if (!backs_.empty()) s.azBackGon = DegToGon(TachyAzimuthDeg(st_.n, st_.e, backs_[0].cp.n, backs_[0].cp.e));
    return s;
  }

  bool GnssBusy() const { for (const MenuItem& m : gnss_->Menu()) if (m.id == "discard" && m.enabled) return true; return false; }   // trwa pomiar GNSS

  static double Mm(double v) { return std::round(v * 1000.0) / 1000.0; }   // wspolrzedne "znane z mapy" do 1 mm

  int BackCount() const { return (int)backs_.size(); }
  int ReadCount() const { int n = 0; for (const Back& b : backs_) if (b.read) n++; return n; }
  bool Used(const std::string& id) const { for (const Back& b : backs_) if (b.name == id) return true; return false; }
  static const ControlPoint* FindIn(const std::vector<ControlPoint>& v, const std::string& id) {
    static thread_local ControlPoint hold; for (const ControlPoint& c : v) if (c.id == id) { hold = c; return &hold; } return nullptr;
  }
  void ClearBacksights() { for (int i = BackCount(); i >= 1; i--) w_.RemoveBacksight(i); backs_.clear(); }
  void ClearReadings() { for (Back& b : backs_) b.read = false; oriented_ = false; lastMsg_.clear(); half_.valid = false; face_ = 1; solved_ = false; stakeOn_ = false; }

  // Odczyt: w trybie jednego polozenia od razu; w trybie dwoch KL zapisuje polowe (false), KP zwraca srednia (true) i wraca do KL.
  bool Acquire(const std::string& tag, double camAz, double camPitch, double az0, double slopeM, const TachyNoise& nz, TachyReading& out) {
    if (!twoFace_) { out = TachyMeasure(camAz, camPitch, az0, slopeM, nz, rng_); return true; }
    const TachyReading r = TachyMeasureFace(camAz, camPitch, az0, slopeM, nz, rng_, face_, collGon_, indexGon_);
    char b[150];
    if (face_ == 1) {
      half_.valid = true; half_.tag = tag; half_.r = r; half_.slopeM = slopeM;
      std::snprintf(b, sizeof b, "KL zapisane (Hz %.4f V %.4f). Przerzuc luneta (%s), wyceluj i zmierz w KP", r.hzGon, r.vGon, LV_KEYNAME_FLIP);
      lastMsg_ = b; return false;
    }
    if (!half_.valid || half_.tag != tag || std::fabs(half_.slopeM - slopeM) > 0.1) { lastMsg_ = "KP: najpierw zmierz ten sam cel w KL"; return false; }
    const TachyFaceMean m = TachyCombineFaces(half_.r, r);
    half_.valid = false; face_ = 1; out = m.r;
    std::snprintf(b, sizeof b, "KL+KP: c=%.1f i=%.1f mgon", -m.twoC * 500.0, m.index * 1000.0);   // c = -(Hz_KP - 200 - Hz_KL)/2, w mgon
    faceNote_ = b; return true;
  }

  double AzTo(const Back& b) const { return TachyAzimuthDeg(st_.n, st_.e, b.cp.n, b.cp.e); }

  // Orientacja = srednia kolowa z (azymut kamery - azymut ze wspolrzednych) po odczytanych nawiazaniach.
  void Reorient() {
    if (free_) { SolveFree(); return; }
    std::vector<double> d;
    for (const Back& b : backs_) if (b.read) d.push_back(NormDeg(b.azMeasDeg - AzTo(b)));
    oriented_ = !d.empty(); az0_ = TachyCircularMeanDeg(d); st_.az0Deg = az0_;
  }

  // Wciecie wstecz z odczytanych nawiazan: wspolrzedne stanowiska, orientacja kola i wysokosc stanowiska.
  void SolveFree() {
    std::vector<TachyResObs> o; double hs = 0; int k = 0;
    for (const Back& b : backs_) if (b.read) {
      o.push_back({b.cp.n, b.cp.e, b.azMeasDeg, b.hdMeasM});
      hs += b.cp.h + kPrism[prismIdx_] - b.dHMeasM - st_.instrH; k++;
    }
    const TachyFreeStation f = TachyResection(o);
    solved_ = f.ok; oriented_ = f.ok;
    if (!f.ok) return;
    st_.n = f.n; st_.e = f.e; st_.h = hs / k; az0_ = NormDeg(-f.rotDeg); st_.az0Deg = az0_; freeRms_ = f.rmsM;
  }

  TachyBacksight Residuals(const Back& b) const {
    TachyBacksight r; r.name = b.name; r.azGon = DegToGon(AzTo(b));
    r.hzGon = NormGon(DegToGon(b.azMeasDeg - az0_));
    r.dirResidMgon = DegToGon(AngleDiffDeg(b.azMeasDeg - AzTo(b), az0_)) * 1000.0;
    const double ph = kPrism[prismIdx_], hd = std::hypot(b.cp.n - st_.n, b.cp.e - st_.e);
    r.distCoordM = std::hypot(hd, (b.cp.h + ph) - (st_.h + st_.instrH)); r.distMeasuredM = b.distMeasM;
    r.distResidMm = (r.distMeasuredM - r.distCoordM) * 1000.0;
    r.uncertain = std::fabs(r.dirResidMgon) > 20.0 || std::fabs(r.distResidMm) > 20.0 + 0.005 * r.distCoordM;   // 20 mgon; 20 mm + 5 ppm
    return r;
  }

  // Zwraca komunikat, jesli celownik trafil w lustro ktoregos nawiazania (albo zasloniete); "" gdy nie trafil w zadne.
  std::string TryBacksight(double camAz, double camPitch) {
    const GridPos s = w_.StationGrid();
    for (int i = (std::max)(0, BackCount() - 1); i < BackCount(); i++) {       // stoi (widoczne jest) tylko ostatnio postawione lustro
      const GridPos g = w_.PointGrid(i + 1);
      const TachyAim a = TachyAimError(g.n - s.n, g.e - s.e, (g.h + kPrism[prismIdx_]) - (s.h + st_.instrH), camAz, camPitch);
      if (!a.ok) continue;
      if (PointsHere()) return lastMsg_ = "Orientacja zablokowana po pierwszym punkcie (nowe stanowisko: menu)";   // inaczej az0 zmieniloby sie w trakcie serii
      if (!w_.LineClear(false, i + 1)) return lastMsg_ = "Linia celowa zaslonieta";
      Back& b = backs_[i];
      if (twoFace_ || free_) {
        TachyReading rd;
        if (!Acquire("B" + std::to_string(i), camAz, camPitch, 0.0, a.distM, noise_, rd)) return lastMsg_;
        b.azMeasDeg = GonToDeg(rd.hzGon); b.distMeasM = rd.slopeM;
        const double vr = GonToDeg(rd.vGon) * TACHY_PI / 180.0;
        b.hdMeasM = rd.slopeM * std::sin(vr); b.dHMeasM = rd.slopeM * std::cos(vr);
      } else {
        b.azMeasDeg = TachyOrient(camAz, noise_, rng_);
        b.distMeasM = TachyMeasure(camAz, camPitch, 0.0, a.distM, noise_, rng_).slopeM;
      }
      b.read = true; Reorient();
      if (free_ && !solved_) { faceNote_.clear(); return lastMsg_ = "Nawiazanie " + b.name + " zmierzone; wciecie wymaga min. 2 nawiazan"; }
      const TachyBacksight r = Residuals(b);
      char m[200]; std::snprintf(m, sizeof m, "Nawiazanie %s: kierunek %.1f mgon, odleglosc %.1f mm (%d z %d)%s%s%s", b.name.c_str(), r.dirResidMgon, r.distResidMm,
                                 ReadCount(), BackCount(), r.uncertain ? " - niepewne" : "", faceNote_.empty() ? "" : "; ", faceNote_.c_str());
      faceNote_.clear();
      return lastMsg_ = m;
    }
    return "";
  }

  std::string MeasurePrism(double camAz, double camPitch) {
    const GridPos s = w_.StationGrid(), g = w_.PoleGroundGrid();
    const double ph = kPrism[prismIdx_];
    const TachyAim a = TachyAimError(g.n - s.n, g.e - s.e, (g.h + ph) - (s.h + st_.instrH), camAz, camPitch);
    if (a.distM < 3.0 || a.distM > 300.0) return lastMsg_ = "Lustro poza zasiegiem (3-300 m)";
    if (!a.ok) return lastMsg_ = "Nie celujesz w lustro";
    if (!w_.LineClear(true)) return lastMsg_ = "Linia celowa zaslonieta";
    TachyReading r;
    if (!Acquire("P", camAz, camPitch, az0_, a.distM, noise_, r)) return lastMsg_;
    const TachyResult res = TachyPolar(st_, r, ph);
    const std::string m = Store(r, res, ph, g, stakeOn_ ? ("wytyczanie " + stake_.id) : "");
    return stakeOn_ ? lastMsg_ = m + StakeGuide(res) : m;
  }

  // Pomiar bezlustrowy: odleglosc do pierwszego trafionego punktu (teren, budynek), bez lustra; wiekszy szum odleglosci.
  std::string MeasureReflectorless(double camAz, double camPitch) {
    GridPos hit{0, 0, 0}; double slope = 0;
    if (!w_.TraceSight(camAz, camPitch, hit, slope)) return lastMsg_ = "Brak odbicia (nic nie trafiono w 300 m)";
    if (slope < kMinSightM) return lastMsg_ = "Za blisko (min. 1.5 m)";
    const TachyNoise nz{noise_.angleArcsec, 3.0, 2.0};
    TachyReading r;
    if (!Acquire("R", camAz, camPitch, az0_, slope, nz, r)) return lastMsg_;
    return Store(r, TachyPolar(st_, r, 0.0), 0.0, hit, "bezlustrowy");
  }

  // Podpowiedz wytyczania: o ile i w ktora strone przesunac tyczke (wzgledem kierunku od stanowiska).
  std::string StakeGuide(const TachyResult& res) const {
    const TachyStakeout s = TachyStakeoutCalc(st_.n, st_.e, res.n, res.e, stake_.n, stake_.e);
    char b[160];
    if (s.dist <= 0.02) { std::snprintf(b, sizeof b, " | %s wytyczony (odchylka %.0f mm, dH %+.0f mm)", stake_.id.c_str(), s.dist * 1000.0, (stake_.h - res.h) * 1000.0); return b; }
    std::snprintf(b, sizeof b, " | %s: %s %.3f m, %s %.3f m (razem %.3f m)", stake_.id.c_str(), s.along >= 0 ? "dalej" : "blizej", std::fabs(s.along),
                  s.cross >= 0 ? "w prawo" : "w lewo", std::fabs(s.cross), s.dist);
    return b;
  }

  std::string Store(const TachyReading& r, const TachyResult& res, double prismM, const GridPos& truth, const std::string& note) {
    TachyPoint p; p.name = "T" + std::to_string(points_.size() + 1); p.timeText = w_.TimeText();
    p.hzGon = r.hzGon; p.vGon = r.vGon; p.slopeM = r.slopeM; p.hDistM = res.hDistM;
    p.dHM = r.slopeM * std::cos(GonToDeg(r.vGon) * TACHY_PI / 180.0);
    p.n = res.n; p.e = res.e; p.h = res.h; p.prismM = prismM; p.note = note;
    if (!faceNote_.empty()) { p.note += (p.note.empty() ? "" : "; ") + faceNote_; faceNote_.clear(); }
    p.stationNo = stationNo_;
    if (stationNo_ > 1) p.note += (p.note.empty() ? "" : "; ") + std::string("stanowisko S") + std::to_string(stationNo_);
    if (opt_.showTrueError) {
      p.hasTrue = true; p.dNm = res.n - truth.n; p.dEm = res.e - truth.e; p.dHm = res.h - truth.h; p.d2Dm = std::hypot(p.dNm, p.dEm);
    }
    points_.push_back(p);
    char b[96]; std::snprintf(b, sizeof b, "%s: N %.3f E %.3f H %.3f", p.name.c_str(), p.n, p.e, p.h);
    return lastMsg_ = b;
  }

  IWorld& w_; JobOptions opt_; GnssRng rng_; Libella lib_;
  TachyNoise noise_;
  TachyStation st_{0, 0, 0};
  ControlPoint stationCp_;
  std::vector<Back> backs_;
  struct Half { bool valid = false; std::string tag; TachyReading r{0, 0, 0}; double slopeM = 0; } half_;   // zapisany odczyt KL czekajacy na KP
  int stationNo_ = 1; size_t stationStart_ = 0; std::vector<TachyStationRec> done_;                 // wiele stanowisk: numer biezacego, indeks pierwszego punktu na nim, zamkniete
  bool free_ = false, solved_ = false, stakeOn_ = false; double freeRms_ = 0; ControlPoint stake_;   // stanowisko dowolne (wciecie) i wytyczanie
  bool twoFace_ = false, errDrawn_ = false; int face_ = 1; double collGon_ = 0, indexGon_ = 0; std::string faceNote_;
  bool control_ = false, stationUp_ = false, prismUp_ = false, oriented_ = false, reflectorless_ = false;
  int prismIdx_ = 0; double az0_ = 0;
  std::string lastMsg_;
  std::vector<TachyPoint> points_;
  std::unique_ptr<Job> gnss_;
};

inline std::unique_ptr<Job> CreateTachyStation(IWorld& w, const JobOptions& o) { return std::make_unique<TachyStationJob>(w, o); }
