// Mapa osnowy: panel z mapa swiata (mapa z gry LVL_GMAP_<swiat>.TGA wyciagnieta z VDF przez assets/make_game_maps.py, a bez niej wygenerowana LVL_MAP_<swiat>.TGA),
// punktami osnowy (numery) i znacznikiem bohatera.
// Gra zatrzymana, Enter / klawisz menu zamyka. Dolaczane raz z Plugin.hpp po ControlNet.hpp i Panels.hpp, bez straznika include.
#include <cmath>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace GOTHIC_NAMESPACE {
  inline zCView*              gMapPanel = nullptr, *gMapTop = nullptr, *gMapBottom = nullptr;
  inline std::vector<zCView*> gMapDots;
  // Tryb zaznaczania obszaru (dwa przeciwlegle narozniki): celownik strzalkami, Enter = narozniki 1 i 2; wynik w metrach ukladu mapy (E, N).
  inline bool gMapPick = false; inline int gPickStage = 0; inline size_t gPickBase = 0;
  inline double gPickU = 0.5, gPickV = 0.5, gPickU1 = 0, gPickV1 = 0, gPickHeld[4] = { 0, 0, 0, 0 }; inline DWORD gPickLast = 0;
  inline int gMapX0 = 0, gMapMy0 = 0, gMapW = 0, gMapH = 0, gMapBar = 38, gMapGap = 6; inline WorldMapView gMapMv;
  inline std::function<void( double, double, double, double )> gPickDone;

  inline bool MapActive() { return gMapPanel != nullptr; }

  inline void MapClose() {
    if ( !gMapPanel ) return;
    for ( zCView*& d : gMapDots ) { d->Close(); screen->RemoveItem( d ); delete d; }
    gMapDots.clear(); gMapPick = false; gPickDone = nullptr;
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

  inline void MapPickRedraw() {                                                         // celownik i prostokat: dopisywane za statycznymi kropkami mapy
    while ( gMapDots.size() > gPickBase ) { zCView* d = gMapDots.back(); d->Close(); screen->RemoveItem( d ); delete d; gMapDots.pop_back(); }
    const int cx = gMapX0 + (int)( gPickU * gMapW ), cy = gMapMy0 + (int)( gPickV * gMapH );
    double e1, n1, e2, n2; MapFromUnit( gMapMv, gPickU1, gPickV1, e1, n1 ); MapFromUnit( gMapMv, gPickU, gPickV, e2, n2 );
    if ( gPickStage == 1 ) {
      const int ax = gMapX0 + (int)( gPickU1 * gMapW ), ay = gMapMy0 + (int)( gPickV1 * gMapH );
      const int xs[5] = { ax, cx, cx, ax, ax }, ys[5] = { ay, ay, cy, cy, ay };
      int dots = 0;
      for ( int k = 0; k < 4; k++ ) {
        const double len = hypot( (double)( xs[k + 1] - xs[k] ), (double)( ys[k + 1] - ys[k] ) ); const int steps = (int)( std::max )( 1.0, len / 6.0 );
        for ( int i = 0; i < steps && dots < 400; i++, dots++ ) MapDot( "LVL_DOTY.TGA", xs[k] + ( xs[k + 1] - xs[k] ) * i / steps, ys[k] + ( ys[k + 1] - ys[k] ) * i / steps, 4 );
      }
      MapDot( "LVL_DOTG.TGA", ax, ay, 12 );                                              // naroznik 1
    }
    MapDot( "LVL_DOTR.TGA", cx, cy, 14 );                                                // celownik
    char foot[220];
    if ( gPickStage == 0 ) sprintf( foot, "strzalki - celownik (Shift szybko, Ctrl wolno)   Enter - naroznik 1   Home - anuluj" );
    else sprintf( foot, "obszar %.0f x %.0f m (%.2f ha)   strzalki - celownik   Enter - naroznik 2 i koniec   Backspace - cofnij   Home - anuluj",
                  fabs( e2 - e1 ), fabs( n2 - n1 ), fabs( e2 - e1 ) * fabs( n2 - n1 ) / 10000.0 );
    PanelClose( gMapBottom );
    const int bar = gMapBar, gap = gMapGap;
    gMapBottom = PanelOpen( screen->anx( gMapX0 ), screen->any( gMapMy0 + gMapH + gap ), screen->anx( gMapX0 + gMapW ), screen->any( gMapMy0 + gMapH + gap + bar ) );
    gMapBottom->PrintCXY( zSTRING( foot ) );
  }

  inline void MapPickTick() {
    const DWORD now = GetTickCount(); double dt = ( now - gPickLast ) / 1000.0; gPickLast = now; if ( dt > 0.1 ) dt = 0.1;
    if ( zinput->KeyToggled( LV_KEY_MENU ) ) { MapClose(); return; }
    if ( zinput->KeyToggled( KEY_BACK ) && gPickStage == 1 ) { gPickStage = 0; MapPickRedraw(); return; }
    if ( zinput->KeyToggled( KEY_RETURN ) ) {
      if ( gPickStage == 0 ) { gPickU1 = gPickU; gPickV1 = gPickV; gPickStage = 1; MapPickRedraw(); return; }
      double e1, n1, e2, n2; MapFromUnit( gMapMv, gPickU1, gPickV1, e1, n1 ); MapFromUnit( gMapMv, gPickU, gPickV, e2, n2 );
      const std::function<void( double, double, double, double )> done = gPickDone;
      MapClose();
      if ( done ) done( e1, n1, e2, n2 );
      return;
    }
    const double px = zinput->KeyPressed( KEY_LCONTROL ) ? 1.0 : zinput->KeyPressed( KEY_LSHIFT ) ? 24.0 : 6.0;
    static const int keys[4] = { KEY_LEFT, KEY_RIGHT, KEY_UP, KEY_DOWN };
    static const double dU[4] = { -1, 1, 0, 0 }, dV[4] = { 0, 0, -1, 1 };
    bool moved = false;
    for ( int i = 0; i < 4; i++ ) {
      bool fire = zinput->KeyToggled( keys[i] );
      if ( zinput->KeyPressed( keys[i] ) ) {
        const double before = gPickHeld[i]; gPickHeld[i] += dt;
        if ( !fire && gPickHeld[i] > 0.4 ) { const int a = (int)( ( before - 0.4 ) * 25.0 ), b = (int)( ( gPickHeld[i] - 0.4 ) * 25.0 ); fire = b > a || before <= 0.4; }
      } else gPickHeld[i] = 0;
      if ( !fire ) continue;
      gPickU = ( std::max )( 0.0, ( std::min )( 1.0, gPickU + dU[i] * px / gMapW ) ); gPickV = ( std::max )( 0.0, ( std::min )( 1.0, gPickV + dV[i] * px / gMapH ) ); moved = true;
    }
    if ( moved ) MapPickRedraw();
  }

  inline void MapOpen( bool pick = false, std::function<void( double, double, double, double )> done = nullptr ) {
    if ( MapActive() ) return;
    const std::string wn = ControlWorldName(); WorldMapView mv;
    const std::string key = ControlWorldKey( wn );
    std::error_code fec;                                                    // mapa z gry (pergamin), gdy wyciagnieta z VDF; inaczej mapa wygenerowana z modelu swiata
    const bool gameMap = GameMapFor( wn, mv ) && std::filesystem::exists( "_work/Data/Textures/LVL_GMAP_" + key + ".TGA", fec );
    if ( !gameMap && !WorldMapFor( wn, mv ) ) { HudMessage( ( "Brak mapy osnowy dla swiata " + wn ).c_str() ); return; }
    SvTrace( gameMap ? "mapa osnowy: mapa z gry" : "mapa osnowy: mapa wygenerowana (brak LVL_GMAP; uruchom assets/make_game_maps.py)" );
    ControlEnsureLoaded();
    const int sw = screen->nax( 8192 ), sh = screen->nay( 8192 );          // rozmiar ekranu w pikselach
    const int bar = 38, gap = 6;                                            // paski tekstu nad i pod mapą
    int H = (int)( sh * 0.72 ), W = (int)( H * mv.w / mv.h );               // panel w proporcjach mapy (pergamin 4:3, wygenerowana kwadratowa)
    if ( W > (int)( sw * 0.92 ) ) { W = (int)( sw * 0.92 ); H = (int)( W * mv.h / mv.w ); }
    const int total = H + 2 * bar + 2 * gap, x0 = ( sw - W ) / 2, y0 = ( sh - total ) / 2;
    const int my0 = y0 + bar + gap;                                         // górny brzeg mapy [px]
    gMapX0 = x0; gMapMy0 = my0; gMapW = W; gMapH = H; gMapMv = mv; gMapBar = bar; gMapGap = gap;
    const std::string tex = std::string( gameMap ? "LVL_GMAP_" : "LVL_MAP_" ) + key + ".TGA";
    gMapTop    = PanelOpen( screen->anx( x0 ), screen->any( y0 ), screen->anx( x0 + W ), screen->any( y0 + bar ) );
    gMapPanel  = PanelOpenTex( screen->anx( x0 ), screen->any( my0 ), screen->anx( x0 + W ), screen->any( my0 + H ), tex.c_str() );
    gMapBottom = PanelOpen( screen->anx( x0 ), screen->any( my0 + H + gap ), screen->anx( x0 + W ), screen->any( my0 + H + gap + bar ) );
    ogame->Pause();

    const zVEC3 hero = player ? player->GetPositionWorld() : zVEC3( 0, 0, 0 );
    std::string nearest; double nearestD = 1e18;
    int shown = 0;
    for ( const ControlPoint& c : gControl ) {
      if ( ControlWorldKey( c.world ) != ControlWorldKey( wn ) ) continue;
      double u, v; MapUnit( mv, c.x / 100.0, c.z / 100.0, u, v );
      if ( u < 0 || u > 1 || v < 0 || v > 1 ) continue;
      const int px = x0 + (int)( u * W ), py = my0 + (int)( v * H );
      MapDot( c.id[0] == 'G' ? "LVL_DOTR.TGA" : "LVL_DOTG.TGA", px, py, 12 );   // czerwone: osnowa wzorcowa, zielone: z GNSS
      gMapPanel->Print( (int)( u * 8192 ) + 130, (int)( v * 8192 ) - 330, zSTRING( c.id.substr( 1 ).c_str() ) );
      shown++;
      const double dd = hypot( c.x - hero[VX], c.z - hero[VZ] );
      if ( dd < nearestD ) { nearestD = dd; nearest = c.id; }
    }
    double hu, hv; MapUnit( mv, hero[VX] / 100.0, hero[VZ] / 100.0, hu, hv );
    if ( hu >= 0 && hu <= 1 && hv >= 0 && hv <= 1 ) MapDot( "LVL_DOTY.TGA", x0 + (int)( hu * W ), my0 + (int)( hv * H ), 16 );

    char title[160], foot[200];
    sprintf( title, "Mapa osnowy - swiat %s: %d pkt", ControlWorldKey( wn ).c_str(), shown );
    if ( shown > 0 ) sprintf( foot, "zolty - ty   czerwone G.. - wzorcowa   zielone O.. - GNSS   najblizszy: %s (%.0f m)   Enter - zamknij", nearest.c_str(), nearestD / 100.0 );
    else sprintf( foot, "Brak punktow osnowy w tym swiecie (Zaladuj osnowe geodezyjna swiata)   Enter - zamknij" );
    gMapTop->PrintCXY( zSTRING( title ) );
    gMapBottom->PrintCXY( zSTRING( foot ) );
    if ( pick ) {                                                                       // zaznaczanie obszaru: celownik zaczyna od bohatera (w granicach mapy)
      gMapPick = true; gPickDone = done; gPickStage = 0; gPickBase = gMapDots.size(); gPickLast = GetTickCount();
      gPickU = ( std::max )( 0.0, ( std::min )( 1.0, hu ) ); gPickV = ( std::max )( 0.0, ( std::min )( 1.0, hv ) );
      for ( double& h : gPickHeld ) h = 0;
      sprintf( title, "Zaznacz obszar nalotu - swiat %s%s", key.c_str(), gameMap ? " (mapa z gry, rysunek przyblizony)" : "" ); PanelClose( gMapTop );
      gMapTop = PanelOpen( screen->anx( x0 ), screen->any( y0 ), screen->anx( x0 + W ), screen->any( y0 + bar ) ); gMapTop->PrintCXY( zSTRING( title ) );
      MapPickRedraw();
    }
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
    if ( gMapPick ) { MapPickTick(); return; }
    if ( zinput->KeyToggled( KEY_RETURN ) || zinput->KeyToggled( LV_KEY_MENU ) ) MapClose();
  }
}
