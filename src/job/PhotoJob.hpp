// Robota "Fotogrametria lotnicza" (bez Gothic API): ten sam nalot co ALS (obszar, plan, bateria, dron), ale sensorem jest kamera. Zdjecia powstaja przez rzucanie promieni
// przez piksele z prawdziwej pozycji drona (perspektywa, przeslonienia, kolory z tekstur), zapisywane jako PNG; pozycje i katy z GNSS/IMU z szumem trafiaja do zdjecia.csv.
// Punkty wiazace sa symulowane (znane punkty terenu rzutowane na zdjecia z szumem 0,5 px i sprawdzana widocznoscia), osnowa daje punkty stale (GCP) i kontrolne;
// po locie wyrownanie wiazki (calc/Bundle.hpp) i raport. Zapis stanu: tylko ustawienia i obszar (bez lotu w toku).
#pragma once
#include <set>
#include "AerialJob.hpp"
#include "calc/Bundle.hpp"
#include "calc/Camera.hpp"
#include "calc/Ortho.hpp"
#include "calc/Png.hpp"

struct PhotoStats {
  int photos = 0, ties = 0, obs = 0, gcp = 0, checks = 0; bool adjusted = false;
  double rmsBefore = 0, rmsAfter = 0, checkRmse3d = 0, posCorrRms = 0;      // rms reprojekcji [px]; blad 3D na punktach kontrolnych [m]; rms poprawki pozycji zdjec [m]
};

struct OrthoInfo {                                                         // wynik ortofotomapy i kontrola planimetryczna na punktach osnowy
  int w = 0, h = 0, used = 0, gcpN = 0, checkN = 0; double gsd = 0, coverage = 0, rmseGcp = 0, rmseCheck = 0;
};

class PhotoJob : public AerialJob {
 public:
  PhotoJob(IWorld& w, const JobOptions& o) : AerialJob(w, o) { ApplySensor(); Replan(); }

  std::string Name() const override { return "Fotogrametria lotnicza"; }
  JobKind Kind() const override { return JobKind::Photo; }
  const PhotoStats& Stats() const { return st_; }
  const OrthoInfo& OrthoStats() const { return oi_; }
  bool CanSaveState() const override { return !flying_; }

  std::vector<MenuItem> Menu() const override {
    std::vector<MenuItem> out; char cam[96], res[96], fw[96], sd[96];
    std::snprintf(cam, sizeof cam, "Kamera: %s (zmien)", kCameras[camIdx_].name); std::snprintf(res, sizeof res, "Rozdzielczosc zdjec: %s (zmien)", kImageRes[resIdx_].name);
    std::snprintf(fw, sizeof fw, "Zakladka podluzna: %.0f%% (zmien)", fwd_); std::snprintf(sd, sizeof sd, "Zakladka poprzeczna: %.0f%% (zmien)", side_);
    for (const MenuItem& m : AerialJob::Menu()) {
      if (LidarOnly(m.id)) continue;
      if (m.id == "battery") {
        out.push_back({"cam", cam, "kat widzenia obiektywu: 84 / 63 / 40 st.", !flying_}); out.push_back({"res", res, "wieksza rozdzielczosc = dluzej w grze", !flying_});
        out.push_back({"fwd", fw, "70 / 80 / 90 %", !flying_}); out.push_back({"side", sd, "40 / 60 / 70 %", !flying_});
      }
      out.push_back(m);
      if (m.id == "cancel") {
        out.push_back({"ortho", "Ortofotomapa ze zdjec", "po wyrownaniu wiazki: PNG z plikiem .pgw (uklad siatki), NMPT z punktow, wybor zdjecia najblizej nadiru", CanOrtho()});
        out.push_back({"ortho_cancel", "Przerwij ortofotomape", "trwa generowanie", ortho_ != nullptr});
      }
    }
    return out;
  }
  bool CanFinish() const override { return AerialJob::CanFinish() && !ortho_; }
  std::string NextStep() const override { return ortho_ ? "" : AerialJob::NextStep(); }
  std::string PostFlightStep() const override { return CanOrtho() && oi_.w == 0 ? "ortho" : ""; }   // wyrownanie wiazki jest automatyczne; zostaje ortofotomapa
  void Tick() override { if (ortho_) { if (ortho_->Step(kBudgetMs)) FinishOrtho(false); return; } AerialJob::Tick(); }
  void OnMenu(const std::string& id) override {
    if (LidarOnly(id)) return;
    if (id == "ortho") { StartOrtho(); return; }
    if (id == "ortho_cancel") { FinishOrtho(true); return; }
    if (ortho_) return;                                                     // w trakcie generowania ortofotomapy reszta menu nieaktywna
    if (id == "cam" || id == "res" || id == "fwd" || id == "side") {
      if (flying_) return;
      if (id == "cam") camIdx_ = (camIdx_ + 1) % 3; else if (id == "res") resIdx_ = (resIdx_ + 1) % 3;
      else if (id == "fwd") fwd_ = fwd_ >= 90 ? 70 : fwd_ + 10; else side_ = side_ >= 70 ? 40 : (side_ < 60 ? 60 : 70);
      Replan(); return;
    }
    AerialJob::OnMenu(id);
  }
  std::string Hud() const override {
    char b[260];
    if (ortho_) { std::snprintf(b, sizeof b, "Ortofotomapa: %s", ortho_->Progress().c_str()); return b; }
    if (flying_) { std::snprintf(b, sizeof b, "Fotogrametria: zdjec: %d, szereg %d/%d, %d%%", (int)photos_.size(), CurStrip() + 1, (int)plan_.strips.size(), (int)(100.0 * tSim_ / (std::max)(1e-9, path_->Duration()))); return b; }
    if (!plan_.ok) { std::snprintf(b, sizeof b, "Fotogrametria: obszar %d wierzcholkow (min. 3)", (int)area_.size()); return b; }
    int n = 0; for (const Strip& s : plan_.strips) n += (int)std::ceil(s.lengthM / (std::max)(0.1, geom_.baseM)) + 1;
    std::snprintf(b, sizeof b, "Fotogrametria: szeregow: %d, ok. %d zdjec, GSD %.1f cm (odpowiednik 20 MP: %.1f cm), zakladki %.0f/%.0f%%, lot %.1f min%s",
                  (int)plan_.strips.size(), n, geom_.gsdM * 100.0, geom_.gsdRealM * 100.0, fwd_, side_, plan_.flightTimeS / 60.0, plan_.sorties > 1 ? ", z wymiana baterii" : "");
    return b;
  }
  std::vector<std::string> SummaryLines() const override {
    std::vector<std::string> out; char b[240];
    std::snprintf(b, sizeof b, "Lotow: %d, ostatni: %d zdjec%s", flights_, st_.photos, outDir_.empty() ? "" : (", folder: " + outDir_).c_str()); out.push_back(b);
    if (st_.adjusted) { std::snprintf(b, sizeof b, "Wyrownanie wiazki: rms reprojekcji %.2f -> %.2f px, blad na punktach kontrolnych %.3f m (%d), GCP %d", st_.rmsBefore, st_.rmsAfter, st_.checkRmse3d, st_.checks, st_.gcp); out.push_back(b); }
    return out;
  }
  void SaveState(StateKV& s) const override { AerialJob::SaveState(s); s.Bool("photo", true); s.Int("photo.cam", camIdx_); s.Int("photo.res", resIdx_); s.Num("photo.fwd", fwd_); s.Num("photo.side", side_); }
  bool LoadState(const StateKV& s) override {
    if (!s.GetBool("photo") || !AerialJob::LoadState(s)) return false;
    adjOk_ = false; camIdx_ = s.GetInt("photo.cam") % 3; resIdx_ = s.GetInt("photo.res") % 3; fwd_ = s.GetNum("photo.fwd", 80); side_ = s.GetNum("photo.side", 60); Replan(); return true;
  }

 protected:
  struct Photo { TrajSample truth, est; int strip; double t; };
  struct Ctl { std::string id; Vec3 truth, known; bool gcp; };            // punkt osnowy: prawdziwa pozycja (lokalna), wspolrzedne znane z osnowy (lokalne), rola
  static bool LidarOnly(const std::string& id) { for (const char* k : {"preset", "calib", "adjust", "adjust_cancel", "products", "products_cancel", "sidelap"}) if (id == k) return true; return false; }

  void ApplySensor() override {                                            // plan: kat widzenia = szerokosc pasa, zakladka poprzeczna, czestotliwosc zdjec = predkosc / baza
    geom_ = PhotoGeometry(kCameras[camIdx_], kImageRes[resIdx_], params_.aglM, fwd_, side_, params_.speedMps);
    params_.sensor.fovDeg = kCameras[camIdx_].hfovDeg; params_.sidelapPct = side_; params_.sensor.pulseHz = 1.0 / geom_.intervalS; params_.sensor.lineHz = 1.0;
  }
  bool ShotBusy() const override { return busy_; }

  void EmitPulse() override {                                              // zdjecie: pozycja zapamietana w chwili wyzwolenia, renderowanie w kolejnych klatkach (czas lotu stoi)
    if (!setup_) Setup();
    const double f = (tSim_ - t0_) / kSampleDt; shot_.truth = Lerp(s0_, s1_, f); shot_.est = Lerp(e0_, e1_, f); shot_.strip = s0_.strip; shot_.t = tSim_ + clockOffset_;
    const kImageRes_t& r = kImageRes[resIdx_]; W_ = r.w; H_ = r.h; img_.assign((size_t)W_ * H_ * 3, 0); row_ = 0; busy_ = true;
    const Rot3 R = RotFromRpy(shot_.truth.rollDeg, shot_.truth.pitchDeg, shot_.truth.yawDeg); M_ = MulR(R, boreTrue_); const Vec3 o = Mul(R, lever_);
    org_ = {shot_.truth.pos.x + o.x, shot_.truth.pos.y + o.y, shot_.truth.pos.z + o.z};
  }
  void StepShot(std::chrono::steady_clock::time_point t0) override {
    const bool unlimited = pulseCap_ > 0; const double f = geom_.focalPx, cx = W_ / 2.0, cy = H_ / 2.0; const double pi = 3.14159265358979323846;
    while (row_ < H_) {
      for (int x = 0; x < W_; x++) {
        Vec3 d = PixelRayBody(x + 0.5, row_ + 0.5, f, cx, cy); const double l = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z); d = Mul(M_, Vec3{d.x / l, d.y / l, d.z / l});
        double az = std::atan2(d.x, d.y) * 180.0 / pi; if (az < 0) az += 360.0; const double el = std::asin((std::max)(-1.0, (std::min)(1.0, d.z))) * 180.0 / pi;
        double dist = 0, cosInc = 1; bool vob = false; unsigned rgb = 0xA0C8F0;                               // niebo, gdy promien nic nie trafia
        if (!w_.AerialRay(GridPos{org_.y, org_.x, org_.z}, az, el, kMaxRangeM, dist, cosInc, vob, &rgb)) rgb = 0xA0C8F0;
        unsigned char* p = &img_[((size_t)row_ * W_ + x) * 3]; p[0] = (unsigned char)((rgb >> 16) & 255); p[1] = (unsigned char)((rgb >> 8) & 255); p[2] = (unsigned char)(rgb & 255);
      }
      row_++;
      if (!unlimited && std::chrono::steady_clock::now() - t0 > std::chrono::milliseconds(kBudgetMs)) return;
    }
    FinishShot();
  }

  void Setup() {                                                           // poczatek lotu: folder zdjec, punkty wiazace (znane punkty terenu) i punkty osnowy
    setup_ = true; adjOk_ = false; fgeom_ = geom_; photos_.clear(); obs_.clear(); feats_.clear(); ctl_.clear(); st_ = PhotoStats{}; std::error_code ec;
    photoDir_ = outDir_ + "/zdjecia"; std::filesystem::create_directories(photoDir_, ec);
    double nmin = 1e18, nmax = -1e18, emin = 1e18, emax = -1e18; for (const PlanPt& a : area_) { nmin = (std::min)(nmin, a.n); nmax = (std::max)(nmax, a.n); emin = (std::min)(emin, a.e); emax = (std::max)(emax, a.e); }
    const double step = (std::max)(4.0, std::sqrt((nmax - nmin) * (emax - emin) / 700.0)); GnssRng fr(seed_ ^ 0xFEEDu);
    for (double n = nmin + step / 2; n < nmax; n += step) for (double e = emin + step / 2; e < emax; e += step) {
      const double jn = n + (fr.Uniform() - 0.5) * step * 0.6, je = e + (fr.Uniform() - 0.5) * step * 0.6; if (!InArea(jn, je)) continue;
      double h = homeGroundH_; w_.GroundHeightM(jn, je, h); feats_.push_back({je - originE_, jn - originN_, h - originH_});
    }
    std::vector<ControlPoint> cps; for (const ControlPoint& c : w_.ControlNearHero(1e9)) if (InArea(c.n, c.e)) cps.push_back(c);
    std::sort(cps.begin(), cps.end(), [](const ControlPoint& a, const ControlPoint& b) { return a.id < b.id; });
    for (size_t i = 0; i < cps.size(); i++) {
      const GridPos tg = GnssToGrid(cps[i].x, cps[i].y, cps[i].z);                                              // prawdziwe polozenie znaku (gra), znane = wspolrzedne z osnowy
      ctl_.push_back({cps[i].id, Vec3{tg.e - originE_, tg.n - originN_, tg.h - originH_}, Vec3{cps[i].e - originE_, cps[i].n - originN_, cps[i].h - originH_}, cps.size() < 2 || i % 2 == 0});
    }
    if (std::filesystem::exists(outDir_ + "/lot.raw")) { /* plik roboczy LiDAR nieuzywany; usuwany po locie */ }
  }

  void FinishShot() {
    busy_ = false; char b[64]; std::snprintf(b, sizeof b, "/IMG_%04d.png", (int)photos_.size() + 1);
    if (!WritePng(photoDir_ + b, W_, H_, img_)) w_.Say("Nie mozna zapisac zdjecia");
    Photo p = shot_; photos_.push_back(p); Observe((int)photos_.size() - 1);
  }
  // Obserwacje piksela: znane punkty (wiazace i osnowa) rzutowane z prawdziwej pozycji, z widocznoscia (promien) i szumem pomiaru.
  void Observe(int pi) {
    const Photo& ph = photos_[pi]; const double f = geom_.focalPx, cx = W_ / 2.0, cy = H_ / 2.0; const Rot3 R = RotFromRpy(ph.truth.rollDeg, ph.truth.pitchDeg, ph.truth.yawDeg);
    const Vec3 lo = Mul(R, lever_); const Vec3 cam{ph.truth.pos.x + lo.x, ph.truth.pos.y + lo.y, ph.truth.pos.z + lo.z};
    auto visit = [&](int idx, const Vec3& local, double sigma) {
      const Vec3 P{local.x + originE_, local.y + originN_, local.z + originH_}; double u, v;
      if (!ProjectWorld(P, ph.truth, boreTrue_, lever_, f, cx, cy, u, v) || u < 2 || v < 2 || u > W_ - 2 || v > H_ - 2) return;
      const double dx = cam.x - P.x, dy = cam.y - P.y, dz = cam.z - P.z, l = std::sqrt(dx * dx + dy * dy + dz * dz); if (l < 1.0) return;
      const Vec3 tg{P.x + dx / l * 0.5, P.y + dy / l * 0.5, P.z + dz / l * 0.5};                              // punkt tuz nad powierzchnia: promien do niego nie moze byc zaslonieciem
      if (w_.RouteBlocked(GridPos{cam.y, cam.x, cam.z}, GridPos{tg.y, tg.x, tg.z})) return;
      obs_.push_back({pi, idx, u + pixRng_.Normal() * sigma, v + pixRng_.Normal() * sigma});
    };
    for (size_t k = 0; k < feats_.size(); k++) visit((int)k, feats_[k], 0.5);
    for (size_t k = 0; k < ctl_.size(); k++) visit((int)(feats_.size() + k), ctl_[k].truth, 0.7);
  }

  long long ProduceResults(bool complete, bool keepRaw) override {
    (void)keepRaw; std::error_code ec; std::filesystem::remove(outDir_ + "/lot.raw", ec);
    st_.photos = (int)photos_.size(); st_.ties = (int)feats_.size();
    WriteCsv();
    Adjust(complete);
    char b[200];
    if (st_.photos == 0) w_.Say("Fotogrametria: brak zdjec");
    else if (st_.adjusted) { std::snprintf(b, sizeof b, "Fotogrametria zakonczona: %d zdjec, rms reprojekcji %.2f -> %.2f px, blad na punktach kontrolnych %.3f m", st_.photos, st_.rmsBefore, st_.rmsAfter, st_.checkRmse3d); w_.Say(b); }
    else { std::snprintf(b, sizeof b, "Fotogrametria %s: %d zdjec (bez wyrownania)", complete ? "zakonczona" : "przerwana", st_.photos); w_.Say(b); }
    setup_ = false; busy_ = false; return st_.photos;
  }

  void WriteCsv() {
    std::ofstream f(outDir_ + "/zdjecia.csv"); f << "id;plik;t_s;szereg;E_gnss;N_gnss;H_gnss;roll_imu;pitch_imu;yaw_imu" << (opt_.showTrueError ? ";E_prawdziwe;N_prawdziwe;H_prawdziwe;roll;pitch;yaw" : "") << "\n"; char b[400];
    for (size_t i = 0; i < photos_.size(); i++) {
      const Photo& p = photos_[i]; const TrajSample &e = p.est, &t = p.truth;
      int n = std::snprintf(b, sizeof b, "%d;IMG_%04d.png;%.2f;%d;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f", (int)i + 1, (int)i + 1, p.t, p.strip, e.pos.x, e.pos.y, e.pos.z, e.rollDeg, e.pitchDeg, e.yawDeg);
      if (opt_.showTrueError) n += std::snprintf(b + n, sizeof b - n, ";%.3f;%.3f;%.3f;%.3f;%.3f;%.3f", t.pos.x, t.pos.y, t.pos.z, t.rollDeg, t.pitchDeg, t.yawDeg);
      f << b << "\n";
    }
  }

  // Wyrownanie wiazki: poza zdjec z GNSS/IMU (a-priori), punkty wiazace wolne, osnowa: GCP stale i punkty kontrolne wolne; raport i plik z poprawionymi pozami.
  void Adjust(bool complete) {
    std::ofstream rep(outDir_ + "/fotogrametria_raport.txt"); char b[360];
    rep << "Fotogrametria lotnicza\nLot: " << (complete ? "zakonczony" : "przerwany") << "\n";
    const PhotoGeom g = geom_; const ImageRes& res = kImageRes[resIdx_];
    std::snprintf(b, sizeof b, "Kamera: %s, zdjecia %d x %d; AGL %.0f m, predkosc %.0f m/s\nGSD: %.1f cm (renderowane), %.1f cm (odpowiednik matrycy 20 MP); pas %.0f x %.0f m\nZakladka podluzna %.0f%%, poprzeczna %.0f%%; baza %.1f m, odstep szeregow %.1f m\n",
                  kCameras[camIdx_].name, res.w, res.h, params_.aglM, params_.speedMps, g.gsdM * 100, g.gsdRealM * 100, g.swathM, g.alongM, fwd_, side_, g.baseM, g.spacingM); rep << b;
    rep << "Profil bledow GNSS/IMU: " << profile_used_.name << "\n";
    const int nc = (int)photos_.size(); std::vector<int> cnt(feats_.size() + ctl_.size(), 0); for (const BaObs& o : obs_) cnt[o.pt]++;
    std::vector<BaObs> use; for (const BaObs& o : obs_) if (cnt[o.pt] >= 2) use.push_back(o);
    if (nc < 3 || use.size() < 30) { rep << "Wyrownanie wiazki: za malo zdjec albo obserwacji (zdjec " << nc << ", obserwacji " << use.size() << ") - pominiete\n"; return; }
    std::vector<BaCam> cams(nc), prior(nc), truth(nc);
    for (int i = 0; i < nc; i++) {
      const TrajSample &e = photos_[i].est, &t = photos_[i].truth;
      BaCam c; c.p[0] = e.pos.x - originE_; c.p[1] = e.pos.y - originN_; c.p[2] = e.pos.z - originH_; c.a[0] = e.rollDeg; c.a[1] = e.pitchDeg; c.a[2] = e.yawDeg; cams[i] = prior[i] = c;
      BaCam q; q.p[0] = t.pos.x - originE_; q.p[1] = t.pos.y - originN_; q.p[2] = t.pos.z - originH_; q.a[0] = t.rollDeg; q.a[1] = t.pitchDeg; q.a[2] = t.yawDeg; truth[i] = q;
    }
    GnssRng ir(seed_ ^ 0xC0DEu); std::vector<BaPoint> pts;                                                       // poczatkowe 3D: znane polozenie z bledem 1 m (zastepuje triangulacje)
    for (const Vec3& v : feats_) pts.push_back({Vec3{v.x + ir.Normal(), v.y + ir.Normal(), v.z + ir.Normal()}, false});
    for (const Ctl& c : ctl_) { if (c.gcp) pts.push_back({c.known, true}); else pts.push_back({Vec3{c.known.x + ir.Normal(), c.known.y + ir.Normal(), c.known.z + ir.Normal()}, false}); }
    BaParams bp; bp.f = g.focalPx; bp.cx = W0() / 2.0; bp.cy = H0() / 2.0; bp.lever = lever_; bp.Rb = RotFromRpy(0, 0, 0);
    const ErrorProfile pr = profile_used_; bp.sigmaPx = 0.6; bp.sigmaPos = (std::max)(0.1, 2.0 * pr.gnssHSigmaM + 0.5 * pr.driftMPerMin); bp.sigmaAtt = 1.0;
    const BaResult r = BundleAdjust(cams, prior, pts, use, bp);
    if (!r.ok) { rep << "Wyrownanie wiazki nie powiodlo sie: " << r.msg << "\n"; return; }
    adjOk_ = true; adjCams_ = cams; adjPts_ = pts; useObs_ = use;
    st_.adjusted = true; st_.obs = (int)use.size(); st_.rmsBefore = r.rmsBefore; st_.rmsAfter = r.rmsAfter; for (const Ctl& c : ctl_) { if (c.gcp) st_.gcp++; else st_.checks++; }
    double cs = 0; for (int i = 0; i < nc; i++) cs += std::pow(cams[i].p[0] - prior[i].p[0], 2) + std::pow(cams[i].p[1] - prior[i].p[1], 2) + std::pow(cams[i].p[2] - prior[i].p[2], 2); st_.posCorrRms = std::sqrt(cs / nc);
    double ce = 0; int cn = 0, nf = (int)feats_.size();
    for (size_t k = 0; k < ctl_.size(); k++) if (!ctl_[k].gcp) { const Vec3 &a = pts[nf + k].x, &t = ctl_[k].known; ce += std::pow(a.x - t.x, 2) + std::pow(a.y - t.y, 2) + std::pow(a.z - t.z, 2); cn++; }
    st_.checkRmse3d = cn ? std::sqrt(ce / cn) : 0;
    std::snprintf(b, sizeof b, "Zdjec: %d, punktow wiazacych: %d, obserwacji: %d, GCP: %d, punkty kontrolne: %d\nrms reprojekcji [px]: przed %.2f, po wyrownaniu %.2f (iteracji %d)\nrms poprawki pozycji zdjec wzgledem GNSS: %.3f m\n",
                  nc, st_.ties, st_.obs, st_.gcp, st_.checks, r.rmsBefore, r.rmsAfter, r.iters, st_.posCorrRms); rep << b;
    if (cn) { std::snprintf(b, sizeof b, "Blad na punktach kontrolnych (wolne punkty osnowy, nie uzyte jako wiezy): rms 3D %.3f m (%d punktow)\n", st_.checkRmse3d, cn); rep << b; }
    else rep << "Brak punktow kontrolnych (potrzebne co najmniej 2 punkty osnowy w obszarze: pierwszy GCP, drugi kontrolny)\n";
    double m[3] = {0, 0, 0}, m2[3] = {0, 0, 0};                                                                  // poprawka montazu kamery = srednia roznica IMU - wyrownanie
    for (int i = 0; i < nc; i++) for (int k = 0; k < 3; k++) { double d = prior[i].a[k] - cams[i].a[k]; if (k == 2) { d = std::fmod(d, 360.0); if (d > 180) d -= 360; if (d < -180) d += 360; } m[k] += d; m2[k] += d * d; }
    std::snprintf(b, sizeof b, "Boresight kamery (srednia roznica IMU - wyrownanie, st.): roll %+.3f, pitch %+.3f, yaw %+.3f (rozrzut %.3f %.3f %.3f)\n", m[0] / nc, m[1] / nc, m[2] / nc,
                  std::sqrt((std::max)(0.0, m2[0] / nc - m[0] * m[0] / nc / nc)), std::sqrt((std::max)(0.0, m2[1] / nc - m[1] * m[1] / nc / nc)), std::sqrt((std::max)(0.0, m2[2] / nc - m[2] * m[2] / nc / nc))); rep << b;
    if (opt_.showTrueError) {
      double e0 = 0, e1 = 0; for (int i = 0; i < nc; i++) { for (int k = 0; k < 3; k++) { e0 += std::pow(prior[i].p[k] - truth[i].p[k], 2); e1 += std::pow(cams[i].p[k] - truth[i].p[k], 2); } }
      TrajectoryNoise tn(seed_, pr); double br, bpi, by, ir2, ip, iy; tn.BoresightDeg(br, bpi, by); tn.BiasDeg(ir2, ip, iy);
      std::snprintf(b, sizeof b, "Prawdziwy blad pozycji zdjec: GNSS %.3f m -> po wyrownaniu %.3f m (rms 3D)\nPrawdziwy boresight kamery: %+.3f %+.3f %+.3f st.; bias IMU: %+.3f %+.3f %+.3f st.; oczekiwana poprawka (bias - boresight): %+.3f %+.3f %+.3f\n",
                    std::sqrt(e0 / nc), std::sqrt(e1 / nc), br, bpi, by, ir2, ip, iy, ir2 - br, ip - bpi, iy - by); rep << b;
    }
    std::ofstream cf(outDir_ + "/fotogrametria_wyrownanie.csv"); cf << "id;plik;E;N;H;roll;pitch;yaw;dE;dN;dH\n";
    for (int i = 0; i < nc; i++) { std::snprintf(b, sizeof b, "%d;IMG_%04d.png;%.3f;%.3f;%.3f;%.4f;%.4f;%.4f;%+.3f;%+.3f;%+.3f\n", i + 1, i + 1, cams[i].p[0] + originE_, cams[i].p[1] + originN_, cams[i].p[2] + originH_, cams[i].a[0], cams[i].a[1], cams[i].a[2],
                                    cams[i].p[0] - prior[i].p[0], cams[i].p[1] - prior[i].p[1], cams[i].p[2] - prior[i].p[2]); cf << b; }
  }
  // Ortofotomapa ze zdjec: NMPT z punktow po wyrownaniu, kolor z zdjecia najblizej nadiru przez poprawiona poze; wynik PNG + .pgw w ukladzie siatki.
  bool CanOrtho() const { return adjOk_ && !flying_ && !ortho_ && !adj_ && !prod_; }
  void StartOrtho() {
    if (!CanOrtho()) return;
    OrthoParams p; p.f = fgeom_.focalPx; p.imgW = W_; p.imgH = H_; p.cx = W_ / 2.0; p.cy = H_ / 2.0; p.lever = lever_; p.Rb = RotFromRpy(0, 0, 0); p.gsd = fgeom_.gsdM; p.originE = originE_; p.originN = originN_;
    double nmin = 1e18, nmax = -1e18, emin = 1e18, emax = -1e18;
    for (const PlanPt& a : area_) { nmin = (std::min)(nmin, a.n - originN_); nmax = (std::max)(nmax, a.n - originN_); emin = (std::min)(emin, a.e - originE_); emax = (std::max)(emax, a.e - originE_); p.polygon.push_back({a.n - originN_, a.e - originE_}); }
    p.minN = nmin; p.maxN = nmax; p.minE = emin; p.maxE = emax;
    std::vector<OrthoCam> cams; for (const BaCam& c : adjCams_) { OrthoCam o; for (int k = 0; k < 3; k++) { o.p[k] = c.p[k]; o.a[k] = c.a[k]; } cams.push_back(o); }
    std::vector<Vec3> pts; for (const BaPoint& q : adjPts_) pts.push_back(q.x);
    const std::string dir = photoDir_;
    ortho_ = std::make_unique<Orthorectifier>(p, cams, pts, [dir](int c, std::vector<unsigned char>& rgb) { char b[48]; std::snprintf(b, sizeof b, "/IMG_%04d.png", c + 1); int w, h; return ReadPng(dir + b, w, h, rgb); },
                                              outDir_ + "/ortofotomapa.png", outDir_ + "/ortofotomapa.pgw");
    orthoP_ = p; w_.Say("Ortofotomapa: start (NMPT, wybor zdjec, kolory)");
  }
  void FinishOrtho(bool cancelled) {
    if (!ortho_) return;
    const OrthoResult r = ortho_->Result(); ortho_.reset();
    if (cancelled) { w_.Say("Ortofotomapa przerwana"); return; }
    if (!r.ok) { w_.Say(r.msg.empty() ? "Nie udalo sie wygenerowac ortofotomapy" : r.msg); return; }
    oi_ = OrthoInfo{}; oi_.w = r.w; oi_.h = r.h; oi_.used = r.used; oi_.gsd = r.gsd; oi_.coverage = r.coverage;
    const int nf = (int)feats_.size(); double sg = 0, sc = 0, tg = 0, tc = 0; int ng = 0, nk = 0; std::set<int> gp, kp;                    // blad planimetryczny: promien przez zmierzony piksel znaku, poprawiona poza, plaszczyzna H znaku
    for (const BaObs& o : useObs_) {
      if (o.pt < nf) continue;
      const Ctl& c = ctl_[o.pt - nf]; OrthoCam oc; for (int k = 0; k < 3; k++) { oc.p[k] = adjCams_[o.cam].p[k]; oc.a[k] = adjCams_[o.cam].a[k]; }
      double dE, dN; if (!OrthoPointError(orthoP_, oc, o.u, o.v, c.known, dE, dN)) continue;
      const double e2 = dE * dE + dN * dN, t2 = std::pow(dE + c.known.x - c.truth.x, 2) + std::pow(dN + c.known.y - c.truth.y, 2);
      if (c.gcp) { sg += e2; tg += t2; ng++; gp.insert(o.pt); } else { sc += e2; tc += t2; nk++; kp.insert(o.pt); }
    }
    oi_.gcpN = (int)gp.size(); oi_.checkN = (int)kp.size(); oi_.rmseGcp = ng ? std::sqrt(sg / ng) : 0; oi_.rmseCheck = nk ? std::sqrt(sc / nk) : 0;
    std::ofstream f(outDir_ + "/ortofotomapa_raport.txt"); char b[300];
    std::snprintf(b, sizeof b, "Ortofotomapa ze zdjec\nObraz: %d x %d px, GSD %.3f m (zdjecia: %.3f m), pokrycie %.1f%% obszaru, uzytych zdjec: %d\n", r.w, r.h, r.gsd, fgeom_.gsdM, r.coverage * 100.0, r.used); f << b;
    f << "Uklad: plik ortofotomapa.pgw (siatka N/E, srodek lewego gornego piksela), gora obrazu = polnoc.\n";
    if (nk) { std::snprintf(b, sizeof b, "RMSE planimetryczne na punktach kontrolnych (nie uzytych jako wiezy): %.3f m (%d punktow, %d pomiarow)\n", oi_.rmseCheck, oi_.checkN, nk); f << b; }
    if (ng) { std::snprintf(b, sizeof b, "RMSE planimetryczne na punktach stalych GCP: %.3f m (%d punktow, %d pomiarow)\n", oi_.rmseGcp, oi_.gcpN, ng); f << b; }
    if (!nk && !ng) f << "Brak punktow osnowy w obszarze: bez kontroli planimetrycznej.\n";
    if (opt_.showTrueError && (nk || ng)) { std::snprintf(b, sizeof b, "Wzgledem prawdziwego polozenia znakow: punkty kontrolne %.3f m, GCP %.3f m (zawiera blad wspolrzednych osnowy)\n", nk ? std::sqrt(tc / nk) : 0.0, ng ? std::sqrt(tg / ng) : 0.0); f << b; }
    f << "NMPT z punktow wiazacych (interpolacja wazona odleglosci); budynki i drzewa wygladzone, bez wykrywania przeslonic i bez wyrownania kolorow miedzy zdjeciami.\n";
    std::snprintf(b, sizeof b, "Ortofotomapa gotowa: %d x %d px, GSD %.2f m, pokrycie %.0f%%, blad planimetryczny na punktach kontrolnych %.3f m", r.w, r.h, r.gsd, r.coverage * 100.0, oi_.rmseCheck); w_.Say(b);
  }
  int W0() const { return kImageRes[resIdx_].w; }
  int H0() const { return kImageRes[resIdx_].h; }

  using kImageRes_t = ImageRes;
  int camIdx_ = 1, resIdx_ = 0; double fwd_ = 80, side_ = 60; PhotoGeom geom_{};
  bool setup_ = false, busy_ = false; int W_ = 320, H_ = 240, row_ = 0; std::vector<unsigned char> img_; Rot3 M_{}; Vec3 org_{0, 0, 0}; Photo shot_{};
  bool adjOk_ = false; PhotoGeom fgeom_{}; std::vector<BaCam> adjCams_; std::vector<BaPoint> adjPts_; std::vector<BaObs> useObs_; OrthoParams orthoP_; OrthoInfo oi_; std::unique_ptr<Orthorectifier> ortho_;
  std::string photoDir_; std::vector<Photo> photos_; std::vector<Vec3> feats_; std::vector<Ctl> ctl_; std::vector<BaObs> obs_; GnssRng pixRng_{777}; PhotoStats st_;
};

inline std::unique_ptr<Job> CreatePhotoJob(IWorld& w, const JobOptions& o) { return std::make_unique<PhotoJob>(w, o); }
