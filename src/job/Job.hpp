// Interfejs roboty geodezyjnej i świata, w którym działa (bez Gothic API).
#pragma once
#include <memory>
#include <string>
#include <vector>
#include "calc/FieldBook.hpp"

enum class RodRole { Back, Fore };
enum class MarkerKind { Reper, Turning };

struct MenuItem { std::string id, label, hint; bool enabled; };
struct JobOptions { TraverseKind kind; CalcMode mode; };

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
  virtual void   Say(const std::string& textAscii) = 0;
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
  virtual FieldBook Finish(bool complete) = 0;                    // complete=false przy przerwaniu
};

struct JobType { std::string id, name; std::unique_ptr<Job> (*create)(IWorld&, const JobOptions&); };
