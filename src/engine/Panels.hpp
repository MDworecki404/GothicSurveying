// Panele tekstowe zCView z tłem (widać je także nad lunetą) i komunikat HUD. Dołączane raz z Plugin.hpp, bez strażnika include.
#include <cstdio>
#include <map>
#include <string>
#include <vector>

namespace GOTHIC_NAMESPACE {
  inline void SvTrace( const char* s ) {   // diagnostyka: GothicSurveying.log w katalogu roboczym gry
    FILE* f = fopen( "GothicSurveying.log", "a" );
    if ( f ) { fputs( s, f ); fputs( "\n", f ); fclose( f ); }
  }

  inline const char* LV_FONT = "FONT_OLD_10_WHITE.TGA";   // mniejsza czcionka; większa: FONT_OLD_20_WHITE.TGA

  inline const int LV_FRAME_PX = 5;                           // grubość złotej ramki paneli [piksele ekranu]
  inline std::map<zCView*, std::vector<zCView*>> gPanelFrames;   // cztery paski ramki każdego panelu (zamykane razem z nim)

  inline zCView* PanelBar( int x0, int y0, int x1, int y1, const char* tex ) {
    zCView* b = new zCView( x0, y0, x1, y1 );
    b->InsertBack( tex );
    screen->InsertItem( b ); b->Open();
    return b;
  }

  inline zCView* PanelOpen( int x0, int y0, int x1, int y1 ) {
    zCView* v = new zCView( x0, y0, x1, y1 );
    v->InsertBack( "LVL_PANEL.TGA" );
    v->SetAlphaBlendFunc( zRND_ALPHA_FUNC_BLEND );
    v->SetFont( LV_FONT );
    screen->InsertItem( v );
    v->Open();
    const int tx = screen->nax( LV_FRAME_PX ), ty = screen->nay( LV_FRAME_PX );   // stała grubość w pikselach, niezależnie od rozmiaru panelu
    gPanelFrames[v] = { PanelBar( x0, y0, x1, y0 + ty, "LVL_GOLDH.TGA" ), PanelBar( x0, y1 - ty, x1, y1, "LVL_GOLDH.TGA" ),
                        PanelBar( x0, y0, x0 + tx, y1, "LVL_GOLDV.TGA" ), PanelBar( x1 - tx, y0, x1, y1, "LVL_GOLDV.TGA" ) };
    return v;
  }
  inline void PanelClose( zCView*& v ) {
    if ( !v ) return;
    auto it = gPanelFrames.find( v );
    if ( it != gPanelFrames.end() ) {
      for ( zCView* b : it->second ) { b->Close(); screen->RemoveItem( b ); delete b; }
      gPanelFrames.erase( it );
    }
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
