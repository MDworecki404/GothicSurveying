// Stawianie obiektów pod stopami bohatera. Dołączane raz z Plugin.hpp, bez strażnika include.
namespace GOTHIC_NAMESPACE {
  inline const char*  LV_VIS_INSTR = "LVL_INSTR.3DS";   // modele i tekstury: assets/make_models.py
  inline const char*  LV_VIS_ROD   = "LVL_ROD.3DS";
  inline const char*  LV_VIS_PLATE = "LVL_PLATE.3DS";
  inline const float  LV_INSTR_H   = 150.0f;            // wysokość osi lunety nad gruntem [cm]
  inline const float  LV_PLATE_H   = 4.5f;              // wysokość żabki [cm]

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
    v->Release();
    return v;
  }

  inline void RemoveVobSafe( zCVob*& v ) {
    if ( !v ) return;
    if ( zCWorld* w = ogame->GetWorld() ) w->RemoveVob( v );
    v = nullptr;
  }
}
