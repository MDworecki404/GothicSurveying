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
    zCView* d = new zCView( screen->anx( centerXpx - sizePx / 2 ), screen->any( centerYpx - sizePx / 2 ),
                            screen->anx( centerXpx + sizePx / 2 ), screen->any( centerYpx + sizePx / 2 ) );
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
    const int sw = screen->nax( 8192 ), sh = screen->nay( 8192 );          // rozmiar ekranu w pikselach
    const int S = (int)( sh * 0.72 ), bar = 38, gap = 6;                    // mapa kwadratowa S x S px, paski tekstu nad i pod nią
    const int total = S + 2 * bar + 2 * gap, x0 = ( sw - S ) / 2, y0 = ( sh - total ) / 2;
    const int my0 = y0 + bar + gap;                                         // górny brzeg mapy [px]
    const std::string tex = "LVL_MAP_" + ControlWorldKey( wn ) + ".TGA";
    gMapTop    = PanelOpen( screen->anx( x0 ), screen->any( y0 ), screen->anx( x0 + S ), screen->any( y0 + bar ) );
    gMapPanel  = PanelOpenTex( screen->anx( x0 ), screen->any( my0 ), screen->anx( x0 + S ), screen->any( my0 + S ), tex.c_str() );
    gMapBottom = PanelOpen( screen->anx( x0 ), screen->any( my0 + S + gap ), screen->anx( x0 + S ), screen->any( my0 + S + gap + bar ) );
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

  // Podglad planu nalotu: powiekszony kadr obszaru (obszar zolty, szeregi zielone, dojazd czerwony) z punktami osnowy tego swiata w kadrze.
  inline void MapOpenPlan( const std::vector<MapLine>& lines ) {
    if ( MapActive() || lines.empty() ) return;
    double minN = 1e18, maxN = -1e18, minE = 1e18, maxE = -1e18;
    for ( const MapLine& l : lines ) {
      minN = ( std::min )( minN, ( std::min )( l.n1, l.n2 ) ); maxN = ( std::max )( maxN, ( std::max )( l.n1, l.n2 ) );
      minE = ( std::min )( minE, ( std::min )( l.e1, l.e2 ) ); maxE = ( std::max )( maxE, ( std::max )( l.e1, l.e2 ) );
    }
    const int sw = screen->nax( 8192 ), sh = screen->nay( 8192 );
    const int S = (int)( sh * 0.72 ), bar = 38, gap = 6;
    const int total = S + 2 * bar + 2 * gap, x0 = ( sw - S ) / 2, y0 = ( sh - total ) / 2, my0 = y0 + bar + gap;
    gMapTop    = PanelOpen( screen->anx( x0 ), screen->any( y0 ), screen->anx( x0 + S ), screen->any( y0 + bar ) );
    gMapPanel  = PanelOpen( screen->anx( x0 ), screen->any( my0 ), screen->anx( x0 + S ), screen->any( my0 + S ) );
    gMapBottom = PanelOpen( screen->anx( x0 ), screen->any( my0 + S + gap ), screen->anx( x0 + S ), screen->any( my0 + S + gap + bar ) );
    ogame->Pause();
    const double sc = 0.84 * S / ( std::max )( ( std::max )( maxN - minN, maxE - minE ), 10.0 ), cn = ( minN + maxN ) / 2, ce = ( minE + maxE ) / 2;
    auto px = [&]( double e ) { return x0 + (int)( S / 2 + ( e - ce ) * sc ); };
    auto py = [&]( double n ) { return my0 + (int)( S / 2 - ( n - cn ) * sc ); };       // polnoc u gory
    static const char* tex[3] = { "LVL_DOTY.TGA", "LVL_DOTG.TGA", "LVL_DOTR.TGA" };
    int dots = 0, ctrl = 0, strips = 0;
    for ( const MapLine& l : lines ) {
      if ( l.color == 1 ) strips++;
      const int ax = px( l.e1 ), ay = py( l.n1 ), bx = px( l.e2 ), by = py( l.n2 );
      const double len = hypot( (double)( bx - ax ), (double)( by - ay ) ), step = l.color == 2 ? 14.0 : 7.0;
      const int k = (int)( std::max )( 1.0, len / step );
      for ( int i = 0; i <= k && dots < 1500; i++, dots++ ) MapDot( tex[l.color], ax + (int)( ( bx - ax ) * (double)i / k ), ay + (int)( ( by - ay ) * (double)i / k ), l.color == 2 ? 3 : 5 );
    }
    ControlEnsureLoaded();
    const std::string wn = ControlWorldName();
    for ( const ControlPoint& c : gControl ) {                                          // osnowa w kadrze: punkt odniesienia dla ukladu planu
      if ( ControlWorldKey( c.world ) != ControlWorldKey( wn ) ) continue;
      const int cx = px( c.e ), cy = py( c.n );
      if ( cx < x0 || cx > x0 + S || cy < my0 || cy > my0 + S ) continue;
      MapDot( c.id[0] == 'G' ? "LVL_DOTR.TGA" : "LVL_DOTG.TGA", cx, cy, 10 ); ctrl++;
      gMapPanel->Print( (int)( 8192.0 * ( cx - x0 ) / S ) + 130, (int)( 8192.0 * ( cy - my0 ) / S ) - 330, zSTRING( c.id.c_str() ) );
    }
    char title[160], foot[200];
    sprintf( title, "Plan nalotu - %d szeregow, kadr %.0f x %.0f m, polnoc u gory", strips, maxE - minE, maxN - minN );
    sprintf( foot, "zolte - obszar, zielone - szeregi, czerwone male - dojazd; punkty osnowy w kadrze: %d   Enter - zamknij", ctrl );
    gMapTop->PrintCXY( zSTRING( title ) ); gMapBottom->PrintCXY( zSTRING( foot ) );
  }

  inline void MapTick() {
    if ( !gMapPanel ) return;
    if ( zinput->KeyToggled( KEY_RETURN ) || zinput->KeyToggled( LV_KEY_MENU ) ) MapClose();
  }
}
