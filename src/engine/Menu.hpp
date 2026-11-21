// Menu geodezyjne: panel z listą pozycji, nawigacja ↑/↓, Enter wybiera, klawisz menu zamyka; gra zatrzymana, gdy menu otwarte.
// Dołączane raz z Plugin.hpp, bez strażnika include. Wymaga job/Job.hpp (MenuItem) dołączonego wcześniej.
#include <functional>
#include <string>
#include <vector>

namespace GOTHIC_NAMESPACE {
  struct MenuView { std::string title; std::vector<MenuItem> items; };

  inline bool      gMenuOpen = false;
  inline MenuView  gMenu;
  inline int       gMenuCursor = 0;
  inline zCView*   gMenuPanel = nullptr;
  inline std::function<void( const std::string& )> gMenuChoose;

  inline bool MenuIsOpen() { return gMenuOpen; }

  inline void MenuClose() {
    if ( !gMenuOpen ) return;
    gMenuOpen = false;
    PanelClose( gMenuPanel );
    ogame->Unpause();
  }

  inline void MenuRedraw() {
    if ( !gMenuPanel ) return;
    gMenuPanel->ClrPrintwin();
    gMenuPanel->PrintCX( 350, zSTRING( gMenu.title.c_str() ) );
    for ( size_t i = 0; i < gMenu.items.size(); i++ ) {
      const MenuItem& m = gMenu.items[i];
      std::string line = (int)i == gMenuCursor ? "> " : "  ";
      line += m.enabled ? m.label : "[" + m.label + "] - " + m.hint;
      gMenuPanel->PrintCX( 1000 + (int)i * 400, zSTRING( line.c_str() ) );
    }
    gMenuPanel->PrintCX( 1000 + (int)gMenu.items.size() * 400 + 350, zSTRING( "Gora/Dol - wybor, Enter - zatwierdz, " LV_KEYNAME_MENU " - zamknij" ) );
  }

  inline int MenuNextEnabled( int from, int dir ) {   // pomija pozycje nieaktywne; -1 gdy żadnej nie ma
    const int n = (int)gMenu.items.size();
    for ( int k = 1; k <= n; k++ ) {
      int i = ( ( from + dir * k ) % n + n ) % n;
      if ( gMenu.items[i].enabled ) return i;
    }
    return -1;
  }

  inline void MenuOpen( const MenuView& view, std::function<void( const std::string& )> onChoose ) {
    MenuClose();
    gMenu = view; gMenuChoose = onChoose;
    gMenuCursor = 0;
    if ( !gMenu.items.empty() && !gMenu.items[0].enabled ) { int i = MenuNextEnabled( 0, 1 ); gMenuCursor = i < 0 ? 0 : i; }
    gMenuPanel = PanelOpen( 1500, 900, 6700, 1000 + ( (int)view.items.size() + 2 ) * 400 + 700 );
    gMenuOpen = true;
    ogame->Pause();
    MenuRedraw();
  }

  inline void MenuTick() {
    if ( !gMenuOpen ) return;
    if ( zinput->KeyToggled( LV_KEY_MENU ) ) { MenuClose(); return; }
    int moved = 0;
    if ( zinput->KeyToggled( KEY_UP ) )   moved = -1;
    if ( zinput->KeyToggled( KEY_DOWN ) ) moved = 1;
    if ( moved ) { int i = MenuNextEnabled( gMenuCursor, moved ); if ( i >= 0 ) { gMenuCursor = i; MenuRedraw(); } }
    if ( zinput->KeyToggled( KEY_RETURN ) && gMenuCursor >= 0 && gMenuCursor < (int)gMenu.items.size() && gMenu.items[gMenuCursor].enabled ) {
      std::string id = gMenu.items[gMenuCursor].id;
      auto choose = gMenuChoose;                      // kopia: wywołanie może otworzyć kolejne menu
      MenuClose();
      if ( choose ) choose( id );
    }
  }
}
