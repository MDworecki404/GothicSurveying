// Rejestr robót: tu dopisuje się nowe typy (każdy w osobnym pliku job/*.hpp).
#pragma once
#include "Job.hpp"
#include "LevelingTraverse.hpp"

inline const std::vector<JobType>& JobTypes() {
  static const std::vector<JobType> types = {
    {"traverse", "Ciag niwelacyjny", &CreateLevelingTraverse},
  };
  return types;
}
