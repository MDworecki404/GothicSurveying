// Interfejs roboty geodezyjnej i świata, w którym działa (bez Gothic API).
#pragma once
#include <memory>
#include <string>
#include <vector>
#include "calc/FieldBook.hpp"
#include "calc/Control.hpp"
#include "calc/GnssReport.hpp"
#include "calc/Libella.hpp"
#include "calc/State.hpp"
#include "calc/TachyReport.hpp"

enum class RodRole { Back, Fore };
enum class MarkerKind { Reper, Turning };
enum class JobKind { Leveling, Gnss, Tachy, Scan, Aerial };
struct MapLine { double n1, e1, n2, e2; int color; };   // nakladka na mape osnowy: 0 obszar, 1 szereg, 2 dojazd

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
  virtual void     ScanHead(double azTrueDeg) { (void)azTrueDeg; }                   // obrot glowicy skanera ku azymutowi biezacej kolumny
  virtual void     ScanBeam(double azTrueDeg, double elevDeg, double distM) { (void)azTrueDeg; (void)elevDeg; (void)distM; }   // wiazka do narysowania (distM < 0: usun wszystkie)
  virtual bool     PlaceScanner() { return PlaceTachymeter(); }                     // skaner laserowy na statywie pod stopami (domyslnie jak tachimetr)
  virtual bool     PlaceBase(GridPos& g) { (void)g; return false; }                // stacja bazowa RTK pod stopami; g = prawdziwe wspolrzedne gruntu
  virtual bool     PlaceTachymeterOnPoint(int pointId) { (void)pointId; return false; }   // tachimetr dokladnie nad znakiem punktu (bohater nie musi tam stac)
  virtual bool     PlacePrismOnPoint(int pointId) { (void)pointId; return false; }        // tyczka z lustrem dokladnie na znaku punktu
  virtual bool     TwoFaceByDefault() const { return false; }                       // tachimetr domyslnie w dwoch polozeniach lunety (KL+KP)
  virtual bool     InstrumentLevelingEnabled() const { return false; }               // czy instrument trzeba poziomowac (libella)
  virtual bool     ScanRay(double azDeg, double elevDeg, double& distM, double& cosInc, bool& isVob, bool& isTarget, unsigned* rgb = nullptr) const { (void)azDeg; (void)elevDeg; (void)distM; (void)cosInc; (void)isVob; (void)isTarget; (void)rgb; return false; }   // skaner: promien z oka instrumentu (az od polnocy, elev nad poziomem); false = brak trafienia w 120 m
  virtual bool     ScanRegistrationByDefault() const { return false; }                // skan domyslnie w ukladzie lokalnym z rejestracja na kulach
  virtual bool     PlaceScanTarget(int slot, const ControlPoint& cp) { (void)slot; (void)cp; return false; }   // kula referencyjna na znaku punktu osnowy (slot 1-6)
  virtual void     RemoveScanTarget(int slot) { (void)slot; }
  virtual std::string ScanOutDir() { return ""; }                                    // folder wynikow skanu (tworzy); "" = blad
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
  // Nalot ALS (dron)
  virtual bool     AerialRay(const GridPos& from, double azDeg, double elevDeg, double maxM, double& distM, double& cosInc, bool& isVob, unsigned* rgb = nullptr) const { (void)from; (void)azDeg; (void)elevDeg; (void)maxM; (void)distM; (void)cosInc; (void)isVob; (void)rgb; return false; }   // promien z dowolnego punktu; false = brak trafienia
  virtual bool     GroundHeightM(double n, double e, double& h) const { (void)n; (void)e; (void)h; return false; }   // wysokosc terenu statycznego w (N, E); false = brak
  virtual bool     HeroGround(GridPos& g) const { (void)g; return false; }                  // grunt pod stopami bohatera
  virtual bool     PlaceDrone(const GridPos& pos) { (void)pos; return false; }
  virtual void     RemoveDrone() {}
  virtual void     SetDronePose(const GridPos& pos, double yawDeg, double pitchDeg, double rollDeg, double propPhaseDeg) { (void)pos; (void)yawDeg; (void)pitchDeg; (void)rollDeg; (void)propPhaseDeg; }
  virtual void     SetFollowCamera(bool on) { (void)on; }                                    // kamera za dronem
  virtual bool     RouteBlocked(const GridPos& a, const GridPos& b) const { (void)a; (void)b; return false; }   // przeszkoda na trasie (ostrzezenie)
  virtual std::string AerialOutDir() { return ""; }                                         // folder wynikow lotu (tworzy); "" = blad
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
  virtual std::vector<std::string> SummaryLines() const { return {}; }   // podsumowanie roboty (skaner) do panelu po zakonczeniu
  virtual bool CanSaveState() const { return false; }              // czy robota umie zapisac stan (zapis gry w trakcie roboty)
  virtual void SaveState(StateKV& s) const { (void)s; }
  virtual bool LoadState(const StateKV& s) { (void)s; return false; }   // false = stan nie pasuje
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
  virtual std::vector<MapLine> MapOverlay() const { return {}; }        // linie do narysowania na mapie osnowy (plan nalotu)
};

struct JobType { std::string id, name; std::unique_ptr<Job> (*create)(IWorld&, const JobOptions&); bool gnss = false, tachy = false, scan = false, aerial = false; };
