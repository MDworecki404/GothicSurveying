// Tymczasowa klejka (Task 5): stare klawisze F7-F11 na nowych modułach silnika. Zastępowana kontrolerem w Tasku 6.
// F7 niwelator, F8 łata, F9 żabka (pod stopami), F10 luneta, F11 usuń wszystko. Odczyty tylko wypisywane w HUD.
#include <vector>

namespace GOTHIC_NAMESPACE {
  struct LvRod { zVEC3 foot; zCVob* vob; int id; };

  inline zCWorld*  lvWorld = nullptr;
  inline zCVob*    lvInstrVob = nullptr;
  inline zVEC3     lvInstr;
  inline bool      lvHasInstr = false;
  inline int       lvRodCount = 0;
  inline std::vector<LvRod> lvRods;
  inline std::vector<std::pair<zVEC3, zCVob*>> lvPlates;

  inline void LvForget() {   // voby znikają razem ze światem
    ScopeLeave();
    lvRods.clear(); lvPlates.clear(); lvInstrVob = nullptr; lvHasInstr = false; lvRodCount = 0;
  }

  inline void LvClear() {
    ScopeLeave();
    RemoveVobSafe( lvInstrVob );
    for ( auto& r : lvRods ) RemoveVobSafe( r.vob );
    for ( auto& p : lvPlates ) RemoveVobSafe( p.second );
    LvForget();
  }

  inline void LvEnterScope() {
    if ( !lvHasInstr ) { HudMessage( "Najpierw postaw niwelator (F7)" ); return; }
    ScopeCallbacks cb;
    cb.targets = []() { std::vector<RodTarget> t; for ( auto& r : lvRods ) t.push_back( { r.id, r.foot } ); return t; };
    cb.onReading = []( int pointId, int midMm, int upMm, int loMm ) {
      char b[120]; sprintf( b, "Lata %d: odczyt %d mm (gorna %d, dolna %d)", pointId, midMm, upMm, loMm ); HudMessage( b ); return true;
    };
    cb.onEnter = []() {
      for ( auto& r : lvRods ) {                     // łaty zwrócone do instrumentu
        zVEC3 d( lvInstr[VX] - r.foot[VX], 0, lvInstr[VZ] - r.foot[VZ] );
        if ( d.Length() > 0.1f ) { d.Normalize(); r.vob->SetHeadingAtWorld( d ); }
      }
      if ( lvInstrVob ) lvInstrVob->showVisual = 0;
    };
    cb.onLeave = []() { if ( lvInstrVob ) lvInstrVob->showVisual = 1; };
    ScopeEnter( lvInstr, cb );
  }

  inline void LevelLoop() {
    if ( ogame->GetWorld() != lvWorld ) { LvForget(); lvWorld = ogame->GetWorld(); }
    if ( !lvWorld ) return;
    ScopeTick();
    HudTick();
    if ( ScopeActive() ) return;

    zVEC3 p;
    if ( zinput->KeyToggled( KEY_F7 ) && FeetGround( p ) ) {
      RemoveVobSafe( lvInstrVob );
      lvInstr = p; lvInstrVob = SpawnVob( LV_VIS_INSTR, p ); lvHasInstr = true;
      HudMessage( "Niwelator postawiony (F10 - widok lunety)" );
    }
    if ( zinput->KeyToggled( KEY_F9 ) && FeetGround( p ) ) lvPlates.push_back( { p, SpawnVob( LV_VIS_PLATE, p ) } );
    if ( zinput->KeyToggled( KEY_F8 ) && FeetGround( p ) ) {
      for ( auto& pl : lvPlates ) {
        float dx = pl.first[VX] - p[VX], dz = pl.first[VZ] - p[VZ];
        if ( sqrtf( dx * dx + dz * dz ) < 30.0f ) { p = pl.first; p[VY] += LV_PLATE_H; break; }
      }
      lvRods.push_back( { p, SpawnVob( LV_VIS_ROD, p ), ++lvRodCount } );
      HudMessage( "Lata postawiona" );
    }
    if ( zinput->KeyToggled( KEY_F10 ) ) LvEnterScope();
    if ( zinput->KeyToggled( KEY_F11 ) ) LvClear();
  }
}
