// Robota „ciąg niwelacyjny” (dowiązany / zamknięty / otwarty): automat kroków bez Gothic API.
#pragma once
#include <cstdio>
#include <map>
#include "Job.hpp"

class LevelingTraverse : public Job {
 public:
  LevelingTraverse(IWorld& w, const JobOptions& o) : w_(w), opt_(o) {}

  std::string Name() const override { return "Ciag niwelacyjny"; }

  std::vector<MenuItem> Menu() const override {
    const bool ended = Ended();
    const char* endLabel = opt_.kind == TraverseKind::Anchored ? "Postaw late wprzod (punkt koncowy - reper B)"
                         : opt_.kind == TraverseKind::Closed   ? "Postaw late wprzod (powrot na reper A)"
                                                               : "Postaw late wprzod (punkt koncowy)";
    return {
      {"reper_a", "Postaw reper A", "stan w miejscu reperu", !hasA_},
      {"instrument", "Postaw niwelator", "najpierw reper A; nie po domknieciu ciagu", hasA_ && !instrument_ && !ended},
      {"rod_back", "Postaw late wstecz", "najpierw niwelator", instrument_ && !backRod_},
      {"rod_fore_turn", "Postaw late wprzod (punkt obrotowy)", "najpierw niwelator i lata wstecz", instrument_ && backRod_ && !foreRod_},
      {"rod_fore_end", endLabel, "najpierw niwelator i lata wstecz", instrument_ && backRod_ && !foreRod_},
      {"move_next", "Przeloz na nastepne stanowisko", "po obu odczytach, na punkcie obrotowym", backRead_ && foreRead_ && !foreIsEnd_},
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
      instrument_ = true; foreRod_ = false; backRead_ = foreRead_ = false;
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
      curBack_ = forePoint_; instrument_ = false; foreRod_ = false; backRod_ = true; backRead_ = foreRead_ = false; foreIsEnd_ = false;
    }
    // "finish": kontroler wywołuje Finish(true)
  }

  std::vector<int> ActiveRodPoints() const override {
    std::vector<int> p;
    if (backRod_) p.push_back(curBack_);
    if (foreRod_ && forePoint_ != curBack_) p.push_back(forePoint_);
    return p;
  }

  bool OnReading(int pointId, int midMm, int upMm, int loMm) override {
    if (!instrument_ || midMm < 0 || midMm > 3000) return false;
    const bool hasStadia = upMm >= 0 || loMm >= 0;
    if (hasStadia && (upMm < 0 || loMm < 0 || upMm > 3000 || loMm > 3000)) return false;
    if (backRod_ && !backRead_ && pointId == curBack_) {
      backMm_ = midMm; backSight_ = w_.SightDistCm(pointId); backRead_ = true; return true;
    }
    if (foreRod_ && !foreRead_ && pointId == forePoint_) {
      foreMm_ = midMm; foreSight_ = w_.SightDistCm(pointId); foreRead_ = true; return true;
    }
    return false;
  }

  std::string Hud() const override {
    char b[160]; const int k = (int)stands_.size() + 1;
    if (!hasA_) return "Postaw reper A (F12 - menu)";
    if (Ended()) return "Ciag domkniety. Zakoncz robote (F12 - menu)";
    const char* todo = !instrument_ ? "postaw niwelator"
                     : !backRod_ ? "postaw late wstecz"
                     : !backRead_ && !foreRod_ ? "odczyt wstecz (F10 - luneta) lub postaw late wprzod"
                     : !foreRod_ ? "postaw late wprzod"
                     : !backRead_ || !foreRead_ ? "odczyty (F10 - luneta)"
                     : "przeloz na nastepne stanowisko";
    std::snprintf(b, sizeof b, "Stanowisko %d: %s", k, todo);
    return b;
  }

  bool CanFinish() const override { return Ended(); }

  FieldBook Finish(bool complete) override {
    std::vector<Stand> stands = stands_; std::vector<std::string> names = foreNames_;
    if (backRead_ && foreRead_) { stands.push_back(Current()); names.push_back(names_.count(forePoint_) ? names_.at(forePoint_) : ""); }
    const double hStart = hasA_ ? w_.PointHeightM(1) : 0.0;
    const double hEnd = (opt_.kind == TraverseKind::Anchored && foreIsEnd_) ? w_.PointHeightM(forePoint_) : hStart;
    return BuildFieldBook(opt_.kind, opt_.mode, stands, names, hStart, hEnd, complete);
  }

 private:
  bool Ended() const { return foreIsEnd_ && backRead_ && foreRead_; }
  Stand Current() const { return Stand{backMm_, foreMm_, backSight_, foreSight_}; }

  IWorld& w_; JobOptions opt_;
  bool hasA_ = false, instrument_ = false, backRod_ = false, foreRod_ = false, backRead_ = false, foreRead_ = false, foreIsEnd_ = false;
  int curBack_ = 0, forePoint_ = 0, nextId_ = 1, turnNo_ = 0, backMm_ = 0, foreMm_ = 0;
  double backSight_ = 0, foreSight_ = 0;
  std::map<int, std::string> names_;
  std::vector<Stand> stands_; std::vector<std::string> foreNames_;
};

inline std::unique_ptr<Job> CreateLevelingTraverse(IWorld& w, const JobOptions& o) { return std::make_unique<LevelingTraverse>(w, o); }
