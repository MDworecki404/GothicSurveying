// Panele tekstowe zCView z tłem (widać je także nad lunetą) i komunikat HUD. Dołączane raz z Plugin.hpp, bez strażnika include.
#include <cstdio>
#include <string>

namespace GOTHIC_NAMESPACE {
  inline void SvTrace( const char* s ) {   // diagnostyka: GothicSurveying.log w katalogu roboczym gry
    FILE* f = fopen( "GothicSurveying.log", "a" );
    if ( f ) { fputs( s, f ); fputs( "\n", f ); fclose( f ); }
  }

  inline const char* LV_FONT = "FONT_OLD_10_WHITE.TGA";   // mniejsza czcionka; większa: FONT_OLD_20_WHITE.TGA

  inline zCView* PanelOpen( int x0, int y0, int x1, int y1 ) {
    zCView* v = new zCView( x0, y0, x1, y1 );
    v->InsertBack( "LVL_PANEL.TGA" );
    v->SetAlphaBlendFunc( zRND_ALPHA_FUNC_BLEND );
    v->SetFont( LV_FONT );
    screen->InsertItem( v );
    v->Open();
    return v;
  }
  inline void PanelClose( zCView*& v ) {
    if ( !v ) return;
    v->Close(); screen->RemoveItem( v ); delete v; v = nullptr;
  }

  inline zCView* gHud = nullptr;     // komunikat na górze ekranu, znika po 7 s
  inline DWORD   gHudUntil = 0;

  inline void HudClose() { PanelClose( gHud ); }
  inline void HudMessage( const char* s ) {
    SvTrace( s );
    HudClose();
    gHud = PanelOpen( 400, 150, 7800, 900 );
    gHud->PrintCXY( zSTRING( s ) );
    gHudUntil = GetTickCount() + 7000;
  }
  inline void HudTick() { if ( gHud && GetTickCount() > gHudUntil ) HudClose(); }
}
