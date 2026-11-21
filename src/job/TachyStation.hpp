// Robota "Tachimetria (pomiar biegunowy)": stanowisko o znanych wspolrzednych, orientacja na lustro stojace na punkcie nawiazania,
// pomiar punktow na lustro albo bezlustrowy (bez Gothic API). Wspolrzedne punktow licza sie z odczytanych katow i odleglosci.
#pragma once
#include <cmath>
#include <cstdio>
#include "Job.hpp"
#include "Keys.hpp"

class TachyStationJob : public Job {
 public:
  TachyStationJob(IWorld& w, const JobOptions& o) : w_(w), opt_(o), rng_((uint32_t)(w.Random01() * 4e9) + 1) { w_.SetPrismHeightM(kPrism[0]); }

  std::string Name() const override { return "Tachimetria (pomiar biegunowy)"; }
  JobKind Kind() const override { return JobKind::Tachy; }

  std::vector<MenuItem> Menu() const override {
    char ph[64]; std::snprintf(ph, sizeof ph, "Wysokosc lustra: %.3f m (zmien)", kPrism[prismIdx_]);
    const bool fresh = points_.empty();
    return {
      {"station", stationUp_ ? "Przestaw tachimetr (stanowisko)" : "Postaw tachimetr (stanowisko)", "nie po zmierzeniu punktu", fresh},
      {"backsight", backUp_ ? "Przestaw punkt nawiazania (lustro)" : "Postaw punkt nawiazania (lustro)", "najpierw tachimetr; nie po zmierzeniu punktu", stationUp_ && fresh},
      {"orient", "Orientacja na lustro nawiazania", "najpierw tachimetr i punkt nawiazania", stationUp_ && backUp_},
      {"meas", reflectorless_ ? "Rodzaj pomiaru: bezlustrowy (zmien)" : "Rodzaj pomiaru: na lustro (zmien)", "najpierw tachimetr", stationUp_},
      {"prism", prismUp_ ? "Przestaw tyczke z lustrem" : "Postaw tyczke z lustrem", "najpierw tachimetr", stationUp_},
      {"prismh", ph, "najpierw tachimetr", stationUp_},
      {"view", "Patrz przez lunete (" LV_KEYNAME_SCOPE ")", reflectorless_ ? "najpierw orientacja" : "najpierw orientacja i tyczka z lustrem", oriented_ && (reflectorless_ || prismUp_)},
      {"finish", "Zakoncz robote", "najpierw zmierz co najmniej jeden punkt", CanFinish()},
    };
  }

  void OnMenu(const std::string& id) override {
    for (const MenuItem& m : Menu()) if (m.id == id && !m.enabled) return;
    if (id == "station") {
      if (!w_.PlaceTachymeter()) { w_.Say("Brak gruntu pod stopami"); return; }
      stationUp_ = true; oriented_ = false;
    } else if (id == "backsight") {
      if (!w_.PlaceBacksight()) { w_.Say("Brak gruntu pod stopami"); return; }
      backUp_ = true; oriented_ = false;
    } else if (id == "prism") {
      if (!w_.PlacePrism()) { w_.Say("Brak gruntu pod stopami"); return; }
      prismUp_ = true;
    } else if (id == "prismh") {
      prismIdx_ = (prismIdx_ + 1) % kPrismCount;
      w_.SetPrismHeightM(kPrism[prismIdx_]);          // modele lustra (tyczka i nawiazanie) maja od teraz te wysokosc
    } else if (id == "meas") {
      reflectorless_ = !reflectorless_;
    } else if (id == "orient") {
      mode_ = Mode::Orient;
    } else if (id == "view") {
      mode_ = Mode::Measure;
    }
    // "finish": kontroler wywoluje FinishTachy(true)
  }

  std::string TachyFire(double camAzDeg, double camPitchDeg) override {
    if (!stationUp_) return "Najpierw postaw tachimetr";
    if (mode_ == Mode::Orient) return Orient(camAzDeg, camPitchDeg);
    if (!oriented_) return "Najpierw orientacja na nawiazanie";
    if (reflectorless_) return MeasureReflectorless(camAzDeg, camPitchDeg);
    if (!prismUp_) return "Najpierw postaw tyczke z lustrem";
    return MeasurePrism(camAzDeg, camPitchDeg);
  }

  std::vector<std::string> TachyPanel(double camAzDeg, double camPitchDeg) const override {
    const double az = std::isfinite(camAzDeg) ? camAzDeg : 0.0, pitch = std::isfinite(camPitchDeg) ? camPitchDeg : 0.0;
    const double hz = NormGon(DegToGon(az - (oriented_ ? az0_ : 0.0)));
    const double v = DegToGon((std::min)(180.0, (std::max)(0.0, 90.0 - pitch)));
    char b[96]; std::vector<std::string> out;
    out.push_back(mode_ == Mode::Orient ? "Tryb: orientacja na lustro nawiazania" : "Tryb: pomiar punktow");
    std::snprintf(b, sizeof b, "Hz: %.4f g   V: %.4f g%s", hz, v, oriented_ ? "" : "  (przed orientacja)"); out.push_back(b);
    if (reflectorless_) std::snprintf(b, sizeof b, "Pomiar bezlustrowy   Punkty: %d", (int)points_.size());
    else std::snprintf(b, sizeof b, "Pomiar na lustro %.3f m   Punkty: %d", kPrism[prismIdx_], (int)points_.size());
    out.push_back(b);
    out.push_back(lastMsg_.empty() ? (mode_ == Mode::Orient ? "Enter - orientuj na lustro nawiazania" : "Enter - zmierz punkt") : lastMsg_);
    return out;
  }

  std::string Hud() const override {
    if (!stationUp_) return std::string("Postaw tachimetr (") + LV_KEYNAME_MENU + " - menu)";
    if (!backUp_) return "Postaw punkt nawiazania z lustrem";
    if (!oriented_) return "Zorientuj tachimetr na lustro nawiazania (menu - Orientacja)";
    if (!reflectorless_ && !prismUp_) return "Postaw tyczke z lustrem albo wybierz pomiar bezlustrowy";
    return std::string("Mierz punkty (") + LV_KEYNAME_SCOPE + " - luneta), potem zakoncz robote";
  }

  bool CanFinish() const override { return !points_.empty(); }

  TachyReport FinishTachy(bool complete) override {
    TachyReport r; r.complete = complete; r.showTrueError = opt_.showTrueError; r.points = points_;
    r.stationN = st_.n; r.stationE = st_.e; r.stationH = st_.h; r.instrH = st_.instrH; r.azBackGon = DegToGon(st_.azBackDeg);
    return r;
  }

  // Interfejs niwelacji: ta robota go nie uzywa.
  std::vector<int> ActiveRodPoints() const override { return {}; }
  int RodPointFor(RodRole) const override { return -1; }
  bool OnReading(int, int) override { return false; }
  FieldBook Finish(bool) override { return FieldBook{}; }

 private:
  enum class Mode { Orient, Measure };
  static constexpr int kPrismCount = 4;
  static constexpr double kPrism[kPrismCount] = {1.5, 1.8, 2.0, 1.3};   // wysokosc srodka lustra nad gruntem [m]; tyczki maja model o tej wysokosci
  static constexpr double kMinSightM = 1.5;                             // minimalna odleglosc pomiaru bezlustrowego

  static double Mm(double v) { return std::round(v * 1000.0) / 1000.0; }   // wspolrzedne "znane z mapy" do 1 mm

  std::string Orient(double camAz, double camPitch) {
    if (!backUp_) return lastMsg_ = "Najpierw postaw punkt nawiazania";
    const GridPos s = w_.StationGrid(), b = w_.PointGrid(1);
    const TachyAim a = TachyAimError(b.n - s.n, b.e - s.e, (b.h + kPrism[prismIdx_]) - (s.h + st_.instrH), camAz, camPitch);
    if (a.distM < 3.0 || a.distM > 300.0) return lastMsg_ = "Nawiazanie poza zasiegiem (3-300 m)";
    if (!a.ok) return lastMsg_ = "Nie celujesz w lustro nawiazania";
    if (!w_.LineClear(false)) return lastMsg_ = "Linia celowa zaslonieta";
    st_.n = Mm(s.n); st_.e = Mm(s.e); st_.h = Mm(s.h);
    st_.azBackDeg = TachyAzimuthDeg(st_.n, st_.e, Mm(b.n), Mm(b.e));
    az0_ = TachyOrient(camAz, noise_, rng_); st_.az0Deg = az0_;
    oriented_ = true; mode_ = Mode::Measure;               // po orientacji od razu mierzymy (bez wychodzenia z lunety)
    return lastMsg_ = "Orientacja ustawiona";
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
  bool stationUp_ = false, backUp_ = false, prismUp_ = false, oriented_ = false, reflectorless_ = false;
  Mode mode_ = Mode::Measure; int prismIdx_ = 0; double az0_ = 0;
  std::string lastMsg_;
  std::vector<TachyPoint> points_;
};

inline std::unique_ptr<Job> CreateTachyStation(IWorld& w, const JobOptions& o) { return std::make_unique<TachyStationJob>(w, o); }
