// Widok przez lunete tachimetru: kamera w osi lunety, obrot w poziomie i pochylanie w pionie, krzyz, zblizenie, panel z odczytami Hz/V.
// Enter = wykonaj (orientacja albo pomiar, wg robota), klawisz lunety wychodzi. Dolaczane raz z Plugin.hpp po World.hpp, bez straznika include.
#include <chrono>
#include <cmath>
#include <functional>
#include <string>
#include <vector>

namespace GOTHIC_NAMESPACE {
  struct TachyCallbacks {
    std::function<std::string( double camAzDeg, double camPitchDeg )> fire;                  // Enter: komunikat (ASCII) do wyswietlenia
    std::function<std::vector<std::string>( double, double )> panel;                         // linie panelu odczytow
    std::function<std::string()> flip;                                                       // K: przerzut lunety; komunikat
    std::function<void()> onLeave;
  };

  inline bool       gTachyOn = false, gTachyFovSaved = false;
  inline float      gTachyAz = 0, gTachyPitch = 0, gTachyFovX = 0, gTachyFovY = 0;   // azymut i pochylenie osi lunety [rad]
  inline std::chrono::steady_clock::time_point gTachyLast;   // zegar o duzej rozdzielczosci (GetTickCount ma ok. 15 ms ziarno => szarpany obrot przy malych predkosciach)
  inline int        gTachyAnim = 0;                                // 0 brak, 1 przelozenie lunety, 2 obrot alidady o 200 g
  inline float      gTachyAnimT = 0, gTachyAnimAz = 0, gTachyAnimPitch = 0;   // czas animacji [s] i poczatkowy azymut/pochylenie [rad]
  inline float      gTachyHold = 0;                                // jak dlugo trzymana jest strzalka obrotu [s] (narasta predkosc w trybie dokladnym)
  inline zVEC3      gTachyGround;
  inline zCView     *gTachyReticle = nullptr, *gTachyPanel = nullptr;
  inline std::string gTachyText;
  inline TachyCallbacks gTachyCb;

  inline bool TachyViewActive() { return gTachyOn; }

  // Wlasne zblizenie (niezalezne od Scope.hpp, ktorego ScopeTick co klatke przywraca FOV niwelacji).
  inline void TachyApplyZoom() {
    zCCamera* c = zCCamera::activeCam;
    if ( !c ) return;
    const float rad = 3.14159265f / 180.0f;
    if ( gTachyOn ) {
      if ( !gTachyFovSaved ) { c->GetFOV( gTachyFovX, gTachyFovY ); gTachyFovSaved = true; }
      ScopeMipBias( true, gZoom );
      c->SetFOV( (float)( 2 * atan( tan( gTachyFovX * rad / 2 ) / gZoom ) / rad ), (float)( 2 * atan( tan( gTachyFovY * rad / 2 ) / gZoom ) / rad ) );
    } else if ( gTachyFovSaved ) {
      ScopeMipBias( false, 1.0f );
      c->SetFOV( gTachyFovX, gTachyFovY ); gTachyFovSaved = false;
    }
  }

  inline void TachyViewRedraw() {
    const double azDeg = gTachyAz * 180.0 / 3.14159265, pitchDeg = gTachyPitch * 180.0 / 3.14159265;
    std::vector<std::string> lines = gTachyCb.panel ? gTachyCb.panel( azDeg, pitchDeg ) : std::vector<std::string>();
    std::string text; for ( const std::string& l : lines ) text += l + "|";
    if ( gTachyPanel && text == gTachyText ) return;
    gTachyText = text;
    const int H = 130 + (int)lines.size() * 235 + 150;          // stały odstęp wierszy [jednostki ekranu]; Print liczy y w 8192 na wysokość panelu
    if ( !gTachyPanel ) gTachyPanel = PanelOpen( 150, 6600, 4600, 6600 + H );
    gTachyPanel->ClrPrintwin();
    for ( size_t i = 0; i < lines.size(); i++ ) gTachyPanel->Print( 300, ( 130 + (int)i * 235 ) * 8192 / H, zSTRING( lines[i].c_str() ) );
  }

  inline void TachyViewLeave() {
    if ( !gTachyOn ) return;
    gTachyOn = false;
    TachyApplyZoom();                                           // przywraca FOV
    PanelClose( gTachyPanel ); gTachyText.clear();
    if ( gTachyReticle ) { gTachyReticle->Close(); screen->RemoveItem( gTachyReticle ); delete gTachyReticle; gTachyReticle = nullptr; }
    if ( player ) player->showVisual = 1;
    gWorld.SetDressVisible( true ); gWorld.SetInstrumentVisible( true );
    if ( gTachyCb.onLeave ) gTachyCb.onLeave();
    ogame->Unpause();
  }

  inline void TachyViewEnter( const TachyCallbacks& cb ) {
    zCVob* cam = ogame->GetCameraVob();
    if ( !cam || gTachyOn || !gWorld.HasInstrument() ) return;
    gTachyCb = cb; gTachyGround = gWorld.InstrumentGround();
    zVEC3 at = cam->GetAtVectorWorld();
    gTachyAz = atan2f( at[VX], at[VZ] ); gTachyPitch = 0;       // zaczynamy tam, gdzie patrzyl gracz, poziomo
    if ( player ) player->showVisual = 0;                       // kamera w osi lunety - nic nie przeslania widoku
    gWorld.SetDressVisible( false ); gWorld.SetInstrumentVisible( false );
    gTachyReticle = new zCView( 0, 0, 8192, 8192 );
    gTachyReticle->InsertBack( LV_RETICLE );
    gTachyReticle->SetAlphaBlendFunc( zRND_ALPHA_FUNC_BLEND );
    screen->InsertItem( gTachyReticle );
    ViewOpenNow( gTachyReticle );
    gTachyOn = true; gTachyLast = std::chrono::steady_clock::now(); gTachyHold = 0; gTachyAnim = 0;
    ogame->Pause();
    TachyViewRedraw();
    HudMessage( LV_KEYNAME_AIM " - najechanie na lustro, " LV_KEYNAME_ZOOM " - powiekszenie, Shift - tryb dokladny, K - przelozenie lunety, X - obrot alidady o 200 g" );
  }

  inline void TachyViewTick() {
    TachyApplyZoom();
    if ( !gTachyOn ) return;
    const auto now = std::chrono::steady_clock::now();
    float dt = std::chrono::duration<float>( now - gTachyLast ).count(); gTachyLast = now;
    if ( dt > 0.1f ) dt = 0.1f;
    if ( zinput->KeyToggled( LV_KEY_SCOPE ) ) { TachyViewLeave(); return; }
    if ( gTachyAnim ) {                                         // trwa przelozenie lunety albo obrot alidady: bez sterowania, kamera podaza za animacja
      const float kPi = 3.14159265f, dur = gTachyAnim == 1 ? 1.4f : 0.7f;
      gTachyAnimT += dt;
      const TachyPose p = gTachyAnim == 1 ? TachyFlipPose( gTachyAnimT / dur, gTachyAnimAz * 180.0 / kPi, gTachyAnimPitch * 180.0 / kPi )
                                          : TachyTurnPose( gTachyAnimT / dur, gTachyAnimAz * 180.0 / kPi, gTachyAnimPitch * 180.0 / kPi );
      gTachyAz = (float)( p.azDeg * kPi / 180.0 ); gTachyPitch = (float)( p.pitchDeg * kPi / 180.0 );
      if ( gTachyAnimT >= dur ) gTachyAnim = 0;
      TachyViewRedraw();
      zCVob* cam = ogame->GetCameraVob();
      if ( cam ) {
        cam->SetPositionWorld( zVEC3( gTachyGround[VX], gTachyGround[VY] + LV_INSTR_H, gTachyGround[VZ] ) );
        cam->SetHeadingAtWorld( zVEC3( sinf( gTachyAz ) * cosf( gTachyPitch ), sinf( gTachyPitch ), cosf( gTachyAz ) * cosf( gTachyPitch ) ) );
      }
      return;
    }
    const bool turning = zinput->KeyPressed( KEY_LEFT ) || zinput->KeyPressed( KEY_RIGHT ) || zinput->KeyPressed( KEY_UP ) || zinput->KeyPressed( KEY_DOWN );
    gTachyHold = turning ? gTachyHold + dt : 0.0f;                                    // puszczenie strzalki zeruje narastanie predkosci
    const float rate = ( zinput->KeyPressed( KEY_LSHIFT ) ? (float)PreciseTurnRateDegS( gTachyHold, gZoom ) : LV_TURN_FAST ) * 3.14159265f / 180.0f;
    if ( zinput->KeyPressed( KEY_LEFT ) )  gTachyAz -= rate * dt;
    if ( zinput->KeyPressed( KEY_RIGHT ) ) gTachyAz += rate * dt;
    if ( zinput->KeyPressed( KEY_UP ) )    gTachyPitch += rate * dt;
    if ( zinput->KeyPressed( KEY_DOWN ) )  gTachyPitch -= rate * dt;
    const float lim = 89.0f * 3.14159265f / 180.0f;             // nie przechodzimy przez zenit/nadir
    if ( gTachyPitch > lim ) gTachyPitch = lim;
    if ( gTachyPitch < -lim ) gTachyPitch = -lim;
    ZoomKeys();
    if ( zinput->KeyToggled( LV_KEY_FLIP ) && gTachyCb.flip ) {
      auto faceOf = []() { const std::vector<std::string> l = gTachyCb.panel ? gTachyCb.panel( 0, 0 ) : std::vector<std::string>(); return !l.empty() && l[0].find( "KP" ) != std::string::npos ? 2 : 1; };
      const int before = faceOf();
      const std::string m = gTachyCb.flip(); if ( !m.empty() ) HudMessage( m.c_str() );
      if ( faceOf() != before ) { gTachyAnim = 1; gTachyAnimT = 0; gTachyAnimAz = gTachyAz; gTachyAnimPitch = gTachyPitch; return; }   // polozenie sie zmienilo: pokaz przelozenie lunety
    }
    if ( zinput->KeyToggled( LV_KEY_ALIDADE ) ) { gTachyAnim = 2; gTachyAnimT = 0; gTachyAnimAz = gTachyAz; gTachyAnimPitch = gTachyPitch; return; }
    if ( zinput->KeyToggled( LV_KEY_AIM ) ) {                   // najazd z celowym bledem: lustro ma sie pojawic w polu widzenia, ale nie w krzyzu
      const std::vector<zVEC3> t = gWorld.PrismTops();
      if ( t.empty() ) HudMessage( "Brak lustra do najechania" );
      else {
        static size_t next = 0; const zVEC3& p = t[next++ % t.size()];
        const float dx = p[VX] - gTachyGround[VX], dz = p[VZ] - gTachyGround[VZ], dy = p[VY] - ( gTachyGround[VY] + LV_INSTR_H );
        const float fx = 2 * atanf( tanf( gTachyFovX * 3.14159265f / 360.0f ) / gZoom ), fy = 2 * atanf( tanf( gTachyFovY * 3.14159265f / 360.0f ) / gZoom );
        gTachyAz = atan2f( dx, dz ) + 0.25f * fx;               // ~1/4 pola widzenia w prawo i ~1/6 w gore od lustra
        gTachyPitch = atan2f( dy, sqrtf( dx * dx + dz * dz ) ) + 0.17f * fy;
      }
    }
    if ( zinput->KeyToggled( KEY_RETURN ) && gTachyCb.fire ) {
      const std::string msg = gTachyCb.fire( gTachyAz * 180.0 / 3.14159265, gTachyPitch * 180.0 / 3.14159265 );
      if ( !msg.empty() ) HudMessage( msg.c_str() );
    }
    TachyViewRedraw();
    zCVob* cam = ogame->GetCameraVob();
    if ( cam ) {
      cam->SetPositionWorld( zVEC3( gTachyGround[VX], gTachyGround[VY] + LV_INSTR_H, gTachyGround[VZ] ) );
      cam->SetHeadingAtWorld( zVEC3( sinf( gTachyAz ) * cosf( gTachyPitch ), sinf( gTachyPitch ), cosf( gTachyAz ) * cosf( gTachyPitch ) ) );
    }
  }
}
