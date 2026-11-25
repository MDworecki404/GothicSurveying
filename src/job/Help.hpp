// Pomoc kontekstowa (F1): krotkie teksty ASCII (do 88 znakow w linii) dla ekranu glownego, rodzajow robot i mapy.
#pragma once
#include <string>
#include <vector>
#include "Job.hpp"
#include "Keys.hpp"

inline std::string HelpContextFor(JobKind k) {
  switch (k) {
    case JobKind::Leveling: return "job:leveling";
    case JobKind::Gnss: return "job:gnss";
    case JobKind::Tachy: return "job:tachy";
    case JobKind::Scan: return "job:scan";
    case JobKind::Aerial: return "job:aerial";
    case JobKind::Photo: return "job:photo";
    case JobKind::Polygon: return "job:polygon";
  }
  return "home";
}

inline std::vector<std::string> HelpLines(const std::string& context) {
  std::vector<std::string> menu = {
    "Menu: Gora/Dol - wybor, Enter - zatwierdz, " LV_KEYNAME_BACK " - wstecz, " LV_KEYNAME_MENU " - zamknij.",
    "Linia DALEJ podpowiada nastepny krok; kursor od razu staje na nim.",
    "Pozycje w [nawiasach] sa niedostepne: podpis pod lista podaje powod.",
  };
  std::vector<std::string> extra;
  if (context == "home") extra = {
    "Projekt grupuje pomiary jednego swiata; wyniki pomiarow sa w ich folderach.",
    "Cyfry 1-9 wybieraja pozycje listy. Nowy pomiar: kreator (gotowe ustawienia) albo reczny.",
    "Po zakonczeniu pomiaru mozna od razu zaczac nastepny, tez tego samego rodzaju.",
  };
  else if (context == "job:leveling") extra = {
    LV_KEYNAME_SCOPE " - luneta: odczyt lat; " LV_KEYNAME_ROD_BACK "/" LV_KEYNAME_ROD_FORE " - obrot na lata wstecz/wprzod; " LV_KEYNAME_ZOOM " - powiekszenie.",
    "Kolejnosc: reper A, niwelator, lata wstecz, lata wprzod, odczyty w lunecie, przelozenie.",
  };
  else if (context == "job:gnss") extra = {
    "Postaw tyczke, rozpocznij pomiar punktu, poczekaj na fix i zapisz punkt.",
    "RTK sluzy do zdjecia sytuacyjnego; punkty osnowy zakladasz w robocie Statyka GNSS.",
    "Statyka (osobna robota): poprawki z sieci albo wlasna baza; min. 30 min sesji.",
  };
  else if (context == "job:tachy") extra = {
    LV_KEYNAME_SCOPE " - luneta tachimetru; Enter - orientacja albo pomiar; " LV_KEYNAME_FLIP " - przerzut lunety (KL/KP).",
    "Kolejnosc: stanowisko, nawiazania (orientacja w lunecie), tyczka z lustrem, pomiar.",
  };
  else if (context == "job:scan") extra = {
    "Kolejnosc: stanowisko skanera, wypoziomowanie, kule (min. 3 przy rejestracji), skan.",
    "Zakres: pelne 360 st. albo okno z lunety (Enter: lewy dolny, potem prawy gorny rog).",
    "Wynik: pliki .pts i .las w folderze skanu.",
  };
  else if (context == "job:aerial") extra = {
    "Kolejnosc: obszar (min. 3 wierzcholki lub zaznaczenie na mapie), parametry, start lotu.",
    "Po locie: wyrownanie szeregow z kalibracja boresightu, potem produkty z chmury.",
  };
  else if (context == "job:photo") extra = {
    "Kolejnosc: obszar, kamera i zakladki, start lotu; wyrownanie wiazki liczy sie samo.",
    "Po wyrownaniu mozna wygenerowac ortofotomape (PNG z plikiem .pgw).",
  };
  else if (context == "job:polygon") extra = {
    "Kolejnosc: nawiazania z osnowy, punkty ciagu, tachimetr i lustro na kazdym punkcie.",
    "Stanowisko: lustro tyl i przod w KL, przerzut " LV_KEYNAME_FLIP ", to samo w KP. Boki z obu koncow.",
    "Ciag nawiazany w limitach: Zapisz punkty ciagu jako osnowe (poligonizacja).",
    "Podglad rachunku pokazuje odchylki; Powtorz stanowisko poprawia zly kat.",
  };
  else if (context == "map") extra = {
    "Mapa: strzalki przesuwaja (Shift - szybciej, Ctrl - dokladniej); Enter - zatwierdz.",
    "Przy zaznaczaniu obszaru (nalot) " LV_KEYNAME_ZOOM " obraca prostokat; Home zamyka mape.",
  };
  else extra = { "Brak osobnej pomocy dla tego ekranu." };
  for (const std::string& l : menu) extra.push_back(l);
  return extra;
}
