// Robota "Niwelacja trygonometryczna": przewyzszenia tachimetrem z odczytow V i odleglosci skosnej, kazdy odcinek tam i z powrotem
// (bez Gothic API). Odcinek S->T: tachimetr na S, lustro na T (tam); potem tachimetr na T, lustro na S (powrot). Srednia usuwa wplyw krzywizny/refrakcji
// i bledow wysokosci; suma tam + powrot jest kontrola. Wysokosc reperu A jest znana (wysokosc terenu w grze); kolejne punkty z przewyzszen.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdio>
#include "Job.hpp"
#include "Keys.hpp"

class TrigLevelJob : public Job {
 public:
  TrigLevelJob(IWorld& w, const JobOptions& o) : w_(w), opt_(o), rng_((uint32_t)(w.Random01() * 4e9) + 1) { w_.SetPrismHeightM(kPrism[0]); lib_.enabled = w_.InstrumentLevelingEnabled(); }
  Libella* GetLibella() override { return &lib_; }

  std::string Name() const override { return "Niwelacja trygonometryczna"; }
  JobKind Kind() const override { return JobKind::Tachy; }          // widok lunety i raport jak w tachimetrii

  std::vector<MenuItem> Menu() const override {
    char ph[64]; std::snprintf(ph, sizeof ph, "Wysokosc lustra: %.3f m (zmien)", kPrism[prismIdx_]);
    const std::string instAt = Name_(InstPoint()), prismAt = Name_(PrismPoint());
    return {
      {"reper_a", "Postaw reper A (punkt startowy)", "stan w miejscu reperu", !hasA_},
      {"inst", (instUp_ ? "Przestaw tachimetr na punkcie " : "Postaw tachimetr na punkcie ") + instAt, "najpierw reper A; tachimetr staje nad znakiem punktu", hasA_ && InstPoint() > 0},
      {"prism", T_ == 0 && phase_ == 0 ? "Postaw punkt celu z lustrem" : (prismUp_ ? "Przestaw lustro na punkcie " : "Postaw lustro na punkcie ") + prismAt,
       "najpierw reper A; tam: nowy punkt celu pod stopami, powrot: lustro na punkcie startowym", hasA_},
      {"prismh", ph, "najpierw reper A", hasA_},
      {"view", "Patrz przez lunete (" LV_KEYNAME_SCOPE ")", "najpierw tachimetr i lustro na swoich punktach", instUp_ && prismUp_},
      {"finish", "Zakoncz robote", "najpierw zmierz co najmniej jeden odcinek tam i z powrotem", CanFinish()},
    };
  }

  void OnMenu(const std::string& id) override {
    for (const MenuItem& m : Menu()) if (m.id == id && !m.enabled) return;
    if (id == "reper_a") {
      if (!w_.PlaceMarker(1, MarkerKind::Reper)) { w_.Say("Brak gruntu pod stopami"); return; }
      hasA_ = true; S_ = 1; hS_ = w_.PointHeightM(1); nextId_ = 2;
      char b[80]; std::snprintf(b, sizeof b, "Reper A: H = %.3f m", hS_); w_.Say(b);
    } else if (id == "inst") {
      if (!w_.PlaceTachymeterOnPoint(InstPoint())) { w_.Say("Nie mozna postawic tachimetru na punkcie " + Name_(InstPoint())); return; }
      instUp_ = true; if (lib_.enabled) { const double u1 = w_.Random01(), u2 = w_.Random01(); lib_.Reset(u1, u2); }   // nowe ustawienie: instrument krzywo na statywie
    } else if (id == "prism") {
      if (phase_ == 0 && T_ == 0) {                   // nowy punkt celu: znak i lustro w miejscu stania
        if (!w_.PlaceMarker(nextId_, MarkerKind::Turning)) { w_.Say("Brak gruntu pod stopami"); return; }
        if (!w_.PlacePrismOnPoint(nextId_)) { w_.Say("Nie mozna postawic lustra"); return; }
        T_ = nextId_++;
      } else {
        if (!w_.PlacePrismOnPoint(PrismPoint())) { w_.Say("Nie mozna postawic lustra na punkcie " + Name_(PrismPoint())); return; }
      }
      prismUp_ = true;
    } else if (id == "prismh") {
      prismIdx_ = (prismIdx_ + 1) % kPrismCount; w_.SetPrismHeightM(kPrism[prismIdx_]);
    }
    // "view": kontroler otwiera lunete; "finish": FinishTachy(true)
  }

  std::string TachyFire(double camAzDeg, double camPitchDeg) override {
    if (!instUp_ || !prismUp_) return "Najpierw tachimetr i lustro na swoich punktach";
    if (!lib_.Within(kTachyLevelArcmin)) { char m[120]; std::snprintf(m, sizeof m, "Tachimetr niewypoziomowany (pochylenie %.1f'): menu - Poziomuj instrument", lib_.Mag()); return lastMsg_ = m; }
    const GridPos s = w_.StationGrid(), g = w_.PoleGroundGrid();
    const double ph = kPrism[prismIdx_];
    const TachyAim a = TachyAimError(g.n - s.n, g.e - s.e, (g.h + ph) - (s.h + kInstrH), camAzDeg, camPitchDeg);
    if (a.distM < 3.0 || a.distM > 300.0) return lastMsg_ = "Lustro poza zasiegiem (3-300 m)";
    if (!a.ok) return lastMsg_ = "Nie celujesz w lustro";
    if (!w_.LineClear(true)) return lastMsg_ = "Linia celowa zaslonieta";
    const TachyReading r = TachyMeasure(camAzDeg, camPitchDeg, 0.0, a.distM, noise_, rng_);
    const double dhM = TrigHeightDiffM(r.slopeM, r.vGon, kInstrH, ph), hd = r.slopeM * std::sin(GonToDeg(r.vGon) * TACHY_PI / 180.0);
    TachyPoint p; p.name = Name_(phase_ == 0 ? S_ : T_) + ">" + Name_(phase_ == 0 ? T_ : S_) + (phase_ == 0 ? " tam" : " powrot"); p.timeText = w_.TimeText();
    p.hzGon = r.hzGon; p.vGon = r.vGon; p.slopeM = r.slopeM; p.hDistM = hd; p.dHM = dhM; p.prismM = ph; p.h = phase_ == 0 ? hS_ + dhM : hS_ - dhM;
    p.note = "dh=" + Mm(dhM * 1000.0) + " mm";
    points_.push_back(p);
    char b[160];
    if (phase_ == 0) {
      cur_ = TrigLeg(); cur_.from = Name_(S_); cur_.to = Name_(T_); cur_.distM = hd; cur_.vFwdGon = r.vGon; cur_.dhFwdMm = dhM * 1000.0;
      cur_.hasTrue = opt_.showTrueError; trueDhMm_ = (g.h - s.h) * 1000.0;
      phase_ = 1; instUp_ = prismUp_ = false;
      std::snprintf(b, sizeof b, "Tam %s>%s: dh %+.1f mm. Przenies tachimetr na %s, lustro na %s i zmierz z powrotem", cur_.from.c_str(), cur_.to.c_str(), dhM * 1000.0, cur_.to.c_str(), cur_.from.c_str());
      return lastMsg_ = b;
    }
    cur_.vBackGon = r.vGon; cur_.dhBackMm = dhM * 1000.0; cur_.distM = (cur_.distM + hd) / 2.0;
    cur_.sumMm = cur_.dhFwdMm + cur_.dhBackMm; cur_.meanMm = (cur_.dhFwdMm - cur_.dhBackMm) / 2.0;
    cur_.tolMm = 10.0 + 20.0 * cur_.distM / 1000.0; cur_.ok = std::fabs(cur_.sumMm) <= cur_.tolMm + 1e-9;
    cur_.hFromM = hS_; cur_.hToM = hS_ + cur_.meanMm / 1000.0; cur_.errMm = cur_.meanMm - trueDhMm_;
    legs_.push_back(cur_);
    std::snprintf(b, sizeof b, "Odcinek %s-%s: dh %+.1f mm, suma %+.1f mm (dop. %.1f) %s; H %s = %.3f m", cur_.from.c_str(), cur_.to.c_str(), cur_.meanMm, cur_.sumMm, cur_.tolMm,
                  cur_.ok ? "ok" : "PRZEKROCZONA", cur_.to.c_str(), cur_.hToM);
    hS_ = cur_.hToM; S_ = T_; T_ = 0; phase_ = 0; instUp_ = prismUp_ = false;
    return lastMsg_ = b;
  }

  std::vector<std::string> TachyPanel(double camAzDeg, double camPitchDeg) const override {
    (void)camAzDeg;
    const double pitch = std::isfinite(camPitchDeg) ? camPitchDeg : 0.0;
    const double v = DegToGon((std::min)(180.0, (std::max)(0.0, 90.0 - pitch)));
    char b[120]; std::vector<std::string> out;
    std::snprintf(b, sizeof b, "V: %.4f g   Tachimetr: %s   Lustro: %s", v, Name_(InstPoint()).c_str(), Name_(PrismPoint()).c_str()); out.push_back(b);
    std::snprintf(b, sizeof b, "Odcinek %s>%s, pomiar %s   Odcinkow gotowych: %d", Name_(S_).c_str(), Name_(T_).c_str(), phase_ == 0 ? "tam" : "z powrotem", (int)legs_.size()); out.push_back(b);
    if (lib_.enabled) { std::snprintf(b, sizeof b, lib_.Within(kTachyLevelArcmin) ? "Instrument: poziomo (%.1f')" : "Instrument: NIEWYPOZIOMOWANY (%.1f') - menu: Poziomuj", lib_.Mag()); out.push_back(b); }
    out.push_back(lastMsg_.empty() ? "Enter - zmierz V i odleglosc do lustra" : lastMsg_);
    return out;
  }

  std::string Hud() const override {
    if (!hasA_) return std::string("Postaw reper A (") + LV_KEYNAME_MENU + " - menu)";
    if (!instUp_ || !prismUp_) return std::string(phase_ == 0 ? "Tam: " : "Powrot: ") + "tachimetr na " + Name_(InstPoint()) + ", lustro na " + Name_(PrismPoint()) + " (" + LV_KEYNAME_MENU + " - menu)";
    return std::string("Zmierz w lunecie (") + LV_KEYNAME_SCOPE + ")";
  }

  bool CanFinish() const override { return !legs_.empty() && phase_ == 0; }

  TachyReport FinishTachy(bool complete) override {
    TachyReport r; r.complete = complete; r.showTrueError = opt_.showTrueError; r.trig = true; r.legs = legs_; r.points = points_;
    r.coordSource = "niwelacja trygonometryczna";
    return r;
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

  static std::string Mm(double v) { char b[32]; std::snprintf(b, sizeof b, "%+.1f", v); return b; }
  static std::string Name_(int id) { return id <= 0 ? "?" : id <= 26 ? std::string(1, (char)('A' + id - 1)) : "P" + std::to_string(id); }
  int InstPoint() const { return phase_ == 0 ? S_ : T_; }
  int PrismPoint() const { return phase_ == 0 ? T_ : S_; }

  IWorld& w_; JobOptions opt_; GnssRng rng_; TachyNoise noise_; Libella lib_;
  bool hasA_ = false, instUp_ = false, prismUp_ = false;
  int S_ = 0, T_ = 0, phase_ = 0, nextId_ = 2, prismIdx_ = 0;      // S_ punkt startowy odcinka, T_ cel (0 = jeszcze nie postawiony), phase_ 0 = tam, 1 = powrot
  double hS_ = 0, trueDhMm_ = 0; TrigLeg cur_;
  std::string lastMsg_;
  std::vector<TrigLeg> legs_; std::vector<TachyPoint> points_;
};

inline std::unique_ptr<Job> CreateTrigLevel(IWorld& w, const JobOptions& o) { return std::make_unique<TrigLevelJob>(w, o); }
