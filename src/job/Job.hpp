// Interfejs roboty geodezyjnej i świata, w którym działa (bez Gothic API).
#pragma once
#include <memory>
#include <string>
#include <vector>
#include "calc/FieldBook.hpp"
#include "calc/Control.hpp"
#include "calc/GnssReport.hpp"
#include "calc/Libella.hpp"
#include "calc/TachyReport.hpp"

enum class RodRole { Back, Fore };
enum class MarkerKind { Reper, Turning };
enum class JobKind { Leveling, Gnss, Tachy };

struct MenuItem { std::string id, label, hint; bool enabled; };
struct JobOptions { TraverseKind kind; CalcMode mode; bool showTrueError = false; };

// Świat gry z punktu widzenia roboty. Implementacja silnikowa: engine/World.hpp; w testach: sztuczna.
class IWorld {
 public:
  virtual ~IWorld() = default;
  virtual bool   PlaceMarker(int pointId, MarkerKind kind) = 0;   // pod stopami bohatera; false = brak gruntu
  virtual bool   FeetWithinCm(int pointId, double cm) const = 0;
  virtual void   PlaceRod(RodRole role, int pointId) = 0;         // przenosi obiekt łaty na punkt
  virtual void   RemoveRod(RodRole role) = 0;
  virtual bool   PlaceInstrument() = 0;                           // pod stopami; nowe stanowisko
  virtual void   RemoveInstrument() = 0;
  virtual double PointHeightM(int pointId) const = 0;             // prawdziwa wysokość terenu punktu
  virtual double SightDistCm(int pointId) const = 0;              // pozioma, od instrumentu
  virtual double Random01() = 0;                                  // 0..1 (los obsadzki)
  virtual void   ShiftInstrumentMm(double dyMm) = 0;              // podnosi (+) / obniża (-) niwelator stojący na stanowisku
  virtual void   Say(const std::string& textAscii) = 0;
  // GNSS (domyslne ciala, zeby swiaty niwelacji ich nie musialy znac)
  virtual bool     PlaceBase(GridPos& g) { (void)g; return false; }                // stacja bazowa RTK pod stopami; g = prawdziwe wspolrzedne gruntu
  virtual bool     PlaceTachymeterOnPoint(int pointId) { (void)pointId; return false; }   // tachimetr dokladnie nad znakiem punktu (bohater nie musi tam stac)
  virtual bool     PlacePrismOnPoint(int pointId) { (void)pointId; return false; }        // tyczka z lustrem dokladnie na znaku punktu
  virtual bool     TwoFaceByDefault() const { return false; }                       // tachimetr domyslnie w dwoch polozeniach lunety (KL+KP)
  virtual bool     InstrumentLevelingEnabled() const { return false; }               // czy instrument trzeba poziomowac (libella)
  virtual bool     PlacePole() { return false; }                  // tyczka z antena pod stopami; false = brak gruntu
  virtual void     RemovePole() {}
  virtual SkyMask  SkyHorizon() const { return SkyMask{}; }       // maska horyzontu z promieni ku niebu z miejsca tyczki
  virtual GridPos  PoleGroundGrid() const { return GridPos{0, 0, 0}; }   // prawdziwe wspolrzedne gruntu pod tyczka
  virtual double   NowSec() const { return 0; }                   // czas gry [s]; stoi, gdy gra zatrzymana
  virtual std::string TimeText() const { return ""; }             // czas rzeczywisty GG:MM:SS do dziennika
  virtual std::string StoreControlPoint(const ControlPoint& cp) { (void)cp; return ""; }   // osnowa: zapis punktu i znak w swiecie; zwraca id ("" = blad)
  // Tachimetria
  virtual GridPos  StationGrid() const { return GridPos{0, 0, 0}; }            // prawdziwe wspolrzedne gruntu pod tachimetrem
  virtual GridPos  PointGrid(int slot) const { (void)slot; return GridPos{0, 0, 0}; }   // prawdziwe wspolrzedne znaku nawiazania nr slot (1-5)
  virtual bool     PlaceTachymeter() { return false; }                         // tachimetr (nie niwelator) na statywie pod stopami
  virtual bool     PlaceBacksight(int slot) { (void)slot; return false; }      // znak nawiazania nr slot (1-5) z lustrem na tyczce, pod stopami
  virtual void     RemoveBacksight(int slot) { (void)slot; }
  virtual bool     PlaceBacksightAt(int slot, const ControlPoint& cp) { (void)slot; (void)cp; return false; }   // nawiazanie na znaku punktu osnowy
  virtual bool     PlaceTachymeterAt(const ControlPoint& cp) { (void)cp; return false; }                        // tachimetr na znaku punktu osnowy
  virtual std::vector<ControlPoint> ControlNearHero(double maxM) const { (void)maxM; return {}; }               // punkty osnowy tego swiata w promieniu od bohatera
  virtual std::vector<ControlPoint> ControlNearStation(double minM, double maxM) const { (void)minM; (void)maxM; return {}; }   // ... od stanowiska
  virtual void     SetPrismHeightM(double heightM) { (void)heightM; }          // fizyczna wysokosc lustra (model tyczek)
  virtual bool     TraceSight(double camAzDeg, double camPitchDeg, GridPos& hit, double& slopeM) const { (void)camAzDeg; (void)camPitchDeg; (void)hit; (void)slopeM; return false; }   // pomiar bezlustrowy: pierwszy trafiony punkt
  virtual bool     PlacePrism() { return false; }                              // tyczka z lustrem (w miejscu tyczki GNSS); false = brak gruntu
  virtual bool     LineClear(bool toPrism, int slot = 1) const { (void)toPrism; (void)slot; return true; }   // linia celowa od oka do lustra (tyczki) albo do lustra nawiazania nr slot niezaslonieta
};

class Job {
 public:
  virtual ~Job() = default;
  virtual std::string Name() const = 0;
  virtual std::vector<MenuItem> Menu() const = 0;
  virtual void OnMenu(const std::string& id) = 0;
  virtual std::vector<int> ActiveRodPoints() const = 0;           // punkty z łatą, do wyboru pod pionową kreską
  virtual int  RodPointFor(RodRole role) const = 0;               // punkt, na którym stoi łata o tej roli; < 0 = brak
  virtual bool OnReading(int pointId, int midMm) = 0;             // odczyt środkowej kreski; dalmierz liczy robota sama
  virtual std::string Hud() const = 0;                            // ASCII
  virtual bool CanFinish() const = 0;
  virtual Libella* GetLibella() { return nullptr; }               // libella instrumentu roboty (nullptr = brak poziomowania)
  virtual std::string InstrumentBlock() const { return ""; }      // niepusty = odczyt zablokowany (np. niwelator niewypoziomowany)
  virtual FieldBook Finish(bool complete) = 0;                    // complete=false przy przerwaniu
  virtual JobKind Kind() const { return JobKind::Leveling; }
  virtual TachyReport FinishTachy(bool complete) { (void)complete; return TachyReport{}; }
  virtual bool NeedsChoice(const std::string& menuId) const { (void)menuId; return false; }    // pozycja menu wymaga wyboru punktu z listy (osnowa)
  virtual std::vector<MenuItem> TachyChoices(const std::string& menuId) const { (void)menuId; return {}; }   // lista wyboru: id pozycji "<menuId>:<punkt>"
  virtual std::string TachyFire(double camAzDeg, double camPitchDeg) { (void)camAzDeg; (void)camPitchDeg; return ""; }   // Enter w lunecie; komunikat ASCII
  virtual std::string TachyFlip() { return ""; }                  // klawisz przerzutu lunety (KL <-> KP); komunikat ASCII
  virtual std::vector<std::string> TachyPanel(double camAzDeg, double camPitchDeg) const { (void)camAzDeg; (void)camPitchDeg; return {}; }
  virtual GnssReport FinishGnss(bool complete) { (void)complete; return GnssReport{}; }
  virtual void Tick() {}                                          // co klatke (symulacje czasu rzeczywistego)
  virtual std::vector<std::string> StatusLines() const { return {}; }   // panel statusu (ASCII); pusty = brak panelu
};

struct JobType { std::string id, name; std::unique_ptr<Job> (*create)(IWorld&, const JobOptions&); bool gnss = false, tachy = false; };
