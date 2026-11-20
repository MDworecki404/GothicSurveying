// Widok przez lunetę niwelatora: kamera w osi lunety, zbliżenie, krzyż i kreski dalmiercze, obrót, wybór łaty pod kreską,
// wpisywanie odczytów (środkowa, górna, dolna kreska). Dołączane raz z Plugin.hpp, bez strażnika include.
#include <cmath>
#include <functional>
#include <vector>
#include "calc/LevelMath.hpp"

namespace GOTHIC_NAMESPACE {
  inline const char*  LV_RETICLE   = "LVL_RETICLE.TGA";
  inline const float  LV_ROD_LEN   = 300.0f;   // długość łaty [cm]
  inline const float  LV_ROD_HALF  = 5.0f;     // pionowa kreska musi leżeć na łacie (pół szerokości 6 cm, z zapasem) [cm]
  inline const float  LV_MAX_DIST  = 6000.0f;  // maks. celowa [cm]
  inline const float  LV_ZOOM      = 20.0f;    // powiększenie lunety
  inline const float  LV_TURN_FAST = 12.0f;    // obrót niwelatora [stopnie/s]
  inline const float  LV_TURN_SLOW = 1.0f;     // obrót powolny (Shift) [stopnie/s]
  inline const float  LV_STADIA_K  = 100.0f;   // stała dalmierza nitkowego: odległość = k * (górny - dolny odczyt)

  struct RodTarget { int pointId; zVEC3 foot; };
  struct ScopeCallbacks {
    std::function<std::vector<RodTarget>()> targets;                           // łaty do wyboru pod pionową kreską
    std::function<bool( int pointId, int midMm, int upMm, int loMm )> onReading;  // false = odczyt odrzucony
    std::function<void()> onEnter, onLeave;                                    // np. ukrycie niwelatora, obrót łat
  };

  inline bool      gScopeOn = false, gEntry = false, gFovSaved = false, gHairsDone = false;
  inline float     gAz = 0, gFovX = 0, gFovY = 0;      // azymut osi lunety [rad]: kierunek (sin az, 0, cos az)
  inline DWORD     gLastTick = 0;
  inline zVEC3     gInstrGround;
  inline zCView   *gReticle = nullptr, *gHairU = nullptr, *gHairL = nullptr, *gEntryPanel = nullptr;
  inline ScopeCallbacks gCb;
  inline int       gEntryPoint = -1, gEntryLen = 0, gEntryMm = 0, gEntryStep = 0, gMidMm = 0, gUpMm = 0;

  inline bool ScopeActive() { return gScopeOn; }

  // Zbliżenie lunety; poza widokiem przywraca FOV gry.
  inline void ScopeApplyZoom() {
    zCCamera* c = zCCamera::activeCam;
    if ( !c ) return;
    if ( gScopeOn ) {
      if ( !gFovSaved ) { c->GetFOV( gFovX, gFovY ); gFovSaved = true; }
      const float rad = 3.14159265f / 180.0f;
      c->SetFOV( (float)( 2 * atan( tan( gFovX * rad / 2 ) / LV_ZOOM ) / rad ), (float)( 2 * atan( tan( gFovY * rad / 2 ) / LV_ZOOM ) / rad ) );
    } else if ( gFovSaved ) {
      c->SetFOV( gFovX, gFovY ); gFovSaved = false;
    }
  }

  // Kreski dalmiercze: tg(kąt między nimi) = 1/k, każda leży o pół tego kąta od osi. Na ekranie (pion = 8192) odstęp od środka
  // wynosi 4096 * (1/(2k)) / tg(pionowe FOV zbliżenia / 2) = 4096 * zoom / (2k * tg(pionowe FOV gry / 2)).
  inline void ScopeOpenHairs() {
    const float rad = 3.14159265f / 180.0f;
    int off = (int)( 4096.0 * LV_ZOOM / ( 2.0 * LV_STADIA_K * tan( gFovY * rad / 2 ) ) );
    if ( off < 40 || off > 3600 ) { SvTrace( "kreski dalmiercze poza zakresem ekranu - pominiete" ); return; }
    auto hair = [&]( int y ) {
      zCView* v = new zCView( 3500, y - 11, 4700, y + 11 );
      v->InsertBack( "LVL_HAIR.TGA" );
      v->SetAlphaBlendFunc( zRND_ALPHA_FUNC_BLEND );
      screen->InsertItem( v ); v->Open();
      return v;
    };
    gHairU = hair( 4096 - off ); gHairL = hair( 4096 + off );
  }

  inline void ScopeLeave() {
    if ( !gScopeOn ) return;
    gScopeOn = gEntry = false;
    ScopeApplyZoom();                                // przywraca FOV
    PanelClose( gHairU ); PanelClose( gHairL ); PanelClose( gEntryPanel ); gHairsDone = false;
    if ( gReticle ) { gReticle->Close(); screen->RemoveItem( gReticle ); delete gReticle; gReticle = nullptr; }
    HudClose();
    if ( player ) player->showVisual = 1;
    if ( gCb.onLeave ) gCb.onLeave();
    ogame->Unpause();
  }

  inline void ScopeEnter( const zVEC3& instrumentGround, const ScopeCallbacks& cb ) {
    zCVob* cam = ogame->GetCameraVob();
    if ( !cam || gScopeOn ) return;
    gCb = cb; gInstrGround = instrumentGround;
    zVEC3 at = cam->GetAtVectorWorld();
    gAz = atan2f( at[VX], at[VZ] );                  // zaczynamy tam, gdzie patrzył gracz
    if ( player ) player->showVisual = 0;            // kamera jest w osi lunety - nic nie przesłania widoku
    if ( gCb.onEnter ) gCb.onEnter();
    HudClose();
    gReticle = new zCView( 0, 0, 8192, 8192 );
    gReticle->InsertBack( LV_RETICLE );
    gReticle->SetAlphaBlendFunc( zRND_ALPHA_FUNC_BLEND );
    screen->InsertItem( gReticle );
    gReticle->Open();
    gScopeOn = true; gEntry = false; gLastTick = GetTickCount();
    ogame->Pause();
    SvTrace( "widok lunety: wlaczony" );
  }

  // Enter w widoku: łata pod pionową kreską krzyża, sprawdź celową i przejdź do wpisywania odczytów.
  inline void ScopeBeginReading() {
    zCWorld* world = ogame->GetWorld();
    const zVEC3 dir( sinf( gAz ), 0, cosf( gAz ) );
    std::vector<RodTarget> targets = gCb.targets ? gCb.targets() : std::vector<RodTarget>();
    int best = -1; double bestLat = 1e9, bestFwd = 0;
    for ( size_t i = 0; i < targets.size(); i++ ) {
      double dx = targets[i].foot[VX] - gInstrGround[VX], dz = targets[i].foot[VZ] - gInstrGround[VZ];
      double fwd = dx * dir[VX] + dz * dir[VZ], lat = fabs( dx * dir[VZ] - dz * dir[VX] );
      if ( fwd > 100.0 && fwd < LV_MAX_DIST && lat < LV_ROD_HALF && lat < bestLat ) { best = (int)i; bestLat = lat; bestFwd = fwd; }
    }
    if ( best < 0 ) { HudMessage( "Pionowa kreska nie lezy na lacie" ); return; }
    const RodTarget& r = targets[best];

    double lineY = gInstrGround[VY] + LV_INSTR_H;
    zVEC3 from( gInstrGround[VX], (float)lineY, gInstrGround[VZ] );
    zVEC3 to = zVEC3( r.foot[VX], (float)lineY, r.foot[VZ] ) - from;
    float len = to.Length();
    if ( len > 40.0f ) {   // przeszkoda na poziomej linii celowej, tuz przed lata
      to = to * ( ( len - 30.0f ) / len );
      if ( world->TraceRayNearestHit( from, to, (zCVob*)nullptr,
           zTRACERAY_VOB_IGNORE_NO_CD_DYN | zTRACERAY_VOB_IGNORE_CHARACTER | zTRACERAY_POLY_IGNORE_TRANSP ) ) {
        HudMessage( "Linia celowa zaslonieta" ); return;
      }
    }
    double ideal = LevelReading( lineY, r.foot[VY], bestFwd, 0.0, 0.0 );
    if ( ideal < 0 || ideal > LV_ROD_LEN ) { HudMessage( "Pozioma kreska jest poza lata (nad nia lub pod nia)" ); return; }

    gEntry = true; gEntryPoint = r.pointId; gEntryLen = 0; gEntryMm = 0; gEntryStep = 0;
    HudClose();
    gEntryPanel = PanelOpen( 2000, 5500, 6200, 7500 );   // pole wpisywania na dole lunety
  }

  inline void ScopeFinishEntry( int upMm, int loMm ) {
    gEntry = false; PanelClose( gEntryPanel );
    if ( !gCb.onReading || !gCb.onReading( gEntryPoint, gMidMm, upMm, loMm ) ) HudMessage( "Odczyt odrzucony (ta lata juz ma odczyt albo nie jest na tym stanowisku)" );
  }

  inline void ScopeTick() {
    ScopeApplyZoom();
    if ( !gScopeOn ) return;
    DWORD now = GetTickCount();
    float dt = ( now - gLastTick ) / 1000.0f; gLastTick = now;
    if ( dt > 0.1f ) dt = 0.1f;
    if ( !gHairsDone && gFovSaved ) { gHairsDone = true; ScopeOpenHairs(); }     // FOV gry znany dopiero po pierwszym zastosowaniu zbliżenia

    if ( zinput->KeyToggled( KEY_F10 ) ) {           // F10: anuluj wpisywanie, a gdy go nie ma - wyjdź z widoku
      if ( gEntry ) { gEntry = false; PanelClose( gEntryPanel ); } else { ScopeLeave(); return; }
    }

    if ( gEntry ) {
      static const int digitKeys[10] = { KEY_0, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9 };
      for ( int d = 0; d < 10; d++ )
        if ( zinput->KeyToggled( digitKeys[d] ) && gEntryLen < 4 ) { gEntryMm = gEntryMm * 10 + d; gEntryLen++; }
      if ( zinput->KeyToggled( KEY_BACK ) && gEntryLen > 0 ) { gEntryMm /= 10; gEntryLen--; }
      if ( zinput->KeyToggled( KEY_RETURN ) ) {
        // krok 0: kreska środkowa (wymagana), kroki 1-2: kreski dalmiercze (Enter bez liczby pomija odległość)
        bool valid = gEntryLen > 0 && gEntryMm <= 3000;
        if ( gEntryStep == 0 && valid ) { gMidMm = gEntryMm; gEntryStep = 1; gEntryLen = gEntryMm = 0; }
        else if ( gEntryStep == 1 && valid ) { gUpMm = gEntryMm; gEntryStep = 2; gEntryLen = gEntryMm = 0; }
        else if ( gEntryStep == 2 && valid ) ScopeFinishEntry( gUpMm, gEntryMm );
        else if ( gEntryStep > 0 && gEntryLen == 0 ) ScopeFinishEntry( -1, -1 );
      }
      if ( gEntry && gEntryPanel ) {
        static const char* titles[3] = { "srodkowa kreska [mm]:", "gorna kreska dalmiercza [mm]:", "dolna kreska dalmiercza [mm]:" };
        char b[96];
        sprintf( b, "Lata %d - %s", gEntryPoint, titles[gEntryStep] );
        gEntryPanel->ClrPrintwin();
        gEntryPanel->PrintCX( 700, zSTRING( b ) );
        sprintf( b, "%d", gEntryMm );
        gEntryPanel->PrintCX( 3500, zSTRING( gEntryLen ? b : "_" ) );       // wpisana wartość
        gEntryPanel->PrintCX( 6300, zSTRING( gEntryStep ? "Enter bez liczby - pomin odleglosc   F10 - anuluj" : "Enter - zatwierdz   F10 - anuluj" ) );
      }
    } else {
      const float rate = ( zinput->KeyPressed( KEY_LSHIFT ) ? LV_TURN_SLOW : LV_TURN_FAST ) * 3.14159265f / 180.0f;
      if ( zinput->KeyPressed( KEY_LEFT ) )  gAz -= rate * dt;
      if ( zinput->KeyPressed( KEY_RIGHT ) ) gAz += rate * dt;
      if ( zinput->KeyToggled( KEY_RETURN ) ) ScopeBeginReading();
    }

    // kamera w osi lunety, patrzy poziomo w kierunku azymutu
    zCVob* cam = ogame->GetCameraVob();
    if ( cam ) {
      cam->SetPositionWorld( zVEC3( gInstrGround[VX], gInstrGround[VY] + LV_INSTR_H, gInstrGround[VZ] ) );
      cam->SetHeadingAtWorld( zVEC3( sinf( gAz ), 0, cosf( gAz ) ) );
    }
  }
}
