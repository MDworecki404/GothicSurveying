// Sprawdzenie: g++ tests/test_level_math.cpp -o t && ./t   (albo cl /EHsc test_level_math.cpp)
#include <cassert>
#include <cstdio>
#include "../src/LevelMath.hpp"

int main() {
  const double instrLine = 150.0;                       // linia celowa 150 cm nad gruntem stanowiska A (y=0)
  double rA = LevelReading( instrLine, 0.0, 2000, 0, 0 );   // łata na A (reper, y=0)
  double rB = LevelReading( instrLine, 50.0, 2000, 0, 0 );  // łata na B, 50 cm wyżej
  double hA = 100000.0;                                 // reper A: 1000.00 m
  double horizon = LevelHorizon( hA, rA );
  double hB = LevelForeHeight( horizon, rB );
  assert( std::fabs( hB - 100050.0 ) < 1e-9 );          // B = A + 50 cm
  // błąd kolimacji 20" na 20 m daje ~0.19 cm; na jednakowych odległościach BS/FS się znosi
  double cB = LevelReading( instrLine, 50.0, 2000, 20, 0 ) - rB;
  assert( std::fabs( cB - 0.1939 ) < 1e-3 );
  double hB2 = LevelForeHeight( LevelHorizon( hA, LevelReading( instrLine, 0.0, 2000, 20, 0 ) ),
                                LevelReading( instrLine, 50.0, 2000, 20, 0 ) );
  assert( std::fabs( hB2 - 100050.0 ) < 1e-9 );
  // dalmierz: celowa 20 m = 2000 cm, środek 147,8 cm -> kreski 157,8 i 137,8 cm, odległość z nich = 2000 cm
  double up = LevelStadiaReading( 147.8, 2000, true ), lo = LevelStadiaReading( 147.8, 2000, false );
  assert( std::fabs( up - 157.8 ) < 1e-9 && std::fabs( lo - 137.8 ) < 1e-9 );
  assert( std::fabs( LevelStadiaDist( up, lo ) - 2000.0 ) < 1e-9 );
  assert( std::fabs( LevelStadiaDist( lo, up ) - 2000.0 ) < 1e-9 );   // kolejność odczytów nie gra roli
  std::puts( "OK" );
  return 0;
}
