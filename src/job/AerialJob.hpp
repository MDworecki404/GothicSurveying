// Robota "Skaning lotniczy z drona (ALS)" (bez Gothic API): plan nalotu z obszaru (wierzcholki pod stopami albo na punktach osnowy), parametry,
// podglad planu na mapie osnowy (MapOverlay) i eksport CSV. Lot, skan i wyniki: dopisywane w kolejnych etapach (patrz Tick/StartFlight).
#pragma once
#include <cstdio>
#include <filesystem>
#include <fstream>
#include "Job.hpp"
#include "calc/Flight.hpp"
#include "calc/Georef.hpp"

class AerialJob : public Job {
 public:
  AerialJob(IWorld& w, const JobOptions& o) : w_(w), opt_(o) { params_.sensor.pulseHz = kAlsPresets[preset_].pulseHz; params_.sensor.lineHz = kAlsPresets[preset_].lineHz; }

  std::string Name() const override { return "Skaning lotniczy z drona"; }
  JobKind Kind() const override { return JobKind::Aerial; }
  bool NeedsChoice(const std::string& menuId) const override { return menuId == "area_ctrl"; }

  std::vector<MenuItem> Menu() const override {
    char agl[96], spd[96], sid[96], az[96], fol[120], prof[120], pre[120];
    std::snprintf(agl, sizeof agl, "Wysokosc nad terenem (AGL): %.0f m (zmien)", params_.aglM);
    std::snprintf(spd, sizeof spd, "Predkosc: %.0f m/s (zmien)", params_.speedMps);
    std::snprintf(sid, sizeof sid, "Zakladka boczna: %.0f%% (zmien)", params_.sidelapPct);
    if (azMode_ == 0) std::snprintf(az, sizeof az, "Kierunek szeregow: wzdluz najdluzszego boku (zmien)"); else std::snprintf(az, sizeof az, "Kierunek szeregow: %.0f st. (zmien)", kAzChoices[azMode_]);
    std::snprintf(fol, sizeof fol, "%s", params_.followTerrain ? "Wysokosc: podaza za terenem (zmien)" : "Wysokosc: stala bezwzgledna (zmien)");
    std::snprintf(prof, sizeof prof, "Profil bledow GNSS/IMU: %s (zmien)", kAlsProfiles[profile_].name);
    std::snprintf(pre, sizeof pre, "Gestosc impulsow: %s (zmien)", kAlsPresets[preset_].name);
    return {
      {"area_here", "Dodaj wierzcholek obszaru (pod stopami)", "max 12 wierzcholkow", !flying_},
      {"area_ctrl", "Dodaj wierzcholek obszaru (punkt osnowy)", "wybierz punkt osnowy z listy", !flying_},
      {"area_clear", "Wyczysc obszar", "brak wierzcholkow", !flying_ && !area_.empty()},
      {"agl", agl, "30 / 50 / 80 / 100 / 150 m", !flying_}, {"speed", spd, "3 / 5 / 6 / 8 m/s", !flying_}, {"sidelap", sid, "20 / 30 / 40 %", !flying_},
      {"azimuth", az, "wzdluz boku albo 0 / 45 / 90 / 135 st.", !flying_}, {"follow", fol, "AGL liczone od terenu albo stala wysokosc", !flying_},
      {"profile", prof, "RTK dobry / PPK / slaby", !flying_}, {"preset", pre, "wiecej impulsow = gestsza chmura, dluzej w grze", !flying_},
      {"plan_csv", "Zapisz plan nalotu (CSV)", "najpierw zdefiniuj obszar", plan_.ok && !flying_},
      {"launch", "Start lotu", "", !flying_},
    };
  }

  std::vector<MenuItem> TachyChoices(const std::string& menuId) const override {
    std::vector<MenuItem> out;
    if (menuId != "area_ctrl") return out;
    for (const ControlPoint& c : w_.ControlNearHero(1e9)) { char b[120]; std::snprintf(b, sizeof b, "%s  N %.3f E %.3f", c.id.c_str(), c.n, c.e); out.push_back({"area_ctrl:" + c.id, b, "", true}); }
    return out;
  }

  void OnMenu(const std::string& id) override {
    const size_t colon = id.find(':');
    const std::string base = colon == std::string::npos ? id : id.substr(0, colon), arg = colon == std::string::npos ? "" : id.substr(colon + 1);
    for (const MenuItem& m : Menu()) if (m.id == base && !m.enabled) return;
    if (base == "area_here") {
      GridPos g; if (!w_.HeroGround(g)) { w_.Say("Brak gruntu pod stopami"); return; }
      AddVertex({g.n, g.e});
    } else if (base == "area_ctrl") {
      for (const ControlPoint& c : w_.ControlNearHero(1e9)) if (c.id == arg) { AddVertex({c.n, c.e}); return; }
      w_.Say("Brak takiego punktu osnowy");
    } else if (base == "area_clear") { area_.clear(); Replan();
    } else if (base == "agl") { Cycle(params_.aglM, kAgl, 5);
    } else if (base == "speed") { Cycle(params_.speedMps, kSpeed, 4);
    } else if (base == "sidelap") { Cycle(params_.sidelapPct, kSidelap, 3);
    } else if (base == "azimuth") { azMode_ = (azMode_ + 1) % 5; Replan();
    } else if (base == "follow") { params_.followTerrain = !params_.followTerrain;
    } else if (base == "profile") { profile_ = (profile_ + 1) % 3;
    } else if (base == "preset") { preset_ = (preset_ + 1) % 4; params_.sensor.pulseHz = kAlsPresets[preset_].pulseHz; params_.sensor.lineHz = kAlsPresets[preset_].lineHz; Replan();
    } else if (base == "plan_csv") { SavePlanCsv();
    } else if (base == "launch") { Launch(); }
  }

  std::string Hud() const override {
    char b[220];
    if (!plan_.ok) { std::snprintf(b, sizeof b, "Nalot ALS: obszar %d wierzcholkow (min. 3)", (int)area_.size()); return b; }
    std::snprintf(b, sizeof b, "Nalot ALS: szeregow: %d, pas %.0f m, odstep %.0f m, lot %.1f min, ok. %.1f pkt/m2%s",
                  (int)plan_.strips.size(), plan_.swathM, plan_.spacingM, plan_.flightTimeS / 60.0, plan_.estDensityPtsM2, plan_.batteryOk ? "" : " (ZA DLUGI)");
    return b;
  }
  bool CanFinish() const override { return flights_ > 0 && !flying_; }

  std::vector<MapLine> MapOverlay() const override {
    std::vector<MapLine> out;
    if (area_.size() >= 2) for (size_t i = 0; i < area_.size(); i++) {
      const size_t j = (i + 1) % area_.size(); if (area_.size() == 2 && i == 1) break;
      out.push_back({area_[i].n, area_[i].e, area_[j].n, area_[j].e, 0});
    }
    if (!plan_.ok) return out;
    PlanPt cur = PlanHome();
    for (const Strip& s : plan_.strips) { out.push_back({cur.n, cur.e, s.a.n, s.a.e, 2}); out.push_back({s.a.n, s.a.e, s.b.n, s.b.e, 1}); cur = s.b; }
    out.push_back({cur.n, cur.e, PlanHome().n, PlanHome().e, 2});
    return out;
  }

  std::vector<int> ActiveRodPoints() const override { return {}; }
  int RodPointFor(RodRole) const override { return -1; }
  bool OnReading(int, int) override { return false; }
  FieldBook Finish(bool) override { return FieldBook{}; }

 private:
  static constexpr int kMaxVertices = 12;
  static constexpr double kAgl[5] = {30, 50, 80, 100, 150}, kSpeed[4] = {3, 5, 6, 8}, kSidelap[3] = {20, 30, 40}, kAzChoices[5] = {0, 0, 45, 90, 135};   // [0] = automatycznie

  void Cycle(double& v, const double* tab, int n) {
    int i = 0; for (int k = 0; k < n; k++) if (std::fabs(tab[k] - v) < 1e-9) i = k;
    v = tab[(i + 1) % n]; Replan();
  }
  void AddVertex(PlanPt p) {
    if ((int)area_.size() >= kMaxVertices) { w_.Say("Osiagnieto limit 12 wierzcholkow obszaru"); return; }
    area_.push_back(p); Replan();
  }
  PlanPt PlanHome() const { return home_set_ ? home_ : (area_.empty() ? PlanPt{0, 0} : area_[0]); }
  double AutoAzimuth() const {                                          // kierunek najdluzszego boku obszaru, 0..180 st.
    double best = -1, az = 0;
    for (size_t i = 0; i < area_.size(); i++) {
      const PlanPt a = area_[i], b = area_[(i + 1) % area_.size()]; const double d = std::hypot(b.n - a.n, b.e - a.e);
      if (d > best + 1e-9) { best = d; az = std::atan2(b.e - a.e, b.n - a.n) * 180.0 / 3.14159265358979323846; if (az < 0) az += 180.0; if (az >= 180.0) az -= 180.0; }
    }
    return az;
  }
  void Replan() {
    params_.area = area_; params_.stripAzDeg = azMode_ == 0 ? AutoAzimuth() : kAzChoices[azMode_];
    plan_ = area_.size() >= 3 ? PlanFlight(params_, PlanHome()) : FlightPlan{};
  }
  void SavePlanCsv() {
    const std::string dir = w_.AerialOutDir(); if (dir.empty()) { w_.Say("Nie mozna utworzyc folderu wynikow"); return; }
    std::ofstream f(dir + "/plan_nalotu.csv", std::ios::binary | std::ios::trunc); f << PlanCsv(plan_, params_);
    w_.Say("Zapisano plan nalotu: plan_nalotu.csv");
  }
  void Launch() {
    if (area_.size() < 3 || !plan_.ok) { w_.Say(plan_.msg.empty() ? "Najpierw zdefiniuj obszar (min. 3 wierzcholki)" : plan_.msg); return; }
    GridPos g; if (!w_.HeroGround(g)) { w_.Say("Brak gruntu pod stopami (start drona)"); return; }
    home_ = {g.n, g.e}; home_set_ = true; Replan();                       // dron startuje spod stop bohatera
    if (!plan_.batteryOk) { w_.Say(plan_.msg); return; }
    // lot, skan i wyniki: nastepny etap
  }

  IWorld& w_; JobOptions opt_;
  FlightParams params_; FlightPlan plan_; std::vector<PlanPt> area_;
  int azMode_ = 0, profile_ = 0, preset_ = 1, flights_ = 0; bool flying_ = false, home_set_ = false; PlanPt home_{0, 0};
};

inline std::unique_ptr<Job> CreateAerialJob(IWorld& w, const JobOptions& o) { return std::make_unique<AerialJob>(w, o); }
