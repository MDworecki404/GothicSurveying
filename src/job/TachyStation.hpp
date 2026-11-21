// Robota "Tachimetria (pomiar biegunowy)": stanowisko (na punkcie osnowy GNSS albo o znanych wspolrzednych), do 5 nawiazan z lustrami,
// orientacja ze sredniej kierunkow z odchylkami kontrolnymi, pomiar punktow na lustro albo bezlustrowy (bez Gothic API).
// Wspolrzedne punktow licza sie z odczytanych katow i odleglosci; geometria celowania uzywa prawdziwych polozen obiektow.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdio>
#include "Job.hpp"
#include "Keys.hpp"

class TachyStationJob : public Job {
 public:
  TachyStationJob(IWorld& w, const JobOptions& o) : w_(w), opt_(o), rng_((uint32_t)(w.Random01() * 4e9) + 1) {
    control_ = !w_.ControlNearHero(1e9).empty();      // domyslnie osnowa GNSS, gdy w swiecie sa jej punkty
    w_.SetPrismHeightM(kPrism[0]);
  }

  std::string Name() const override { return "Tachimetria (pomiar biegunowy)"; }
  JobKind Kind() const override { return JobKind::Tachy; }

  std::vector<MenuItem> Menu() const override {
    char ph[64]; std::snprintf(ph, sizeof ph, "Wysokosc lustra: %.3f m (zmien)", kPrism[prismIdx_]);
    const bool fresh = points_.empty(), canSwitchSrc = !stationUp_ && (control_ || !w_.ControlNearHero(1e9).empty());
    const int n = BackCount();
    return {
      {"src", control_ ? "Zrodlo wspolrzednych: osnowa GNSS (zmien)" : "Zrodlo wspolrzednych: prawdziwe (zmien)", "przed postawieniem tachimetru; osnowa wymaga punktow", canSwitchSrc},
      {"station", stationUp_ ? "Przestaw tachimetr (stanowisko)" : "Postaw tachimetr (stanowisko)", "nie po zmierzeniu punktu", fresh},
      {"backsight", "Dodaj nawiazanie (lustro)", "najpierw tachimetr; max 5; nie po zmierzeniu punktu", stationUp_ && fresh && n < kMaxBack},
      {"unback", "Usun ostatnie nawiazanie", "brak nawiazan; nie po zmierzeniu punktu", n > 0 && fresh},
      {"meas", reflectorless_ ? "Rodzaj pomiaru: bezlustrowy (zmien)" : "Rodzaj pomiaru: na lustro (zmien)", "najpierw tachimetr", stationUp_},
      {"prism", prismUp_ ? "Przestaw tyczke z lustrem" : "Postaw tyczke z lustrem", "najpierw tachimetr", stationUp_},
      {"prismh", ph, "najpierw tachimetr", stationUp_},
      {"view", "Patrz przez lunete (" LV_KEYNAME_SCOPE ")", "najpierw tachimetr i co najmniej jedno nawiazanie", stationUp_ && n >= 1},
      {"finish", "Zakoncz robote", "najpierw zmierz co najmniej jeden punkt", CanFinish()},
    };
  }

  bool NeedsChoice(const std::string& menuId) const override { return control_ && (menuId == "station" || menuId == "backsight"); }

  std::vector<MenuItem> TachyChoices(const std::string& menuId) const override {
    std::vector<MenuItem> out; char b[120];
    if (menuId == "station") {
      for (const ControlPoint& c : w_.ControlNearHero(kStationRadiusM)) {
        std::snprintf(b, sizeof b, "%s  N %.3f E %.3f H %.3f", c.id.c_str(), c.n, c.e, c.h);
        out.push_back({"station:" + c.id, b, "", true});
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
    if (base == "src") {
      control_ = !control_;
    } else if (base == "station") {
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
      stationUp_ = true; ClearReadings();
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
      backs_.push_back(b); ClearReadings();
    } else if (base == "unback") {
      w_.RemoveBacksight(BackCount()); backs_.pop_back(); ClearReadings();
    } else if (base == "prism") {
      if (!w_.PlacePrism()) { w_.Say("Brak gruntu pod stopami"); return; }
      prismUp_ = true;
    } else if (base == "prismh") {
      prismIdx_ = (prismIdx_ + 1) % kPrismCount;
      w_.SetPrismHeightM(kPrism[prismIdx_]);          // modele lustra (tyczka i nawiazania) maja od teraz te wysokosc
    } else if (base == "meas") {
      reflectorless_ = !reflectorless_;
    }
    // "view": kontroler otwiera luneta; "finish": kontroler wywoluje FinishTachy(true)
  }

  std::string TachyFire(double camAzDeg, double camPitchDeg) override {
    if (!stationUp_) return "Najpierw postaw tachimetr";
    const std::string bs = TryBacksight(camAzDeg, camPitchDeg);      // celowanie w lustro nawiazania ma pierwszenstwo
    if (!bs.empty()) return bs;
    if (!oriented_) return lastMsg_ = "Najpierw zmierz nawiazanie (celuj w lustro nawiazania)";
    if (reflectorless_) return MeasureReflectorless(camAzDeg, camPitchDeg);
    if (!prismUp_) return "Najpierw postaw tyczke z lustrem";
    return MeasurePrism(camAzDeg, camPitchDeg);
  }

  std::vector<std::string> TachyPanel(double camAzDeg, double camPitchDeg) const override {
    const double az = std::isfinite(camAzDeg) ? camAzDeg : 0.0, pitch = std::isfinite(camPitchDeg) ? camPitchDeg : 0.0;
    const double hz = NormGon(DegToGon(az - (oriented_ ? az0_ : 0.0)));
    const double v = DegToGon((std::min)(180.0, (std::max)(0.0, 90.0 - pitch)));
    char b[110]; std::vector<std::string> out;
    std::snprintf(b, sizeof b, "Hz: %.4f g   V: %.4f g%s", hz, v, oriented_ ? "" : "  (przed orientacja)"); out.push_back(b);
    std::snprintf(b, sizeof b, "Nawiazania: %d z %d zmierzone   orientacja: %s", ReadCount(), BackCount(), oriented_ ? "tak" : "nie"); out.push_back(b);
    if (reflectorless_) std::snprintf(b, sizeof b, "Pomiar bezlustrowy   Punkty: %d", (int)points_.size());
    else std::snprintf(b, sizeof b, "Pomiar na lustro %.3f m   Punkty: %d", kPrism[prismIdx_], (int)points_.size());
    out.push_back(b);
    out.push_back(lastMsg_.empty() ? "Enter - zmierz (lustro nawiazania albo punkt)" : lastMsg_);
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

  TachyReport FinishTachy(bool complete) override {
    TachyReport r; r.complete = complete; r.showTrueError = opt_.showTrueError; r.points = points_;
    r.stationN = st_.n; r.stationE = st_.e; r.stationH = st_.h; r.instrH = st_.instrH;
    r.coordSource = control_ ? "osnowa GNSS" : "prawdziwe"; r.stationName = control_ ? stationCp_.id : "";
    for (const Back& b : backs_) {
      if (!b.read) continue;
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

 private:
  struct Back { std::string name; ControlPoint cp; bool read = false; double azMeasDeg = 0, distMeasM = 0; };   // cp.n,e,h = wspolrzedne uzywane do obliczen
  static constexpr int kMaxBack = 5;
  static constexpr int kPrismCount = 4;
  static constexpr double kPrism[kPrismCount] = {1.5, 1.8, 2.0, 1.3};   // wysokosc srodka lustra nad gruntem [m]; tyczki maja model o tej wysokosci
  static constexpr double kMinSightM = 1.5;                             // minimalna odleglosc pomiaru bezlustrowego
  static constexpr double kStationRadiusM = 2.0;                        // znak osnowy dla stanowiska max 2 m od bohatera

  static double Mm(double v) { return std::round(v * 1000.0) / 1000.0; }   // wspolrzedne "znane z mapy" do 1 mm

  int BackCount() const { return (int)backs_.size(); }
  int ReadCount() const { int n = 0; for (const Back& b : backs_) if (b.read) n++; return n; }
  bool Used(const std::string& id) const { for (const Back& b : backs_) if (b.name == id) return true; return false; }
  static const ControlPoint* FindIn(const std::vector<ControlPoint>& v, const std::string& id) {
    static thread_local ControlPoint hold; for (const ControlPoint& c : v) if (c.id == id) { hold = c; return &hold; } return nullptr;
  }
  void ClearBacksights() { for (int i = BackCount(); i >= 1; i--) w_.RemoveBacksight(i); backs_.clear(); }
  void ClearReadings() { for (Back& b : backs_) b.read = false; oriented_ = false; lastMsg_.clear(); }

  double AzTo(const Back& b) const { return TachyAzimuthDeg(st_.n, st_.e, b.cp.n, b.cp.e); }

  // Orientacja = srednia kolowa z (azymut kamery - azymut ze wspolrzednych) po odczytanych nawiazaniach.
  void Reorient() {
    std::vector<double> d;
    for (const Back& b : backs_) if (b.read) d.push_back(NormDeg(b.azMeasDeg - AzTo(b)));
    oriented_ = !d.empty(); az0_ = TachyCircularMeanDeg(d); st_.az0Deg = az0_;
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
    for (int i = 0; i < BackCount(); i++) {
      const GridPos g = w_.PointGrid(i + 1);
      const TachyAim a = TachyAimError(g.n - s.n, g.e - s.e, (g.h + kPrism[prismIdx_]) - (s.h + st_.instrH), camAz, camPitch);
      if (!a.ok) continue;
      if (!points_.empty()) return lastMsg_ = "Orientacja zablokowana po pierwszym punkcie (nawiazania bez zmian)";   // inaczej az0 zmieniloby sie w trakcie serii
      if (!w_.LineClear(false, i + 1)) return lastMsg_ = "Linia celowa zaslonieta";
      Back& b = backs_[i];
      b.azMeasDeg = TachyOrient(camAz, noise_, rng_);
      b.distMeasM = TachyMeasure(camAz, camPitch, 0.0, a.distM, noise_, rng_).slopeM;
      b.read = true; Reorient();
      const TachyBacksight r = Residuals(b);
      char m[140]; std::snprintf(m, sizeof m, "Nawiazanie %s: kierunek %.1f mgon, odleglosc %.1f mm (%d z %d)%s", b.name.c_str(), r.dirResidMgon, r.distResidMm,
                                 ReadCount(), BackCount(), r.uncertain ? " - niepewne" : "");
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
    const TachyReading r = TachyMeasure(camAz, camPitch, az0_, a.distM, noise_, rng_);
    return Store(r, TachyPolar(st_, r, ph), ph, g, "");
  }

  // Pomiar bezlustrowy: odleglosc do pierwszego trafionego punktu (teren, budynek), bez lustra; wiekszy szum odleglosci.
  std::string MeasureReflectorless(double camAz, double camPitch) {
    GridPos hit{0, 0, 0}; double slope = 0;
    if (!w_.TraceSight(camAz, camPitch, hit, slope)) return lastMsg_ = "Brak odbicia (nic nie trafiono w 300 m)";
    if (slope < kMinSightM) return lastMsg_ = "Za blisko (min. 1.5 m)";
    const TachyNoise nz{noise_.angleArcsec, 3.0, 2.0};
    const TachyReading r = TachyMeasure(camAz, camPitch, az0_, slope, nz, rng_);
    return Store(r, TachyPolar(st_, r, 0.0), 0.0, hit, "bezlustrowy");
  }

  std::string Store(const TachyReading& r, const TachyResult& res, double prismM, const GridPos& truth, const char* note) {
    TachyPoint p; p.name = "T" + std::to_string(points_.size() + 1); p.timeText = w_.TimeText();
    p.hzGon = r.hzGon; p.vGon = r.vGon; p.slopeM = r.slopeM; p.hDistM = res.hDistM;
    p.dHM = r.slopeM * std::cos(GonToDeg(r.vGon) * TACHY_PI / 180.0);
    p.n = res.n; p.e = res.e; p.h = res.h; p.prismM = prismM; p.note = note;
    if (opt_.showTrueError) {
      p.hasTrue = true; p.dNm = res.n - truth.n; p.dEm = res.e - truth.e; p.dHm = res.h - truth.h; p.d2Dm = std::hypot(p.dNm, p.dEm);
    }
    points_.push_back(p);
    char b[96]; std::snprintf(b, sizeof b, "%s: N %.3f E %.3f H %.3f", p.name.c_str(), p.n, p.e, p.h);
    return lastMsg_ = b;
  }

  IWorld& w_; JobOptions opt_; GnssRng rng_;
  TachyNoise noise_;
  TachyStation st_{0, 0, 0};
  ControlPoint stationCp_;
  std::vector<Back> backs_;
  bool control_ = false, stationUp_ = false, prismUp_ = false, oriented_ = false, reflectorless_ = false;
  int prismIdx_ = 0; double az0_ = 0;
  std::string lastMsg_;
  std::vector<TachyPoint> points_;
};

inline std::unique_ptr<Job> CreateTachyStation(IWorld& w, const JobOptions& o) { return std::make_unique<TachyStationJob>(w, o); }
