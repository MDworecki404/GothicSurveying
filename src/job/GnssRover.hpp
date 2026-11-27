// Robota "Pomiar GNSS (RTK rover)": tyczka z odbiornikiem, sesje punktow z symulacja jakosci (bez Gothic API).
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include "Job.hpp"
#include "Keys.hpp"

class GnssRover : public Job {
 public:
  // isStatic: osobna robota "Statyka GNSS (osnowa)": poprawki z sieci albo wlasnej bazy, sesja w minutach (1 s gry = 1 min obserwacji), min. 30 min do zapisu.
  GnssRover(IWorld& w, const JobOptions& o, bool isStatic = false) : w_(w), opt_(o), static_(isStatic) {}

  std::string Name() const override { return static_ ? "Statyka GNSS (osnowa)" : "Pomiar GNSS (RTK)"; }
  JobKind Kind() const override { return JobKind::Gnss; }

  std::vector<MenuItem> Menu() const override {
    char ant[64]; std::snprintf(ant, sizeof ant, "Wysokosc anteny: %.3f m (zmien)", kAntenna[antIdx_]);
    std::vector<MenuItem> items = {
      {"pole", static_ ? (poleUp_ ? "Przestaw odbiornik na statywie" : "Postaw odbiornik na statywie") : poleUp_ ? "Przestaw tyczke z odbiornikiem" : "Postaw tyczke z odbiornikiem", "nie w trakcie pomiaru punktu", !measuring_ || stakeOn_},
      {"fine", "Precyzyjne ustawienie tyczki (milimetry)", "najpierw postaw tyczke; nie w trakcie pomiaru punktu", poleUp_ && (!measuring_ || stakeOn_)},   // obsluguje kontroler
      {"antenna", ant, "nie w trakcie pomiaru punktu", !measuring_},
      {"prefix", std::string("Prefiks punktow: ") + kDetailPrefix[prefixIdx_] + " (zmien)", "T teren, B budynek, D droga, P plot, S slup, Z zielen, K krawedz", !measuring_},
      {"corr", ownBase_ ? "Poprawki RTK: wlasna baza (zmien)" : "Poprawki RTK: siec (zmien)", "nie w trakcie pomiaru punktu; wlasna baza = stawiasz stacje bazowa", !measuring_},
      {"base", baseUp_ ? "Przestaw baze (stacja bazowa)" : "Postaw baze (stacja bazowa)", "tylko przy poprawkach z wlasnej bazy; na punkcie osnowy (2 m) baza zna wspolrzedne", ownBase_ && !measuring_},
      {"start", "Rozpocznij pomiar punktu", "najpierw postaw tyczke", poleUp_ && !measuring_},
      {"store", "Zapisz punkt", static_ ? "fix i co najmniej 30 min sesji" : "fix i 10 epok (bez fixu: 5 epok z rozwiazaniem)", CanStore()},
      {"control", "Zaloz punkt osnowy", "fix i co najmniej 30 min sesji", CanControl()},
      {"design", design_.empty() ? "Wygeneruj punkty projektowe (budynek 10 x 6 m)" : "Wygeneruj nowe punkty projektowe (budynek 10 x 6 m)", "nie w trakcie pomiaru ani wytyczania; prostokat kilka metrow od ciebie, wspolrzedne na liscie wyboru", !measuring_},
      {"stake", stakeOn_ ? "Wytyczanie: punkt " + design_[target_].id + " (zmien)" : "Wytycz punkt projektowy", "najpierw tyczka z odbiornikiem i punkty projektowe; odbiornik pokazuje, w ktora strone przestawic tyczke", poleUp_ && !PendingIdx().empty() && (!measuring_ || stakeOn_)},
      {"confirm", "Zatwierdz wytyczony punkt", "fix RTK i odleglosc tyczki od punktu projektowego co najwyzej 3 cm", CanConfirm()},
      {"unstake", "Zakoncz wytyczanie", "trwa wytyczanie", stakeOn_},
      {"discard", "Odrzuc pomiar", "trwa pomiar punktu", measuring_},
      {"finish", "Zakoncz robote", "najpierw zapisz co najmniej jeden punkt", CanFinish()},
    };
    if (!static_) items.erase(std::remove_if(items.begin(), items.end(), [](const MenuItem& m) { return m.id == "control"; }), items.end());   // osnowa tylko ze statyki (nie z RTK)
    else items.erase(std::remove_if(items.begin(), items.end(), [](const MenuItem& m) { return m.id == "design" || m.id == "stake" || m.id == "confirm" || m.id == "unstake"; }), items.end());   // wytyczanie tylko w RTK
    return items;
  }

  bool NeedsChoice(const std::string& menuId) const override { return menuId == "stake"; }
  std::vector<MenuItem> TachyChoices(const std::string& menuId) const override {
    std::vector<MenuItem> out; char b[120];
    if (menuId == "stake") for (int i : PendingIdx()) { std::snprintf(b, sizeof b, "%s  N %.3f E %.3f", design_[i].id.c_str(), design_[i].n, design_[i].e); out.push_back({"stake:" + design_[i].id, b, "", true}); }
    return out;
  }

  void OnMenu(const std::string& fullId) override {
    const size_t colon = fullId.find(':');
    const std::string id = colon == std::string::npos ? fullId : fullId.substr(0, colon), arg = colon == std::string::npos ? "" : fullId.substr(colon + 1);
    for (const MenuItem& m : Menu()) if (m.id == id && !m.enabled) return;
    if (id == "design") {
      GridPos g{0, 0, 0};
      if (!w_.HeroGround(g)) { w_.Say("Brak gruntu pod stopami"); return; }
      const double n0 = std::round(g.n) + 5.0, e0 = std::round(g.e) + 5.0;           // prostokat 10 m (N) x 6 m (E), 5 m od bohatera
      design_ = {{"W1", n0, e0, false}, {"W2", n0 + 10.0, e0, false}, {"W3", n0 + 10.0, e0 + 6.0, false}, {"W4", n0, e0 + 6.0, false}};
      target_ = 0; w_.Say("Wygenerowano punkty projektowe W1-W4 (budynek 10 x 6 m); wybierz je w menu: Wytycz punkt projektowy");
    } else if (id == "stake") {
      int idx = -1; for (int i : PendingIdx()) if (design_[i].id == arg) idx = i;
      if (idx < 0) { w_.Say("Brak takiego punktu do wytyczenia"); return; }
      target_ = idx;
      if (!stakeOn_) { BeginSession(true); stakeOn_ = true; }
    } else if (id == "unstake") {
      stakeOn_ = false; measuring_ = false; session_.reset();
    } else if (id == "confirm") {
      Confirm();
    } else if (id == "pole") {
      if (!(static_ ? w_.PlaceGnssTripod() : w_.PlacePole())) { w_.Say("Brak gruntu pod stopami"); return; }
      poleUp_ = true;
    } else if (id == "corr") {
      ownBase_ = !ownBase_;
    } else if (id == "base") {
      GridPos g{0, 0, 0};
      if (!w_.PlaceBase(g)) { w_.Say("Brak gruntu pod stopami"); return; }
      baseTruth_ = g; baseUp_ = true;
      const bool known = !w_.ControlNearHero(2.0).empty();            // baza na znaku osnowy zna swoje wspolrzedne; na dowolnym punkcie ma blad pozycji autonomicznej
      GnssRng r((uint32_t)(w_.Random01() * 4e9) + 1); const double s = known ? 0.005 : 1.2;
      baseOff_[0] = s * r.Normal(); baseOff_[1] = s * r.Normal(); baseOff_[2] = 1.5 * s * r.Normal();
      baseNote_ = known ? "baza na punkcie osnowy" : "baza na dowolnym punkcie (wspolrzedne bazy z bledem)";
    } else if (id == "prefix") {
      prefixIdx_ = (prefixIdx_ + 1) % kDetailPrefixCount;
    } else if (id == "antenna") {
      antIdx_ = (antIdx_ + 1) % kAntennaCount;
    } else if (id == "start") {
      BeginSession(false);
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
      measuring_ = false; session_.reset(); stakeOn_ = false;
    }
    // "finish": kontroler wywoluje FinishGnss(true)
  }

  void Tick() override {
    if (!measuring_ || !session_) return;
    const double now = w_.NowSec();
    if (now < lastEpoch_) { lastEpoch_ = now; return; }           // skok czasu wstecz (np. wczytanie zapisu)
    for (int steps = 0; now - lastEpoch_ >= 1.0 && steps < 5; steps++) {     // najwyzej 5 epok na klatke
      lastEpoch_ += 1.0;
      virtT_ += static_ ? kStaticEpochSec : 1.0;                 // statyka: kazda sekunda gry to minuta obserwacji
      if (stakeOn_) { stakeTruth_ = w_.PoleGroundGrid(); session_->SetTruth(stakeTruth_); }   // tyczka mogla zostac przestawiona
      session_->Step(static_ ? virtT_ : lastEpoch_, w_.SkyHorizon());
    }
  }

  std::vector<std::string> StatusLines() const override {
    if (!measuring_ || !session_) {
      if (!poleUp_) return ownBase_ && !baseUp_ ? std::vector<std::string>{"Wlasna baza nie stoi - bez poprawek wynik autonomiczny"} : std::vector<std::string>{};
      return {std::string(static_ ? "Odbiornik stoi. " : "Tyczka stoi. ") + LV_KEYNAME_MENU + " - Rozpocznij pomiar punktu"};
    }
    const GnssFix& f = session_->Fix();
    char b[128]; std::vector<std::string> out;
    if (stakeOn_) {
      std::snprintf(b, sizeof b, "Rozwiazanie: %s  Satelity: %d/%d  PDOP: %.1f", GnssSolName(f.type), f.used, f.visible, f.pdop); out.push_back(b);
      const std::string g = FineGuide(w_.PoleGroundGrid()); out.push_back(g.empty() ? "Wytyczanie: brak rozwiazania" : g);
      const std::string hint = GnssFixHint(f);
      out.push_back(CanConfirm() ? std::string("Punkt wytyczony - ") + LV_KEYNAME_MENU + ": Zatwierdz wytyczony punkt" : f.type != SolType::Fix ? "Czekaj na fix RTK" + (hint.empty() ? std::string() : ": " + hint) : std::string("Przestaw tyczke (menu: Postaw / Precyzyjne ustawienie)"));
      return out;
    }
    out.push_back(std::string("Rozwiazanie: ") + GnssSolName(f.type) + (sessionStatic_ ? "  (statyka)" : "") + (sessionBase_ ? (baseUp_ ? "  [wlasna baza]" : "  [brak bazy]") : ""));
    std::snprintf(b, sizeof b, "Satelity: %d/%d  PDOP: %.1f", f.used, f.visible, f.pdop); out.push_back(b);
    if (sessionStatic_) std::snprintf(b, sizeof b, "Sesja: %d min (min. %d)  RMS H/V: %.3f/%.3f m", f.epochs, kStaticEpochs, f.hRmsMm / 1000.0, f.vRmsMm / 1000.0);
    else std::snprintf(b, sizeof b, "Epoki: %d  RMS H/V: %.3f/%.3f m", f.epochs, f.hRmsMm / 1000.0, f.vRmsMm / 1000.0);
    out.push_back(b);
    std::snprintf(b, sizeof b, "Niebo zasloniete: %.0f%%  najblizsza przeszkoda: %.1f m", f.blockedPct, f.nearestObstacleM); out.push_back(b);
    if (f.type != SolType::Fix) out.push_back("Brak fixu: " + GnssFixHint(f));
    if (f.type == SolType::None) out.push_back("brak rozwiazania");
    else { std::snprintf(b, sizeof b, "N %.3f  E %.3f  H %.3f", f.n, f.e, f.h); out.push_back(b); }
    return out;
  }

  std::string Hud() const override {
    if (!poleUp_) return std::string(static_ ? "Postaw odbiornik na statywie (" : "Postaw tyczke z odbiornikiem (") + LV_KEYNAME_MENU + " - menu)";
    if (stakeOn_) return "Wytyczanie " + design_[target_].id + ": ustaw tyczke wg wskazan odbiornika (" + std::string(LV_KEYNAME_MENU) + " - menu)";
    if (measuring_) return CanStore() ? std::string("Mozna zapisac punkt (") + LV_KEYNAME_MENU + " - menu)" : std::string("Pomiar punktu trwa - czekaj na rozwiazanie");
    return points_.empty() ? std::string("Rozpocznij pomiar punktu (") + LV_KEYNAME_MENU + " - menu)"
                           : std::string("Punkt zapisany. Nastepny punkt lub zakoncz robote (") + LV_KEYNAME_MENU + " - menu)";
  }

  bool CanFinish() const override { return !points_.empty() && !measuring_; }
  std::string NextStep() const override {
    if (!poleUp_) return "pole";
    if (stakeOn_) return CanConfirm() ? "confirm" : "";
    if (!measuring_) return "start";
    return CanStore() ? "store" : "";                  // w trakcie sesji czekamy na wystarczajaco epok
  }

  GnssReport FinishGnss(bool complete) override {
    GnssReport r; r.complete = complete; r.showTrueError = opt_.showTrueError; r.points = points_;
    return r;
  }

  // Interfejs niwelacji: ta robota go nie uzywa.
  std::vector<int> ActiveRodPoints() const override { return {}; }
  int RodPointFor(RodRole) const override { return -1; }
  bool OnReading(int, int) override { return false; }
  FieldBook Finish(bool) override { return FieldBook{}; }

 public:
  bool CanSaveState() const override { return true; }
  void SaveState(StateKV& s) const override {                                // trwajacy pomiar punktu (sesja) nie jest zapisywany: po wczytaniu trzeba go zaczac od nowa
    s.Bool("poleUp", poleUp_); s.Bool("ownBase", ownBase_); s.Bool("baseUp", baseUp_); s.Int("ant", antIdx_); s.Int("prefixIdx", prefixIdx_);
    s.Int("design.n", (long long)design_.size()); s.Int("target", target_);                    // wytyczanie w toku (sesja) nie jest zapisywane, lista punktow tak
    for (size_t i = 0; i < design_.size(); i++) { const std::string k = "design." + std::to_string(i); s.Str(k + ".id", design_[i].id); s.Num(k + ".n", design_[i].n); s.Num(k + ".e", design_[i].e); s.Bool(k + ".done", design_[i].done); }
    s.Num("base.n", baseTruth_.n); s.Num("base.e", baseTruth_.e); s.Num("base.h", baseTruth_.h);
    s.Num("off0", baseOff_[0]); s.Num("off1", baseOff_[1]); s.Num("off2", baseOff_[2]); s.Str("baseNote", baseNote_);
    PutVec(s, "points", points_, PutGnssPoint);
  }
  bool LoadState(const StateKV& s) override {
    poleUp_ = s.GetBool("poleUp"); ownBase_ = s.GetBool("ownBase"); baseUp_ = s.GetBool("baseUp"); antIdx_ = s.GetInt("ant") % kAntennaCount; prefixIdx_ = s.GetInt("prefixIdx", 3) % kDetailPrefixCount;
    design_.clear();
    for (int i = 0, nd = s.GetInt("design.n"); i < nd; i++) { const std::string k = "design." + std::to_string(i); design_.push_back({s.GetStr(k + ".id"), s.GetNum(k + ".n"), s.GetNum(k + ".e"), s.GetBool(k + ".done")}); }
    target_ = design_.empty() ? 0 : s.GetInt("target") % (int)design_.size(); stakeOn_ = false;
    baseTruth_ = GridPos{s.GetNum("base.n"), s.GetNum("base.e"), s.GetNum("base.h")};
    baseOff_[0] = s.GetNum("off0"); baseOff_[1] = s.GetNum("off1"); baseOff_[2] = s.GetNum("off2"); baseNote_ = s.GetStr("baseNote");
    points_ = GetVec<GnssPoint>(s, "points", GetGnssPoint);
    measuring_ = false; session_.reset();
    return true;
  }

 private:
  static constexpr int kStaticEpochs = 30;                 // minuty sesji (ksiazka: statyka co najmniej kilkadziesiat minut)
  static constexpr double kStaticEpochSec = 60.0;
  static constexpr int kAntennaCount = 5;
  static constexpr double kAntenna[kAntennaCount] = {2.0, 2.1, 2.5, 1.5, 1.8};   // presety [m]; pierwszy = prawdziwa wysokosc tyczki

  struct DesignPt { std::string id; double n, e; bool done; };
  std::vector<int> PendingIdx() const { std::vector<int> v; for (size_t i = 0; i < design_.size(); i++) if (!design_[i].done) v.push_back((int)i); return v; }

  void BeginSession(bool kinematic) {
    truth_ = w_.PoleGroundGrid();
    session_ = std::make_unique<GnssSession>((uint32_t)(w_.Random01() * 4e9), w_.NowSec(), truth_, kAntenna[antIdx_]);
    virtT_ = w_.NowSec();
    if (ownBase_ || static_)
      session_->SetBase(!ownBase_ || baseUp_, ownBase_ && baseUp_ ? std::hypot(truth_.n - baseTruth_.n, truth_.e - baseTruth_.e) : 0.0,
                        ownBase_ ? baseOff_[0] : 0.0, ownBase_ ? baseOff_[1] : 0.0, ownBase_ ? baseOff_[2] : 0.0, static_);
    session_->SetKinematic(kinematic); stakeTruth_ = truth_;
    sessionBase_ = ownBase_; sessionStatic_ = static_;
    lastEpoch_ = w_.NowSec(); measuring_ = true;
  }

  // Wskazanie odbiornika w miejscu tyczki: ostatni odczyt (jego blad wzgledem prawdy z epoki) przeniesiony na biezace polozenie tyczki.
  bool StakeEstimate(const GridPos& poleTruth, double& n, double& e) const {
    if (!stakeOn_ || !session_ || session_->Fix().type == SolType::None) return false;
    const GnssFix& f = session_->Fix();
    n = poleTruth.n + (f.n - stakeTruth_.n); e = poleTruth.e + (f.e - stakeTruth_.e);
    return true;
  }

 public:
  // Wskazowka dla gracza: o ile i w ktora strone przestawic tyczke (azymut wzgledem polnocy, rosnie ku wschodowi).
  std::string FineGuide(const GridPos& poleTruth) const override {
    double n, e;
    if (!StakeEstimate(poleTruth, n, e)) return "";
    const DesignPt& d = design_[target_];
    const double dn = d.n - n, de = d.e - e, dist = std::hypot(dn, de);
    double az = std::atan2(de, dn) * 180.0 / 3.14159265358979323846; if (az < 0) az += 360.0;
    static const char* const dir[8] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
    char b[110];
    if (dist < 0.0005) std::snprintf(b, sizeof b, "%s: dokladnie na punkcie", d.id.c_str());
    else std::snprintf(b, sizeof b, "%s: %.3f m na %s (az %.1f) dN %+.3f dE %+.3f", d.id.c_str(), dist, dir[(int)((az + 22.5) / 45.0) % 8], az, dn, de);
    return b;
  }

 private:
  bool CanConfirm() const {
    double n, e;
    if (!StakeEstimate(w_.PoleGroundGrid(), n, e) || session_->Fix().type != SolType::Fix) return false;
    return std::hypot(design_[target_].n - n, design_[target_].e - e) <= kStakeTolM;
  }

  void Confirm() {
    const GridPos pole = w_.PoleGroundGrid(); double n, e;
    if (!StakeEstimate(pole, n, e)) return;
    const DesignPt d = design_[target_]; truth_ = pole;
    const double devMm = std::hypot(d.n - n, d.e - e) * 1000.0, trueMm = std::hypot(d.n - pole.n, d.e - pole.e) * 1000.0;
    char note[160]; std::snprintf(note, sizeof note, "wytyczenie %s (projekt N %.3f E %.3f), odchylka wg odbiornika %.0f mm", d.id.c_str(), d.n, d.e, devMm);
    std::string nt = note;
    if (opt_.showTrueError) { char t[64]; std::snprintf(t, sizeof t, ", prawdziwa odchylka kolka %.0f mm", trueMm); nt += t; }
    const GridPos est{n, e, session_->Fix().h};
    Store(nt, d.id, &est);
    design_[target_].done = true;
    const std::vector<int> left = PendingIdx();
    if (left.empty()) { stakeOn_ = false; w_.Say("Wszystkie punkty projektowe wytyczone. Zakoncz robote"); return; }   // Store zamknal sesje
    target_ = left[0]; BeginSession(true);                                 // nastepny punkt: nowa sesja kinematyczna (stakeOn_ zostaje)
  }

  static constexpr double kStakeTolM = 0.03;

  bool CanControl() const {                                          // osnowa tylko ze statyki: fix i co najmniej 30 min
    return static_ && measuring_ && session_ && session_->Fix().type == SolType::Fix && session_->Fix().epochs >= kStaticEpochs;
  }

  bool CanStore() const {
    if (!measuring_ || !session_) return false;
    const GnssFix& f = session_->Fix();
    if (sessionStatic_) return f.type == SolType::Fix && f.epochs >= kStaticEpochs;          // statyka: tylko fix po pelnej sesji
    return (f.type == SolType::Fix && f.epochs >= 10) || (f.type != SolType::None && f.type != SolType::Fix && f.epochs >= 5);
  }

  void Store(const std::string& forcedNote = "", const std::string& name = "", const GridPos* est = nullptr) {   // name/est: wytyczanie (nazwa punktu projektowego, wskazanie w miejscu tyczki)
    const GnssFix& f = session_->Fix();
    const std::string pre = kDetailPrefix[prefixIdx_];
    int num = 1; for (const GnssPoint& q : points_) if (q.name.compare(0, pre.size(), pre) == 0) num++;   // numeracja osobno dla kazdego prefiksu
    GnssPoint p; p.name = name.empty() ? pre + std::to_string(num) : name; p.timeText = w_.TimeText();
    p.n = est ? est->n : f.n; p.e = est ? est->e : f.e; p.h = f.h; p.solution = GnssSolName(f.type); p.used = f.used; p.pdop = f.pdop;
    p.hRmsMm = f.hRmsMm; p.vRmsMm = f.vRmsMm; p.epochs = f.epochs; p.antennaM = kAntenna[antIdx_];
    p.note = !forcedNote.empty() ? forcedNote : f.type == SolType::Fix ? "" : "zapis bez fixu";
    auto add = [&](const std::string& t) { p.note += (p.note.empty() ? "" : "; ") + t; };
    if (sessionStatic_) add("statyka " + std::to_string(f.epochs) + " min");
    if (sessionBase_) add(baseUp_ ? baseNote_ + ", linia bazowa " + std::to_string((int)std::hypot(truth_.n - baseTruth_.n, truth_.e - baseTruth_.e)) + " m" : "bez bazy: wynik autonomiczny");
    if (opt_.showTrueError) {
      p.hasTrue = true; p.dNmm = (p.n - truth_.n) * 1000.0; p.dEmm = (p.e - truth_.e) * 1000.0; p.dHmm = (f.h - truth_.h) * 1000.0;
      p.d2Dmm = std::sqrt(p.dNmm * p.dNmm + p.dEmm * p.dEmm);
    }
    points_.push_back(p); measuring_ = false; session_.reset();
  }

  IWorld& w_; JobOptions opt_;
  bool poleUp_ = false, measuring_ = false, baseUp_ = false, sessionBase_ = false, sessionStatic_ = false;
  GridPos baseTruth_{0, 0, 0}; double baseOff_[3] = {0, 0, 0}; std::string baseNote_;
  const bool static_; bool ownBase_ = false;      // poprawki: siec (domyslnie, bez bazy: mozna zalozyc pierwsza osnowe) albo wlasna baza
  std::vector<DesignPt> design_; int target_ = 0; bool stakeOn_ = false; GridPos stakeTruth_{0, 0, 0};   // wytyczanie: punkty projektowe, biezacy cel, prawdziwe polozenie tyczki z ostatniej epoki
  int antIdx_ = 0, prefixIdx_ = 3; double lastEpoch_ = 0, virtT_ = 0; GridPos truth_{0, 0, 0};
  std::unique_ptr<GnssSession> session_;
  std::vector<GnssPoint> points_;
};

inline std::unique_ptr<Job> CreateGnssRover(IWorld& w, const JobOptions& o) { return std::make_unique<GnssRover>(w, o); }
inline std::unique_ptr<Job> CreateGnssStatic(IWorld& w, const JobOptions& o) { return std::make_unique<GnssRover>(w, o, true); }
