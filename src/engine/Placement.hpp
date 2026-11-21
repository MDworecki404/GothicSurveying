// Stawianie obiektów pod stopami bohatera. Dołączane raz z Plugin.hpp, bez strażnika include.
namespace GOTHIC_NAMESPACE {
  inline const char*  LV_VIS_INSTR = "LVL_INSTR.3DS";   // modele i tekstury: assets/make_models.py
  inline const char*  LV_VIS_ROD   = "LVL_ROD.3DS";
  inline const char*  LV_VIS_PLATE = "LVL_PLATE.3DS";
  inline const char*  LV_VIS_REPER = "LVL_REPER.3DS";
  inline const char*  LV_VIS_POLE  = "LVL_POLE.3DS";     // tyczka z odbiornikiem GNSS (2,00 m + antena)
  inline const float  LV_POLE_H    = 200.0f;             // wysokość tyczki [cm]; promienie do nieba idą od jej szczytu
  inline const char*  LV_VIS_HELMET = "LVL_HELMET.3DS";   // kask i kamizelka odblaskowa bohatera (doczepiane do węzłów szkieletu)
  inline const char*  LV_VIS_VEST  = "LVL_VEST.3DS";
  inline const float  LV_INSTR_H   = 150.0f;            // wysokość osi lunety nad gruntem [cm]
  inline const float  LV_PLATE_H   = 4.5f;              // wysokość żabki [cm]
  inline const float  LV_REPER_H   = 12.0f;             // wysokość znaku reperu [cm]; łata staje na jego szczycie

  // Grunt pod stopami bohatera (voby ignorowane -> tylko teren).
  inline bool FeetGround( zVEC3& hit ) {
    zCWorld* w = ogame->GetWorld();
    if ( !player || !w ) return false;
    if ( !w->TraceRayNearestHit( player->GetPositionWorld(), zVEC3( 0, -3000, 0 ), (zCVob*)player,
                                 zTRACERAY_VOB_IGNORE | zTRACERAY_POLY_IGNORE_TRANSP ) )
      return false;
    hit = w->traceRayReport.foundIntersection;
    return true;
  }

  inline zCVob* SpawnVob( const char* visual, const zVEC3& p ) {
    zCVob* v = new zCVob();
    v->SetVisual( visual );
    v->SetCollDet( 0 );                                  // nie blokuje promieni ani gracza
    v->SetPositionWorld( p );
    ogame->GetWorld()->AddVob( v );
    return v;                                            // zachowujemy własną referencję: wskaźnik zostaje ważny także po zwolnieniu świata
  }

  // Vob nie należy już do bieżącego świata (np. po wczytaniu zapisu świat zwolnił vob; zostaje tylko nasza referencja).
  inline bool VobIsStale( zCVob* v ) { return v && v->GetHomeWorld() != ogame->GetWorld(); }

  inline void RemoveVobSafe( zCVob*& v ) {
    if ( !v ) return;
    zCWorld* w = ogame->GetWorld();
    if ( w && v->GetHomeWorld() == w ) w->RemoveVob( v );
    v->Release();                                        // oddajemy własną referencję
    v = nullptr;
  }
  inline void DropVobRef( zCVob*& v ) { if ( v ) { v->Release(); v = nullptr; } }   // bez dotykania świata
}
