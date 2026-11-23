// Zapis i odczyt stanu roboty (bez Gothic API): plaski slownik klucz=wartosc w tekscie plus serializery struktur raportow.
// Klucze maja przedrostki "a.b.3.c"; liczba elementow wektora w "<przedrostek>.n". Wartosci tekstowe: % koduje znaki specjalne.
#pragma once
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>
#include "Control.hpp"
#include "FieldBook.hpp"
#include "GnssReport.hpp"
#include "TachyReport.hpp"

struct StateKV {
  std::map<std::string, std::string> m;

  void Str(const std::string& k, const std::string& v) { m[k] = v; }
  void Num(const std::string& k, double v) { char b[40]; std::snprintf(b, sizeof b, "%.17g", v); m[k] = b; }
  void Int(const std::string& k, long long v) { m[k] = std::to_string(v); }
  void Bool(const std::string& k, bool v) { m[k] = v ? "1" : "0"; }

  bool Has(const std::string& k) const { return m.count(k) > 0; }
  std::string GetStr(const std::string& k, const std::string& def = "") const { auto it = m.find(k); return it == m.end() ? def : it->second; }
  double GetNum(const std::string& k, double def = 0) const { auto it = m.find(k); return it == m.end() ? def : std::atof(it->second.c_str()); }
  int GetInt(const std::string& k, int def = 0) const { auto it = m.find(k); return it == m.end() ? def : std::atoi(it->second.c_str()); }
  bool GetBool(const std::string& k, bool def = false) const { auto it = m.find(k); return it == m.end() ? def : it->second == "1"; }

  std::string ToText() const {
    std::string o;
    for (const auto& kv : m) {
      o += kv.first + "=";
      for (char c : kv.second) {
        if (c == '%') o += "%25"; else if (c == '\n') o += "%0A"; else if (c == '\r') o += "%0D"; else o += c;
      }
      o += "\n";
    }
    return o;
  }
  static StateKV FromText(const std::string& text) {
    StateKV s; size_t pos = 0;
    while (pos < text.size()) {
      size_t end = text.find('\n', pos); if (end == std::string::npos) end = text.size();
      const std::string line = text.substr(pos, end - pos); pos = end + 1;
      const size_t eq = line.find('=');
      if (eq == std::string::npos || eq == 0) continue;
      std::string v; const std::string raw = line.substr(eq + 1);
      for (size_t i = 0; i < raw.size(); i++) {
        if (raw[i] == '%' && i + 2 < raw.size()) {
          const std::string h = raw.substr(i + 1, 2);
          if (h == "25") { v += '%'; i += 2; continue; }
          if (h == "0A") { v += '\n'; i += 2; continue; }
          if (h == "0D") { v += '\r'; i += 2; continue; }
        }
        v += raw[i];
      }
      s.m[line.substr(0, eq)] = v;
    }
    return s;
  }
};

// --- serializery struktur: Put<T>(kv, "przedrostek", x) / Get<T>(kv, "przedrostek", x) ---
inline void PutControl(StateKV& s, const std::string& p, const ControlPoint& c) {
  s.Str(p + ".id", c.id); s.Str(p + ".world", c.world); s.Num(p + ".n", c.n); s.Num(p + ".e", c.e); s.Num(p + ".h", c.h);
  s.Num(p + ".x", c.x); s.Num(p + ".y", c.y); s.Num(p + ".z", c.z); s.Str(p + ".sol", c.solution); s.Num(p + ".pdop", c.pdop); s.Int(p + ".ep", c.epochs); s.Str(p + ".t", c.timeText);
}
inline ControlPoint GetControl(const StateKV& s, const std::string& p) {
  ControlPoint c; c.id = s.GetStr(p + ".id"); c.world = s.GetStr(p + ".world"); c.n = s.GetNum(p + ".n"); c.e = s.GetNum(p + ".e"); c.h = s.GetNum(p + ".h");
  c.x = s.GetNum(p + ".x"); c.y = s.GetNum(p + ".y"); c.z = s.GetNum(p + ".z"); c.solution = s.GetStr(p + ".sol"); c.pdop = s.GetNum(p + ".pdop"); c.epochs = s.GetInt(p + ".ep"); c.timeText = s.GetStr(p + ".t");
  return c;
}

inline void PutGnssPoint(StateKV& s, const std::string& p, const GnssPoint& g) {
  s.Str(p + ".name", g.name); s.Str(p + ".t", g.timeText); s.Num(p + ".n", g.n); s.Num(p + ".e", g.e); s.Num(p + ".h", g.h); s.Str(p + ".sol", g.solution);
  s.Int(p + ".used", g.used); s.Num(p + ".pdop", g.pdop); s.Num(p + ".hr", g.hRmsMm); s.Num(p + ".vr", g.vRmsMm); s.Int(p + ".ep", g.epochs); s.Num(p + ".ant", g.antennaM);
  s.Str(p + ".note", g.note); s.Bool(p + ".ht", g.hasTrue); s.Num(p + ".dn", g.dNmm); s.Num(p + ".de", g.dEmm); s.Num(p + ".dh", g.dHmm); s.Num(p + ".d2", g.d2Dmm);
}
inline GnssPoint GetGnssPoint(const StateKV& s, const std::string& p) {
  GnssPoint g; g.name = s.GetStr(p + ".name"); g.timeText = s.GetStr(p + ".t"); g.n = s.GetNum(p + ".n"); g.e = s.GetNum(p + ".e"); g.h = s.GetNum(p + ".h"); g.solution = s.GetStr(p + ".sol");
  g.used = s.GetInt(p + ".used"); g.pdop = s.GetNum(p + ".pdop"); g.hRmsMm = s.GetNum(p + ".hr"); g.vRmsMm = s.GetNum(p + ".vr"); g.epochs = s.GetInt(p + ".ep"); g.antennaM = s.GetNum(p + ".ant", 2.0);
  g.note = s.GetStr(p + ".note"); g.hasTrue = s.GetBool(p + ".ht"); g.dNmm = s.GetNum(p + ".dn"); g.dEmm = s.GetNum(p + ".de"); g.dHmm = s.GetNum(p + ".dh"); g.d2Dmm = s.GetNum(p + ".d2");
  return g;
}

inline void PutTachyPoint(StateKV& s, const std::string& p, const TachyPoint& t) {
  s.Str(p + ".name", t.name); s.Str(p + ".t", t.timeText); s.Num(p + ".hz", t.hzGon); s.Num(p + ".v", t.vGon); s.Num(p + ".sl", t.slopeM); s.Num(p + ".hd", t.hDistM); s.Num(p + ".dhm", t.dHM);
  s.Num(p + ".n", t.n); s.Num(p + ".e", t.e); s.Num(p + ".h", t.h); s.Num(p + ".pr", t.prismM); s.Str(p + ".note", t.note); s.Int(p + ".st", t.stationNo);
  s.Bool(p + ".ht", t.hasTrue); s.Num(p + ".dn", t.dNm); s.Num(p + ".de", t.dEm); s.Num(p + ".dh", t.dHm); s.Num(p + ".d2", t.d2Dm);
}
inline TachyPoint GetTachyPoint(const StateKV& s, const std::string& p) {
  TachyPoint t; t.name = s.GetStr(p + ".name"); t.timeText = s.GetStr(p + ".t"); t.hzGon = s.GetNum(p + ".hz"); t.vGon = s.GetNum(p + ".v"); t.slopeM = s.GetNum(p + ".sl"); t.hDistM = s.GetNum(p + ".hd"); t.dHM = s.GetNum(p + ".dhm");
  t.n = s.GetNum(p + ".n"); t.e = s.GetNum(p + ".e"); t.h = s.GetNum(p + ".h"); t.prismM = s.GetNum(p + ".pr", 1.5); t.note = s.GetStr(p + ".note"); t.stationNo = s.GetInt(p + ".st", 1);
  t.hasTrue = s.GetBool(p + ".ht"); t.dNm = s.GetNum(p + ".dn"); t.dEm = s.GetNum(p + ".de"); t.dHm = s.GetNum(p + ".dh"); t.d2Dm = s.GetNum(p + ".d2");
  return t;
}

inline void PutTachyBacksight(StateKV& s, const std::string& p, const TachyBacksight& b) {
  s.Str(p + ".name", b.name); s.Num(p + ".az", b.azGon); s.Num(p + ".hz", b.hzGon); s.Num(p + ".dr", b.dirResidMgon); s.Num(p + ".dm", b.distMeasuredM);
  s.Num(p + ".dc", b.distCoordM); s.Num(p + ".ds", b.distResidMm); s.Bool(p + ".unc", b.uncertain);
}
inline TachyBacksight GetTachyBacksight(const StateKV& s, const std::string& p) {
  TachyBacksight b; b.name = s.GetStr(p + ".name"); b.azGon = s.GetNum(p + ".az"); b.hzGon = s.GetNum(p + ".hz"); b.dirResidMgon = s.GetNum(p + ".dr"); b.distMeasuredM = s.GetNum(p + ".dm");
  b.distCoordM = s.GetNum(p + ".dc"); b.distResidMm = s.GetNum(p + ".ds"); b.uncertain = s.GetBool(p + ".unc");
  return b;
}

inline void PutTachyStationRec(StateKV& s, const std::string& p, const TachyStationRec& r) {
  s.Str(p + ".label", r.label); s.Str(p + ".src", r.coordSource); s.Str(p + ".name", r.stationName); s.Num(p + ".n", r.n); s.Num(p + ".e", r.e); s.Num(p + ".h", r.h);
  s.Num(p + ".ih", r.instrH); s.Num(p + ".azb", r.azBackGon); s.Int(p + ".b.n", (long long)r.backsights.size());
  for (size_t i = 0; i < r.backsights.size(); i++) PutTachyBacksight(s, p + ".b." + std::to_string(i), r.backsights[i]);
}
inline TachyStationRec GetTachyStationRec(const StateKV& s, const std::string& p) {
  TachyStationRec r; r.label = s.GetStr(p + ".label"); r.coordSource = s.GetStr(p + ".src"); r.stationName = s.GetStr(p + ".name"); r.n = s.GetNum(p + ".n"); r.e = s.GetNum(p + ".e"); r.h = s.GetNum(p + ".h");
  r.instrH = s.GetNum(p + ".ih", 1.5); r.azBackGon = s.GetNum(p + ".azb");
  const int n = s.GetInt(p + ".b.n"); for (int i = 0; i < n; i++) r.backsights.push_back(GetTachyBacksight(s, p + ".b." + std::to_string(i)));
  return r;
}

inline void PutTrigLeg(StateKV& s, const std::string& p, const TrigLeg& l) {
  s.Str(p + ".from", l.from); s.Str(p + ".to", l.to); s.Num(p + ".d", l.distM); s.Num(p + ".vf", l.vFwdGon); s.Num(p + ".vb", l.vBackGon); s.Num(p + ".df", l.dhFwdMm); s.Num(p + ".db", l.dhBackMm);
  s.Num(p + ".sum", l.sumMm); s.Num(p + ".mean", l.meanMm); s.Num(p + ".tol", l.tolMm); s.Num(p + ".hf", l.hFromM); s.Num(p + ".ht", l.hToM); s.Bool(p + ".ok", l.ok); s.Bool(p + ".true", l.hasTrue); s.Num(p + ".err", l.errMm);
}
inline TrigLeg GetTrigLeg(const StateKV& s, const std::string& p) {
  TrigLeg l; l.from = s.GetStr(p + ".from"); l.to = s.GetStr(p + ".to"); l.distM = s.GetNum(p + ".d"); l.vFwdGon = s.GetNum(p + ".vf"); l.vBackGon = s.GetNum(p + ".vb"); l.dhFwdMm = s.GetNum(p + ".df"); l.dhBackMm = s.GetNum(p + ".db");
  l.sumMm = s.GetNum(p + ".sum"); l.meanMm = s.GetNum(p + ".mean"); l.tolMm = s.GetNum(p + ".tol"); l.hFromM = s.GetNum(p + ".hf"); l.hToM = s.GetNum(p + ".ht"); l.ok = s.GetBool(p + ".ok", true); l.hasTrue = s.GetBool(p + ".true"); l.errMm = s.GetNum(p + ".err");
  return l;
}

inline void PutStand(StateKV& s, const std::string& p, const Stand& st) {
  s.Int(p + ".bk", st.backMm); s.Int(p + ".fr", st.foreMm); s.Num(p + ".sb", st.sightBackCm); s.Num(p + ".sf", st.sightForeCm);
  s.Num(p + ".tb", st.stadiaBackCm); s.Num(p + ".tf", st.stadiaForeCm); s.Int(p + ".b2", st.back2Mm); s.Int(p + ".f2", st.fore2Mm);
}
inline Stand GetStand(const StateKV& s, const std::string& p) {
  Stand st{s.GetInt(p + ".bk"), s.GetInt(p + ".fr"), s.GetNum(p + ".sb"), s.GetNum(p + ".sf")};
  st.stadiaBackCm = s.GetNum(p + ".tb", -1); st.stadiaForeCm = s.GetNum(p + ".tf", -1); st.back2Mm = s.GetInt(p + ".b2", -1); st.fore2Mm = s.GetInt(p + ".f2", -1);
  return st;
}

template <class T, class F>
void PutVec(StateKV& s, const std::string& p, const std::vector<T>& v, F put) {
  s.Int(p + ".n", (long long)v.size());
  for (size_t i = 0; i < v.size(); i++) put(s, p + "." + std::to_string(i), v[i]);
}
template <class T, class F>
std::vector<T> GetVec(const StateKV& s, const std::string& p, F get) {
  std::vector<T> v; const int n = s.GetInt(p + ".n");
  for (int i = 0; i < n; i++) v.push_back(get(s, p + "." + std::to_string(i)));
  return v;
}
