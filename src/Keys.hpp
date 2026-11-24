#pragma once
// Wszystkie klawisze pluginu w jednym miejscu. Unikamy: F5/F6 (zapis/odczyt gry), F11 (menu GD3D11), F12 (zrzut ekranu Steam),
// cyfr i Backspace (wpis odczytu), strzałek/Shift/Enter (obrót i zatwierdzanie).
// Nazwy (do tekstów w grze i dokumentacji) - bez Gothic API, używane także przez job/.
#define LV_KEYNAME_MENU "Home"
#define LV_KEYNAME_SCOPE "F10"
#define LV_KEYNAME_ROD_BACK "Z"
#define LV_KEYNAME_ROD_FORE "X"
#define LV_KEYNAME_AIM "Z"
#define LV_KEYNAME_FLIP "K"
#define LV_KEYNAME_ZOOM "PgUp/PgDn"
#define LV_KEYNAME_BACK "Lewo"       // w menu: poprzedni ekran

// Kody klawiszy: tylko w silniku (testy ich nie mają); namespace jednej wersji gry na build (Plugin.cpp kompiluje tylko wybraną).
#ifdef KEY_HOME
namespace GOTHIC_NAMESPACE {
  inline const int LV_KEY_MENU     = KEY_HOME;   // menu geodezyjne
  inline const int LV_KEY_SCOPE    = KEY_F10;    // wejście do lunety / wyjście
  inline const int LV_KEY_ROD_BACK = KEY_Z;      // w lunecie: obrót na łatę wstecz
  inline const int LV_KEY_ROD_FORE = KEY_X;      // w lunecie: obrót na łatę wprzód
  inline const int LV_KEY_BACK     = KEY_LEFT;   // w menu: wstecz (poprzedni ekran)
  inline const int LV_KEY_ZOOM_IN  = KEY_PGUP;   // w lunecie: wieksze powiekszenie
  inline const int LV_KEY_ZOOM_OUT = KEY_PGDN;   // w lunecie: mniejsze powiekszenie
  inline const int LV_KEY_FLIP     = KEY_K;      // w lunecie tachimetru: przerzut lunety KL <-> KP
  inline const int LV_KEY_AIM      = KEY_Z;      // w lunecie tachimetru: szybkie najechanie na kolejne lustro
}
#endif
