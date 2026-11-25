// Kreatory (bez Gothic API): gotowe zestawy ustawien dla typowych scenariuszy. Kreator nie ma wlasnej logiki: tworzy robote z JobTypes()
// i wykonuje te same OnMenu(id), ktore uzytkownik wykonalby recznie. Kroki sa warunkowe (tylko gdy etykieta zawiera whenLabel), wiec
// nie cofaja ustawien, ktore swiat ma juz wlaczone domyslnie.
#pragma once
#include <memory>
#include <string>
#include <vector>
#include "Jobs.hpp"

struct WizStep { std::string id, whenLabel; };
struct Wizard { std::string id, name, typeId, desc; JobOptions opts; std::vector<WizStep> setup; };

inline const std::vector<Wizard>& Wizards() {
  static const std::vector<Wizard> w = {
    {"osnowa", "Pomierz osnowe (GNSS)", "gnss_static", "statyka na punktach osnowy: baza na znanym punkcie, sesja min. 30 min, mniejszy szum; punkty zakladasz z pomiaru",
     JobOptions{TraverseKind::Open, CalcMode::Auto, false}, {}},
    {"niwelacja", "Niwelacja ciagu A -> B", "traverse", "ciag niwelacyjny dowiazany: reper A, punkty obrotowe, reper B, dziennik liczony automatycznie",
     JobOptions{TraverseKind::Anchored, CalcMode::Auto, false}, {}},
    {"tachy_inwent", "Inwentaryzacja tachimetrem", "tachy", "stanowisko, nawiazania, pomiar punktow na lustro; wyniki w dzienniku tachimetrycznym",
     JobOptions{TraverseKind::Open, CalcMode::Auto, false}, {}},
    {"skan_budynku", "Zeskanuj budynek (skaner)", "scan", "skan z rejestracja na kulach referencyjnych na punktach osnowy; wynik .las i .pts",
     JobOptions{TraverseKind::Open, CalcMode::Auto, false}, {{"reg", "wylaczona"}}},
    {"nalot_als", "Nalot z drona (LiDAR)", "aerial", "plan nalotu z obszaru, szeregi kalibracyjne, wyrownanie i produkty z chmury",
     JobOptions{TraverseKind::Open, CalcMode::Auto, false}, {{"calib", "wylaczone"}}},
    {"zdjecia", "Zdjecia z drona (fotogrametria)", "photo", "plan nalotu, zdjecia, wyrownanie wiazki i ortofotomapa",
     JobOptions{TraverseKind::Open, CalcMode::Auto, false}, {}},
  };
  return w;
}

inline std::unique_ptr<Job> StartWizardUnapplied(IWorld& w, const Wizard& wz) {
  for (const JobType& t : JobTypes()) if (t.id == wz.typeId) return t.create(w, wz.opts);
  return nullptr;
}

// Wykonuje kroki kreatora na istniejacej robocie: pozycja musi istniec, byc aktywna, a jej etykieta zawierac whenLabel.
inline void ApplyWizardSetup(Job& job, const Wizard& wz) {
  for (const WizStep& s : wz.setup) {
    for (const MenuItem& m : job.Menu()) {
      if (m.id != s.id) continue;
      if (m.enabled && m.label.find(s.whenLabel) != std::string::npos) job.OnMenu(s.id);
      break;
    }
  }
}

inline std::unique_ptr<Job> StartWizard(IWorld& w, const Wizard& wz) {
  std::unique_ptr<Job> job = StartWizardUnapplied(w, wz);
  if (job) ApplyWizardSetup(*job, wz);
  return job;
}
