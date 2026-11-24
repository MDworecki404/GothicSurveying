// Stawianie obiektów pod stopami bohatera. Dołączane raz z Plugin.hpp, bez strażnika include.
namespace GOTHIC_NAMESPACE {
  inline const char*  LV_VIS_INSTR = "LVL_INSTR.3DS";   // modele i tekstury: assets/make_models.py
  inline const char*  LV_VIS_ROD[3] = { "LVL_ROD.3DS", "LVL_RODL4.3DS", "LVL_RODL5.3DS" };   // łata wysunięta na 3, 4 i 5 m
  inline const char*  LV_VIS_PLATE = "LVL_PLATE.3DS";
  inline const char*  LV_VIS_REPER = "LVL_REPER.3DS";
  inline const char*  LV_VIS_SCANNER = "LVL_SCANNER.3DS";   // skaner laserowy na statywie (os 150 cm nad gruntem)
  inline const char*  LV_VIS_SCANHEAD = "LVL_SCANHEAD.3DS";   // obrotowa glowica skanera (osobny vob)
  inline const char*  LV_VIS_DRONE = "LVL_DRONE.3DS";   // dron ALS: korpus z glowica LiDAR (poczatek ukladu = antena GNSS)
  inline const char*  LV_VIS_PROP = "LVL_PROP.3DS";     // smiglo drona (osobny vob, obraca sie)
  inline const char*  LV_VIS_SPHERE = "LVL_SPHERE.3DS";   // kula referencyjna skanera
  inline const char*  LV_VIS_TACHY = "LVL_TACHY.3DS";    // tachimetr na statywie (os lunety 150 cm nad gruntem)
  // Tyczka z lustrem: model o wysokosci srodka lustra h (1,3 / 1,5 / 1,8 / 2,0 m).
  inline const char* PrismModel( double h ) { return h < 1.4 ? "LVL_PRISM13.3DS" : h < 1.65 ? "LVL_PRISM15.3DS" : h < 1.9 ? "LVL_PRISM18.3DS" : "LVL_PRISM20.3DS"; }
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
    v->dontWriteIntoArchive = 1;                         // nie trafia do zapisu gry (inaczej po wczytaniu zostaja duplikaty / duchy)
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
