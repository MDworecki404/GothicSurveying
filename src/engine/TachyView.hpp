// Widok przez lunete tachimetru: kamera w osi lunety, obrot w poziomie i pochylanie w pionie, krzyz, zblizenie, panel z odczytami Hz/V.
// Enter = wykonaj (orientacja albo pomiar, wg robota), klawisz lunety wychodzi. Dolaczane raz z Plugin.hpp po World.hpp, bez straznika include.
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
  inline DWORD      gTachyLast = 0;
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
      c->SetFOV( (float)( 2 * atan( tan( gTachyFovX * rad / 2 ) / gZoom ) / rad ), (float)( 2 * atan( tan( gTachyFovY * rad / 2 ) / gZoom ) / rad ) );
    } else if ( gTachyFovSaved ) {
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
    gTachyOn = true; gTachyLast = GetTickCount();
    ogame->Pause();
    TachyViewRedraw();
    HudMessage( LV_KEYNAME_AIM " - szybkie najechanie na lustro, " LV_KEYNAME_ZOOM " - powiekszenie" );
  }

  inline void TachyViewTick() {
    TachyApplyZoom();
    if ( !gTachyOn ) return;
    const DWORD now = GetTickCount();
    float dt = ( now - gTachyLast ) / 1000.0f; gTachyLast = now;
    if ( dt > 0.1f ) dt = 0.1f;
    if ( zinput->KeyToggled( LV_KEY_SCOPE ) ) { TachyViewLeave(); return; }
    const float rate = ( zinput->KeyPressed( KEY_LSHIFT ) ? LV_TURN_SLOW : LV_TURN_FAST ) * 3.14159265f / 180.0f;
    if ( zinput->KeyPressed( KEY_LEFT ) )  gTachyAz -= rate * dt;
    if ( zinput->KeyPressed( KEY_RIGHT ) ) gTachyAz += rate * dt;
    if ( zinput->KeyPressed( KEY_UP ) )    gTachyPitch += rate * dt;
    if ( zinput->KeyPressed( KEY_DOWN ) )  gTachyPitch -= rate * dt;
    const float lim = 89.0f * 3.14159265f / 180.0f;             // nie przechodzimy przez zenit/nadir
    if ( gTachyPitch > lim ) gTachyPitch = lim;
    if ( gTachyPitch < -lim ) gTachyPitch = -lim;
    ZoomKeys();
    if ( zinput->KeyToggled( LV_KEY_FLIP ) && gTachyCb.flip ) { const std::string m = gTachyCb.flip(); if ( !m.empty() ) HudMessage( m.c_str() ); }
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
