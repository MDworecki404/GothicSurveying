// Rejestr robót: tu dopisuje się nowe typy (każdy w osobnym pliku job/*.hpp).
#pragma once
#include "Job.hpp"
#include "LevelingTraverse.hpp"
#include "GnssRover.hpp"
#include "TachyStation.hpp"

inline const std::vector<JobType>& JobTypes() {
  static const std::vector<JobType> types = {
    {"traverse", "Ciag niwelacyjny", &CreateLevelingTraverse},
    {"gnss", "Pomiar GNSS (RTK)", &CreateGnssRover, true, false},
    {"tachy", "Tachimetria (pomiar biegunowy)", &CreateTachyStation, false, true},
  };
  return types;
}
