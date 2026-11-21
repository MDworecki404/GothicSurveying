// Podgląd dziennika w grze: tabela w panelu, ↑/↓ przewijanie, Enter/klawisz menu zamyka (gra zatrzymana). Bez strażnika include.
#include <string>
#include <vector>

namespace GOTHIC_NAMESPACE {
  inline zCView*  gFbPanel = nullptr;
  inline FieldBook gFb;
  inline std::string gFbFooter;
  inline int      gFbOffset = 0;
  inline const int LV_FB_VISIBLE_ROWS = 18;

  inline bool FieldBookVisible() { return gFbPanel != nullptr; }

  inline void FieldBookRedraw() {
    if ( !gFbPanel ) return;
    const int X[6] = { 300, 1500, 2700, 3900, 5100, 6300 };
    gFbPanel->ClrPrintwin();
    gFbPanel->PrintCX( 250, zSTRING( "Dziennik niwelacji" ) );
    const char* head[6] = { "Punkt", "Wstecz", "Wprzod", "dh [mm]", "popr.", "H [m]" };
    int y = 900;
    for ( int c = 0; c < 6; c++ ) gFbPanel->Print( X[c], y, zSTRING( head[c] ) );
    y += 320;
    char b[64];
    gFbPanel->Print( X[0], y, zSTRING( gFb.startName.c_str() ) );
    sprintf( b, "%.3f", gFb.startHeightM ); gFbPanel->Print( X[5], y, zSTRING( b ) );
    y += 270;
    for ( int i = gFbOffset; i < (int)gFb.rows.size() && i < gFbOffset + LV_FB_VISIBLE_ROWS; i++, y += 270 ) {
      const FbRow& r = gFb.rows[i];
      gFbPanel->Print( X[0], y, zSTRING( r.pointName.c_str() ) );
      sprintf( b, "%s", ReadingPair( r.backMm, r.back2Mm ).c_str() );  gFbPanel->Print( X[1], y, zSTRING( b ) );
      sprintf( b, "%s", ReadingPair( r.foreMm, r.fore2Mm ).c_str() );  gFbPanel->Print( X[2], y, zSTRING( b ) );
      sprintf( b, "%.1f", r.dhMm );  gFbPanel->Print( X[3], y, zSTRING( b ) );
      if ( gFb.sum.hasClosure ) { sprintf( b, "%.1f", r.vMm ); gFbPanel->Print( X[4], y, zSTRING( b ) ); }
      sprintf( b, "%.3f", r.heightM ); gFbPanel->Print( X[5], y, zSTRING( b ) );
    }
    char s[160];
    if ( gFb.sum.hasClosure )
      sprintf( s, "Suma dh = %.1f mm   fh = %.1f mm   fh dop. = %.1f mm   %s", gFb.sum.sumDhMm, gFb.sum.fhMm, gFb.sum.fhDopMm,
               gFb.sum.withinTolerance ? "w tolerancji" : "PRZEKROCZONY" );
    else sprintf( s, "Suma dh = %.1f mm   ciag otwarty (brak kontroli)", gFb.sum.sumDhMm );
    gFbPanel->PrintCX( 7000, zSTRING( s ) );
    gFbPanel->PrintCX( 7450, zSTRING( gFbFooter.c_str() ) );
  }

  inline void FieldBookShow( const FieldBook& fb, const std::string& footer ) {
    gFb = fb; gFbFooter = footer + "   (Gora/Dol - przewijanie, Enter - zamknij)"; gFbOffset = 0;
    if ( gFbPanel ) PanelClose( gFbPanel ); else ogame->Pause();
    gFbPanel = PanelOpen( 500, 500, 7700, 7700 );
    FieldBookRedraw();
  }

  inline void FieldBookClose() { if ( !gFbPanel ) return; PanelClose( gFbPanel ); ogame->Unpause(); }

  inline void FieldBookTick() {
    if ( !gFbPanel ) return;
    if ( zinput->KeyToggled( KEY_RETURN ) || zinput->KeyToggled( LV_KEY_MENU ) ) { FieldBookClose(); return; }
    int maxOff = (int)gFb.rows.size() - LV_FB_VISIBLE_ROWS;   // bez std::max: makro max z windows.h
    if ( maxOff < 0 ) maxOff = 0;
    if ( zinput->KeyToggled( KEY_DOWN ) && gFbOffset < maxOff ) { gFbOffset++; FieldBookRedraw(); }
    if ( zinput->KeyToggled( KEY_UP ) && gFbOffset > 0 ) { gFbOffset--; FieldBookRedraw(); }
  }

  // Prosty panel z listą wierszy tekstu (wynik sprawdzania w trybie samodzielnym): ↑/↓ przewijanie, Enter zamyka.
  inline zCView*     gTxtPanel = nullptr;
  inline std::string gTxtTitle, gTxtFooter;
  inline std::vector<std::string> gTxtLines;
  inline int         gTxtOffset = 0;
  inline const int   LV_TXT_VISIBLE = 20;

  inline bool TextPanelVisible() { return gTxtPanel != nullptr; }
  inline void TextPanelRedraw() {
    if ( !gTxtPanel ) return;
    gTxtPanel->ClrPrintwin();
    gTxtPanel->PrintCX( 250, zSTRING( gTxtTitle.c_str() ) );
    int y = 1000;
    for ( int i = gTxtOffset; i < (int)gTxtLines.size() && i < gTxtOffset + LV_TXT_VISIBLE; i++, y += 300 )
      gTxtPanel->Print( 300, y, zSTRING( gTxtLines[i].c_str() ) );
    gTxtPanel->PrintCX( 7450, zSTRING( gTxtFooter.c_str() ) );
  }
  inline void TextPanelShow( const std::string& title, const std::vector<std::string>& lines, const std::string& footer ) {
    gTxtTitle = title; gTxtLines = lines; gTxtFooter = footer + "   (Gora/Dol - przewijanie, Enter - zamknij)"; gTxtOffset = 0;
    if ( gTxtPanel ) PanelClose( gTxtPanel ); else ogame->Pause();
    gTxtPanel = PanelOpen( 500, 500, 7700, 7700 );
    TextPanelRedraw();
  }
  inline void TextPanelClose() { if ( !gTxtPanel ) return; PanelClose( gTxtPanel ); ogame->Unpause(); }
  inline void TextPanelTick() {
    if ( !gTxtPanel ) return;
    if ( zinput->KeyToggled( KEY_RETURN ) || zinput->KeyToggled( LV_KEY_MENU ) ) { TextPanelClose(); return; }
    int maxOff = (int)gTxtLines.size() - LV_TXT_VISIBLE; if ( maxOff < 0 ) maxOff = 0;
    if ( zinput->KeyToggled( KEY_DOWN ) && gTxtOffset < maxOff ) { gTxtOffset++; TextPanelRedraw(); }
    if ( zinput->KeyToggled( KEY_UP ) && gTxtOffset > 0 ) { gTxtOffset--; TextPanelRedraw(); }
  }
}
