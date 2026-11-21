// Robota "Pomiar GNSS (RTK rover)": tyczka z odbiornikiem, sesje punktow z symulacja jakosci (bez Gothic API).
#pragma once
#include <cmath>
#include <cstdio>
#include <memory>
#include "Job.hpp"
#include "Keys.hpp"

class GnssRover : public Job {
 public:
  GnssRover(IWorld& w, const JobOptions& o) : w_(w), opt_(o) {}

  std::string Name() const override { return "Pomiar GNSS (RTK)"; }
  JobKind Kind() const override { return JobKind::Gnss; }

  std::vector<MenuItem> Menu() const override {
    char ant[64]; std::snprintf(ant, sizeof ant, "Wysokosc anteny: %.3f m (zmien)", kAntenna[antIdx_]);
    return {
      {"pole", poleUp_ ? "Przestaw tyczke z odbiornikiem" : "Postaw tyczke z odbiornikiem", "nie w trakcie pomiaru punktu", !measuring_},
      {"fine", "Precyzyjne ustawienie tyczki (milimetry)", "najpierw postaw tyczke; nie w trakcie pomiaru punktu", poleUp_ && !measuring_},   // obsluguje kontroler
      {"antenna", ant, "nie w trakcie pomiaru punktu", !measuring_},
      {"start", "Rozpocznij pomiar punktu", "najpierw postaw tyczke", poleUp_ && !measuring_},
      {"store", "Zapisz punkt", "fix i 10 epok (bez fixu: 5 epok z rozwiazaniem)", CanStore()},
      {"control", "Zaloz punkt osnowy", "fix i 20 epok", CanControl()},
      {"discard", "Odrzuc pomiar", "trwa pomiar punktu", measuring_},
      {"finish", "Zakoncz robote", "najpierw zapisz co najmniej jeden punkt", CanFinish()},
    };
  }

  void OnMenu(const std::string& id) override {
    for (const MenuItem& m : Menu()) if (m.id == id && !m.enabled) return;
    if (id == "pole") {
      if (!w_.PlacePole()) { w_.Say("Brak gruntu pod stopami"); return; }
      poleUp_ = true;
    } else if (id == "antenna") {
      antIdx_ = (antIdx_ + 1) % kAntennaCount;
    } else if (id == "start") {
      truth_ = w_.PoleGroundGrid();
      session_ = std::make_unique<GnssSession>((uint32_t)(w_.Random01() * 4e9), w_.NowSec(), truth_, kAntenna[antIdx_]);
      lastEpoch_ = w_.NowSec(); measuring_ = true;
    } else if (id == "store") {
      Store();
    } else if (id == "control") {
      const GnssFix& f = session_->Fix();
      ControlPoint cp; cp.n = f.n; cp.e = f.e; cp.h = f.h; cp.solution = GnssSolName(f.type); cp.pdop = f.pdop; cp.epochs = f.epochs; cp.timeText = w_.TimeText();
      GnssFromGrid(w_.PoleGroundGrid(), cp.x, cp.y, cp.z);       // prawdziwe polozenie znaku w grze [cm]
      const std::string cid = w_.StoreControlPoint(cp);
      if (cid.empty()) { w_.Say("Blad zapisu osnowy"); return; }
      Store("osnowa " + cid);
    } else if (id == "discard") {
      measuring_ = false; session_.reset();
    }
    // "finish": kontroler wywoluje FinishGnss(true)
  }

  void Tick() override {
    if (!measuring_ || !session_) return;
    const double now = w_.NowSec();
    if (now < lastEpoch_) { lastEpoch_ = now; return; }           // skok czasu wstecz (np. wczytanie zapisu)
    for (int steps = 0; now - lastEpoch_ >= 1.0 && steps < 5; steps++) {     // najwyzej 5 epok na klatke
      lastEpoch_ += 1.0;
      session_->Step(lastEpoch_, w_.SkyHorizon());
    }
  }

  std::vector<std::string> StatusLines() const override {
    if (!measuring_ || !session_) {
      if (!poleUp_) return {};
      return {std::string("Tyczka stoi. ") + LV_KEYNAME_MENU + " - Rozpocznij pomiar punktu"};
    }
    const GnssFix& f = session_->Fix();
    char b[128]; std::vector<std::string> out;
    out.push_back(std::string("Rozwiazanie: ") + GnssSolName(f.type));
    std::snprintf(b, sizeof b, "Satelity: %d/%d  PDOP: %.1f", f.used, f.visible, f.pdop); out.push_back(b);
    std::snprintf(b, sizeof b, "Epoki: %d  RMS H/V: %.3f/%.3f m", f.epochs, f.hRmsMm / 1000.0, f.vRmsMm / 1000.0); out.push_back(b);
    std::snprintf(b, sizeof b, "Niebo zasloniete: %.0f%%  najblizsza przeszkoda: %.1f m", f.blockedPct, f.nearestObstacleM); out.push_back(b);
    if (f.type != SolType::Fix) out.push_back("Brak fixu: " + GnssFixHint(f));
    if (f.type == SolType::None) out.push_back("brak rozwiazania");
    else { std::snprintf(b, sizeof b, "N %.3f  E %.3f  H %.3f", f.n, f.e, f.h); out.push_back(b); }
    return out;
  }

  std::string Hud() const override {
    if (!poleUp_) return std::string("Postaw tyczke z odbiornikiem (") + LV_KEYNAME_MENU + " - menu)";
    if (measuring_) return CanStore() ? std::string("Mozna zapisac punkt (") + LV_KEYNAME_MENU + " - menu)" : std::string("Pomiar punktu trwa - czekaj na rozwiazanie");
    return points_.empty() ? std::string("Rozpocznij pomiar punktu (") + LV_KEYNAME_MENU + " - menu)"
                           : std::string("Punkt zapisany. Nastepny punkt lub zakoncz robote (") + LV_KEYNAME_MENU + " - menu)";
  }

  bool CanFinish() const override { return !points_.empty() && !measuring_; }

  GnssReport FinishGnss(bool complete) override {
    GnssReport r; r.complete = complete; r.showTrueError = opt_.showTrueError; r.points = points_;
    return r;
  }

  // Interfejs niwelacji: ta robota go nie uzywa.
  std::vector<int> ActiveRodPoints() const override { return {}; }
  int RodPointFor(RodRole) const override { return -1; }
  bool OnReading(int, int) override { return false; }
  FieldBook Finish(bool) override { return FieldBook{}; }

 private:
  static constexpr int kAntennaCount = 5;
  static constexpr double kAntenna[kAntennaCount] = {2.0, 2.1, 2.5, 1.5, 1.8};   // presety [m]; pierwszy = prawdziwa wysokosc tyczki

  bool CanControl() const {                                          // osnowa tylko z fixem i co najmniej 20 epokami
    return measuring_ && session_ && session_->Fix().type == SolType::Fix && session_->Fix().epochs >= 20;
  }

  bool CanStore() const {
    if (!measuring_ || !session_) return false;
    const GnssFix& f = session_->Fix();
    return (f.type == SolType::Fix && f.epochs >= 10) || (f.type != SolType::None && f.type != SolType::Fix && f.epochs >= 5);
  }

  void Store(const std::string& forcedNote = "") {
    const GnssFix& f = session_->Fix();
    GnssPoint p; p.name = "P" + std::to_string(points_.size() + 1); p.timeText = w_.TimeText();
    p.n = f.n; p.e = f.e; p.h = f.h; p.solution = GnssSolName(f.type); p.used = f.used; p.pdop = f.pdop;
    p.hRmsMm = f.hRmsMm; p.vRmsMm = f.vRmsMm; p.epochs = f.epochs; p.antennaM = kAntenna[antIdx_];
    p.note = !forcedNote.empty() ? forcedNote : f.type == SolType::Fix ? "" : "zapis bez fixu";
    if (opt_.showTrueError) {
      p.hasTrue = true; p.dNmm = (f.n - truth_.n) * 1000.0; p.dEmm = (f.e - truth_.e) * 1000.0; p.dHmm = (f.h - truth_.h) * 1000.0;
      p.d2Dmm = std::sqrt(p.dNmm * p.dNmm + p.dEmm * p.dEmm);
    }
    points_.push_back(p); measuring_ = false; session_.reset();
  }

  IWorld& w_; JobOptions opt_;
  bool poleUp_ = false, measuring_ = false;
  int antIdx_ = 0; double lastEpoch_ = 0; GridPos truth_{0, 0, 0};
  std::unique_ptr<GnssSession> session_;
  std::vector<GnssPoint> points_;
};

inline std::unique_ptr<Job> CreateGnssRover(IWorld& w, const JobOptions& o) { return std::make_unique<GnssRover>(w, o); }
