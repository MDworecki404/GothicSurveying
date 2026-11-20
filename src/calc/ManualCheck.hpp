// Sprawdzanie dziennika wypełnionego przez gracza (tryb samodzielny) i eksport z werdyktem. Bez Gothic API.
#pragma once
#include <cmath>
#include <limits>
#include <string>
#include <vector>
#include "FieldBook.hpp"

struct PlayerEntries {                       // wpisy gracza; NaN = nie wpisano
  std::vector<double> dhMm, vMm, heightM;    // po jednym na stanowisko; heightM = wysokość punktu wprzód
  double sumDhMm = std::numeric_limits<double>::quiet_NaN();
  double fhMm = std::numeric_limits<double>::quiet_NaN();
};

struct CellVerdict { double correct = 0, player = 0, diff = 0; bool entered = false, ok = false; };   // diff w mm

struct ManualVerdict {
  std::vector<CellVerdict> dh, v, height;
  CellVerdict sumDh, fh;
  bool hasClosure = false;
  int ok = 0, total = 0;
};

namespace mcdetail {
inline CellVerdict Cell(double correct, double player, bool inMeters, double tolMm) {
  CellVerdict c; c.correct = correct; c.player = player; c.entered = !std::isnan(player);
  if (c.entered) { c.diff = (player - correct) * (inMeters ? 1000.0 : 1.0); c.ok = std::fabs(c.diff) <= tolMm + 1e-9; }
  return c;
}
inline double At(const std::vector<double>& v, size_t i) { return i < v.size() ? v[i] : std::numeric_limits<double>::quiet_NaN(); }
}  // namespace mcdetail

inline ManualVerdict CheckManual(const FieldBook& fb, const PlayerEntries& p, double tolMm = 1.0) {
  using namespace mcdetail;
  ManualVerdict r; r.hasClosure = fb.sum.hasClosure;
  for (size_t i = 0; i < fb.rows.size(); i++) {
    r.dh.push_back( Cell( fb.rows[i].dhMm, At( p.dhMm, i ), false, tolMm ) );
    if ( r.hasClosure ) r.v.push_back( Cell( fb.rows[i].vMm, At( p.vMm, i ), false, tolMm ) );
    r.height.push_back( Cell( fb.rows[i].heightM, At( p.heightM, i ), true, tolMm ) );
  }
  r.sumDh = Cell( fb.sum.sumDhMm, p.sumDhMm, false, tolMm );
  if ( r.hasClosure ) r.fh = Cell( fb.sum.fhMm, p.fhMm, false, tolMm );
  auto count = [&]( const CellVerdict& c ) { r.total++; if ( c.ok ) r.ok++; };
  for ( auto& c : r.dh ) count( c );
  for ( auto& c : r.v ) count( c );
  for ( auto& c : r.height ) count( c );
  count( r.sumDh ); if ( r.hasClosure ) count( r.fh );
  return r;
}

// Wiersze tabeli „Sprawdzenie”: komorka, poprawna, twoja, roznica [mm], ok (tak/nie). Wspólne dla CSV i DOCX.
inline std::vector<std::vector<std::string>> VerdictRows(const ManualVerdict& v) {
  using fbdetail::Num;
  std::vector<std::vector<std::string>> rows;
  auto add = [&]( const std::string& name, const CellVerdict& c, int decimals ) {
    rows.push_back( { name, Num( c.correct, decimals ), c.entered ? Num( c.player, decimals ) : "", c.entered ? Num( c.diff, 1 ) : "", c.ok ? "tak" : "nie" } );
  };
  for ( size_t i = 0; i < v.dh.size(); i++ ) add( "dh_" + std::to_string( i + 1 ) + "_mm", v.dh[i], 1 );
  for ( size_t i = 0; i < v.v.size(); i++ ) add( "v_" + std::to_string( i + 1 ) + "_mm", v.v[i], 1 );
  for ( size_t i = 0; i < v.height.size(); i++ ) add( "H_" + std::to_string( i + 1 ) + "_m", v.height[i], 3 );
  add( "suma_dh_mm", v.sumDh, 1 );
  if ( v.hasClosure ) add( "fh_mm", v.fh, 1 );
  return rows;
}

inline std::string ToCsvWithVerdict(const FieldBook& fb, const ManualVerdict& v) {
  std::string o = ToCsv(fb) + "\nsprawdzenie\nkomorka;poprawna;twoja;roznica;ok\n";
  for (const auto& r : VerdictRows(v)) o += r[0] + ";" + r[1] + ";" + r[2] + ";" + r[3] + ";" + r[4] + "\n";
  o += "wynik_sprawdzenia;" + std::to_string(v.ok) + "/" + std::to_string(v.total) + "\n";
  return o;
}
