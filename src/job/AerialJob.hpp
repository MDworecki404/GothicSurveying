// Robota "Skaning lotniczy z drona (ALS)" (bez Gothic API): plan nalotu z obszaru (wierzcholki pod stopami albo na punktach osnowy), parametry,
// podglad planu na mapie osnowy (MapOverlay) i eksport CSV. Lot, skan i wyniki: dopisywane w kolejnych etapach (patrz Tick/StartFlight).
#pragma once
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include "Job.hpp"
#include "calc/AlsLas.hpp"
#include "calc/Flight.hpp"
#include "calc/Georef.hpp"

class AerialJob : public Job {
 public:
  AerialJob(IWorld& w, const JobOptions& o) : w_(w), opt_(o), seed_((uint32_t)(w.Random01() * 4e9) + 1), rangeRng_(seed_ ^ 0x5bd1e995u) {
    params_.sensor.pulseHz = kAlsPresets[preset_].pulseHz; params_.sensor.lineHz = kAlsPresets[preset_].lineHz;
  }
  ~AerialJob() override { if (flying_) { raw_.Close(); traj_.close(); w_.RemoveDrone(); } }   // bez konwersji: wyniki powstaja tylko po locie albo przerwaniu

  // Haki testowe: limit impulsow na klatke (0 = budzet czasu), podmiana profilu bledow, wylaczenie szumu odleglosci.
  void TestHooks(int pulseCap, const ErrorProfile* profile, bool rangeNoise) { pulseCap_ = pulseCap; haveOverride_ = profile != nullptr; if (profile) override_ = *profile; rangeNoise_ = rangeNoise; idleStepS_ = pulseCap > 0 ? 1.0 : 0.0; }

  std::string Name() const override { return "Skaning lotniczy z drona"; }
  JobKind Kind() const override { return JobKind::Aerial; }
  bool NeedsChoice(const std::string& menuId) const override { return menuId == "area_ctrl"; }

  std::vector<MenuItem> Menu() const override {
    char agl[96], spd[96], sid[96], az[96], fol[120], prof[120], pre[120], bat[120];
    std::snprintf(agl, sizeof agl, "Wysokosc nad terenem (AGL): %.0f m (zmien)", params_.aglM);
    std::snprintf(spd, sizeof spd, "Predkosc: %.0f m/s (zmien)", params_.speedMps);
    std::snprintf(sid, sizeof sid, "Zakladka boczna: %.0f%% (zmien)", params_.sidelapPct);
    if (azMode_ == 0) std::snprintf(az, sizeof az, "Kierunek szeregow: wzdluz najdluzszego boku (zmien)"); else std::snprintf(az, sizeof az, "Kierunek szeregow: %.0f st. (zmien)", kAzChoices[azMode_]);
    std::snprintf(fol, sizeof fol, "%s", params_.followTerrain ? "Wysokosc: podaza za terenem (zmien)" : "Wysokosc: stala bezwzgledna (zmien)");
    std::snprintf(bat, sizeof bat, "Bateria: %.0f min lotu, zapas %.0f%% (zmien)", params_.batteryMin, params_.reservePct);
    std::snprintf(prof, sizeof prof, "Profil bledow GNSS/IMU: %s (zmien)", kAlsProfiles[profile_].name);
    std::snprintf(pre, sizeof pre, "Gestosc impulsow: %s (zmien)", kAlsPresets[preset_].name);
    return {
      {"area_here", "Dodaj wierzcholek obszaru (pod stopami)", "max 12 wierzcholkow", !flying_},
      {"area_ctrl", "Dodaj wierzcholek obszaru (punkt osnowy)", "wybierz punkt osnowy z listy", !flying_},
      {"area_map", "Zaznacz obszar na mapie (dwa narozniki)", "mapa swiata; zastepuje obecny obszar", !flying_},
      {"area_clear", "Wyczysc obszar", "brak wierzcholkow", !flying_ && !area_.empty()},
      {"agl", agl, "30 / 50 / 80 / 100 / 150 m", !flying_}, {"speed", spd, "3 / 5 / 6 / 8 m/s", !flying_}, {"sidelap", sid, "20 / 30 / 40 %", !flying_},
      {"azimuth", az, "wzdluz boku albo 0 / 45 / 90 / 135 st.", !flying_}, {"follow", fol, "AGL liczone od terenu albo stala wysokosc", !flying_},
      {"battery", bat, "po wyczerpaniu dron wraca do bazy, wymienia baterie i leci dalej", !flying_},
      {"profile", prof, "RTK dobry / PPK / slaby", !flying_}, {"preset", pre, "wiecej impulsow = gestsza chmura, dluzej w grze", !flying_},
      {"plan_csv", "Zapisz plan nalotu (CSV)", "najpierw zdefiniuj obszar", plan_.ok && !flying_},
      {"launch", "Start lotu", "", !flying_},
      {"cancel", "Przerwij lot", "trwa lot", flying_},
      {"camera", camFollow_ ? "Kamera: za dronem (zmien)" : "Kamera: swobodna (zmien)", "widok z tylu drona w czasie lotu", flying_},
      {"finish", "Zakoncz robote", "najpierw wykonaj lot", flights_ > 0 && !flying_},
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
    } else if (base == "area_rect") {                                       // z mapy: "N1:E1:N2:E2" (dwa przeciwlegle narozniki, dowolna kolejnosc)
      double n1, e1, n2, e2;
      if (std::sscanf(arg.c_str(), "%lf:%lf:%lf:%lf", &n1, &e1, &n2, &e2) != 4) return;
      const double nlo = (std::min)(n1, n2), nhi = (std::max)(n1, n2), elo = (std::min)(e1, e2), ehi = (std::max)(e1, e2);
      if (nhi - nlo < 1.0 || ehi - elo < 1.0) { w_.Say("Zaznaczony obszar jest za maly (min. 1 m w kazdym wymiarze)"); return; }
      area_ = {{nlo, elo}, {nlo, ehi}, {nhi, ehi}, {nhi, elo}}; Replan();
    } else if (base == "area_clear") { area_.clear(); Replan();
    } else if (base == "agl") { Cycle(params_.aglM, kAgl, 5);
    } else if (base == "speed") { Cycle(params_.speedMps, kSpeed, 4);
    } else if (base == "sidelap") { Cycle(params_.sidelapPct, kSidelap, 3);
    } else if (base == "azimuth") { azMode_ = (azMode_ + 1) % 5; Replan();
    } else if (base == "battery") { Cycle(params_.batteryMin, kBattery, 5);
    } else if (base == "follow") { params_.followTerrain = !params_.followTerrain;
    } else if (base == "profile") { profile_ = (profile_ + 1) % 3;
    } else if (base == "preset") { preset_ = (preset_ + 1) % 4; params_.sensor.pulseHz = kAlsPresets[preset_].pulseHz; params_.sensor.lineHz = kAlsPresets[preset_].lineHz; Replan();
    } else if (base == "plan_csv") { SavePlanCsv();
    } else if (base == "launch") { Launch();
    } else if (base == "cancel") { FinishFlight(false, false); w_.Say("Lot przerwany");
    } else if (base == "camera") { camFollow_ = !camFollow_; w_.SetFollowCamera(camFollow_); }
  }

  // Lot w czasie gry: w kazdej klatce Tick() puszcza impulsy w budzecie czasu; czas symulacji rosnie z impulsami (gestosc nie zalezy od FPS).
  void Tick() override {
    if (!flying_) return;
    const auto t0 = std::chrono::steady_clock::now();
    auto over = [&]() { return std::chrono::steady_clock::now() - t0 > std::chrono::milliseconds(kBudgetMs); };
    const double dtP = 1.0 / params_.sensor.pulseHz; int pulses = 0;
    double idle = idleStepS_;                                               // czas lotu poza szeregiem przypadajacy na te klatke
    if (idle <= 0) { idle = (std::min)(0.2, std::chrono::duration<double>(t0 - lastTick_).count()); if (idle < 0) idle = 0; }
    lastTick_ = t0;
    while (tSim_ < path_->Duration()) {
      while (tSim_ >= t1_) AdvanceSample();
      if (!(s0_.scanning && s1_.scanning)) {                                // wznios, dolot, zakret, powrot: bez impulsow, ale dron leci
        if (idle <= 1e-9) break;
        const double step = (std::min)(idle, t1_ - tSim_); tSim_ += step; idle -= step; continue;
      }
      if (s0_.strip != curStrip_) { curStrip_ = s0_.strip; raw_.Flush(); traj_.flush(); stripStartCount_ = raw_.Count(); stripStartTrajBytes_ = trajBytes_; stripStartSimT_ = tSim_ + clockOffset_; }
      EmitPulse(); tSim_ += dtP; pulses++;
      if ((pulseCap_ > 0 && pulses >= pulseCap_) || (pulseCap_ == 0 && (pulses & 63) == 0 && over())) break;
    }
    if (tSim_ >= path_->Duration()) { FinishFlight(true, false); return; }
    const TrajSample tr = Lerp(s0_, s1_, (tSim_ - t0_) / kSampleDt);
    w_.SetDronePose(GridPos{tr.pos.y, tr.pos.x, tr.pos.z}, tr.yawDeg, tr.pitchDeg, tr.rollDeg, std::fmod(tSim_ * 3000.0, 360.0));
  }

  std::vector<std::string> SummaryLines() const override {
    std::vector<std::string> out; char b[200];
    std::snprintf(b, sizeof b, "Lotow: %d, ostatni: %lld punktow%s", flights_, lastCount_, outDir_.empty() ? "" : (", folder: " + outDir_).c_str()); out.push_back(b);
    return out;
  }

  bool CanSaveState() const override { return true; }
  void SaveState(StateKV& s) const override {                          // lot w toku: wznowienie od poczatku biezacego szeregu
    raw_.Flush(); traj_.flush();
    s.Bool("aerial", true); s.Int("area.n", (long long)area_.size());
    for (size_t i = 0; i < area_.size(); i++) { s.Num("area." + std::to_string(i) + ".n", area_[i].n); s.Num("area." + std::to_string(i) + ".e", area_[i].e); }
    s.Num("battery", params_.batteryMin); s.Num("agl", params_.aglM); s.Num("speed", params_.speedMps); s.Num("sidelap", params_.sidelapPct); s.Bool("follow", params_.followTerrain);
    s.Int("azMode", azMode_); s.Int("profile", profile_); s.Int("preset", preset_); s.Int("seed", (long long)seed_); s.Int("flights", flights_); s.Int("lastCount", lastCount_); s.Str("outDir", outDir_);
    s.Bool("flying", flying_); s.Int("resumeStrip", CurStrip()); s.Int("stripStartCount", stripStartCount_); s.Int("stripStartTrajBytes", stripStartTrajBytes_);
    s.Num("stripStartSimT", stripStartSimT_); s.Num("home.n", home_.n); s.Num("home.e", home_.e); s.Num("home.h", homeGroundH_); s.Bool("homeSet", home_set_);
  }
  bool LoadState(const StateKV& s) override {
    if (!s.GetBool("aerial")) return false;
    if (flying_) { raw_.Close(); traj_.close(); w_.RemoveDrone(); flying_ = false; }
    area_.clear(); const int n = s.GetInt("area.n");
    for (int i = 0; i < n; i++) area_.push_back({s.GetNum("area." + std::to_string(i) + ".n"), s.GetNum("area." + std::to_string(i) + ".e")});
    params_.batteryMin = s.GetNum("battery", 25); params_.aglM = s.GetNum("agl", 50); params_.speedMps = s.GetNum("speed", 5); params_.sidelapPct = s.GetNum("sidelap", 30); params_.followTerrain = s.GetBool("follow", true);
    azMode_ = s.GetInt("azMode") % 5; profile_ = s.GetInt("profile") % 3; preset_ = s.GetInt("preset") % 4; seed_ = (uint32_t)s.GetNum("seed", seed_);
    params_.sensor.pulseHz = kAlsPresets[preset_].pulseHz; params_.sensor.lineHz = kAlsPresets[preset_].lineHz;
    flights_ = s.GetInt("flights"); lastCount_ = (long long)s.GetNum("lastCount"); outDir_ = s.GetStr("outDir");
    home_ = {s.GetNum("home.n"), s.GetNum("home.e")}; homeGroundH_ = s.GetNum("home.h"); home_set_ = s.GetBool("homeSet"); camFollow_ = false;
    Replan();
    if (s.GetBool("flying")) {
      stripStartCount_ = (long long)s.GetNum("stripStartCount"); stripStartTrajBytes_ = (long long)s.GetNum("stripStartTrajBytes"); stripStartSimT_ = s.GetNum("stripStartSimT");
      if (!BeginFlight(s.GetInt("resumeStrip"))) w_.Say("Nie mozna wznowic lotu");
    }
    return true;
  }

  std::string Hud() const override {
    char b[220];
    if (flying_) { std::snprintf(b, sizeof b, "Lot ALS: szereg %d/%d, %d%%, %lld punktow", CurStrip() + 1, (int)plan_.strips.size(), (int)(100.0 * tSim_ / (std::max)(1e-9, path_->Duration())), raw_.Count()); return b; }
    if (!plan_.ok) { std::snprintf(b, sizeof b, "Nalot ALS: obszar %d wierzcholkow (min. 3)", (int)area_.size()); return b; }
    char loty[48] = ""; if (plan_.sorties > 1) std::snprintf(loty, sizeof loty, ", %d loty (wymiana baterii)", plan_.sorties);
    std::snprintf(b, sizeof b, "Nalot ALS: szeregow: %d, pas %.0f m, odstep %.0f m, lot %.1f min%s, ok. %.1f pkt/m2",
                  (int)plan_.strips.size(), plan_.swathM, plan_.spacingM, plan_.flightTimeS / 60.0, loty, plan_.estDensityPtsM2);
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
  FieldBook Finish(bool) override { FinishFlight(false, true); return FieldBook{}; }   // lot w toku: wynik czesciowy; lot.raw zostaje (wczytanie zapisu moze wznowic lot)

 private:
  static constexpr int kMaxVertices = 12;
  static constexpr double kAgl[5] = {30, 50, 80, 100, 150}, kSpeed[4] = {3, 5, 6, 8}, kSidelap[3] = {20, 30, 40}, kBattery[5] = {5, 10, 15, 25, 40}, kAzChoices[5] = {0, 0, 45, 90, 135};   // [0] = automatycznie

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
    homeGroundH_ = g.h; outDir_ = w_.AerialOutDir();
    if (outDir_.empty()) { w_.Say("Nie mozna utworzyc folderu wynikow"); return; }
    { std::ofstream f(outDir_ + "/plan_nalotu.csv", std::ios::binary | std::ios::trunc); f << PlanCsv(plan_, params_); }
    stripStartCount_ = 0; stripStartTrajBytes_ = 0; stripStartSimT_ = 0; groundWarned_ = false;
    int blocked = 0;                                                      // ostrzezenie (bez omijania): szeregi przecinajace teren lub obiekty na wysokosci przelotu
    for (const Strip& s : plan_.strips) {
      double ha = homeGroundH_, hb = homeGroundH_; if (params_.followTerrain) { w_.GroundHeightM(s.a.n, s.a.e, ha); w_.GroundHeightM(s.b.n, s.b.e, hb); }
      if (w_.RouteBlocked(GridPos{s.a.n, s.a.e, ha + params_.aglM}, GridPos{s.b.n, s.b.e, hb + params_.aglM})) blocked++;
    }
    if (blocked > 0) w_.Say("Uwaga: przeszkody na trasie (" + std::to_string(blocked) + " z " + std::to_string(plan_.strips.size()) + " szeregow); lot sie odbedzie");
    if (BeginFlight(0)) w_.Say("Start lotu: " + std::to_string(plan_.strips.size()) + " szeregow" + (plan_.msg.empty() ? "" : "; " + plan_.msg));
  }

  // Start (albo wznowienie od szeregu fromStrip) lotu: sciezka, szum, pliki robocze, dron w swiecie.
  bool BeginFlight(int fromStrip) {
    const bool resume = fromStrip > 0; fromStrip_ = fromStrip;
    FlightPlan fp = plan_; fp.strips.erase(std::remove_if(fp.strips.begin(), fp.strips.end(), [&](const Strip& s) { return s.id < fromStrip; }), fp.strips.end());
    if (fp.strips.empty()) return false;
    const ErrorProfile prof = haveOverride_ ? override_ : kAlsProfiles[profile_];
    profile_used_ = prof;
    path_ = std::make_unique<FlightPath>(fp, [this](double n, double e) { double h; if (w_.GroundHeightM(n, e, h)) return h;
        if (!groundWarned_) { groundWarned_ = true; w_.Say("Brak wysokosci terenu: uzyto wysokosci punktu startu"); } return homeGroundH_; },
        params_.aglM, params_.speedMps, params_.followTerrain, homeGroundH_, home_);
    noise_ = std::make_unique<TrajectoryNoise>(seed_, prof); boreTrue_ = noise_->TrueBoresight();
    originN_ = area_[0].n; originE_ = area_[0].e; originH_ = homeGroundH_;
    rawPath_ = outDir_ + "/lot.raw"; std::string trajPath = outDir_ + "/trajektoria.csv";
    std::error_code ec;
    bool append = resume;
    if (resume) {                                                          // dopisujemy tylko do plikow zgodnych ze stanem (nie krotszych, nie po zakonczonym locie)
      const auto rs = std::filesystem::file_size(rawPath_, ec); const bool r1 = !ec; const auto ts = std::filesystem::file_size(trajPath, ec); const bool r2 = !ec;
      append = r1 && r2 && rs >= (std::uintmax_t)stripStartCount_ * kAlsRawBytes && ts >= (std::uintmax_t)stripStartTrajBytes_;
      if (!append) {
        const std::string nd = w_.AerialOutDir();
        if (nd.empty()) { w_.Say("Nie mozna utworzyc folderu wynikow"); path_.reset(); return false; }
        outDir_ = nd; rawPath_ = outDir_ + "/lot.raw"; trajPath = outDir_ + "/trajektoria.csv";
        w_.Say("Wznowienie: poprzedni plik roboczy nie pasuje do zapisu (albo lot juz zakonczony); pozostale szeregi trafia do nowego folderu");
      }
    }
    if (append) { std::filesystem::resize_file(rawPath_, (std::uintmax_t)stripStartCount_ * kAlsRawBytes, ec); std::filesystem::resize_file(trajPath, (std::uintmax_t)stripStartTrajBytes_, ec); }
    if (!raw_.Open(rawPath_, append)) { w_.Say("Nie mozna otworzyc plikow lotu"); path_.reset(); return false; }
    traj_.open(trajPath, std::ios::binary | (append ? std::ios::app : std::ios::trunc)); trajBytes_ = append ? stripStartTrajBytes_ : 0;
    if (!traj_.good()) { raw_.Close(); w_.Say("Nie mozna otworzyc plikow lotu"); path_.reset(); return false; }
    if (!append) { const std::string hd = "t;N_prawdziwe;E_prawdziwe;H_prawdziwe;N_gnss;E_gnss;H_gnss;roll;pitch;yaw;roll_imu;pitch_imu;yaw_imu;szereg\n"; traj_ << hd; trajBytes_ = (long long)hd.size(); }
    if (!w_.PlaceDrone(GridPos{home_.n, home_.e, homeGroundH_})) { raw_.Close(); traj_.close(); w_.Say("Nie mozna postawic drona"); path_.reset(); return false; }
    resumed_ = append; clockOffset_ = append ? stripStartSimT_ - path_->FirstScanTime() : 0.0;
    tSim_ = 0; t0_ = 0; t1_ = kSampleDt; s0_ = path_->At(0); e0_ = noise_->Estimate(s0_); s1_ = path_->At(t1_); e1_ = noise_->Estimate(s1_); LogRow(s1_, e1_);
    curStrip_ = -1; lastTick_ = std::chrono::steady_clock::now(); flying_ = true; return true;
  }

  static double LerpAng(double a, double b, double f) { double d = std::fmod(b - a, 360.0); if (d > 180) d -= 360; if (d < -180) d += 360; return a + d * f; }
  static TrajSample Lerp(const TrajSample& a, const TrajSample& b, double f) {
    f = (std::max)(0.0, (std::min)(1.0, f)); TrajSample r = a;
    r.pos = {a.pos.x + (b.pos.x - a.pos.x) * f, a.pos.y + (b.pos.y - a.pos.y) * f, a.pos.z + (b.pos.z - a.pos.z) * f};
    r.rollDeg = a.rollDeg + (b.rollDeg - a.rollDeg) * f; r.pitchDeg = a.pitchDeg + (b.pitchDeg - a.pitchDeg) * f; r.yawDeg = LerpAng(a.yawDeg, b.yawDeg, f); r.t = a.t + (b.t - a.t) * f;
    return r;
  }
  void AdvanceSample() { s0_ = s1_; e0_ = e1_; t0_ = t1_; t1_ = t0_ + kSampleDt; s1_ = path_->At(t1_); e1_ = noise_->Estimate(s1_); LogRow(s1_, e1_); }
  void LogRow(const TrajSample& t, const TrajSample& e) {                // po wznowieniu zapisywane sa tylko probki na szeregach (dojazd nie)
    if (resumed_ && !t.scanning) return;
    char b[300]; const int n = std::snprintf(b, sizeof b, "%.2f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f;%d\n", t.t + clockOffset_, t.pos.y, t.pos.x, t.pos.z, e.pos.y, e.pos.x, e.pos.z,
                                             t.rollDeg, t.pitchDeg, t.yawDeg, e.rollDeg, e.pitchDeg, e.yawDeg, t.scanning ? t.strip : -1);
    traj_.write(b, n); trajBytes_ += n;
  }
  void EmitPulse() {
    const double f = (tSim_ - t0_) / kSampleDt; const TrajSample truth = Lerp(s0_, s1_, f), est = Lerp(e0_, e1_, f);
    const double theta = MirrorThetaDeg(tSim_, params_.sensor); const AlsRay r = TrueRay(truth, boreTrue_, lever_, theta);
    const double pi = 3.14159265358979323846; double az = std::atan2(r.dir.x, r.dir.y) * 180.0 / pi; if (az < 0) az += 360.0;
    const double el = std::asin((std::max)(-1.0, (std::min)(1.0, r.dir.z))) * 180.0 / pi;
    double dist = 0, cosInc = 1; bool vob = false; unsigned rgb = 0x808080;
    if (!w_.AerialRay(GridPos{r.origin.y, r.origin.x, r.origin.z}, az, el, kMaxRangeM, dist, cosInc, vob, &rgb) || dist < 1.0 || dist > kMaxRangeM) return;
    const double range = dist + (rangeNoise_ ? rangeRng_.Normal() * ScanNoiseSigmaM(dist) : 0.0);
    const Vec3 p = GeorefPoint(est, boreNom_, lever_, theta, range);
    AlsPoint o{(float)(p.x - originE_), (float)(p.y - originN_), (float)(p.z - originH_), kGpsT0 + clockOffset_ + tSim_, (std::uint16_t)s0_.strip, ScanIntensity(dist, cosInc, vob),
               (std::uint8_t)((rgb >> 16) & 255), (std::uint8_t)((rgb >> 8) & 255), (std::uint8_t)(rgb & 255), (float)theta, (float)range};
    raw_.Append(o);
  }
  bool InArea(double n, double e) const {
    bool in = false;
    for (size_t i = 0, j = area_.size() - 1; i < area_.size(); j = i++)
      if (((area_[i].n > n) != (area_[j].n > n)) && (e < (area_[j].e - area_[i].e) * (n - area_[i].n) / (area_[j].n - area_[i].n) + area_[i].e)) in = !in;
    return in;
  }
  void FinishFlight(bool complete, bool keepRaw) {
    if (!flying_) return;
    flying_ = false; raw_.Close(); traj_.close();
    const AlsStats st = AlsConvert(rawPath_, originN_, originE_, originH_, outDir_ + "/lot.pts", outDir_ + "/lot.las");
    const std::vector<StripQc> qc = AlsStripCheck(rawPath_);
    std::vector<ControlPoint> cps; for (const ControlPoint& c : w_.ControlNearHero(1e9)) if (InArea(c.n, c.e)) cps.push_back(c);
    const std::vector<ControlQc> cq = AlsControlCheck(rawPath_, originN_, originE_, originH_, cps);
    WriteInfo(st, qc, cq, complete);
    if (!keepRaw) {                                                        // surowe dane zostaja jako lot_surowy.bin (wejscie do wyrownania), lot.raw nie sluzy juz do wznowienia
      std::error_code ec; const std::string kept = outDir_ + "/lot_surowy.bin"; std::filesystem::remove(kept, ec); std::filesystem::rename(rawPath_, kept, ec);
    }
    w_.RemoveDrone(); if (camFollow_) { camFollow_ = false; w_.SetFollowCamera(false); }
    flights_++; lastCount_ = st.count; path_.reset();
    char b[160];
    if (st.count == 0) w_.Say("Lot zakonczony: brak punktow (promienie nie trafily w teren)");
    else { std::snprintf(b, sizeof b, "Lot %s: %lld punktow, wyniki w folderze lotu", complete ? "zakonczony" : "przerwany", st.count); w_.Say(b); }
  }
  void WriteInfo(const AlsStats& st, const std::vector<StripQc>& qc, const std::vector<ControlQc>& cq, bool complete) {
    std::ofstream f(outDir_ + "/lot_opis.txt"); char b[260];
    f << "Skaning lotniczy z drona (ALS)\nLot: " << (complete ? "zakonczony" : "przerwany") << (resumed_ ? " (wznowiony po zapisie gry; trajektoria dojazdu nie jest zapisana)" : "") << "\n";
    f << "Profil bledow GNSS/IMU: " << profile_used_.name << "\nGestosc impulsow: " << kAlsPresets[preset_].name << "\n";
    std::snprintf(b, sizeof b, "AGL: %.0f m, predkosc: %.0f m/s, zakladka: %.0f%%, kierunek szeregow: %.1f st., wysokosc: %s\n", params_.aglM, params_.speedMps, params_.sidelapPct, params_.stripAzDeg, params_.followTerrain ? "podaza za terenem" : "stala bezwzgledna");
    f << b;
    std::snprintf(b, sizeof b, "Szeregow: %d, pas: %.1f m, odstep: %.1f m, czas lotu (plan): %.1f min, ok. %.1f pkt/m2\n", (int)plan_.strips.size(), plan_.swathM, plan_.spacingM, plan_.flightTimeS / 60.0, plan_.estDensityPtsM2); f << b;
    std::snprintf(b, sizeof b, "Lever arm (antena -> skaner): %.2f %.2f %.2f m; zasieg promienia: %.0f m\n", lever_.x, lever_.y, lever_.z, kMaxRangeM); f << b;
    std::snprintf(b, sizeof b, "Punktow: %lld\n", st.count); f << b;
    if (st.ok) { std::snprintf(b, sizeof b, "Zakres: E %.3f..%.3f, N %.3f..%.3f, H %.3f..%.3f\n", st.minE, st.maxE, st.minN, st.maxN, st.minH, st.maxH); f << b; }
    if (opt_.showTrueError && noise_) { double r, p, y; noise_->BoresightDeg(r, p, y); std::snprintf(b, sizeof b, "Prawdziwy blad boresightu (roll, pitch, yaw): %+.3f %+.3f %+.3f st.\n", r, p, y); f << b; }
    f << "Kontrola szeregow (roznice H w zakladkach, szereg b - szereg a):\n";
    for (const StripQc& q : qc) { std::snprintf(b, sizeof b, "  szereg %d-%d: komorek %d, meanDz=%+.3f m, rms=%.3f m\n", q.a, q.b, q.cells, q.meanDzM, q.rmsDzM); f << b; }
    if (qc.empty()) f << "  (jeden szereg)\n";
    f << "Kontrola na punktach osnowy (H chmury - H punktu):\n";
    for (const ControlQc& q : cq) { std::snprintf(b, sizeof b, "  %s: dh=%+.3f m (%d punktow)\n", q.id.c_str(), q.dhM, q.pts); f << b; }
    if (cq.empty()) f << "  (brak punktow osnowy w obszarze)\n";
  }

  int CurStrip() const { return curStrip_ >= 0 ? curStrip_ : fromStrip_; }   // szereg w toku (przed pierwszym impulsem: ten, od ktorego start)
  static constexpr int kBudgetMs = 12;                                   // budzet czasu lotu na klatke
  static constexpr double kSampleDt = 0.1, kMaxRangeM = 250.0, kGpsT0 = 345600.0;

  IWorld& w_; JobOptions opt_; uint32_t seed_; GnssRng rangeRng_;
  FlightParams params_; FlightPlan plan_; std::vector<PlanPt> area_;
  int azMode_ = 0, profile_ = 0, preset_ = 1, flights_ = 0; bool flying_ = false, home_set_ = false, camFollow_ = false; PlanPt home_{0, 0};
  // lot
  std::unique_ptr<FlightPath> path_; std::unique_ptr<TrajectoryNoise> noise_; ErrorProfile profile_used_{"", 0, 0, 0, 0, 0, 0}, override_{"", 0, 0, 0, 0, 0, 0};
  std::chrono::steady_clock::time_point lastTick_ = std::chrono::steady_clock::now(); double idleStepS_ = 0;   // 0 = czas rzeczywisty klatki
  mutable AlsRawWriter raw_; mutable std::ofstream traj_; std::string outDir_, rawPath_;
  Vec3 lever_{0.0, 0.15, -0.30}; Rot3 boreNom_ = RotFromRpy(0, 0, 0), boreTrue_ = RotFromRpy(0, 0, 0);
  double tSim_ = 0, t0_ = 0, t1_ = kSampleDt, clockOffset_ = 0, stripStartSimT_ = 0, homeGroundH_ = 0, originN_ = 0, originE_ = 0, originH_ = 0;
  TrajSample s0_, s1_, e0_, e1_; int curStrip_ = -1, fromStrip_ = 0, pulseCap_ = 0;
  long long stripStartCount_ = 0, stripStartTrajBytes_ = 0, trajBytes_ = 0, lastCount_ = 0; bool groundWarned_ = false, resumed_ = false, haveOverride_ = false, rangeNoise_ = true;
};

inline std::unique_ptr<Job> CreateAerialJob(IWorld& w, const JobOptions& o) { return std::make_unique<AerialJob>(w, o); }
