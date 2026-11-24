// Proste wpisywanie tekstu w grze (nazwa projektu): litery A-Z (Shift = wielkie), cyfry, '-' i '_' (Shift + '-'), Backspace kasuje,
// Enter zatwierdza (po walidacji), Home anuluje. Klawisze czytane po kodach (jak cyfry w edytorze dziennika). Bez strażnika include.
#include <functional>
#include <string>

namespace GOTHIC_NAMESPACE {
  inline bool        gTiOn = false;
  inline std::string gTiTitle, gTiText;
  inline size_t      gTiMax = 32;
  inline zCView*     gTiPanel = nullptr;
  inline std::function<std::string( const std::string& )> gTiValidate;   // "" = poprawne, inaczej powod
  inline std::function<void( const std::string& )>        gTiOk;
  inline std::function<void()>                            gTiCancel;

  inline bool TextInputActive() { return gTiOn; }

  inline void TextInputRedraw() {
    if ( !gTiPanel ) return;
    gTiPanel->ClrPrintwin();
    gTiPanel->PrintCX( 700, zSTRING( gTiTitle.c_str() ) );
    gTiPanel->PrintCX( 2700, zSTRING( ( "[ " + gTiText + "_ ]" ).c_str() ) );
    const std::string problem = gTiValidate ? gTiValidate( gTiText ) : std::string();
    gTiPanel->PrintCX( 4200, zSTRING( problem.empty() ? "Enter - zatwierdz" : ( "Nie mozna: " + problem ).c_str() ) );
    gTiPanel->PrintCX( 6300, zSTRING( "litery bez polskich znakow (Shift - wielkie), cyfry, - i _ (Shift + -); Backspace - kasuj" ) );
    gTiPanel->PrintCX( 7100, zSTRING( "Enter - zatwierdz, " LV_KEYNAME_MENU " - anuluj" ) );
  }

  inline void TextInputClose() {
    if ( !gTiOn ) return;
    gTiOn = false; PanelClose( gTiPanel ); ogame->Unpause();
  }

  inline void TextInputStart( const std::string& title, const std::string& initial, size_t maxLen, std::function<std::string( const std::string& )> validate,
                              std::function<void( const std::string& )> onOk, std::function<void()> onCancel ) {
    gTiTitle = title; gTiText = initial; gTiMax = maxLen; gTiValidate = validate; gTiOk = onOk; gTiCancel = onCancel;
    if ( !gTiOn ) ogame->Pause();
    PanelClose( gTiPanel );
    gTiPanel = PanelOpen( 1500, 2300, 6700, 5900 );
    gTiOn = true;
    SvTrace( ( "wpisywanie tekstu: " + title ).c_str() );
    TextInputRedraw();
  }

  inline void TextInputTick() {
    if ( !gTiOn ) return;
    if ( zinput->KeyToggled( LV_KEY_MENU ) ) { auto cancel = gTiCancel; TextInputClose(); if ( cancel ) cancel(); return; }
    if ( zinput->KeyToggled( KEY_RETURN ) ) {
      const std::string problem = gTiValidate ? gTiValidate( gTiText ) : std::string();
      if ( !problem.empty() ) { TextInputRedraw(); return; }
      auto ok = gTiOk; const std::string text = gTiText;
      TextInputClose();
      if ( ok ) ok( text );
      return;
    }
    static const int letters[26] = { KEY_A, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G, KEY_H, KEY_I, KEY_J, KEY_K, KEY_L, KEY_M,
                                     KEY_N, KEY_O, KEY_P, KEY_Q, KEY_R, KEY_S, KEY_T, KEY_U, KEY_V, KEY_W, KEY_X, KEY_Y, KEY_Z };
    static const int digits[10] = { KEY_0, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9 };
    const bool shift = zinput->KeyPressed( KEY_LSHIFT ) || zinput->KeyPressed( KEY_RSHIFT );
    bool redraw = false;
    for ( int i = 0; i < 26; i++ ) if ( zinput->KeyToggled( letters[i] ) && gTiText.size() < gTiMax ) { gTiText += (char)( ( shift ? 'A' : 'a' ) + i ); redraw = true; }
    for ( int i = 0; i < 10; i++ ) if ( zinput->KeyToggled( digits[i] ) && gTiText.size() < gTiMax ) { gTiText += (char)( '0' + i ); redraw = true; }
    if ( zinput->KeyToggled( KEY_MINUS ) && gTiText.size() < gTiMax ) { gTiText += shift ? '_' : '-'; redraw = true; }
    if ( zinput->KeyToggled( KEY_BACK ) && !gTiText.empty() ) { gTiText.pop_back(); redraw = true; }
    if ( redraw ) TextInputRedraw();
  }
}
