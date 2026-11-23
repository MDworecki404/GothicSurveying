// Osnowa GNSS w silniku: trwaly plik GothicSurveying/osnowa.csv, znaki (repery) w swiecie niezalezne od robot.
// Dolaczane raz z Plugin.hpp po Placement.hpp i przed World.hpp, bez straznika include.
#include <filesystem>
#include "calc/WorldControl.hpp"
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace GOTHIC_NAMESPACE {
  inline const char*                  LV_CONTROL_FILE = "GothicSurveying/osnowa.csv";
  inline std::vector<ControlPoint>    gControl;                 // cala osnowa z pliku (wszystkie swiaty)
  inline bool                         gControlLoaded = false;
  inline std::map<std::string, zCVob*> gControlMarkers;         // id -> znak w biezacym swiecie
  inline std::string                  gControlSyncWorld;        // swiat, dla ktorego postawiono znaki
  inline DWORD                        gControlLastSync = 0;

  inline std::string ControlWorldName() {                       // nazwa biezacego swiata (np. NEWWORLD); "" gdy brak
    oCWorld* w = ogame ? ogame->GetGameWorld() : nullptr;
    return w ? std::string( w->GetWorldName().ToChar() ) : std::string();
  }

  inline void ControlLoad() {
    std::ifstream f( LV_CONTROL_FILE, std::ios::binary );
    std::stringstream ss; ss << f.rdbuf();
    gControl = ControlFromCsv( ss.str() );                       // brak pliku = pusta osnowa
    gControlLoaded = true;
  }
  inline void ControlEnsureLoaded() { if ( !gControlLoaded ) ControlLoad(); }

  inline bool ControlSave() {
    std::error_code ec; std::filesystem::create_directories( "GothicSurveying", ec );
    std::ofstream out( LV_CONTROL_FILE, std::ios::binary | std::ios::trunc );
    out << ControlToCsv( gControl );
    return out.good();
  }

  inline void ControlForgetMarkers() {                          // po zmianie swiata: oddaj referencje bez dotykania swiata
    for ( auto& kv : gControlMarkers ) DropVobRef( kv.second );
    gControlMarkers.clear();
  }
  inline bool ControlMarkersStale() { for ( auto& kv : gControlMarkers ) if ( VobIsStale( kv.second ) ) return true; return false; }

  // Stawia brakujace znaki punktow osnowy biezacego swiata (bez duplikatow); po zmianie swiata lub wczytaniu zapisu zaczyna od nowa.
  inline void ControlSyncMarkers() {
    if ( !ogame || !ogame->GetWorld() ) return;
    ControlEnsureLoaded();
    const std::string wn = ControlWorldName();
    if ( wn != gControlSyncWorld || ControlMarkersStale() ) { ControlForgetMarkers(); gControlSyncWorld = wn; }
    for ( const ControlPoint& cp : gControl )
      if ( ControlWorldKey( cp.world ) == ControlWorldKey( wn ) && !gControlMarkers.count( cp.id ) ) gControlMarkers[cp.id] = SpawnVob( LV_VIS_REPER, zVEC3( (float)cp.x, (float)cp.y, (float)cp.z ) );
  }

  // Co 0,5 s: pilnuje znakow (nowy swiat, wczytany zapis).
  inline void ControlTick() {
    const DWORD now = GetTickCount();
    if ( now - gControlLastSync < 500 ) return;
    gControlLastSync = now;
    ControlSyncMarkers();
  }

  // Wczytuje wzorcowa osnowe geodezyjna biezacego swiata (dane wbudowane we wtyczke): dopisuje brakujace punkty do osnowy, zapisuje plik,
  // stawia znaki. Zwraca liczbe dopisanych punktow (0 = juz zaladowana albo brak zestawu dla swiata).
  inline int ControlLoadWorldSet() {
    ControlEnsureLoaded();
    const int added = ControlMergeWorldSet( gControl, ControlWorldName() );
    if ( added > 0 ) { ControlSave(); ControlSyncMarkers(); }
    return added;
  }

  // Dodaje punkt do osnowy: id, swiat, zapis pliku, znak w swiecie. Zwraca id; "" przy bledzie zapisu (punkt nie zostaje w pamieci).
  inline std::string ControlAdd( ControlPoint cp ) {
    ControlEnsureLoaded();
    cp.id = ControlNextId( gControl ); cp.world = ControlWorldKey( ControlWorldName() );
    gControl.push_back( cp );
    if ( !ControlSave() ) { gControl.pop_back(); return ""; }
    ControlSyncMarkers();
    return cp.id;
  }
}
