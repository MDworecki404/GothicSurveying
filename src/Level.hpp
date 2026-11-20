// Niwelacja geometryczna w Gothic II NotR (Union API plugin, x86). Wywoływane z Plugin.hpp (hook zCWorld::Render).
//
// Sterowanie (sprzęt staje pod stopami bohatera):
//   F7   postaw niwelator (nowe stanowisko; kasuje horyzont, łaty zostają)
//   F8   postaw łatę (pierwsza łata = reper o znanej, prawdziwej wysokości, kolejne = punkty do wyznaczenia)
//   F9   postaw żabkę (łata postawiona < 30 cm od żabki stoi na niej)
//   F10  wejdź do widoku niwelatora / wyjdź z niego (gra jest zatrzymana, bohater ukryty)
//   F11  usuń wszystko
// W widoku niwelatora (kamera w osi lunety, pozioma, zbliżenie, krzyż kresek):
//   strzałki lewo/prawo   obrót niwelatora w poziomie (Shift = obrót powolny, jak śruba leniwa)
//   Enter                 odczyt: pionowa kreska musi leżeć na łacie; wpisz odczyt z łaty w mm cyframi 0-9
//                         (Backspace kasuje, Enter zatwierdza, F10 anuluje wpisywanie)
//
// Odczyt: opis przy decymetrze to jego numer od podstawy łaty (14 = 1,40 m, czerwone cyfry/E to nieparzyste metry),
// dalej liczysz centymetry w literze E (5 cm: pręt, przerwa, pręt, przerwa, pręt po 1 cm), milimetry szacujesz
// tam, gdzie poziomą kreską przecina łatę krzyż kresek. Linia celowa jest zawsze pozioma (jak w niwelatorze).
// Pierwszy odczyt na stanowisku = wstecz (BS) na łacie o znanej wysokości, kolejne = w przód (FS).
// Wynik: komunikat na ekranie i niwelacja_N.csv w katalogu głównym gry (twój odczyt, odczyt idealny, błąd).
// Teksty w grze są bez polskich znaków (kodowanie). Wysokości w metrach z Y silnika; "prawda" = Y gruntu pod łatą.
#include "LevelMath.hpp"
#include <vector>
#include <cstdio>
#include <cmath>

namespace GOTHIC_NAMESPACE {
  const char*  LV_VIS_INSTR = "LVL_INSTR.3DS";   // modele i krzyż kresek: assets/make_models.py
  const char*  LV_VIS_ROD   = "LVL_ROD.3DS";
  const char*  LV_VIS_PLATE = "LVL_PLATE.3DS";
  const char*  LV_RETICLE   = "LVL_RETICLE.TGA";
  const float  LV_INSTR_H   = 150.0f;   // wysokość osi lunety nad gruntem [cm]
  const float  LV_PLATE_H   = 4.5f;     // wysokość żabki [cm]
  const float  LV_ROD_LEN   = 300.0f;   // długość łaty [cm]
  const float  LV_ROD_HALF  = 5.0f;     // pionowa kreska musi leżeć na łacie (pół szerokości 6 cm, z zapasem) [cm]
  const float  LV_MAX_DIST  = 6000.0f;  // maks. celowa [cm]
  const float  LV_COLLIM    = 0.0f;     // błąd kolimacji ["]; np. 20 pokazuje błąd systematyczny
  const float  LV_ZOOM      = 20.0f;    // powiększenie lunety
  const float  LV_TURN_FAST = 12.0f;    // obrót niwelatora [stopnie/s]
  const float  LV_TURN_SLOW = 1.0f;     // obrót powolny (Shift) [stopnie/s]
  const float  LV_STADIA_K  = 100.0f;   // stała dalmierza nitkowego: odległość = k * (górny - dolny odczyt)

  struct LvRod   { zVEC3 foot; zCVob* vob; bool known; double H; int id; };
  struct LvPlate { zVEC3 pos;  zCVob* vob; };

  zCWorld*              lvWorld  = nullptr;
  zCVob*                lvInstrVob = nullptr;
  zVEC3                 lvInstr;           // grunt pod instrumentem
  bool                  lvHasInstr = false, lvHasHorizon = false;
  double                lvHorizon = 0;     // [cm]
  int                   lvSetup = 0, lvRodCount = 0, lvFileNo = 0;
  std::vector<LvRod>    lvRods;
  std::vector<LvPlate>  lvPlates;
  FILE*                 lvLog = nullptr;

  // widok przez lunetę i wpisywanie odczytu
  bool    lvView = false, lvEntry = false, lvFovSaved = false;
  float   lvAz = 0;                        // azymut osi lunety [rad]: kierunek (sin az, 0, cos az)
  float   lvFovX = 0, lvFovY = 0;
  DWORD   lvLastTick = 0;
  zCView* lvReticle = nullptr;
  zCView* lvHairU = nullptr;               // kreski dalmiercze (górna i dolna), położenie zależy od FOV
  zCView* lvHairL = nullptr;
  bool    lvHairsDone = false;             // kreski już próbowano otworzyć w tym widoku
  int     lvEntryRod = -1, lvEntryLen = 0, lvEntryMm = 0;
  int     lvEntryStep = 0, lvMidMm = 0, lvUpMm = 0;   // krok: 0 środkowa, 1 górna, 2 dolna kreska
  double  lvEntryIdealCm = 0, lvEntryDist = 0;

  inline void LvTrace( const char* s ) {   // diagnostyka: GothicSurveying.log w katalogu roboczym gry
    FILE* f = fopen( "GothicSurveying.log", "a" );
    if ( f ) { fputs( s, f ); fputs( "\n", f ); fclose( f ); }
  }

  // Pole tekstowe: osobny widok z tłem (LVL_PANEL.TGA); tekst rysuje sam widok, więc widać go także nad lunetą.
  zCView* lvHud = nullptr;          // komunikaty (znika po kilku sekundach)
  zCView* lvEntryPanel = nullptr;   // pole wpisywania odczytu
  DWORD   lvHudUntil = 0;

  zCView* LvPanelOpen( int x0, int y0, int x1, int y1 ) {
    zCView* v = new zCView( x0, y0, x1, y1 );
    v->InsertBack( "LVL_PANEL.TGA" );
    v->SetAlphaBlendFunc( zRND_ALPHA_FUNC_BLEND );
    v->SetFont( "FONT_OLD_20_WHITE.TGA" );
    screen->InsertItem( v );
    v->Open();
    return v;
  }
  void LvPanelClose( zCView*& v ) {
    if ( !v ) return;
    v->Close(); screen->RemoveItem( v ); delete v; v = nullptr;
  }

  inline void LvMsg( const char* s ) {
    LvTrace( s );
    LvPanelClose( lvHud );
    lvHud = LvPanelOpen( 400, 150, 7800, 900 );
    lvHud->PrintCXY( zSTRING( s ) );
    lvHudUntil = GetTickCount() + 7000;
  }

  zCVob* LvSpawn( const char* visual, const zVEC3& p ) {
    zCVob* v = new zCVob();
    v->SetVisual( visual );
    v->SetCollDet( 0 );            // nie blokuje promieni ani gracza
    v->SetPositionWorld( p );
    lvWorld->AddVob( v );
    v->Release();
    return v;
  }

  void LvApplyZoom() {
    zCCamera* c = zCCamera::activeCam;
    if ( !c ) return;
    if ( lvView ) {
      if ( !lvFovSaved ) { c->GetFOV( lvFovX, lvFovY ); lvFovSaved = true; }
      const float rad = 3.14159265f / 180.0f;
      c->SetFOV( (float)( 2 * atan( tan( lvFovX * rad / 2 ) / LV_ZOOM ) / rad ), (float)( 2 * atan( tan( lvFovY * rad / 2 ) / LV_ZOOM ) / rad ) );
    } else if ( lvFovSaved ) {
      c->SetFOV( lvFovX, lvFovY ); lvFovSaved = false;
    }
  }

  void LvLeaveView() {
    if ( !lvView ) return;
    lvView = lvEntry = false;
    LvApplyZoom();                                   // przywraca FOV
    LvPanelClose( lvHairU ); LvPanelClose( lvHairL ); lvHairsDone = false;
    if ( lvReticle ) { lvReticle->Close(); screen->RemoveItem( lvReticle ); delete lvReticle; lvReticle = nullptr; }
    LvPanelClose( lvEntryPanel ); LvPanelClose( lvHud );
    if ( player ) player->showVisual = 1;
    if ( lvInstrVob ) lvInstrVob->showVisual = 1;
    ogame->Unpause();
  }

  void LvForget() {   // voby znikają razem ze światem - tylko zapominamy listy
    LvLeaveView(); LvPanelClose( lvEntryPanel ); LvPanelClose( lvHud );
    lvRods.clear(); lvPlates.clear(); lvInstrVob = nullptr;
    lvHasInstr = lvHasHorizon = false; lvRodCount = lvSetup = 0;
  }

  void LvClear() {
    LvLeaveView();
    if ( lvWorld ) {
      if ( lvInstrVob ) lvWorld->RemoveVob( lvInstrVob );
      for ( auto& r : lvRods )   lvWorld->RemoveVob( r.vob );
      for ( auto& p : lvPlates ) lvWorld->RemoveVob( p.vob );
    }
    LvForget();
  }

  // Grunt pod stopami bohatera (voby ignorowane -> tylko teren).
  bool LvFeet( zVEC3& hit ) {
    if ( !player ) return false;
    if ( !lvWorld->TraceRayNearestHit( player->GetPositionWorld(), zVEC3( 0, -3000, 0 ), (zCVob*)player,
                                       zTRACERAY_VOB_IGNORE | zTRACERAY_POLY_IGNORE_TRANSP ) )
      return false;
    hit = lvWorld->traceRayReport.foundIntersection;
    return true;
  }

  // stadiaCm < 0 = nie wpisano odczytów kresek dalmierczych
  void LvLogRow( const char* kind, const LvRod& r, double userCm, double idealCm, double H, double distCm, double stadiaCm ) {
    if ( !lvLog ) {
      char name[64]; sprintf( name, "niwelacja_%d.csv", ++lvFileNo );
      lvLog = fopen( name, "w" );
      if ( lvLog ) fputs( "stanowisko;laty;typ;odczyt_m;odczyt_idealny_m;roznica_odczytu_mm;H_m;H_prawda_m;blad_H_mm;celowa_m;dalmierz_m;blad_dalmierza_m\n", lvLog );
    }
    if ( lvLog ) {
      double truth = r.foot[VY] / 100.0;
      fprintf( lvLog, "%d;%d;%s;%.3f;%.3f;%.1f;%.3f;%.3f;%.1f;%.1f;", lvSetup, r.id, kind, userCm / 100.0, idealCm / 100.0,
               ( userCm - idealCm ) * 10.0, H / 100.0, truth, ( H / 100.0 - truth ) * 1000.0, distCm / 100.0 );
      if ( stadiaCm >= 0 ) fprintf( lvLog, "%.1f;%.1f\n", stadiaCm / 100.0, ( stadiaCm - distCm ) / 100.0 );
      else fputs( ";\n", lvLog );
      fflush( lvLog );
    }
  }

  // Kreski dalmiercze: tg(kąt między nimi) = 1/k, każda leży o pół tego kąta od osi. Na ekranie (pion = 8192) odstęp od środka
  // wynosi 4096 * (1/(2k)) / tg(pionowe FOV zbliżenia / 2) = 4096 * zoom / (2k * tg(pionowe FOV gry / 2)).
  void LvOpenHairs() {
    const float rad = 3.14159265f / 180.0f;
    int off = (int)( 4096.0 * LV_ZOOM / ( 2.0 * LV_STADIA_K * tan( lvFovY * rad / 2 ) ) );
    if ( off < 40 || off > 3600 ) { LvTrace( "kreski dalmiercze poza zakresem ekranu - pominiete" ); return; }
    auto hair = [&]( int y ) {
      zCView* v = new zCView( 3500, y - 14, 4700, y + 14 );
      v->InsertBack( "LVL_HAIR.TGA" );
      v->SetAlphaBlendFunc( zRND_ALPHA_FUNC_BLEND );
      screen->InsertItem( v ); v->Open();
      return v;
    };
    lvHairU = hair( 4096 - off ); lvHairL = hair( 4096 + off );
  }

  void LvEnterView() {
    if ( !lvHasInstr ) { LvMsg( "Najpierw postaw niwelator (F7)" ); return; }
    zCVob* cam = ogame->GetCameraVob();
    if ( !cam ) return;
    zVEC3 at = cam->GetAtVectorWorld();
    lvAz = atan2f( at[VX], at[VZ] );                 // zaczynamy tam, gdzie patrzył gracz
    for ( auto& r : lvRods ) {                       // łaty zwrócone do instrumentu
      zVEC3 d( lvInstr[VX] - r.foot[VX], 0, lvInstr[VZ] - r.foot[VZ] );
      if ( d.Length() > 0.1f ) { d.Normalize(); r.vob->SetHeadingAtWorld( d ); }
    }
    if ( player ) player->showVisual = 0;            // kamera jest w osi lunety - nic nie przesłania widoku
    if ( lvInstrVob ) lvInstrVob->showVisual = 0;
    LvPanelClose( lvHud );
    lvReticle = new zCView( 0, 0, 8192, 8192 );
    lvReticle->InsertBack( LV_RETICLE );
    lvReticle->SetAlphaBlendFunc( zRND_ALPHA_FUNC_BLEND );
    screen->InsertItem( lvReticle );
    lvReticle->Open();
    lvView = true; lvEntry = false; lvLastTick = GetTickCount();
    ogame->Pause();
    LvTrace( "widok lunety: wlaczony" );
  }

  // Enter w widoku: łata pod pionową kreską krzyża, sprawdź celową i przejdź do wpisywania odczytu.
  void LvBeginReading() {
    zVEC3 dir( sinf( lvAz ), 0, cosf( lvAz ) );
    int best = -1; double bestLat = 1e9, bestFwd = 0;
    for ( size_t i = 0; i < lvRods.size(); i++ ) {
      double dx = lvRods[i].foot[VX] - lvInstr[VX], dz = lvRods[i].foot[VZ] - lvInstr[VZ];
      double fwd = dx * dir[VX] + dz * dir[VZ], lat = fabs( dx * dir[VZ] - dz * dir[VX] );
      if ( fwd > 100.0 && fwd < LV_MAX_DIST && lat < LV_ROD_HALF && lat < bestLat ) { best = (int)i; bestLat = lat; bestFwd = fwd; }
    }
    if ( best < 0 ) { LvMsg( "Pionowa kreska nie lezy na lacie" ); return; }
    LvRod& r = lvRods[best];

    double lineY = lvInstr[VY] + LV_INSTR_H;
    zVEC3 from( lvInstr[VX], (float)lineY, lvInstr[VZ] );
    zVEC3 to = zVEC3( r.foot[VX], (float)lineY, r.foot[VZ] ) - from;
    float len = to.Length();
    if ( len > 40.0f ) {   // przeszkoda na poziomej linii celowej, tuz przed lata
      to = to * ( ( len - 30.0f ) / len );
      if ( lvWorld->TraceRayNearestHit( from, to, (zCVob*)nullptr,
           zTRACERAY_VOB_IGNORE_NO_CD_DYN | zTRACERAY_VOB_IGNORE_CHARACTER | zTRACERAY_POLY_IGNORE_TRANSP ) ) {
        LvMsg( "Linia celowa zaslonieta" ); return;
      }
    }
    double ideal = LevelReading( lineY, r.foot[VY], bestFwd, LV_COLLIM, 0.0 );
    if ( ideal < 0 || ideal > LV_ROD_LEN ) { LvMsg( "Pozioma kreska jest poza lata (nad nia lub pod nia)" ); return; }

    lvEntry = true; lvEntryRod = best; lvEntryLen = 0; lvEntryMm = 0; lvEntryStep = 0; lvEntryIdealCm = ideal; lvEntryDist = bestFwd;
    LvPanelClose( lvHud );
    lvEntryPanel = LvPanelOpen( 2000, 5500, 6200, 7500 );   // pole wpisywania na dole lunety
  }

  // upMm / loMm < 0: odczytów kresek dalmierczych nie wpisano
  void LvFinishReading( int midMm, int upMm, int loMm ) {
    LvRod& r = lvRods[lvEntryRod];
    double userCm = midMm / 10.0;
    double stadiaCm = ( upMm >= 0 && loMm >= 0 ) ? LevelStadiaDist( upMm / 10.0, loMm / 10.0, LV_STADIA_K ) : -1.0;
    char buf[300], dm[80] = "";
    if ( stadiaCm >= 0 ) sprintf( dm, " Dalmierz: %.1f m (prawdziwa %.1f m).", stadiaCm / 100.0, lvEntryDist / 100.0 );
    if ( !lvHasHorizon ) {
      if ( !r.known ) { LvMsg( "Wstecz (BS) musi byc na lacie o znanej wysokosci" ); return; }
      lvHorizon = LevelHorizon( r.H, userCm );
      lvHasHorizon = true;
      LvLogRow( "BS", r, userCm, lvEntryIdealCm, r.H, lvEntryDist, stadiaCm );
      sprintf( buf, "BS: lata %d odczyt %.3f m, horyzont %.3f m.%s Obroc na inna lata (FS)", r.id, userCm / 100.0, lvHorizon / 100.0, dm );
    } else {
      r.H = LevelForeHeight( lvHorizon, userCm );
      r.known = true;
      LvLogRow( "FS", r, userCm, lvEntryIdealCm, r.H, lvEntryDist, stadiaCm );
      sprintf( buf, "FS: lata %d odczyt %.3f m, H = %.3f m (blad: %.1f mm).%s", r.id, userCm / 100.0, r.H / 100.0,
               ( r.H - r.foot[VY] ) * 10.0, dm );
    }
    LvMsg( buf );
  }

  void LvViewTick() {
    DWORD now = GetTickCount();
    float dt = ( now - lvLastTick ) / 1000.0f; lvLastTick = now;
    if ( dt > 0.1f ) dt = 0.1f;
    if ( !lvHairsDone && lvFovSaved ) { lvHairsDone = true; LvOpenHairs(); }     // FOV gry znany dopiero po pierwszym zastosowaniu zbliżenia

    if ( zinput->KeyToggled( KEY_F10 ) ) {           // F10: anuluj wpisywanie, a gdy go nie ma - wyjdź z widoku
      if ( lvEntry ) { lvEntry = false; LvPanelClose( lvEntryPanel ); } else { LvLeaveView(); return; }
    }
    if ( zinput->KeyToggled( KEY_F11 ) ) { LvClear(); return; }

    if ( lvEntry ) {
      static const int digitKeys[10] = { KEY_0, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9 };
      for ( int d = 0; d < 10; d++ )
        if ( zinput->KeyToggled( digitKeys[d] ) && lvEntryLen < 4 ) { lvEntryMm = lvEntryMm * 10 + d; lvEntryLen++; }
      if ( zinput->KeyToggled( KEY_BACK ) && lvEntryLen > 0 ) { lvEntryMm /= 10; lvEntryLen--; }
      if ( zinput->KeyToggled( KEY_RETURN ) ) {
        // krok 0: kreska środkowa (wymagana), kroki 1-2: kreski dalmiercze (Enter bez liczby pomija odległość)
        bool valid = lvEntryLen > 0 && lvEntryMm <= 3000;
        if ( lvEntryStep == 0 && valid ) { lvMidMm = lvEntryMm; lvEntryStep = 1; lvEntryLen = lvEntryMm = 0; }
        else if ( lvEntryStep == 1 && valid ) { lvUpMm = lvEntryMm; lvEntryStep = 2; lvEntryLen = lvEntryMm = 0; }
        else if ( lvEntryStep == 2 && valid ) { lvEntry = false; LvPanelClose( lvEntryPanel ); LvFinishReading( lvMidMm, lvUpMm, lvEntryMm ); }
        else if ( lvEntryStep > 0 && lvEntryLen == 0 ) { lvEntry = false; LvPanelClose( lvEntryPanel ); LvFinishReading( lvMidMm, -1, -1 ); }
      }
      if ( lvEntry && lvEntryPanel ) {
        static const char* titles[3] = { "srodkowa kreska [mm]:", "gorna kreska dalmiercza [mm]:", "dolna kreska dalmiercza [mm]:" };
        char b[96];
        sprintf( b, "Lata %d - %s", lvRods[lvEntryRod].id, titles[lvEntryStep] );
        lvEntryPanel->ClrPrintwin();
        lvEntryPanel->PrintCX( 700, zSTRING( b ) );
        sprintf( b, "%d", lvEntryMm );
        lvEntryPanel->PrintCX( 3500, zSTRING( lvEntryLen ? b : "_" ) );       // wpisana wartość
        lvEntryPanel->PrintCX( 6300, zSTRING( lvEntryStep ? "Enter bez liczby - pomin odleglosc   F10 - anuluj" : "Enter - zatwierdz   F10 - anuluj" ) );
      }
    } else {
      const float rate = ( zinput->KeyPressed( KEY_LSHIFT ) ? LV_TURN_SLOW : LV_TURN_FAST ) * 3.14159265f / 180.0f;
      if ( zinput->KeyPressed( KEY_LEFT ) )  lvAz -= rate * dt;
      if ( zinput->KeyPressed( KEY_RIGHT ) ) lvAz += rate * dt;
      if ( zinput->KeyToggled( KEY_RETURN ) ) LvBeginReading();
    }

    // kamera w osi lunety, patrzy poziomo w kierunku azymutu
    zCVob* cam = ogame->GetCameraVob();
    if ( cam ) {
      cam->SetPositionWorld( zVEC3( lvInstr[VX], lvInstr[VY] + LV_INSTR_H, lvInstr[VZ] ) );
      cam->SetHeadingAtWorld( zVEC3( sinf( lvAz ), 0, cosf( lvAz ) ) );
    }
  }

  void LevelLoop() {
    if ( ogame->GetWorld() != lvWorld ) { LvForget(); lvWorld = ogame->GetWorld(); }
    if ( !lvWorld ) return;
    LvApplyZoom();
    if ( lvHud && GetTickCount() > lvHudUntil ) LvPanelClose( lvHud );

    if ( lvView ) { LvViewTick(); return; }

    zVEC3 p;
    if ( zinput->KeyToggled( KEY_F7 ) && LvFeet( p ) ) {
      if ( lvInstrVob ) lvWorld->RemoveVob( lvInstrVob );
      lvInstr = p; lvInstrVob = LvSpawn( LV_VIS_INSTR, p );
      lvHasInstr = true; lvHasHorizon = false; lvSetup++;
      LvMsg( "Niwelator postawiony (F10 - widok lunety). Odczyt wstecz (BS) na lacie o znanej wysokosci - pierwsza lata to reper" );
    }
    if ( zinput->KeyToggled( KEY_F9 ) && LvFeet( p ) )
      lvPlates.push_back( { p, LvSpawn( LV_VIS_PLATE, p ) } );
    if ( zinput->KeyToggled( KEY_F8 ) && LvFeet( p ) ) {
      for ( auto& pl : lvPlates ) {
        float dx = pl.pos[VX] - p[VX], dz = pl.pos[VZ] - p[VZ];
        if ( sqrtf( dx * dx + dz * dz ) < 30.0f ) { p = pl.pos; p[VY] += LV_PLATE_H; break; }
      }
      LvRod r = { p, LvSpawn( LV_VIS_ROD, p ), lvRods.empty(), p[VY], ++lvRodCount };  // pierwsza = reper
      lvRods.push_back( r );
      LvMsg( "Lata postawiona" );
    }
    if ( zinput->KeyToggled( KEY_F10 ) ) LvEnterView();
    if ( zinput->KeyToggled( KEY_F11 ) ) LvClear();
  }
}
