// Rejestr robót: tu dopisuje się nowe typy (każdy w osobnym pliku job/*.hpp).
#pragma once
#include "Job.hpp"
#include "LevelingTraverse.hpp"
#include "GnssRover.hpp"
#include "TachyStation.hpp"
#include "TrigLevel.hpp"
#include "ScanJob.hpp"
#include "AerialJob.hpp"

inline const std::vector<JobType>& JobTypes() {
  static const std::vector<JobType> types = {
    {"traverse", "Ciag niwelacyjny", &CreateLevelingTraverse},
    {"gnss", "Pomiar GNSS (RTK)", &CreateGnssRover, true, false},
    {"tachy", "Tachimetria (pomiar biegunowy)", &CreateTachyStation, false, true},
    {"trig", "Niwelacja trygonometryczna", &CreateTrigLevel, false, true},
    {"scan", "Skaner laserowy", &CreateScanJob, false, false, true},
    {"aerial", "Skaning lotniczy z drona (ALS)", &CreateAerialJob, false, false, false, true},
  };
  return types;
}
