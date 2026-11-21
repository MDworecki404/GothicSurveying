// Sprawdzenie: g++ -Isrc tests/test_level_math.cpp -o t && ./t   (albo cl /EHsc test_level_math.cpp)
#include <cassert>
#include <cstdio>
#include "calc/LevelMath.hpp"

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
  // dalmierz z prawdziwej celowej: 20 m, środek 1500 mm -> kreski 1600 i 1400 mm, D = 20,0 m
  assert( std::fabs( LevelStadiaMeasured( 1500, 2000 ) - 2000.0 ) < 1e-9 );
  // kreski zaokrąglone do 1 mm jak przy odczycie na oko: 20,25 m -> 101,25 mm od środka -> 1601 i 1399 mm -> D = 20,2 m
  assert( std::fabs( LevelStadiaMeasured( 1500, 2003 ) - 2000.0 ) < 1e-9 );
  assert( std::fabs( LevelStadiaMeasured( 1500, 2025 ) - 2020.0 ) < 1e-9 );
  assert( std::fabs( LevelStadiaMeasured( 1234, 3000 ) - 3000.0 ) < 1e-9 );
  std::puts( "OK" );
  return 0;
}
