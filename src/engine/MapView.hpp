// Mapa osnowy: panel z mapa swiata (tekstura LVL_MAP_<swiat>.TGA wygenerowana z modelu swiata), punktami osnowy (numery) i znacznikiem bohatera.
// Gra zatrzymana, Enter / klawisz menu zamyka. Dolaczane raz z Plugin.hpp po ControlNet.hpp i Panels.hpp, bez straznika include.
#include <cmath>
#include <string>
#include <vector>

namespace GOTHIC_NAMESPACE {
  inline zCView*              gMapPanel = nullptr, *gMapTop = nullptr, *gMapBottom = nullptr;
  inline std::vector<zCView*> gMapDots;

  inline bool MapActive() { return gMapPanel != nullptr; }

  inline void MapClose() {
    if ( !gMapPanel ) return;
    for ( zCView*& d : gMapDots ) { d->Close(); screen->RemoveItem( d ); delete d; }
    gMapDots.clear();
    PanelClose( gMapTop ); PanelClose( gMapBottom ); PanelClose( gMapPanel );
    ogame->Unpause();
  }

  inline void MapDot( const char* tex, int centerXpx, int centerYpx, int sizePx ) {      // mały znacznik na mapie (pozycja i rozmiar w pikselach ekranu)
    zCView* d = new zCView( screen->nax( centerXpx - sizePx / 2 ), screen->nay( centerYpx - sizePx / 2 ),
                            screen->nax( centerXpx + sizePx / 2 ), screen->nay( centerYpx + sizePx / 2 ) );
    d->InsertBack( tex );
    d->SetAlphaBlendFunc( zRND_ALPHA_FUNC_BLEND );
    screen->InsertItem( d ); ViewOpenNow( d );
    gMapDots.push_back( d );
  }

  inline void MapOpen() {
    if ( MapActive() ) return;
    const std::string wn = ControlWorldName(); WorldMapView mv;
    if ( !WorldMapFor( wn, mv ) ) { HudMessage( ( "Brak mapy osnowy dla swiata " + wn ).c_str() ); return; }
    ControlEnsureLoaded();
    const int sw = screen->anx( 8192 ), sh = screen->any( 8192 );          // rozmiar ekranu w pikselach
    const int S = (int)( sh * 0.72 ), bar = 38, gap = 6;                    // mapa kwadratowa S x S px, paski tekstu nad i pod nią
    const int total = S + 2 * bar + 2 * gap, x0 = ( sw - S ) / 2, y0 = ( sh - total ) / 2;
    const int my0 = y0 + bar + gap;                                         // górny brzeg mapy [px]
    const std::string tex = "LVL_MAP_" + ControlWorldKey( wn ) + ".TGA";
    gMapTop    = PanelOpen( screen->nax( x0 ), screen->nay( y0 ), screen->nax( x0 + S ), screen->nay( y0 + bar ) );
    gMapPanel  = PanelOpenTex( screen->nax( x0 ), screen->nay( my0 ), screen->nax( x0 + S ), screen->nay( my0 + S ), tex.c_str() );
    gMapBottom = PanelOpen( screen->nax( x0 ), screen->nay( my0 + S + gap ), screen->nax( x0 + S ), screen->nay( my0 + S + gap + bar ) );
    ogame->Pause();

    const zVEC3 hero = player ? player->GetPositionWorld() : zVEC3( 0, 0, 0 );
    std::string nearest; double nearestD = 1e18;
    int shown = 0;
    for ( const ControlPoint& c : gControl ) {
      if ( ControlWorldKey( c.world ) != ControlWorldKey( wn ) ) continue;
      double u, v; MapUnit( mv, c.x / 100.0, c.z / 100.0, u, v );
      if ( u < 0 || u > 1 || v < 0 || v > 1 ) continue;
      const int px = x0 + (int)( u * S ), py = my0 + (int)( v * S );
      MapDot( c.id[0] == 'G' ? "LVL_DOTR.TGA" : "LVL_DOTG.TGA", px, py, 12 );   // czerwone: osnowa wzorcowa, zielone: z GNSS
      gMapPanel->Print( (int)( u * 8192 ) + 130, (int)( v * 8192 ) - 330, zSTRING( c.id.substr( 1 ).c_str() ) );
      shown++;
      const double dd = hypot( c.x - hero[VX], c.z - hero[VZ] );
      if ( dd < nearestD ) { nearestD = dd; nearest = c.id; }
    }
    double hu, hv; MapUnit( mv, hero[VX] / 100.0, hero[VZ] / 100.0, hu, hv );
    if ( hu >= 0 && hu <= 1 && hv >= 0 && hv <= 1 ) MapDot( "LVL_DOTY.TGA", x0 + (int)( hu * S ), my0 + (int)( hv * S ), 16 );

    char title[160], foot[200];
    sprintf( title, "Mapa osnowy - swiat %s: %d pkt", ControlWorldKey( wn ).c_str(), shown );
    if ( shown > 0 ) sprintf( foot, "zolty - ty   czerwone G.. - wzorcowa   zielone O.. - GNSS   najblizszy: %s (%.0f m)   Enter - zamknij", nearest.c_str(), nearestD / 100.0 );
    else sprintf( foot, "Brak punktow osnowy w tym swiecie (Zaladuj osnowe geodezyjna swiata)   Enter - zamknij" );
    gMapTop->PrintCXY( zSTRING( title ) );
    gMapBottom->PrintCXY( zSTRING( foot ) );
  }

  inline void MapTick() {
    if ( !gMapPanel ) return;
    if ( zinput->KeyToggled( KEY_RETURN ) || zinput->KeyToggled( LV_KEY_MENU ) ) MapClose();
  }
}
