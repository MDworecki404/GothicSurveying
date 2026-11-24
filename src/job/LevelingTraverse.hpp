// Robota „ciąg niwelacyjny” (dowiązany / zamknięty / otwarty): automat kroków bez Gothic API.
#pragma once
#include <cstdio>
#include <map>
#include "Job.hpp"
#include "Keys.hpp"
#include "calc/LevelMath.hpp"

class LevelingTraverse : public Job {
 public:
  LevelingTraverse(IWorld& w, const JobOptions& o) : w_(w), opt_(o) { lib_.enabled = w.InstrumentLevelingEnabled(); }
  Libella* GetLibella() override { return &lib_; }
  std::string InstrumentBlock() const override { return lib_.Within(kLevelBubbleArcmin) ? "" : "Niwelator niewypoziomowany (menu: Poziomuj instrument)"; }

  std::string Name() const override { return "Ciag niwelacyjny"; }

  std::vector<MenuItem> Menu() const override {
    const bool ended = Ended();
    const char* endLabel = opt_.kind == TraverseKind::Anchored ? "Postaw late wprzod (punkt koncowy - reper B)"
                         : opt_.kind == TraverseKind::Closed   ? "Postaw late wprzod (powrot na reper A)"
                                                               : "Postaw late wprzod (punkt koncowy)";
    return {
      {"reper_a", "Postaw reper A", "stan w miejscu reperu", !hasA_},
      {"instrument", instrument_ ? "Przestaw niwelator (przed odczytami)" : "Postaw niwelator", "najpierw reper A; nie po domknieciu ciagu i nie po odczytach", hasA_ && !ended && !second_ && (!instrument_ || (!backRead_ && !foreRead_))},
      {"rod_back", "Postaw late wstecz", "najpierw niwelator", instrument_ && !backRod_},
      {"rod_fore_turn", "Postaw late wprzod (punkt obrotowy)", "najpierw niwelator i lata wstecz", instrument_ && backRod_ && !foreRod_},
      {"rod_fore_end", endLabel, "najpierw niwelator i lata wstecz", instrument_ && backRod_ && !foreRod_},
      {"move_next", "Przeloz na nastepne stanowisko", "po obu odczytach, na punkcie obrotowym", backRead_ && foreRead_ && !foreIsEnd_},
      {"obsadzka", "Obsadzka: zmien wysokosc niwelatora o 2-7 mm", "po odczytach wstecz i wprzod (drugi komplet)", instrument_ && backRead_ && foreRead_ && !second_},
      {"redo_readings", "Powtorz odczyty na tym stanowisku", "najpierw jakis odczyt", instrument_ && (backRead_ || foreRead_)},
      {"finish", "Zakoncz robote", "po odczycie na punkcie koncowym", CanFinish()},
    };
  }

  void OnMenu(const std::string& id) override {
    for (const MenuItem& m : Menu()) if (m.id == id && !m.enabled) return;     // niedozwolone w tym kroku
    if (id == "reper_a") {
      if (!w_.PlaceMarker(1, MarkerKind::Reper)) { w_.Say("Brak gruntu pod stopami"); return; }
      names_[1] = "A"; hasA_ = true; curBack_ = 1; nextId_ = 2;
      char b[80]; std::snprintf(b, sizeof b, "Reper A: H = %.3f m", w_.PointHeightM(1)); w_.Say(b);
    } else if (id == "instrument") {
      if (!w_.PlaceInstrument()) { w_.Say("Brak gruntu pod stopami"); return; }
      instrument_ = true; if (lib_.enabled) { const double u1 = w_.Random01(), u2 = w_.Random01(); lib_.Reset(u1, u2); }   // nowe stanowisko: instrument krzywo na statywie
    } else if (id == "obsadzka") {
      first_ = Current();                                  // komplet I zostaje; po przestawieniu czytamy obie łaty jeszcze raz
      const double sign = w_.Random01(), size = w_.Random01();      // osobno: kolejność argumentów jest nieokreślona
      w_.ShiftInstrumentMm(ObsadzkaDeltaMm(sign, size));
      second_ = true; backRead_ = foreRead_ = false;
      w_.Say("Obsadzka wykonana (niwelator przestawiony o 2-7 mm). Odczytaj obie laty ponownie");
    } else if (id == "redo_readings") {
      backRead_ = foreRead_ = false; if (!second_) backStadia_ = foreStadia_ = -1;
    } else if (id == "rod_back") {
      w_.PlaceRod(RodRole::Back, curBack_); backRod_ = true;
    } else if (id == "rod_fore_turn") {
      const int pid = nextId_;
      if (!w_.PlaceMarker(pid, MarkerKind::Turning)) { w_.Say("Brak gruntu pod stopami"); return; }
      nextId_++; names_[pid] = std::to_string(++turnNo_);
      w_.PlaceRod(RodRole::Fore, pid); forePoint_ = pid; foreRod_ = true; foreIsEnd_ = false;
    } else if (id == "rod_fore_end") {
      if (opt_.kind == TraverseKind::Closed) {
        if (!w_.FeetWithinCm(1, 30.0)) { w_.Say("Stan na reperze A (w promieniu 30 cm)"); return; }
        forePoint_ = 1;
      } else {
        const int pid = nextId_;
        if (!w_.PlaceMarker(pid, opt_.kind == TraverseKind::Anchored ? MarkerKind::Reper : MarkerKind::Turning)) { w_.Say("Brak gruntu pod stopami"); return; }
        nextId_++; names_[pid] = opt_.kind == TraverseKind::Anchored ? "B" : std::to_string(++turnNo_);
        forePoint_ = pid;
      }
      w_.PlaceRod(RodRole::Fore, forePoint_); foreRod_ = true; foreIsEnd_ = true;
    } else if (id == "move_next") {
      stands_.push_back(Current());
      foreNames_.push_back(names_[forePoint_]);
      w_.RemoveInstrument(); w_.RemoveRod(RodRole::Back); w_.PlaceRod(RodRole::Back, forePoint_); w_.RemoveRod(RodRole::Fore);
      curBack_ = forePoint_; instrument_ = false; foreRod_ = false; backRod_ = true; backRead_ = foreRead_ = false; foreIsEnd_ = false; second_ = false;
      backStadia_ = foreStadia_ = -1;
    }
    // "finish": kontroler wywołuje Finish(true)
  }

  std::vector<int> ActiveRodPoints() const override {
    std::vector<int> p;
    if (backRod_) p.push_back(curBack_);
    if (foreRod_ && forePoint_ != curBack_) p.push_back(forePoint_);
    return p;
  }

  int RodPointFor(RodRole role) const override {
    return role == RodRole::Back ? (backRod_ ? curBack_ : -1) : (foreRod_ ? forePoint_ : -1);
  }

  bool OnReading(int pointId, int midMm) override {
    if (!instrument_ || midMm < 0 || midMm > 5000) return false;
    if (backRod_ && !backRead_ && pointId == curBack_) {
      backMm_ = midMm; backSight_ = w_.SightDistCm(pointId); backStadia_ = LevelStadiaMeasured(midMm, backSight_); backRead_ = true; return true;
    }
    if (foreRod_ && !foreRead_ && pointId == forePoint_) {
      foreMm_ = midMm; foreSight_ = w_.SightDistCm(pointId); foreStadia_ = LevelStadiaMeasured(midMm, foreSight_); foreRead_ = true; return true;
    }
    return false;
  }

  std::string Hud() const override {
    char b[160]; const int k = (int)stands_.size() + 1;
    if (!hasA_) return "Postaw reper A (" LV_KEYNAME_MENU " - menu)";
    if (Ended()) return "Ciag domkniety. Zakoncz robote (" LV_KEYNAME_MENU " - menu)";
    const char* todo = !instrument_ ? "postaw niwelator"
                     : !backRod_ ? "postaw late wstecz"
                     : !backRead_ && !foreRod_ ? "odczyt wstecz (" LV_KEYNAME_SCOPE " - luneta) lub postaw late wprzod"
                     : !foreRod_ ? "postaw late wprzod"
                     : !backRead_ || !foreRead_ ? ( second_ ? "II komplet po obsadzce: odczyty (" LV_KEYNAME_SCOPE " - luneta)" : "odczyty (" LV_KEYNAME_SCOPE " - luneta)" )
                     : "przeloz na nastepne stanowisko";
    std::snprintf(b, sizeof b, "Stanowisko %d: %s", k, todo);
    return b;
  }

  bool CanFinish() const override { return Ended(); }
  std::string NextStep() const override {
    if (!hasA_) return "reper_a";
    if (!instrument_) return "instrument";
    if (!backRod_) return "rod_back";
    if (!foreRod_) return "rod_fore_turn";
    if (!backRead_ || !foreRead_) return "scope";
    return foreIsEnd_ ? "" : "move_next";
  }

  FieldBook Finish(bool complete) override {
    std::vector<Stand> stands = stands_; std::vector<std::string> names = foreNames_;
    if (second_ && !(backRead_ && foreRead_)) { stands.push_back(first_); names.push_back(names_.count(forePoint_) ? names_.at(forePoint_) : ""); }   // przerwano w II komplecie: zostaje komplet I
    else if (backRead_ && foreRead_) { stands.push_back(Current()); names.push_back(names_.count(forePoint_) ? names_.at(forePoint_) : ""); }
    const double hStart = hasA_ ? w_.PointHeightM(1) : 0.0;
    const double hEnd = (opt_.kind == TraverseKind::Anchored && foreIsEnd_) ? w_.PointHeightM(forePoint_) : hStart;
    return BuildFieldBook(opt_.kind, opt_.mode, stands, names, hStart, hEnd, complete);
  }

 public:
  bool CanSaveState() const override { return true; }
  void SaveState(StateKV& s) const override {
    s.Bool("hasA", hasA_); s.Bool("instrument", instrument_); s.Bool("backRod", backRod_); s.Bool("foreRod", foreRod_); s.Bool("backRead", backRead_); s.Bool("foreRead", foreRead_);
    s.Bool("foreIsEnd", foreIsEnd_); s.Bool("second", second_); s.Int("curBack", curBack_); s.Int("forePoint", forePoint_); s.Int("nextId", nextId_); s.Int("turnNo", turnNo_);
    s.Int("backMm", backMm_); s.Int("foreMm", foreMm_); s.Num("backSight", backSight_); s.Num("foreSight", foreSight_); s.Num("backStadia", backStadia_); s.Num("foreStadia", foreStadia_);
    s.Int("names.n", (long long)names_.size()); int i = 0;
    for (const auto& kv : names_) { s.Int("names." + std::to_string(i) + ".id", kv.first); s.Str("names." + std::to_string(i) + ".v", kv.second); i++; }
    PutVec(s, "stands", stands_, PutStand);
    s.Int("foreNames.n", (long long)foreNames_.size());
    for (size_t k = 0; k < foreNames_.size(); k++) s.Str("foreNames." + std::to_string(k), foreNames_[k]);
    PutStand(s, "first", first_); s.Num("lib.x", lib_.x); s.Num("lib.y", lib_.y);
  }
  bool LoadState(const StateKV& s) override {
    hasA_ = s.GetBool("hasA"); instrument_ = s.GetBool("instrument"); backRod_ = s.GetBool("backRod"); foreRod_ = s.GetBool("foreRod"); backRead_ = s.GetBool("backRead"); foreRead_ = s.GetBool("foreRead");
    foreIsEnd_ = s.GetBool("foreIsEnd"); second_ = s.GetBool("second"); curBack_ = s.GetInt("curBack"); forePoint_ = s.GetInt("forePoint"); nextId_ = s.GetInt("nextId", 1); turnNo_ = s.GetInt("turnNo");
    backMm_ = s.GetInt("backMm"); foreMm_ = s.GetInt("foreMm"); backSight_ = s.GetNum("backSight"); foreSight_ = s.GetNum("foreSight"); backStadia_ = s.GetNum("backStadia", -1); foreStadia_ = s.GetNum("foreStadia", -1);
    names_.clear(); const int nn = s.GetInt("names.n");
    for (int i = 0; i < nn; i++) names_[s.GetInt("names." + std::to_string(i) + ".id")] = s.GetStr("names." + std::to_string(i) + ".v");
    stands_ = GetVec<Stand>(s, "stands", GetStand);
    foreNames_.clear(); const int fn = s.GetInt("foreNames.n"); for (int k = 0; k < fn; k++) foreNames_.push_back(s.GetStr("foreNames." + std::to_string(k)));
    first_ = GetStand(s, "first"); lib_.x = s.GetNum("lib.x"); lib_.y = s.GetNum("lib.y");
    return true;
  }

 private:
  bool Ended() const { return foreIsEnd_ && backRead_ && foreRead_; }
  Stand Current() const {
    if (!second_) return Stand{backMm_, foreMm_, backSight_, foreSight_, backStadia_, foreStadia_};
    Stand s = first_; s.back2Mm = backMm_; s.fore2Mm = foreMm_; return s;          // komplet I + II
  }

  IWorld& w_; JobOptions opt_; Libella lib_;
  Stand first_{0, 0, 0, 0};                         // komplet I zapamiętany przy obsadzce
  bool second_ = false;                             // trwa (albo skończony) komplet II
  bool hasA_ = false, instrument_ = false, backRod_ = false, foreRod_ = false, backRead_ = false, foreRead_ = false, foreIsEnd_ = false;
  int curBack_ = 0, forePoint_ = 0, nextId_ = 1, turnNo_ = 0, backMm_ = 0, foreMm_ = 0;
  double backSight_ = 0, foreSight_ = 0, backStadia_ = -1, foreStadia_ = -1;
  std::map<int, std::string> names_;
  std::vector<Stand> stands_; std::vector<std::string> foreNames_;
};

inline std::unique_ptr<Job> CreateLevelingTraverse(IWorld& w, const JobOptions& o) { return std::make_unique<LevelingTraverse>(w, o); }
