// Sztuczny swiat dla testow robot lotniczych (ALS i fotogrametria): plaski teren H = 100 m, promien analityczny, liczniki wywolan.
#pragma once
#include <cmath>
#include <filesystem>
#include <string>
#include <vector>
#include "job/Jobs.hpp"

static bool has(const std::string& s, const std::string& part) { return s.find(part) != std::string::npos; }
static const double N0 = 5500000.0, E0 = 6500000.0;

struct FakeAerial : IWorld {
  std::vector<std::string> said; std::string dir; GridPos hero{N0, E0, 100.0}; bool heroOk = true;
  int beams = 0, beamClears = 0; bool lastBeamCleared = false;
  void AerialBeam(const GridPos&, double, double, double d) override { if (d < 0) { beamClears++; lastBeamCleared = true; } else { beams++; lastBeamCleared = false; } }
  int placed = 0, removed = 0, poses = 0, outCalls = 0; bool dirOk = true; GridPos lastPose{0, 0, 0}; int landings = 0; bool wasUp = false;
  bool PlaceMarker(int, MarkerKind) override { return true; }
  bool FeetWithinCm(int, double) const override { return true; }
  void PlaceRod(RodRole, int) override {}
  void RemoveRod(RodRole) override {}
  bool PlaceInstrument() override { return true; }
  void RemoveInstrument() override {}
  double PointHeightM(int) const override { return 0; }
  double SightDistCm(int) const override { return 0; }
  double Random01() override { return 0.5; }
  void ShiftInstrumentMm(double) override {}
  void Say(const std::string& t) override { said.push_back(t); }
  bool HeroGround(GridPos& g) const override { if (!heroOk) return false; g = hero; return true; }
  bool GroundHeightM(double, double, double& h) const override { h = 100.0; return groundOk; }
  bool PlaceDrone(const GridPos&) override { placed++; return true; }
  void RemoveDrone() override { removed++; }
  void SetDronePose(const GridPos& p, double, double, double, double) override { poses++; lastPose = p; if (p.h > 120.0) wasUp = true; else if (wasUp && p.h <= 100.5) { landings++; wasUp = false; } }
  std::string AerialOutDir() override { std::string d = dir; if (outCalls++ > 0) d += "_" + std::to_string(outCalls); if (dirOk) std::filesystem::create_directories(d); return d; }
  std::vector<ControlPoint> ctrlList;                                                      // gdy niepusta: punkty osnowy zwracane zamiast domyslnego G1
  std::vector<ControlPoint> ControlNearHero(double) const override { if (!ctrlList.empty()) return ctrlList; ControlPoint c; c.id = "G1"; c.n = N0 + 5; c.e = E0 + 7; c.h = 100.0; return {c}; }
  bool Said(const std::string& part) const { for (const std::string& s : said) if (has(s, part)) return true; return false; }
  int Count(const std::string& part) const { int n = 0; for (const std::string& s : said) if (has(s, part)) n++; return n; }
  bool rays = true, groundOk = true, blocked = false; int routeChecks = 0;
  mutable int dummy = 0;
  bool RouteBlocked(const GridPos&, const GridPos&) const override { const_cast<FakeAerial*>(this)->routeChecks++; return blocked; }
  bool AerialRay(const GridPos& from, double, double elevDeg, double maxM, double& dist, double& cosInc, bool& isVob, unsigned* rgb) const override {
    const double s = std::sin(elevDeg * 3.14159265358979323846 / 180.0);
    if (!rays || s > -1e-6) return false;
    dist = (from.h - 100.0) / -s; if (dist > maxM || dist < 0) return false;   // plaski teren H = 100 m
    cosInc = -s; isVob = false; if (rgb) *rgb = 0x64A03C; return true;
  }
};

