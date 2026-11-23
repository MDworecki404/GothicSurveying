#include <cassert>
#include <cmath>
#include <cstdio>
#include "calc/State.hpp"

static void kv_roundtrip_with_special_chars() {
  StateKV s;
  s.Str("a", "tekst; z = znakami\nnowa linia i 100% oraz \r"); s.Num("n", 1.0 / 3.0); s.Int("i", -42); s.Bool("b", true); s.Str("empty", "");
  const StateKV r = StateKV::FromText(s.ToText());
  assert(r.GetStr("a") == "tekst; z = znakami\nnowa linia i 100% oraz \r" && r.GetNum("n") == 1.0 / 3.0 && r.GetInt("i") == -42 && r.GetBool("b") && r.GetStr("empty").empty());
  assert(r.GetStr("none", "def") == "def" && r.GetInt("none", 7) == 7 && !r.Has("none"));
  StateKV outer; outer.Str("inner", s.ToText());                                       // zagniezdzenie (stan odbiornika w robocie tachimetrycznej)
  const StateKV in2 = StateKV::FromText(StateKV::FromText(outer.ToText()).GetStr("inner"));
  assert(in2.GetStr("a") == s.GetStr("a") && in2.GetNum("n") == s.GetNum("n"));
}

static void struct_serializers_roundtrip() {
  StateKV s;
  ControlPoint c; c.id = "G07"; c.world = "NEWWORLD"; c.n = 5500012.345; c.e = 6500001.5; c.h = 3.25; c.x = 100; c.y = 325; c.z = 1234.5; c.solution = "osnowa wzorcowa: Cmentarz;x"; c.pdop = 1.8; c.epochs = 25; c.timeText = "09:00:01";
  PutControl(s, "c", c); const ControlPoint c2 = GetControl(StateKV::FromText(s.ToText()), "c");
  assert(c2.id == c.id && c2.solution == c.solution && c2.n == c.n && c2.z == c.z && c2.epochs == 25);
  std::vector<Stand> st = {{1500, 1200, 3000, 3000, 2990, 3010}, {1400, 1650, 2500, 2400}};
  st[1].back2Mm = 1402; st[1].fore2Mm = 1652;
  StateKV t; PutVec(t, "st", st, PutStand);
  const std::vector<Stand> st2 = GetVec<Stand>(StateKV::FromText(t.ToText()), "st", GetStand);
  assert(st2.size() == 2 && st2[0].stadiaBackCm == 2990 && st2[1].back2Mm == 1402 && st2[1].stadiaForeCm == -1 && st2[0].back2Mm == -1);
}

int main() {
  kv_roundtrip_with_special_chars(); struct_serializers_roundtrip();
  std::puts("test_state OK");
  return 0;
}
