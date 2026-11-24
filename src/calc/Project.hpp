// Projekt pomiarowy (bez Gothic API): nazwa, swiat, lista pomiarow (dzisiejsze roboty) i ich stan; zapis w GothicSurveying/projekty/<nazwa>/projekt.txt.
// Format: StateKV (klucz=wartosc). Projekt jest niezalezny od zapisu gry; trwajaca robote przywraca stan_slot<N>.txt, a Reconcile godzi manifest z tym, co faktycznie trwa.
#pragma once
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
#include "State.hpp"

enum class MeasState { New, Running, Done, Aborted };

struct Measurement {
  std::string id, type, name, dir, params, summary, pointsFile;
  MeasState state = MeasState::New;
  bool guided = false;                                  // tryb prowadzony (z kreatora)
};

struct Project {
  std::string name, world, created;
  bool closed = false;
  std::vector<Measurement> items;
  StateKV defaults;                                     // ustawienia domyslne projektu
};

// Nazwa folderu projektu: [A-Za-z0-9_-]{1,32} (bez polskich znakow i separatorow sciezki).
inline bool ProjectNameOk(const std::string& n) {
  if (n.empty() || n.size() > 32) return false;
  for (unsigned char c : n) if (!(std::isalnum(c) && c < 128) && c != '_' && c != '-') return false;
  return true;
}

inline Measurement& AddMeasurement(Project& p, const std::string& typeId, const std::string& typeName) {
  int same = 0; for (const Measurement& m : p.items) if (m.type == typeId) same++;
  int k = (int)p.items.size() + 1;
  auto taken = [&](const std::string& id) { for (const Measurement& m : p.items) if (m.id == id) return true; return false; };
  while (taken("m" + std::to_string(k))) k++;
  Measurement m; m.id = "m" + std::to_string(k); m.type = typeId; m.name = typeName + " " + std::to_string(same + 1);
  p.items.push_back(m);
  return p.items.back();
}

// Folder wynikow z manifestu to dane nieufne (reka edytowany/udostepniony plik): tylko GothicSurveying/<...> bez "..", ':' i '\'.
inline bool SafeResultDir(const std::string& d) {
  if (d.compare(0, 16, "GothicSurveying/") != 0 || d.size() == 16) return false;
  return d.find("..") == std::string::npos && d.find(':') == std::string::npos && d.find('\\') == std::string::npos;
}

inline const Measurement* RunningMeasurement(const Project& p) {
  for (const Measurement& m : p.items) if (m.state == MeasState::Running) return &m;
  return nullptr;
}

inline bool CanAddMeasurement(const Project& p, std::string& why) {
  if (p.closed) { why = "projekt zamkniety (otworz go ponownie)"; return false; }
  if (const Measurement* r = RunningMeasurement(p)) { why = "trwa pomiar: " + r->name; return false; }
  return true;
}

inline void SetState(Project& p, const std::string& id, MeasState s, const std::string& dir = "", const std::string& summary = "") {
  for (Measurement& m : p.items) if (m.id == id) { m.state = s; if (!dir.empty()) m.dir = dir; if (!summary.empty()) m.summary = summary; }
}

// Wznowienie roboty z zapisu gry: pomiar o tym id wraca do "w toku", chyba ze juz jest gotowy (wynik zostaje nietkniety) albo go nie ma - wtedy nowy pomiar.
inline std::string AttachRestored(Project& p, const std::string& pid, const std::string& typeId, const std::string& typeName) {
  for (Measurement& m : p.items) if (!pid.empty() && m.id == pid && m.state != MeasState::Done) { m.state = MeasState::Running; return m.id; }
  Measurement& m = AddMeasurement(p, typeId, typeName); m.state = MeasState::Running;
  return m.id;
}

// Pomiar "w toku" bez aktywnej roboty (np. wczytano starszy zapis) nie moze blokowac nowych: staje sie przerwanym.
inline void Reconcile(Project& p, const std::string& activeId) {
  for (Measurement& m : p.items) if (m.state == MeasState::Running && m.id != activeId) m.state = MeasState::Aborted;
}

inline bool CloseProject(Project& p, std::string& why) {
  if (const Measurement* r = RunningMeasurement(p)) { why = "trwa pomiar: " + r->name; return false; }
  p.closed = true; return true;
}
inline void ReopenProject(Project& p) { p.closed = false; }

inline std::string ProjectToText(const Project& p) {
  StateKV s;
  s.Int("ver", 1); s.Str("name", p.name); s.Str("world", p.world); s.Str("created", p.created); s.Str("state", p.closed ? "closed" : "open");
  s.Str("defaults", p.defaults.ToText());
  s.Int("m.n", (long long)p.items.size());
  for (size_t i = 0; i < p.items.size(); i++) {
    const Measurement& m = p.items[i]; const std::string k = "m." + std::to_string(i + 1) + ".";
    s.Str(k + "id", m.id); s.Str(k + "type", m.type); s.Str(k + "name", m.name); s.Str(k + "dir", m.dir); s.Str(k + "params", m.params);
    s.Str(k + "summary", m.summary); s.Str(k + "points", m.pointsFile); s.Int(k + "state", (int)m.state); s.Bool(k + "guided", m.guided);
  }
  return s.ToText();
}

inline Project ProjectFromText(const std::string& text, bool& ok) {
  Project p; const StateKV s = StateKV::FromText(text);
  ok = s.GetInt("ver") == 1 && ProjectNameOk(s.GetStr("name"));
  if (!ok) return p;
  p.name = s.GetStr("name"); p.world = s.GetStr("world"); p.created = s.GetStr("created"); p.closed = s.GetStr("state") == "closed";
  p.defaults = StateKV::FromText(s.GetStr("defaults"));
  const int n = (std::max)(0, (std::min)(s.GetInt("m.n"), 1000));
  for (int i = 1; i <= n; i++) {
    const std::string k = "m." + std::to_string(i) + ".";
    if (!ProjectNameOk(s.GetStr(k + "id"))) continue;   // pozycja uszkodzona, uciete albo id nie do uzycia w nazwie pliku: pomijamy
    Measurement m; m.id = s.GetStr(k + "id"); m.type = s.GetStr(k + "type"); m.name = s.GetStr(k + "name"); m.dir = s.GetStr(k + "dir");
    m.params = s.GetStr(k + "params"); m.summary = s.GetStr(k + "summary"); m.pointsFile = s.GetStr(k + "points");
    m.state = (MeasState)(std::max)(0, (std::min)(s.GetInt(k + "state"), 3)); m.guided = s.GetBool(k + "guided");
    const std::string& pf = m.pointsFile;                // nazwa pliku w folderze projektu, bez sciezki
    if (pf.find('/') != std::string::npos || pf.find('\\') != std::string::npos || pf.find("..") != std::string::npos) m.pointsFile.clear();
    if (!m.dir.empty() && !SafeResultDir(m.dir)) m.dir.clear();
    p.items.push_back(m);
  }
  return p;
}

inline std::string ProjectFile(const std::string& root, const std::string& name) { return root + "/projekty/" + name + "/projekt.txt"; }

inline bool SaveProject(const Project& p, const std::string& root) {
  namespace fs = std::filesystem; std::error_code ec;
  if (!ProjectNameOk(p.name)) return false;
  fs::create_directories(root + "/projekty/" + p.name, ec);
  const std::string path = ProjectFile(root, p.name), tmp = path + ".tmp";
  { std::ofstream f(tmp, std::ios::binary | std::ios::trunc); if (!f) return false; const std::string t = ProjectToText(p); f.write(t.data(), (std::streamsize)t.size()); if (!f.good()) return false; }
  fs::rename(tmp, path, ec);
  if (ec) { fs::remove(path, ec); ec.clear(); fs::rename(tmp, path, ec); }   // starsze biblioteki nie nadpisuja przy rename
  return !ec;
}

inline bool LoadProject(const std::string& root, const std::string& name, Project& out) {
  if (!ProjectNameOk(name)) return false;
  std::ifstream f(ProjectFile(root, name), std::ios::binary); if (!f) return false;
  const std::string text((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  bool ok = false; Project p = ProjectFromText(text, ok);
  if (!ok) return false;
  p.name = name; out = p; return true;
}

inline std::vector<std::string> ListProjects(const std::string& root) {
  namespace fs = std::filesystem; std::error_code ec; std::vector<std::string> out;
  for (const auto& e : fs::directory_iterator(root + "/projekty", ec)) {
    const std::string n = e.path().filename().string();
    if (e.is_directory(ec) && ProjectNameOk(n) && fs::exists(ProjectFile(root, n), ec)) out.push_back(n);
  }
  std::sort(out.begin(), out.end());
  return out;
}
