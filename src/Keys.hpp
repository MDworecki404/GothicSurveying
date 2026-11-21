#pragma once
// Wszystkie klawisze pluginu w jednym miejscu. Unikamy: F5/F6 (zapis/odczyt gry), F11 (menu GD3D11), F12 (zrzut ekranu Steam),
// cyfr i Backspace (wpis odczytu), strzałek/Shift/Enter (obrót i zatwierdzanie).
// Nazwy (do tekstów w grze i dokumentacji) - bez Gothic API, używane także przez job/.
#define LV_KEYNAME_MENU "Home"
#define LV_KEYNAME_SCOPE "F10"
#define LV_KEYNAME_ROD_BACK "Z"
#define LV_KEYNAME_ROD_FORE "X"

// Kody klawiszy: tylko w silniku (testy ich nie mają); namespace jednej wersji gry na build (Plugin.cpp kompiluje tylko wybraną).
#ifdef KEY_HOME
namespace GOTHIC_NAMESPACE {
  inline const int LV_KEY_MENU     = KEY_HOME;   // menu geodezyjne
  inline const int LV_KEY_SCOPE    = KEY_F10;    // wejście do lunety / wyjście
  inline const int LV_KEY_ROD_BACK = KEY_Z;      // w lunecie: obrót na łatę wstecz
  inline const int LV_KEY_ROD_FORE = KEY_X;      // w lunecie: obrót na łatę wprzód
}
#endif
