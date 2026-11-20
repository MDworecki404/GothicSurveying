// Edytor dziennika w trybie samodzielnym: siatka komórek (dh, [v], H na stanowisko + Σdh, [fh]); strzałki zmieniają komórkę,
// cyfry/minus/kropka wpisują wartość, Backspace kasuje, F12 kończy (sprawdzenie robi kontroler). Bez strażnika include.
#include <cstdlib>
#include <string>
#include <vector>

namespace GOTHIC_NAMESPACE {
  inline bool      gEditOn = false;
  inline FieldBook gEditFb;
  inline std::vector<std::vector<std::string>> gEditGrid;   // [wiersz][kolumna] = wpisany tekst
  inline int       gEditRow = 0, gEditCol = 0;
  inline zCView*   gEditPanel = nullptr;

  inline bool FieldBookEditActive() { return gEditOn; }
  inline int  EditCols( int row ) {                    // wiersze stanowisk: dh, [v], H; wiersz sumy: Σdh, [fh]
    const bool c = gEditFb.sum.hasClosure;
    return row < (int)gEditFb.rows.size() ? ( c ? 3 : 2 ) : ( c ? 2 : 1 );
  }

  inline void FieldBookEditRedraw() {
    if ( !gEditPanel ) return;
    const int X[6] = { 250, 1500, 2600, 3700, 5000, 6300 };   // punkt, wstecz, wprzod, | komorki: dh, [v], H
    const bool closure = gEditFb.sum.hasClosure;
    gEditPanel->ClrPrintwin();
    gEditPanel->PrintCX( 200, zSTRING( "Dziennik - wpisz wyniki obliczen (tryb samodzielny)" ) );
    char ref[120];
    if ( gEditFb.kind == TraverseKind::Anchored && gEditFb.hasEnd ) sprintf( ref, "H reper A = %.3f m   H reper B = %.3f m", gEditFb.startHeightM, gEditFb.endHeightM );
    else sprintf( ref, "H reper A = %.3f m%s", gEditFb.startHeightM, gEditFb.kind == TraverseKind::Closed ? "   (ciag zamkniety: koniec = A)" : "" );
    gEditPanel->PrintCX( 650, zSTRING( ref ) );
    gEditPanel->Print( X[0], 1100, zSTRING( "Punkt" ) );
    gEditPanel->Print( X[1], 1100, zSTRING( "Wstecz" ) );
    gEditPanel->Print( X[2], 1100, zSTRING( "Wprzod" ) );
    gEditPanel->Print( X[3], 1100, zSTRING( "dh [mm]" ) );
    if ( closure ) gEditPanel->Print( X[4], 1100, zSTRING( "popr. v [mm]" ) );
    gEditPanel->Print( X[closure ? 5 : 4], 1100, zSTRING( "H [m]" ) );
    auto cell = [&]( int r, int c, int x, int y ) {
      std::string t = gEditGrid[r][c].empty() ? "_" : gEditGrid[r][c];
      if ( r == gEditRow && c == gEditCol ) t = "[" + t + "]";
      gEditPanel->Print( x, y, zSTRING( t.c_str() ) );
    };
    const int n = (int)gEditFb.rows.size();
    char b[32];
    for ( int r = 0; r < n; r++ ) {
      const int y = 1500 + r * 330;
      gEditPanel->Print( X[0], y, zSTRING( gEditFb.rows[r].pointName.c_str() ) );
      sprintf( b, "%d", gEditFb.rows[r].backMm ); gEditPanel->Print( X[1], y, zSTRING( b ) );
      sprintf( b, "%d", gEditFb.rows[r].foreMm ); gEditPanel->Print( X[2], y, zSTRING( b ) );
      for ( int c = 0; c < EditCols( r ); c++ ) cell( r, c, X[c + 3], y );
    }
    const int ys = 1500 + n * 330 + 250;
    gEditPanel->Print( X[0], ys, zSTRING( "Suma dh [mm]" ) );
    cell( n, 0, X[3], ys );
    if ( closure ) { gEditPanel->Print( X[4], ys, zSTRING( "fh [mm]" ) ); cell( n, 1, X[5], ys ); }
    gEditPanel->PrintCX( 7300, zSTRING( "Strzalki - komorka, cyfry - . - wartosc, Backspace - kasuj, F12 - sprawdz" ) );
  }

  inline void FieldBookEditStart( const FieldBook& fb ) {
    gEditFb = fb; gEditRow = gEditCol = 0;
    gEditGrid.assign( fb.rows.size() + 1, std::vector<std::string>( 3 ) );
    if ( !gEditOn ) ogame->Pause();
    PanelClose( gEditPanel );
    gEditPanel = PanelOpen( 500, 500, 7700, 7700 );
    gEditOn = true;
    FieldBookEditRedraw();
  }

  inline void FieldBookEditCancel() {
    if ( !gEditOn ) return;
    gEditOn = false; PanelClose( gEditPanel ); ogame->Unpause();
  }

  inline PlayerEntries FieldBookEditResult() {
    auto val = []( const std::string& t ) { return t.empty() || t == "-" || t == "." ? std::numeric_limits<double>::quiet_NaN() : atof( t.c_str() ); };
    PlayerEntries p;
    const int n = (int)gEditFb.rows.size();
    const bool closure = gEditFb.sum.hasClosure;
    for ( int r = 0; r < n; r++ ) {
      p.dhMm.push_back( val( gEditGrid[r][0] ) );
      p.vMm.push_back( closure ? val( gEditGrid[r][1] ) : std::numeric_limits<double>::quiet_NaN() );
      p.heightM.push_back( val( gEditGrid[r][closure ? 2 : 1] ) );
    }
    p.sumDhMm = val( gEditGrid[n][0] );
    p.fhMm = closure ? val( gEditGrid[n][1] ) : std::numeric_limits<double>::quiet_NaN();
    return p;
  }

  // Zwraca true, gdy gracz zakończył edycję (F12); wtedy panel jest już zamknięty, a wpisy dostępne przez FieldBookEditResult().
  inline bool FieldBookEditTick() {
    if ( !gEditOn ) return false;
    if ( zinput->KeyToggled( LV_KEY_MENU ) ) { gEditOn = false; PanelClose( gEditPanel ); ogame->Unpause(); return true; }
    const int rows = (int)gEditGrid.size();
    bool redraw = false;
    if ( zinput->KeyToggled( KEY_DOWN ) && gEditRow < rows - 1 ) { gEditRow++; redraw = true; }
    if ( zinput->KeyToggled( KEY_UP ) && gEditRow > 0 ) { gEditRow--; redraw = true; }
    if ( zinput->KeyToggled( KEY_RIGHT ) ) { gEditCol++; redraw = true; }
    if ( zinput->KeyToggled( KEY_LEFT ) ) { gEditCol--; redraw = true; }
    if ( gEditCol < 0 ) gEditCol = 0;
    if ( gEditCol >= EditCols( gEditRow ) ) gEditCol = EditCols( gEditRow ) - 1;

    std::string& t = gEditGrid[gEditRow][gEditCol];
    static const int digitKeys[10] = { KEY_0, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9 };
    for ( int d = 0; d < 10; d++ ) if ( zinput->KeyToggled( digitKeys[d] ) && t.size() < 10 ) { t += (char)( '0' + d ); redraw = true; }
    if ( zinput->KeyToggled( KEY_MINUS ) && t.empty() ) { t = "-"; redraw = true; }
    if ( ( zinput->KeyToggled( KEY_PERIOD ) || zinput->KeyToggled( KEY_COMMA ) ) && t.find( '.' ) == std::string::npos && t.size() < 10 ) { t += '.'; redraw = true; }
    if ( zinput->KeyToggled( KEY_BACK ) && !t.empty() ) { t.pop_back(); redraw = true; }
    if ( redraw ) FieldBookEditRedraw();
    return false;
  }
}
