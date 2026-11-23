// Poziomowanie instrumentu: panel z libella okragla (tekstura LVL_LIBELLA.TGA), babelkiem i trzema srubami ustawczymi. Gra zatrzymana.
// Klawisze 1/2/3 - sruba w gore (obrot w prawo), 4/5/6 - w dol, Shift = dokladnie; Enter / klawisz menu zamyka. Dolaczane raz z Plugin.hpp po MapView.hpp.
#include <cmath>
#include <string>
#include <vector>
#include "calc/Libella.hpp"

namespace GOTHIC_NAMESPACE {
  inline zCView     *gLvlPanel = nullptr, *gLvlTop = nullptr, *gLvlBottom = nullptr, *gLvlBubble = nullptr;
  inline Libella*   gLvlLib = nullptr;
  inline double     gLvlLimit = kLevelBubbleArcmin;
  inline std::string gLvlName;
  inline int        gLvlX0 = 0, gLvlY0 = 0, gLvlS = 0;

  inline bool LevelActive() { return gLvlPanel != nullptr; }

  inline void LevelClose() {
    if ( !gLvlPanel ) return;
    if ( gLvlBubble ) { gLvlBubble->Close(); screen->RemoveItem( gLvlBubble ); delete gLvlBubble; gLvlBubble = nullptr; }
    PanelClose( gLvlTop ); PanelClose( gLvlBottom ); PanelClose( gLvlPanel );
    gLvlLib = nullptr;
    ogame->Unpause();
  }

  // Odleglosc babelka od srodka jako ulamek promienia: do limitu liniowo do okregu tolerancji (0,4 R), dalej liniowo do 1 R przy 20'.
  // Ciagla (bez przeskokow) i jednakowa dla niwelatora (limit 8') i tachimetru (0,5'): okrag tolerancji zawsze w tym samym miejscu.
  inline double LevelRadiusFrac( double mag ) {
    const double lim = gLvlLimit, kMax = Libella::kMax;
    return mag <= lim ? 0.4 * mag / lim : 0.4 + 0.6 * ( std::min )( 1.0, ( mag - lim ) / ( kMax - lim ) );
  }

  inline void LevelRedraw() {
    if ( !gLvlPanel || !gLvlLib ) return;
    if ( gLvlBubble ) { gLvlBubble->Close(); screen->RemoveItem( gLvlBubble ); delete gLvlBubble; gLvlBubble = nullptr; }
    const double R = gLvlS * 0.45, mag = gLvlLib->Mag(), f = mag > 1e-9 ? LevelRadiusFrac( mag ) / mag : 0.0;
    const int cx = gLvlX0 + gLvlS / 2 + (int)( gLvlLib->x * f * R ), cy = gLvlY0 + gLvlS / 2 - (int)( gLvlLib->y * f * R );
    const int sz = 22;
    gLvlBubble = new zCView( screen->anx( cx - sz / 2 ), screen->any( cy - sz / 2 ), screen->anx( cx + sz / 2 ), screen->any( cy + sz / 2 ) );
    gLvlBubble->InsertBack( gLvlLib->Within( gLvlLimit ) ? "LVL_DOTG.TGA" : "LVL_DOTY.TGA" );
    gLvlBubble->SetAlphaBlendFunc( zRND_ALPHA_FUNC_BLEND );
    screen->InsertItem( gLvlBubble ); ViewOpenNow( gLvlBubble );
    char b[200];
    sprintf( b, "Pochylenie %.2f' (%.0f\")   %s (limit %.1f')", gLvlLib->Mag(), gLvlLib->Mag() * 60.0,
             gLvlLib->Within( gLvlLimit ) ? "POZIOMO" : "babelek poza kolkiem", gLvlLimit );
    gLvlBottom->ClrPrintwin();
    const int hUnits = 2 * 38 * 8192 / screen->nay( 8192 );                      // wysokosc paska w jednostkach ekranu; Print liczy y w 8192 na wysokosc widoku
    gLvlBottom->PrintCX( 130 * 8192 / hUnits, zSTRING( b ) );
    gLvlBottom->PrintCX( 330 * 8192 / hUnits, zSTRING( "1/2/3 sruba w gore, 4/5/6 w dol, Shift - dokladnie, Enter - zamknij" ) );
  }

  inline void LevelOpen( Libella* lib, double limitArcmin, const std::string& what ) {
    if ( LevelActive() || !lib ) return;
    gLvlLib = lib; gLvlLimit = limitArcmin; gLvlName = what;
    const int sw = screen->nax( 8192 ), sh = screen->nay( 8192 );
    const int S = (int)( sh * 0.72 ), bar = 38, gap = 6, total = S + 3 * bar + 2 * gap;
    gLvlS = S; gLvlX0 = ( sw - S ) / 2; gLvlY0 = ( sh - total ) / 2 + bar + gap;
    gLvlTop    = PanelOpen( screen->anx( gLvlX0 ), screen->any( gLvlY0 - bar - gap ), screen->anx( gLvlX0 + S ), screen->any( gLvlY0 - gap ) );
    gLvlPanel  = PanelOpenTex( screen->anx( gLvlX0 ), screen->any( gLvlY0 ), screen->anx( gLvlX0 + S ), screen->any( gLvlY0 + S ), "LVL_LIBELLA.TGA" );
    gLvlBottom = PanelOpen( screen->anx( gLvlX0 ), screen->any( gLvlY0 + S + gap ), screen->anx( gLvlX0 + S ), screen->any( gLvlY0 + S + gap + 2 * bar ) );
    ogame->Pause();
    gLvlTop->PrintCXY( zSTRING( ( "Poziomowanie " + what ).c_str() ) );
    for ( int i = 0; i < 3; i++ ) {                                      // numery srub na obwodzie (gora, lewy dol, prawy dol)
      double ux, uy; Libella::ScrewDir( i, ux, uy );
      gLvlPanel->Print( 4096 + (int)( ux * 4096 * 0.93 ) - 60, 4096 - (int)( uy * 4096 * 0.93 ) - 120, zSTRING( std::to_string( i + 1 ).c_str() ) );
    }
    LevelRedraw();
  }

  inline void LevelTick() {
    if ( !gLvlPanel ) return;
    if ( zinput->KeyToggled( KEY_RETURN ) || zinput->KeyToggled( LV_KEY_MENU ) || zinput->KeyToggled( LV_KEY_SCOPE ) ) { LevelClose(); return; }
    static const int up[3] = { KEY_1, KEY_2, KEY_3 }, down[3] = { KEY_4, KEY_5, KEY_6 };
    const double step = zinput->KeyPressed( KEY_LSHIFT ) ? 0.1 : 1.0;                 // grubo 1', dokladnie 0,1'
    bool changed = false;
    for ( int i = 0; i < 3; i++ ) {
      if ( zinput->KeyToggled( up[i] ) )   { gLvlLib->Turn( i, step ); changed = true; }
      if ( zinput->KeyToggled( down[i] ) ) { gLvlLib->Turn( i, -step ); changed = true; }
    }
    if ( changed ) LevelRedraw();
  }
}
