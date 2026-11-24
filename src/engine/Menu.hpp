// Menu geodezyjne: panel z lista pozycji w sekcjach, linia "DALEJ", okno przewijania, nawigacja Gora/Dol, Enter wybiera, Lewo cofa, klawisz menu zamyka;
// gra zatrzymana, gdy menu otwarte. Uklad wierszy liczy job/MenuLayout.hpp (testowany), tu tylko rysowanie.
// Dołączane raz z Plugin.hpp, bez strażnika include. Wymaga job/Job.hpp (MenuItem) i job/MenuLayout.hpp dołączonych wcześniej.
#include <functional>
#include <string>
#include <vector>

namespace GOTHIC_NAMESPACE {
  // title/items jak dotad (stare wywolania {tytul, {pozycje}} dzialaja); reszta opcjonalna.
  struct MenuView { std::string title; std::vector<MenuItem> items; std::string crumbs, nextText, nextId, footer, screenKey, helpContext; bool digits = false; };   // screenKey: pamiec kursora; helpContext: F1; digits: cyfry 1-9 jako skroty

  inline bool      gMenuOpen = false;
  inline MenuView  gMenu;
  inline int       gMenuCursor = 0;
  inline int       gMenuH = 4800;                      // wysokość panelu menu [jednostki ekranu]; wiersze mają stały odstęp niezależnie od liczby pozycji
  inline int       gMenuMaxRows = 16;                  // okno przewijania: tyle wierszy (z nagłówkami sekcji) widać naraz
  inline zCView*   gMenuPanel = nullptr;
  inline std::function<void( const std::string& )> gMenuChoose;
  inline std::function<void()> gMenuBack;
  inline std::function<void( const std::string& )> gMenuHelpFn;   // F1: ustawia ProjectUi (TextPanelShow jest zdefiniowany pozniej)
  inline std::function<void( const std::string&, int )> gMenuRemember;   // zapamietanie pozycji kursora (screenKey, indeks)
  inline std::function<int( const std::string& )> gMenuRecall;           // odczyt zapamietanej pozycji (-1 = brak)

  inline bool MenuIsOpen() { return gMenuOpen; }

  inline void MenuClose() {
    if ( !gMenuOpen ) return;
    gMenuOpen = false;
    PanelClose( gMenuPanel );
    ogame->Unpause();
  }

  inline int MenuPitch() { return 235; }                 // odstęp wierszy w jednostkach ekranu; Print liczy y w 8192 na wysokość panelu

  // Wysokość panelu zależy tylko od liczby wierszy okna i od tego, czy są linie okruszków / DALEJ / strzałek przewijania.
  inline int MenuComputeHeight( const MenuView& v, int rowsShown, bool scrolls ) {
    const int p = MenuPitch();
    int y = 520;
    if ( !v.crumbs.empty() ) y += p;
    if ( !v.nextText.empty() ) y += p + 60;
    y += ( rowsShown + ( scrolls ? 2 : 0 ) ) * p;
    return y + 100 + 2 * p + 100 + 350;                  // podpis (2 linie) + pasek skrótów
  }

  inline void MenuRedraw() {
    if ( !gMenuPanel ) return;
    gMenuPanel->ClrPrintwin();
    const int p = MenuPitch();
    auto L = [&]( int v ) { return v * 8192 / gMenuH; };
    gMenuPanel->PrintCX( L( 130 ), zSTRING( gMenu.title.c_str() ) );
    int y = 130 + p;
    if ( !gMenu.crumbs.empty() ) { gMenuPanel->PrintCX( L( y ), zSTRING( gMenu.crumbs.c_str() ) ); y += p; }
    if ( !gMenu.nextText.empty() ) { gMenuPanel->PrintCX( L( y + 30 ), zSTRING( ( ">> " + gMenu.nextText ).c_str() ) ); y += p + 60; }
    y = ( std::max )( y, 520 );
    const MenuLayout lay = BuildLayout( gMenu.items, gMenuCursor, gMenuMaxRows );
    const bool scrolls = (int)lay.rows.size() > lay.count;
    if ( scrolls ) { gMenuPanel->Print( 600, L( y ), zSTRING( lay.moreAbove ? "   ^ wyzej" : "" ) ); y += p; }
    for ( int r = lay.first; r < lay.first + lay.count; r++ ) {
      const MenuRow& row = lay.rows[r];
      std::string line;
      if ( row.header ) line = "-- " + row.text + " --";
      else {
        const MenuItem& m = gMenu.items[row.item];
        line = row.item == gMenuCursor ? "> " : "  ";
        if ( gMenu.digits && row.item < 9 ) line += std::to_string( row.item + 1 ) + ". ";
        line += m.enabled ? m.label : "[" + m.label + "]";                   // nieaktywne w nawiasach; powod w podpisie pod lista
        if ( !gMenu.nextId.empty() && m.id == gMenu.nextId ) line += "   <- dalej";
      }
      gMenuPanel->Print( row.header ? 400 : 600, L( y ), zSTRING( line.c_str() ) );
      y += p;
    }
    if ( scrolls ) { gMenuPanel->Print( 600, L( y ), zSTRING( lay.moreBelow ? "   v nizej" : "" ) ); y += p; }
    const int hintY = y + 100;
    if ( gMenuCursor >= 0 && gMenuCursor < (int)gMenu.items.size() ) {      // podpis wybranej pozycji, zawijany do szerokosci panelu
      const MenuItem& m = gMenu.items[gMenuCursor];
      const std::string reason = m.why.empty() ? m.hint : m.why;
      const std::string text = m.enabled ? m.hint : ( reason.empty() ? "" : "Niedostepne: " + reason );
      const std::vector<std::string> wrapped = WrapText( text, 88, 2 );
      for ( size_t i = 0; i < wrapped.size(); i++ ) gMenuPanel->PrintCX( L( hintY + (int)i * p ), zSTRING( wrapped[i].c_str() ) );
    }
    const std::string footer = gMenu.footer.empty() ? std::string( "Gora/Dol, Enter - wybor, " ) + ( gMenuBack ? LV_KEYNAME_BACK " - wstecz, " : "" ) + ( gMenu.helpContext.empty() ? "" : LV_KEYNAME_HELP " - pomoc, " ) + LV_KEYNAME_MENU " - zamknij" : gMenu.footer;
    gMenuPanel->PrintCX( L( hintY + 2 * p + 100 ), zSTRING( footer.c_str() ) );
  }

  inline void MenuOpen( const MenuView& view, std::function<void( const std::string& )> onChoose, std::function<void()> onBack = nullptr ) {
    MenuClose();
    gMenu = view; gMenuChoose = onChoose; gMenuBack = onBack;
    gMenuCursor = 0;
    int firstEnabled = -1, recommended = -1;
    for ( size_t i = 0; i < gMenu.items.size(); i++ ) {
      if ( !gMenu.items[i].enabled ) continue;
      if ( firstEnabled < 0 ) firstEnabled = (int)i;
      if ( recommended < 0 && !gMenu.nextId.empty() && gMenu.items[i].id == gMenu.nextId ) recommended = (int)i;
    }
    gMenuCursor = recommended >= 0 ? recommended : ( firstEnabled >= 0 ? firstEnabled : 0 );
    if ( recommended < 0 && !gMenu.screenKey.empty() && gMenuRecall ) {               // bez rekomendacji: wracamy na ostatnio wybrana pozycje tego ekranu
      const int r = gMenuRecall( gMenu.screenKey );
      if ( r >= 0 && r < (int)gMenu.items.size() && gMenu.items[r].enabled ) gMenuCursor = r;
    }   // kursor od razu na rekomendowanym kroku, a bez niego na pierwszej aktywnej pozycji
    const MenuLayout lay = BuildLayout( gMenu.items, gMenuCursor, gMenuMaxRows );
    gMenuH = MenuComputeHeight( gMenu, lay.count, (int)lay.rows.size() > lay.count );
    const int y0 = ( std::max )( 100, ( std::min )( 900, ( 8192 - gMenuH ) / 2 ) );    // wysokie menu wyzej, zeby miescilo sie na ekranie
    gMenuPanel = PanelOpen( 1500, y0, 6700, y0 + gMenuH );
    gMenuOpen = true;
    ogame->Pause();
    char tb[160]; sprintf( tb, "menu: pozycji %d, wierszy %d, okno %d, wysokosc %d, y0 %d", (int)gMenu.items.size(), (int)lay.rows.size(), lay.count, gMenuH, y0 ); SvTrace( tb );
    MenuRedraw();
  }

  inline void MenuTick() {
    if ( !gMenuOpen ) return;
    if ( zinput->KeyToggled( LV_KEY_MENU ) ) { MenuClose(); return; }
    int moved = 0;
    if ( zinput->KeyToggled( KEY_UP ) )   moved = -1;
    if ( zinput->KeyToggled( KEY_DOWN ) ) moved = 1;
    if ( moved && !gMenu.items.empty() ) { gMenuCursor = MoveCursor( gMenu.items, gMenuCursor, moved ); MenuRedraw(); }   // kursor chodzi tez po nieaktywnych: podpis podaje powod
    if ( gMenuBack && zinput->KeyToggled( LV_KEY_BACK ) ) {
      auto back = gMenuBack;                          // kopia: wywołanie może otworzyć kolejne menu
      MenuClose();
      back();
      return;
    }
    if ( !gMenu.helpContext.empty() && zinput->KeyToggled( LV_KEY_HELP ) ) { SvTrace( "menu: F1 (pomoc)" ); if ( gMenuHelpFn ) gMenuHelpFn( gMenu.helpContext ); return; }
    if ( gMenu.digits ) {                              // cyfry 1-9: skrot do pozycji o tym numerze (tylko aktywnej)
      static const int digitKeys[9] = { KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9 };
      for ( int d = 0; d < 9 && d < (int)gMenu.items.size(); d++ )
        if ( zinput->KeyToggled( digitKeys[d] ) && gMenu.items[d].enabled ) { gMenuCursor = d; MenuRedraw(); break; }
    }
    if ( zinput->KeyToggled( KEY_RETURN ) && gMenuCursor >= 0 && gMenuCursor < (int)gMenu.items.size() && gMenu.items[gMenuCursor].enabled ) {
      std::string id = gMenu.items[gMenuCursor].id;
      if ( !gMenu.screenKey.empty() && gMenuRemember ) gMenuRemember( gMenu.screenKey, gMenuCursor );
      auto choose = gMenuChoose;                      // kopia: wywołanie może otworzyć kolejne menu
      MenuClose();
      if ( choose ) choose( id );
    }
  }
}
