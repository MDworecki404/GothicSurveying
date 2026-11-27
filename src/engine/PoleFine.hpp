// Precyzyjne ustawienie tyczki GNSS: kamera schodzi nad podstawę tyczki, strzałki przesuwają ją o milimetry.
// Strzałki względem widoku kamery (góra = od kamery), Shift = 10 mm, Ctrl = 100 mm, przytrzymanie powtarza,
// Enter / klawisz menu zatwierdza, Backspace wraca na pozycję startową. Dołączane raz z Plugin.hpp po World.hpp, bez strażnika include.
#include <cmath>
#include <string>

namespace GOTHIC_NAMESPACE {
  inline std::string CtrlFineGuide( const GridPos& poleTruth );   // engine/Controller.hpp: wskazowka wytyczania z roboty (pusta, gdy robota nie wytycza)
  inline bool      gFineOn = false;
  inline zVEC3     gFineStart, gFinePos;               // pozycja tyczki przy wejściu i bieżąca (grunt)
  inline float     gFineAz = 0;                        // azymut widoku kamery [rad]: kierunek (sin az, 0, cos az) = od kamery w stronę tyczki
  inline DWORD     gFineLast = 0;
  inline float     gFineHeld[4] = { 0, 0, 0, 0 };      // czas przytrzymania: lewo, prawo, góra, dół
  inline zCView*   gFinePanel = nullptr;
  inline std::string gFineText;

  inline bool PoleFineActive() { return gFineOn; }

  inline void PoleFineRedraw() {
    const double offN = ( gFinePos[VZ] - gFineStart[VZ] ) * 10.0, offE = ( gFinePos[VX] - gFineStart[VX] ) * 10.0;
    const GridPos g = GnssToGrid( gFinePos[VX], gFinePos[VY], gFinePos[VZ] );
    char l2[96], l3[110];
    sprintf( l2, "Przesuniecie od startu: N %+.0f mm   E %+.0f mm", offN, offE );
    sprintf( l3, "Pozycja w grze: N %.3f  E %.3f  H %.3f m", g.n, g.e, g.h );
    const std::string guide = CtrlFineGuide( g );        // przy wytyczaniu zamiast prawdziwej pozycji: wskazanie odbiornika
    if ( !guide.empty() ) snprintf( l3, sizeof l3, "%s", guide.c_str() );
    const std::string text = std::string( l2 ) + "|" + l3;
    if ( gFinePanel && text == gFineText ) return;
    gFineText = text;
    PanelClose( gFinePanel );
    const int H = 1700;                                  // jednostki ekranu; PrintCX liczy y w 8192 na wysokość panelu
    gFinePanel = PanelOpen( 1300, 6200, 6900, 6200 + H );
    auto L = [&]( int v ) { return v * 8192 / H; };
    gFinePanel->PrintCX( L( 100 ), zSTRING( "Precyzyjne ustawienie tyczki" ) );
    gFinePanel->PrintCX( L( 400 ), zSTRING( l2 ) );
    gFinePanel->PrintCX( L( 650 ), zSTRING( l3 ) );
    gFinePanel->PrintCX( L( 950 ), zSTRING( "Strzalki - 1 mm   Shift - 10 mm   Ctrl - 100 mm" ) );
    gFinePanel->PrintCX( L( 1200 ), zSTRING( "Enter - gotowe   Backspace - wroc na start" ) );
  }

  inline void PoleFineLeave() {
    if ( !gFineOn ) return;
    gFineOn = false;
    PanelClose( gFinePanel ); gFineText.clear();
    if ( player ) player->showVisual = 1;
    gWorld.SetDressVisible( true );
    ogame->Unpause();
  }

  inline void PoleFineEnter() {
    zCVob* cam = ogame->GetCameraVob();
    if ( !cam || gFineOn || !gWorld.HasPole() ) return;
    gFineStart = gFinePos = gWorld.PoleGround();
    zVEC3 at = cam->GetAtVectorWorld();
    gFineAz = atan2f( at[VX], at[VZ] );                  // kamera patrzy tam, gdzie patrzył gracz
    for ( float& h : gFineHeld ) h = 0;
    if ( player ) player->showVisual = 0;                // bohater i ubiór nie zasłaniają podstawy tyczki
    gWorld.SetDressVisible( false );
    gFineOn = true; gFineLast = GetTickCount();
    ogame->Pause();
    PoleFineRedraw();
  }

  // Jeden krok o (dRight, dForward) milimetrów w układzie widoku.
  inline void PoleFineStep( double rightMm, double fwdMm ) {
    const float s = sinf( gFineAz ), c = cosf( gFineAz );
    const double dx = ( rightMm * c + fwdMm * s ) / 10.0, dz = ( -rightMm * s + fwdMm * c ) / 10.0;   // cm
    if ( gWorld.MovePoleFrom( gFinePos, dx, dz ) ) gFinePos = gWorld.PoleGround();
    else HudMessage( "Brak gruntu w tym miejscu" );
  }

  inline void PoleFineTick() {
    if ( !gFineOn ) return;
    const DWORD now = GetTickCount();
    float dt = ( now - gFineLast ) / 1000.0f; gFineLast = now;
    if ( dt > 0.1f ) dt = 0.1f;
    if ( zinput->KeyToggled( KEY_RETURN ) || zinput->KeyToggled( LV_KEY_MENU ) ) { PoleFineLeave(); return; }
    if ( zinput->KeyToggled( KEY_BACK ) ) {              // wróć na pozycję startową
      if ( gWorld.MovePoleFrom( gFineStart, 0, 0 ) ) gFinePos = gWorld.PoleGround();
    }
    const double mult = zinput->KeyPressed( KEY_LCONTROL ) ? 100.0 : zinput->KeyPressed( KEY_LSHIFT ) ? 10.0 : 1.0;
    static const int keys[4] = { KEY_LEFT, KEY_RIGHT, KEY_UP, KEY_DOWN };
    static const double dirR[4] = { -1, 1, 0, 0 }, dirF[4] = { 0, 0, 1, -1 };
    for ( int i = 0; i < 4; i++ ) {
      bool fire = zinput->KeyToggled( keys[i] );         // pierwsze naciśnięcie: jeden krok
      if ( zinput->KeyPressed( keys[i] ) ) {
        const float before = gFineHeld[i]; gFineHeld[i] += dt;
        if ( !fire && gFineHeld[i] > 0.4f ) {            // po 0,4 s powtarzanie ok. 25 kroków na sekundę
          const int a = (int)( ( before - 0.4f ) * 25.0f ), b = (int)( ( gFineHeld[i] - 0.4f ) * 25.0f );
          fire = b > a || before <= 0.4f;
        }
      } else gFineHeld[i] = 0;
      if ( fire ) PoleFineStep( dirR[i] * mult, dirF[i] * mult );
    }
    PoleFineRedraw();
    // kamera 90 cm za podstawą tyczki (patrząc w jej stronę), 60 cm nad gruntem, spojrzenie na podstawę
    zCVob* cam = ogame->GetCameraVob();
    if ( cam ) {
      const zVEC3 dir( sinf( gFineAz ), 0, cosf( gFineAz ) );
      const zVEC3 camPos = gFinePos - dir * 90.0f + zVEC3( 0, 60, 0 );
      zVEC3 look = ( gFinePos + zVEC3( 0, 15, 0 ) ) - camPos;
      look.Normalize();
      cam->SetPositionWorld( camPos );
      cam->SetHeadingAtWorld( look );
    }
  }
}
