// Prowadzenie w menu (bez Gothic API): sekcje, kolejnosc i powody niedostepnosci pozycji robot; linia "DALEJ"; rekomendacja projektu.
// Menu()/OnMenu() robot zostaja bez zmian - Arrange tylko opisuje i porzadkuje to, co robota zwraca.
#pragma once
#include <algorithm>
#include <string>
#include <vector>
#include "Job.hpp"
#include "calc/Project.hpp"

enum class Section { Prep, Params, Measure, Results, Tools, Advanced, Exit };

inline const char* SectionName(Section s) {
  switch (s) {
    case Section::Prep: return "Przygotowanie";
    case Section::Params: return "Parametry";
    case Section::Measure: return "Pomiar";
    case Section::Results: return "Wyniki";
    case Section::Tools: return "Narzedzia";
    case Section::Advanced: return "Zaawansowane";
    case Section::Exit: return "Wyjscie";
  }
  return "";
}

namespace wf_detail {
struct Slot { const char* id; Section sec; };
// Jedna wspolna tabela dla wszystkich robot: kolejnosc w tabeli = kolejnosc w sekcji; te same pozycje sa w tych samych miejscach.
inline const std::vector<Slot>& Table() {
  static const std::vector<Slot> t = {
    {"reper_a", Section::Prep}, {"ref_n1", Section::Prep}, {"ref_a", Section::Prep}, {"ref_b", Section::Prep}, {"ref_n2", Section::Prep}, {"addpt", Section::Prep}, {"delpt", Section::Prep}, {"src", Section::Prep}, {"station", Section::Prep}, {"newst", Section::Prep}, {"free", Section::Prep},
    {"backsight", Section::Prep}, {"unback", Section::Prep}, {"prism", Section::Prep}, {"prism_back", Section::Prep}, {"prism_fore", Section::Prep}, {"prism_n1", Section::Prep}, {"inst", Section::Prep}, {"instrument", Section::Prep},
    {"pole", Section::Prep}, {"fine", Section::Prep}, {"antenna", Section::Prep}, {"base", Section::Prep}, {"target", Section::Prep}, {"untarget", Section::Prep},
    {"area_here", Section::Prep}, {"area_ctrl", Section::Prep}, {"area_map", Section::Prep}, {"area_clear", Section::Prep}, {"plan_csv", Section::Prep}, {"cam", Section::Prep},
    {"agl", Section::Params}, {"speed", Section::Params}, {"sidelap", Section::Params}, {"preset", Section::Params}, {"res", Section::Params}, {"area_set", Section::Params}, {"area_full", Section::Params},
    {"fwd", Section::Params}, {"side", Section::Params}, {"rodlen", Section::Params}, {"lvl", Section::Params},
    {"rod_back", Section::Measure}, {"rod_fore_turn", Section::Measure}, {"rod_fore_end", Section::Measure}, {"move_next", Section::Measure},
    {"obsadzka", Section::Measure}, {"redo", Section::Measure}, {"redo_readings", Section::Measure}, {"meas", Section::Measure}, {"view", Section::Measure},
    {"stake", Section::Measure}, {"unstake", Section::Measure}, {"start", Section::Measure}, {"store", Section::Measure}, {"control", Section::Measure},
    {"discard", Section::Measure}, {"scan", Section::Measure}, {"launch", Section::Measure}, {"cancel", Section::Measure}, {"scope", Section::Measure},
    {"level", Section::Measure}, {"camera", Section::Measure}, {"prefix", Section::Measure},
    {"adjust", Section::Results}, {"adjust_cancel", Section::Results}, {"products", Section::Results}, {"products_cancel", Section::Results}, {"calc", Section::Results}, {"savectl", Section::Results},
    {"ortho", Section::Results}, {"ortho_cancel", Section::Results}, {"finish", Section::Results},
    {"preview", Section::Tools}, {"map", Section::Tools}, {"tp", Section::Tools},
    {"corr", Section::Advanced}, {"mode", Section::Advanced}, {"prismh", Section::Advanced}, {"faces", Section::Advanced}, {"rgb", Section::Advanced},
    {"reg", Section::Advanced}, {"azimuth", Section::Advanced}, {"follow", Section::Advanced}, {"calib", Section::Advanced},
    {"battery", Section::Advanced}, {"profile", Section::Advanced},
    {"abort", Section::Exit},
  };
  return t;
}
inline const char* DefaultWhy(const std::string& id) {
  if (id == "launch") return "lot juz trwa";
  if (id == "cancel") return "nic nie trwa";
  if (id == "finish") return "najpierw wykonaj pomiar";
  return "niedostepne w tym kroku";
}
}  // namespace wf_detail

// Nadaje sekcje, porzadkuje (stabilnie: sekcja, potem kolejnosc z tabeli) i uzupelnia powod dla pozycji niedostepnych (hint, a gdy pusty - tabela).
inline std::vector<MenuItem> Arrange(JobKind kind, std::vector<MenuItem> items) {
  (void)kind;
  const auto& tab = wf_detail::Table();
  auto rank = [&](const MenuItem& m) -> std::pair<int, int> {
    const std::string base = m.id.compare(0, 2, "g_") == 0 ? m.id.substr(2) : m.id;      // "g_*": odbiornik GNSS w tachimetrii, jak pozycja bazowa
    for (size_t i = 0; i < tab.size(); i++) if (base == tab[i].id) return { (int)tab[i].sec, (int)i };
    return { (int)Section::Advanced, (int)tab.size() };                         // nieznane id: Zaawansowane, za znanymi
  };
  for (MenuItem& m : items) {
    m.section = SectionName((Section)rank(m).first);
    if (!m.enabled) m.why = !m.hint.empty() ? m.hint : wf_detail::DefaultWhy(m.id);
  }
  std::stable_sort(items.begin(), items.end(), [&](const MenuItem& a, const MenuItem& b) { return rank(a) < rank(b); });
  return items;
}

struct Next { std::string id, text; };

// Menu w trybie prowadzonym: nastepny krok, jego towarzysze (koniec ciagu, poziomowanie, zakonczenie), "Pokaz wszystko" i Przerwij.
// Bez towarzyszy gracz w prowadzonym menu nie mialby jak zakonczyc ciagu (rod_fore_end) ani pomiaru (finish).
inline std::vector<MenuItem> GuidedItems(const std::vector<MenuItem>& items, const std::string& nextId) {
  std::vector<MenuItem> g; auto add = [&](const std::string& id) {
    for (const MenuItem& m : g) if (m.id == id) return;
    for (const MenuItem& m : items) if (m.id == id && m.enabled) { g.push_back(m); return; }
  };
  add(nextId);
  if (nextId == "rod_fore_turn") add("rod_fore_end");
  if (nextId == "inst" || nextId == "addpt") add("addpt");                    // ciag poligonowy: kolejne punkty ciagu
  add("level"); add("redo"); add("finish");
  { MenuItem it{ "showall", "Pokaz wszystkie pozycje (tryb zaawansowany)", "wylacza tryb prowadzony dla tego pomiaru", true }; it.section = "Pozostale"; g.push_back(it); }
  for (const MenuItem& m : items) if (m.id == "abort") g.push_back(m);
  return g;
}

// Linia "DALEJ" dla aktywnej roboty: krok z Job::NextStep, a gdy go brak albo jest niedostepny - zakonczenie, jesli mozliwe.
inline Next NextLine(const std::vector<MenuItem>& items, const std::string& stepId) {
  for (const MenuItem& m : items) if (!stepId.empty() && m.id == stepId && m.enabled) return { m.id, "DALEJ: " + m.label };
  for (const MenuItem& m : items) if (m.id == "finish" && m.enabled) return { m.id, "DALEJ: Zakoncz pomiar" };
  return { "", "" };
}

// Rekomendacja na ekranie glownym projektu. activeJobNext: linia DALEJ aktywnej roboty (pusta, gdy nic nie trwa).
inline Next ProjectNext(const Project& p, const Next& activeJobNext) {
  if (p.name.empty()) return { "create", "DALEJ: Zaloz projekt" };
  if (p.closed) return { "reopen", "DALEJ: Projekt zamkniety - otworz ponownie" };
  if (const Measurement* r = RunningMeasurement(p)) {
    std::string step = activeJobNext.text; const std::string pre = "DALEJ: ";
    if (step.compare(0, pre.size(), pre) == 0) step = step.substr(pre.size());
    return { "continue", "DALEJ: Kontynuuj " + r->name + (step.empty() ? "" : " - " + step) };
  }
  if (p.items.empty()) return { "new", "DALEJ: Nowy pomiar (robota albo kreator)" };
  return { "new", "DALEJ: Nowy pomiar albo zamknij projekt" };
}
